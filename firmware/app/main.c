/* M3 application (bench):  boots, serves the watchdog, receives and decodes the vehicle bus, reads the
 * transmission sensors (speeds, fluid temperature, supply, park contacts) and sends bench status
 * frames on CAN bus 1. Solenoid outputs only through the bench test routine (app/bench_out.h).
 * Range / park control (M4a, app/park.h) runs and publishes its decisions; it drives nothing.
 *
 * Heartbeat (BENCH USE ONLY, identifier not cleared for any vehicle bus):
 *   ID 0x7F0, DLC 8, every 100 ms
 *   byte 0     rolling counter (starts at 0 after every reset)
 *   byte 1     flags: bit0 = at least one missed 1 ms tick since start; bit1 = supervisor
 *              replied correctly to the last frame; bit2 = a bad supervisor reply was seen;
 *              bit3 = output enable sent to the supervisor
 *   bytes 2-3  uptime in seconds (big-endian)
 *   bytes 4-7  idle-loop count during the last 100 ms (big-endian); measured against the
 *              20 MHz peripheral clock this gives the CPU clock (spec 05 section 1.2)
 *
 * Input status (BENCH USE ONLY): ID 0x7F1, DLC 8, every 100 ms
 *   bytes 0-1  engine speed, rpm (big-endian)
 *   byte 2     lever range request (5 D, 6 N, 7 R, 8 P, 0 none)
 *   byte 3     lever source (0 CAN-C 0x2EA, 1 shifter bus 0x210, 2 none)
 *   bytes 4-5  input quality bits, 1 = valid: bit0 engine speed, 1 torque T1, 2 T2, 3 T3,
 *              4 pedal, 5 engine temp, 6 brake, 7 engine mode, 8 park speed, 9 soak,
 *              10 park request, 11 lever
 *   byte 6     pedal, 0.5 % per bit
 *   byte 7     brake state
 * The same 8 bytes can be read over UDS as identifier 0xFD00.
 *
 * Sensor status (BENCH USE ONLY): ID 0x7F2, DLC 8, every 100 ms (UDS identifier 0xFD01)
 *   bytes 0-1  speed channel 0 (TIF12), rpm      bytes 2-3  speed channel 1 (TIF13), rpm
 *   byte 4     fluid temperature, degC + 40 (0xFF = not valid)
 *   byte 5     battery, 0.1 V
 *   byte 6     bits 1..0 park (0 unknown, 1 in P, 2 not in P); bit 2 wake 1; bit 3 wake 2;
 *              bits 7..4 quality bits: speed 0, speed 1, fluid temp, battery (1 = valid)
 *   byte 7     fluid temperature fault bits
 *
 * Output status (BENCH USE ONLY): ID 0x7F3, DLC 8, every 100 ms (UDS identifier 0xFD02)
 *   byte 0     driver fault bits (SOL_F_*), byte 1 bit0 drivers ready, bit1 supervisor output
 *              enable sent, bit2 bench test active
 *   byte 2     bench channel, bytes 3-4 bench current mA, byte 5 remaining time 0.1 s,
 *   byte 6     reason the last bench test ended, byte 7 chip 0 status byte
 *
 * Flash programming (spec 16): routine FE30 starts the RAM flasher; identifier FD03 = FWE pin;
 * FD04 = FWE and port registers as the boot software left them (captured at entry);
 * FD05 = CPU clock probe (spec 05 section 1.2).
 *
 * Measurement protocol (spec 10, read-only): 0x361 -> 0x398 on either bus; project index list
 * in app/meas_table.c.
 */
#include "platform/platform.h"
#include "bsw/sched/sched.h"
#include "bsw/sup/sup.h"
#include "bsw/sup/sup_engage.h"
#include "bsw/com/com.h"
#include "bsw/diag/uds.h"
#include "bsw/meas/meas.h"
#include "io/vehicle/vehicle.h"
#include "io/sensors/sensors.h"
#include "io/actuators/sol_drv.h"
#include "app/bench_out.h"
#include "app/park.h"
#include "app/bench_dump.h"
#include "app/flash_entry.h"
#include "io/vehicle/fca_tx.h"
#include "app/meas_table.h"

#define HEARTBEAT_ID 0x7F0u
#define INPUTS_ID    0x7F1u
#define SENSORS_ID   0x7F2u
#define OUTPUTS_ID   0x7F3u

static uint8_t s_counter;

/* E17 (spec 03 section 6.2o): the boot-timeline stamps, read post-boot via the
 * measurement RAM read. g_sched_ms = ms since the scheduler's first 1 ms tick;
 * g_first_sup_ms = the stamp of the first 5 ms supervisor pass. Together with
 * g_sup_rx_hist[] in sup.c (the 0x22 echo of the first 64 passes) one power cycle
 * decides whether the rail is still up when our stream starts and drops after our
 * words, or is already down at pass one (a boot-manager-reign drop). */
uint32_t g_sched_ms;
uint32_t g_first_sup_ms;

/* [I] shaft assignment of the speed channels: confirm on the bench (spec 02 section 4.1) */
static const sensors_cfg_t s_sensors_cfg = { 0u, 1u, { 60u, 60u, 60u } };

static void task_1ms(void)
{
    plat_mb_tick_1ms();   /* input-word history + late-window handshake */
    can_frame_t f;

    g_sched_ms++;                   /* E17: the boot-timeline clock (ms since sched start) */

    plat_wdt_service();
    sensors_tick_1ms();
    for (uint8_t bus = 0u; bus < (uint8_t)CAN_BUS_COUNT; bus++) {
        for (uint8_t n = 0u; n < 8u && plat_can_recv((can_bus_t)bus, &f); n++) {
            if (f.id == 0x7F4u && f.dlc == 8u && f.data[0] == 0xE1u && f.data[1] == 0x12u &&
                f.data[2] == 0xB0u && f.data[3] == 0x0Bu) {
                /* E13: the host-armed window, now at the stock's engagement age —
                 * data[4] = delay seconds (default 12, the boot-era capture's ~+15s
                 * minus the app's own start lag; spec 03 section 6.2i).
                 * E20 (spec 03 section 6.2o): data[5] != 0 = the delay in RAW
                 * 5 ms passes — pass-granular arming so bit 0 can fire INSIDE the
                 * ~320 ms rail window (e.g. 0x32 = pass 50, just after HOLD at
                 * pass 49), the one cell never tested: the fully-mirrored stream
                 * asking while the rail is still up. Cycles raised to 2000
                 * (10 s of asking; the stock never stops). */
                sup_bit0_boot_arm(2000u, f.data[5] != 0u ? f.data[5] : (uint16_t)(f.data[4] * 200u));
                continue;
            }
            if (!uds_rx((can_bus_t)bus, &f) && !meas_rx((can_bus_t)bus, &f)) {
                vehicle_rx((can_bus_t)bus, &f);
            }
        }
    }
    uds_tick_1ms();
    fca_tx_tick_1ms();      /* stock transmit messages: all disabled */
    flash_entry_tick_1ms(); /* hands over to the RAM flasher after routine FE30 */
}

static void task_10ms(void)
{
    static uint8_t s_mb_div;
    static uint8_t s_sol_lvl = 0xFFu;

    if (++s_mb_div >= 10u) {    /* boot-mailbox machine at ~100 ms (spec 03 section 6.2) */
        s_mb_div = 0u;
        plat_mb_tick();
    }
    {   /* supervisor ladder -> bus-0 commissioning mirror (spec 03 section 6.2e): every
         * level change re-runs the configuration job with the 0x8E row = 0x80 | level,
         * the stock's live law (0x9F at HOLD). Runs before sol_tick so the job leaves
         * in the same pass. */
        const uint8_t lvl = sup_engage_status()->lvl_cmd;

        if (lvl != s_sol_lvl) {
            s_sol_lvl = lvl;
            sol_engage_config(lvl);
        }
    }
    vehicle_update_10ms();
    sensors_update_10ms();
    park_tick_10ms();       /* M4a: decisions only, no outputs */
    bench_tick_10ms();      /* safety interlocks first, then the driver job */
    sol_tick_10ms();
    meas_table_update();
    meas_tick_10ms();
}

static void inputs_status(uint8_t d[8])
{
    const vehicle_in_t *v = vehicle_in();
    const uint16_t bits = vehicle_quality_bits(v);

    d[0] = (uint8_t)((uint16_t)v->engine_speed >> 8);
    d[1] = (uint8_t)v->engine_speed;
    d[2] = (uint8_t)v->lever;
    d[3] = (uint8_t)v->lever_src;
    d[4] = (uint8_t)(bits >> 8);
    d[5] = (uint8_t)bits;
    d[6] = (uint8_t)(v->pedal / 5u);
    d[7] = (uint8_t)v->brake;
}

static void sensors_status(uint8_t d[8])
{
    const sensors_in_t *v = sensors_in();
    const int16_t t = (int16_t)(v->fluid_temp / 10 + 40);

    d[0] = (uint8_t)((uint16_t)v->ch_speed[0] >> 8);
    d[1] = (uint8_t)v->ch_speed[0];
    d[2] = (uint8_t)((uint16_t)v->ch_speed[1] >> 8);
    d[3] = (uint8_t)v->ch_speed[1];
    d[4] = (uint8_t)(v->q_fluid_temp != Q_INVALID && t >= 0 && t < 255 ? t : 0xFF);
    d[5] = (uint8_t)(v->battery / 100u > 255u ? 255u : v->battery / 100u);
    d[6] = (uint8_t)((uint8_t)v->park | (v->wake1 ? 0x04u : 0u) | (v->wake2 ? 0x08u : 0u) |
                     (v->q_ch_speed[0] == Q_OK ? 0x10u : 0u) | (v->q_ch_speed[1] == Q_OK ? 0x20u : 0u) |
                     (v->q_fluid_temp == Q_OK ? 0x40u : 0u) | (v->q_battery == Q_OK ? 0x80u : 0u));
    d[7] = v->fluid_temp_faults;
}

static void outputs_status(uint8_t d[8])
{
    const sol_status_t *sv = sol_status();
    uint8_t res[7];

    (void)bench_routine(3u, BENCH_RID, res, 0u, res, sizeof res);
    d[0] = sv->faults;
    d[1] = (uint8_t)((sv->ready ? 1u : 0u) | (sup_status()->enable ? 2u : 0u) |
                     (bench_active() ? 4u : 0u));
    d[2] = res[1];
    d[3] = res[2];
    d[4] = res[3];
    d[5] = res[4];
    d[6] = res[5];
    d[7] = sv->chip[0].status_chip;
}

static void send_inputs(void)
{
    can_frame_t f = { .id = INPUTS_ID, .dlc = 8u };

    inputs_status(f.data);
    (void)plat_can_send(CAN_BUS1, &f);
    f.id = SENSORS_ID;
    sensors_status(f.data);
    (void)plat_can_send(CAN_BUS1, &f);
    f.id = OUTPUTS_ID;
    outputs_status(f.data);
    (void)plat_can_send(CAN_BUS1, &f);
}

/* Leaving the application (programming session, ECU reset) only while the engine is not
 * running. M1 drives no outputs, so no output state has to be prepared first. */
static bool leave_allowed(void)
{
    const vehicle_in_t *v = vehicle_in();
    return !bench_active() && (v->q_engine_speed != Q_OK || v->engine_speed < 50);
}

static int read_did(uint16_t did, uint8_t *out, uint16_t max)
{
    if (did == 0xFD00u && max >= 8u) {
        inputs_status(out);
        return 8;
    }
    if (did == 0xFD01u && max >= 8u) {
        sensors_status(out);
        return 8;
    }
    if (did == 0xFD02u && max >= 8u) {
        outputs_status(out);
        return 8;
    }
    if (did == FLASH_DID) {
        return flash_entry_did(out, max);
    }
    if (did == 0xFD05u) {                   /* CPU clock probe (spec 05 section 1.2) */
        const uint16_t n = plat_clock_probe(out, max);
        return n != 0u ? (int)n : -1;
    }
    if (did == 0xFD04u) {                   /* boot software's FWE / port state at entry */
        const uint16_t n = plat_entry_snapshot(out, max);
        return n != 0u ? (int)n : -1;
    }
    return -1;
}

static int app_routine(uint8_t type, uint16_t rid, const uint8_t *in, uint16_t n, uint8_t *out,
                       uint16_t max)
{
    if (rid == DUMP_RID) {
        return bench_active() ? -0x22 : dump_routine(type, rid, in, n, out, max);
    }
    if (rid == FLASH_RID || rid == FLASH_EN_RID || rid == SUP_PAT_RID || rid == SUP_RAW_RID ||
        rid == SUP_AUTH_RID || rid == SUP_MISR_RID || rid == SUP_BURST_RID ||
        rid == SUP_FOLD_RID) {
        return flash_entry_routine(type, rid, in, n, out, max, leave_allowed());
    }
    return bench_routine(type, rid, in, n, out, max);
}

static const uds_app_t s_uds_app = { leave_allowed, read_did, app_routine };

static void task_100ms(void)
{
    const sched_status_t *st = sched_status();
    uint32_t idle = plat_idle_take();
    uint32_t up_s = st->now_ms / 1000u;
    can_frame_t f = { .id = HEARTBEAT_ID, .dlc = 8u };

    f.data[0] = s_counter++;
    {
        const sup_status_t *sv = sup_status();
        f.data[1] = (uint8_t)((st->missed_ticks != 0u ? 0x01u : 0u) |
                              ((sv->last_rx & SUP_REPLY_MASK) == SUP_REPLY_OK ? 0x02u : 0u) |
                              (sv->replies_bad != 0u ? 0x04u : 0u) | (sv->enable ? 0x08u : 0u));
    }
    f.data[2] = (uint8_t)(up_s >> 8);
    f.data[3] = (uint8_t)up_s;
    f.data[4] = (uint8_t)(idle >> 24);
    f.data[5] = (uint8_t)(idle >> 16);
    f.data[6] = (uint8_t)(idle >> 8);
    f.data[7] = (uint8_t)idle;
    (void)plat_can_send(CAN_BUS1, &f);
    send_inputs();
}

static void task_5ms(void)
{
    if (g_first_sup_ms == 0u) {     /* E17: ms-stamp of the first supervisor pass */
        g_first_sup_ms = g_sched_ms;
    }
    sup_tick_5ms();     /* external supervisor service; output enable stays off */
    /* E19 (spec 03 section 6.2o): the stock's 5 ms engine also queues the bus-0
     * ID job every pass (0x921A0) — the one 5 ms behavior we never mirrored. The
     * RSPI-A lines run to the mechatronic connector; keep them alive. */
    (void)sol_id_poll();
}

static const task_t s_tasks[] = {
    { task_1ms,     1u, 0u },
    { task_5ms,     5u, 2u },
    { task_10ms,   10u, 1u },
    { task_100ms, 100u, 5u },
};

int main(void)
{
    plat_mb_capture();     /* counterpart input FIRST: it may be a power-up transient that
                            * is gone before the scheduler runs (spec 03 section 6.2) */
    plat_init();
    plat_stock_init_mirror();   /* the stock's system writes (spec 03 section 6.2): the
                                 * mailbox input word may depend on the module-stop state */
    (void)plat_mb_poll_init();  /* the valid window may open between capture and here */
    sup_init();
    sol_init();             /* identification + configuration; all channels off */
    (void)plat_mb_poll_init();
    bench_init();
    (void)plat_mb_poll_init();
    vehicle_init();
    park_init();
    sensors_init(&s_sensors_cfg);
    fca_tx_init();
    uds_init(&s_uds_app);
    meas_table_init();
    (void)com_register_hw();
    plat_can_rx_commit();       /* ascending-id mailboxes: the RCAN ID-reorder hardware
                                 * drops ids whose search crosses an unsorted inversion (bench
                                 * 2026-11-03: 0x361 dead while all other ids received) */
    (void)plat_can_rx_register(CAN_BUS1, 0x7F4u);   /* E12 host-armed boot window (spec 03
                                                     * section 6.2h): no self-arm — the bench
                                                     * host arms the early bit-0 window with
                                                     * ONE magic frame at power-on; reset boots
                                                     * never see it again, so a rejecting
                                                     * supervisor cannot loop (E11 lesson) */
    plat_can_rx_commit();
#if defined(TCU_AUTOENGAGE)
    /* E12 (spec 03 section 6.2h): NO self-armed bit-0 window. E11's self-arm looped
     * silently — no state survives the supervisor's reject reset (the boot clears user
     * RAM each handover, or the reject is POR-class; bench 2026-11-05, the loop ran
     * unbounded with the ammeter at idle). The boot window is armed from the bench
     * host instead: one magic frame (id 0x7F4, E1 12 B0 0B) in the first ~50 ms,
     * handled in task_1ms — reset boots never see it again, so the loop is
     * structurally impossible. */
    /* Bench build (m4 series): the supervisor's acceptance window is a BOOT
     * phenomenon — the engaged stock's echo bit0 is already latched at the
     * first readable instant after power-up (shadow-poll campaign 2026-10-31).
     * Our host-armed sequence ran minutes late; the stock's monitor starts
     * within ~100 ms of app boot. Self-arm the full commissioning at init:
     * pattern + ID poll + write + setpoints + tokens + ramp, enable as a
     * grant-gated shadow (cannot reset the TCU — the A-law, 2026-11-01).
     * E9 (spec 03 section 6.2g): the cyclic word takes the ENGAGED shape
     * 0x8E90/0x8E80 — bit 7 held, BIT 4 toggling (2026-10-31 capture; the
     * boot software's bit 2 was the pre-E1 shape) — with bit 0 still withheld
     * by the A-law; the stock's live pair 0x91/0x81 = this base plus bit 0.
     * E10: the job-2 register-0x90 data = the stock's live 0x21 (we sent 0x20
     * since E1 — the switch-enable register's bit 0, the last known cyclic
     * word delta).
     * E16 (spec 03 section 6.2n): the composer decode settles it — the word's
     * bit 0 comes only from the base cell 0xFFF90963 (flash const 0x1918EF =
     * 0x00 on this unit), so the stock's STEADY word is 0x9020 (driving dump:
     * TX 0x9020, 0x24 read-back 0xAA20); the boot-era 0x9021 was the transient
     * pre-calibration base. Our 0x9021 made SSLB1's 0x24 read-back answer
     * 0xAAFF instead of 0xAA20 (E15 RAM read, rx_words[5]) — the register did
     * not hold what we wrote. Back to 0x20 = the stock's steady composition
     * (slot-0 'c' bit 5, tail bit clear). */
    sup_app_pattern3(true, 0x90u, 0x10u, 0x20u);
    sup_drv_sync(true);
    /* Engaged channel staging (spec 03 section 6.2e): the live 0x88 mode flags of
     * the driving stock = the six pressure channels + line pressure in CURRENT
     * mode, park release DISABLED, unused channels OFF. E15 (spec 03 section
     * 6.2m): the stock's keep-warm machine commands two of the CURRENT channels
     * continuously (live cells 0xFFF90586/0xFFF9058A = {11,53}/{11,47}; the
     * driving dump stages E853 on chips 0 and 3) - chip 0 ch 0 and chip 3 ch 0
     * carry setpoint 0x53 (498 = 49.8 mA), every other current stays 0. If the
     * rail ever closes, that is ~100 mA total - announce-class, nowhere near the
     * stock's 1.8 A drive (which we do NOT replicate); FE10 still overrides the
     * currents during bench tests. The stock's keep-warm VALUE CYCLE (0x90586
     * walks 41/34/3A/27/0E/1F/3C/26/44) is not decoded - static snapshot. */
    for (uint8_t ch = 0u; ch < 7u; ch++) {
        sol_set(ch, SOL_MODE_CURRENT, (ch == 0u || ch == 6u) ? 498u : 0u);
    }
    sol_set(7u, SOL_MODE_DISABLED, 0u);
    sol_set(8u, SOL_MODE_OFF, 0u);
    sol_set(9u, SOL_MODE_OFF, 0u);
    sup_engage_arm(true);
    sup_engage_request_enable(true);
#endif
    sched_init(s_tasks, (uint8_t)(sizeof s_tasks / sizeof s_tasks[0]));

    for (;;) {
        if (plat_tick_poll()) {
            sched_tick();
        } else {
            plat_idle_count();
        }
    }
}

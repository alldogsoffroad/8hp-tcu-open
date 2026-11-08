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
static bool s_e40_armed;        /* E40 (spec 03 6.2ac): repeating engaged write job armed
                                 * by the magic frame's data[6]; grant-gated in task_10ms */
static uint8_t s_e40_v8e;       /* E40: its live 0x8E row value (0x9F = stock constant) */
static bool s_e42_armed;        /* E42: the bus-B setpoint job armed by data[7] */
static bool s_e45_cyclic;       /* E45: data[7]=2 selects the cyclic companion */

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
                 * ~320 ms rail window (e.g. 0x32 = pass 50, just after HOLD at pass
                 * 49), the one cell never tested: the fully-mirrored stream asking
                 * while the rail is still up. Cycles raised to 2000 (10 s of asking;
                 * the stock never stops).
                 * E40 (spec 03 section 6.2ac): data[6] != 0 arms the REPEATING
                 * engaged write job with the 0x8E row = data[6] (the stock's job
                 * 0x28 broadcast 8E9F x5 / 9021 x5, repeating while engaged; af01's
                 * 0x24 switch echo never sees the engaged values otherwise).
                 * Grant-gated: the re-send runs only while the A-law shadow is
                 * sending bit 0, so the engaged values cannot precede the grant.
                 * data[6] = 0 leaves it unarmed (this frame does not disarm a
                 * running re-send — power cycle to clear). */
                if (f.data[4] == 0xFEu) {     /* E54 (spec 03 6.2aj): data[6] = the
                                              * driver-bus bit rate override (the
                                              * writes-don't-stick sweep). Does not
                                              * re-arm the ask. */
                    plat_drv_spbr_set(f.data[6]);
                    continue;
                }
                sup_bit0_boot_arm(2000u, f.data[5] != 0u ? f.data[5] : (uint16_t)(f.data[4] * 200u));
                if (f.data[6] != 0u) {
                    s_e40_armed = true;
                    s_e40_v8e = f.data[6];
                }
                if (f.data[7] != 0u) {   /* E42: the bus-B setpoint job (nine-row
                                          * five-select, keep-warm cycling).
                                          * E45 (6.2af): data[7] == 2 arms the bus-B
                                          * CYCLIC COMPANION instead — the stock's
                                          * 45-frame driver cyclic with the supervisor
                                          * witnessing the setpoints at frame 2. */
                    s_e42_armed = true;
                    s_e45_cyclic = f.data[7] >= 2u;
                }
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
        /* E41/E42 (spec 03 section 6.2ad): the bus-B write and setpoint jobs' gating
         * and ~2 s cadence live inside sup_drv_poll (it reads the engage state
         * directly); the magic frame's arms are forwarded there. E40's RSPI-A re-send
         * is gone — it never reached af01 and its burst withdrew the grant (6.2ac). */
        sup_drv_write_arm(s_e40_armed, s_e40_v8e);
        sup_drv_setpoint_arm(s_e42_armed && !s_e45_cyclic);
        /* E45: the cyclic companion mirrors the E43 window-load staging (the
         * park-hold class magnitude); 250 mA -> setpoint 417 (0.6 mA steps). */
        sup_drv_cyclic_arm(s_e45_cyclic, 417u);
    }
    vehicle_update_10ms();
    sensors_update_10ms();
    park_tick_10ms();       /* M4a: decisions only, no outputs */
    bench_tick_10ms();      /* safety interlocks first, then the driver job */
    sol_tick_10ms();
#if defined(TCU_AUTOENGAGE)
    /* E43 (spec 03 section 6.2ae): the window load race. The rail is UP for the
     * first ~370 ms after power-on (passes 2-64, the ring's 0xB0 era) and gates
     * all delivery (keep-warm reads flat at .166 with the rail down — no rail,
     * no current). The stock's solenoid leap lands at +200-250 ms, INSIDE its
     * window, at real current. 6.2w's load-in-window elimination ("1.75 A
     * commanded, zero delivered") ran while bench_channels_off() was wiping
     * every channel every 10 ms — the assassin found later; the test was never
     * re-run clean. Here: channels 0-6 at 250 mA each (1.75 A total) for the
     * first 50 ticks (500 ms, spanning the window), then the keep-warm cycle.
     * If the supervisor's rail-hold decision is current-gated, the rail holds
     * past pass 65 — the ammeter shows it live. */
    {
        static uint8_t s_boot_ticks;
        if (s_boot_ticks < 50u) {
            s_boot_ticks++;
            for (uint8_t ch = 0u; ch <= 6u; ch++) {
                sol_set(ch, SOL_MODE_CURRENT, (ma10_t)2500u);
            }
        } else {
            /* E37 (spec 03 section 6.2aa): the stock's keep-warm VALUE CYCLE. The
             * stock's cells 0xFFF90586/8A walk a 9-value table: 41/34/3A/27/0E/1F/
             * 3C/26/44 (hex, driver setpoint units). Two channels cycle: ch0 and
             * ch6, the cycle advancing every 10 ms. Values in 0.1 mA: x6 (spec 12
             * section 2b). */
            static const uint8_t KEEPWARM[9] = {
                0x41u, 0x34u, 0x3Au, 0x27u, 0x0Eu, 0x1Fu, 0x3Cu, 0x26u, 0x44u
            };
            static uint8_t kw_idx = 0u;
            sol_set(0u, SOL_MODE_CURRENT, (ma10_t)(KEEPWARM[kw_idx] * 6u));
            sol_set(6u, SOL_MODE_CURRENT, (ma10_t)(KEEPWARM[(kw_idx + 4u) % 9u] * 6u));
            kw_idx = (uint8_t)((kw_idx + 1u) % 9u);
        }
    }
#endif
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
                              (sv->replies_bad != 0u ? 0x04u : 0u) | (sv->enable ? 0x08u : 0u) |
                              0x10u);   /* E60f: feed armed from init */
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
    /* E42c (spec 03 section 6.2ad): the boot-trip counter. Survives every reset the
     * supervisor can issue (RAM stays powered; the .noinit section is NOLOAD) and is
     * cleared only by power-off. Read live via 0x2A: a value > 1 on a single power
     * cycle = the TCU soft-reset mid-run (the E42 forensics question: "withdrawal"
     * vs the E28 stall-reset — settled by this byte, not by inference). */
    __attribute__((section(".noinit"))) static uint8_t s_boot_trips;
    s_boot_trips++;
    plat_mb_capture();     /* counterpart input FIRST: it may be a power-up transient that
                            * is gone before the scheduler runs (spec 03 section 6.2) */
    plat_init();
    plat_stock_init_mirror();   /* the stock's system writes (spec 03 section 6.2): the
                                 * mailbox input word may depend on the module-stop state */
    (void)plat_mb_poll_init();  /* the valid window may open between capture and here */
    sup_init();
#if defined(TCU_AUTOENGAGE) && !defined(TCU_E60G_QUIET)
    sol_stream_gate(true);  /* E31 (spec 03 section 6.2w): the load-latch test - the
                             * drivers commission and deliver from the first 10 ms
                             * cyclic, racing the ~370 ms rail window (the stock's
                             * solenoid leap lands at +200-250 ms, anchored capture
                             * 2026-11-06). Bus-0 runs from app start. */
#endif
#if defined(TCU_E60G_QUIET)
    /* E60g (spec 03 6.2au): the grant A/B. Every historical grant (E38/39 3/3,
     * E41b lease-hold) ran with bus-0 DEAD ON THE WIRE - the five-select
     * rotation's words at the supervisor's af01 slot never left the MCU. E60f
     * made the transport real: the ask fully sends (delay=0/left=0), the
     * ladder reaches HOLD, the mirror is alive - and the supervisor refuses
     * (0x22 frozen AA90), fresh boot or aged, stream open or manually closed
     * after the fact. Hypothesis: the LIVE cyclic words on the supervisor's
     * select poison the parser from the first pass. This build gates bus-0
     * OFF from init - zero words on af01 - restoring the historical wire
     * condition deliberately. The bench can still open it live (FE13 sub0F). */
    sol_stream_gate(false);
#endif
    sol_init();             /* identification + configuration; all channels off */
    (void)plat_mb_poll_init();
    bench_init();
    (void)plat_mb_poll_init();
    vehicle_init();
    park_init();
    sensors_init(&s_sensors_cfg);
    fca_tx_init();
#if 0 && defined(TCU_AUTOENGAGE)
    /* E36 (spec 03 section 6.2aa): enable the stock's CAN transmit schedule. DISABLED
     * in E39: the grant (E25 law: mirror + HOLD + one bit-0 word) came 7/7 on E31b
     * with the schedule OFF and failed on every schedule-ON build since (E36, E37,
     * E38 — the latter with the startup handshake removed, exonerating it; bus-0
     * traffic is exonerated too, E31b carried it from app start). E39 = E38 minus
     * this loop: mode bytes + keep-warm cycling stay. If the grant returns, the
     * schedule is the breaker; if not, mode bytes / keep-warm are next. */
    for (uint8_t m = 0u; m < (uint8_t)FCA_TX_MSG_COUNT; m++) {
        fca_tx_enable((fca_tx_msg_t)m, true);
    }
#endif
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
    /* E48 (spec 03 section 6.2af): bits90 bit 0 (0x21, not 0x20) — the driver-comms
     * mode bit. E16 read the stock's engaged 0x9021's bit 0 as a "boot-era transient"
     * and settled us on 0x20; with today's record-2 decode (the supervisor IS chip 2,
     * a two-channel driver) the 0x21 is the app TELLING the supervisor it will be
     * addressed as a driver — without it, every register job on its select (E42/E45/
     * E46/E47's companions) is the reset-class violation. E16's bench: 0x21 alone was
     * benign (no reset; only the 0x24 read-back anomaly — pre-companion era). */
    sup_app_pattern3(true, 0x90u, 0x10u, 0x21u);
    sup_drv_sync(true);
    /* Engaged channel staging (spec 03 section 6.2e): the live 0x88 mode flags of
     * the driving stock = the six pressure channels + line pressure in CURRENT
     * mode, park release DISABLED, unused channels OFF. E15-E19 ran the stock's
     * keep-warm pair (2 x 49.8 mA); E31 supersedes: full-family setpoints from
     * app start (below). FE10 still overrides during bench tests. */
    /* E37 (spec 03 section 6.2aa): match the stock's EXACT channel assignments
     * and keep-warm setpoint cycling. The stock's engaged driving dump:
     *   chips 0-2 mode byte 0x1A (both channels CURRENT)
     *   chip 3 mode byte 0x12 (ch0 CURRENT, ch1 DISABLED)
     *   chip 4 mode byte 0x15 (both OFF — no chip on bench)
     *   keep-warm on ch0 (chip 0) and ch6 (chip 3): cycling setpoints
     *   from the stock's value table (spec 03 6.2m)
     * We had been driving all 7 channels at a static 250mA — completely
     * different from the stock's pattern. Now: exact mode match + cycling. */
    for (uint8_t ch = 0u; ch < 7u; ch++) {
        /* stock mode assignments: ch0-5 = CURRENT (pressure solenoids),
         * ch6 = CURRENT (line pressure), ch7 = DISABLED (park release) */
        sol_set(ch, SOL_MODE_CURRENT, 0u);  /* start at zero; cycling below */
    }
    sol_set(7u, SOL_MODE_DISABLED, 0u);
    sol_set(8u, SOL_MODE_OFF, 0u);
    sol_set(9u, SOL_MODE_OFF, 0u);
    sol_set(7u, SOL_MODE_DISABLED, 0u);
    sol_set(8u, SOL_MODE_OFF, 0u);
    sol_set(9u, SOL_MODE_OFF, 0u);
    sup_engage_arm(true);
    sup_engage_request_enable(true);
#endif
#if defined(TCU_AUTOENGAGE)
    /* E49 (spec 03 section 6.2ag): the STARTUP COMMISSIONING restored (E35r's
     * proven-safe handshake, removed in E38 on a theory that collapsed). The
     * stock's first SSLB1 jobs (transport 0x40CA0) commission the supervisor's
     * driver interface — the 0x8800 mode-register writes (channels disabled)
     * and the commit/read sequence — BEFORE the cyclic stream begins. The
     * E45-E48 wall (register jobs to the supervisor's select = reset under
     * every content/cadence/mode variation) is hypothesized to be the MISSING
     * commissioning: the stock's register writes ride on top of this startup
     * handshake; ours arrived uncommissioned. Sent through the existing
     * alternating transport (even frames -> SSLB1, odd -> SSLB0 reads). */
    {
        static const uint16_t w1[7] = {0x8800u, 0x8800u, 0x8800u, 0x8800u,
                                        0x1200u, 0x0C00u, 0x0A00u};
        static const uint16_t w2[4] = {0x841Fu, 0x0A00u, 0x1200u, 0x0C00u};
        uint16_t rx[8];
        (void)plat_sup_xfer(w1, rx, 7u);
        (void)plat_sup_xfer(w2, rx, 4u);
    }
    /* E51: the boot companion — the stock's driver cyclic from app start,
     * bit0-free (the stock's rail is up at +250 ms, 15 s before any bit0;
     * E50b proved our register jobs survive bit0-free). No ask is armed. */
    sup_drv_cyclic_boot(true);
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

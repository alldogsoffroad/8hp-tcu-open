/* main.c — the rebuilt application (design §6/§8, spec 21 chronology).
 *
 * Init order follows the stock's 19-call sequence at 0x66CA8 (validated,
 * spec 21 §3.1): peripherals → WDT first feed → RAM init → boot mailbox →
 * SR.I opens → init bank (walk, scheduler, CAN) → main_loop.
 *
 * The superloop drains the scheduler's rings (design §6): the 1 ms tick
 * ISR fills them; tasks run in the main loop.
 */
#include "platform/platform.h"
#include "bsw/sched/sched.h"
#include "bsw/sup/sup_cells.h"
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_walk.h"
#include "bsw/sup/sup_batch.h"
#include "bsw/sup/sup_ctrl.h"
#include "bsw/sup/sup_letters.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/com/com.h"
#include "bsw/meas/meas.h"
#include "bsw/diag/uds.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "io/vehicle/fca_tx.h"
#include "io/actuators/sol_ctl.h"
#include "app/bench_out.h"
#include "app/bench_dump.h"
#include "app/flash_entry.h"
#include "app/meas_table.h"
#include "app/park.h"

/* [I] shaft assignment of the speed channels (spec 02 section 4.1) */
static const sensors_cfg_t s_sensors_cfg = { 0u, 1u, { 60u, 60u, 60u } };

/* F2 (protocol_audit §1/F2): how long the ATU tick must be quiet before the
 * CMT fallback re-engages (the audit's "> 10 ms" un-retire threshold). The
 * CMT retires only on SUSTAINED ATU liveness, never on one harvested flag. */
#define ATU_TICK_QUIET_US 10000u

/* ── UDS application glue (carried): leave/did/routine routing ─────────── */
/* bench read-out records FD00/FD01/FD02 (carried wire formats) */
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
    uint8_t res[7] = { 0u };

    (void)bench_routine(3u, BENCH_RID, res, 0u, res, sizeof res);
    d[0] = 0u;                              /* driver fault bits: the drv handlers' cells are ISR-side */
    d[1] = (uint8_t)((drv_job_check_free(DRV_CYC) ? 1u : 0u) |
                     (CELL_GRANT != 0u ? 2u : 0u) | (bench_active() ? 4u : 0u));
    d[2] = res[1];
    d[3] = res[2];
    d[4] = res[3];
    d[5] = res[4];
    d[6] = res[5];
    d[7] = res[6];
}

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


/* ── IPR map (design §5.2; the interrupt sources we enable) ────────────── */
static const plat_ipr_ent_t s_ipr_entries[] = {
    {  4u, 12u, 0xEu },      /* DMAC4 (ADC_A end)          */
    {  4u,  8u,  4u },       /* DMAC5 (ADC_B end)          */
    {  5u,  0u, 0xEu },      /* WDT ITI (interval mode)    */
    { 10u,  4u,  6u },       /* ATU-D0 UDID00 U-compare — the walk tick (VECTOR 224;
                              * stock init 0x970FC arms IPR10 bits 7-4 = 6, raised
                              * to 8 by 0x97198; the old bits 15-12 entry pointed at
                              * the CMID00 C-compare group, which delivers no
                              * interrupt on this silicon — 2026-11-10 bench decode) */
    { 10u,  0u,  6u },       /* ATU-D1 tick                */
    { 27u,  8u,  4u },       /* RSPI-A SPRIA — IPR27 bits 11-8 |= 0x400 (stock 0x963CC
                             * bus-0 branch: and 0xF0FF, or 0x400; E60c-proven live).
                             * Spec 24's "A=nibble15-12" row was WRONG. */
    { 27u,  4u,  4u },       /* RSPI-B SPRIB — IPR27 bits 7-4 |= 0x40 (stock bus-1 branch:
                             * and 0xFF0F, bset#6). NEITHER 15-12 nor 11-8 — r01g's 190
                             * A-deliveries with zero B-deliveries proved it. */
};
static const plat_ipr_map_t s_ipr_map = {
    s_ipr_entries, sizeof(s_ipr_entries) / sizeof(s_ipr_entries[0]),
};

/* ── 1 ms: ADC + CAN pump + mailbox + WDT ──────────────────────────────── */
static void task_1ms(void)
{
    plat_mb_tick_1ms();
    sensors_tick_1ms();                 /* ADC sampling (spec 04) */
    can_frame_t f;
    /* drain both buses */
    for (uint8_t bus = 0u; bus < (uint8_t)CAN_BUS_COUNT; bus++) {
        for (uint8_t n = 0u; n < 8u && plat_can_recv((can_bus_t)bus, &f); n++) {
            (void)uds_rx((can_bus_t)bus, &f);
            (void)meas_rx((can_bus_t)bus, &f);
            vehicle_rx((can_bus_t)bus, &f);
        }
    }
    uds_tick_1ms();
    fca_tx_tick_1ms();
    flash_entry_tick_1ms();             /* FE30 handover countdown (spec 16) */
    plat_wdt_kick();
}

/* ── 5 ms: the supervisor batch (design §7.5, stock 0x8EE82) ─────────── */
static void task_sup_batch(void)
{
    sup_batch_5ms();
}

/* ── 2.5 ms (2/3 ms alternation): the walk monitor (stock 0x873F0) ────── */
static void task_sup_monitor(void)
{
    sup_monitor_poll();
}

/* ── 10 ms: application tasks ──────────────────────────────────────────── */
static void task_10ms(void)
{
    sensors_update_10ms();
    vehicle_update_10ms();
    park_tick_10ms();
    bench_tick_10ms();
    sol_tick_10ms();        /* builds the 45-frame cyclic, queues DRV_CYC */
    meas_table_update();
    meas_tick_10ms();
}

/* ── 100 ms: heartbeat ─────────────────────────────────────────────────── */
static can_frame_t s_hb;
static void task_100ms(void)
{
    s_hb.id = 0x7F0u;
    s_hb.dlc = 8u;
    s_hb.data[0] = 0xE1u;
    s_hb.data[1] = (uint8_t)((CELL_GRANT ? 0x08u : 0u) |
                              (CELL_WALK_STATE != 0u ? 0x10u : 0u));
    s_hb.data[2] = (uint8_t)(plat_tick_count() >> 8);   /* uptime hi */
    s_hb.data[3] = (uint8_t)plat_tick_count();            /* uptime lo */
    s_hb.data[4] = CELL_WALK_STATE;
    s_hb.data[5] = CELL_VALUE;
    s_hb.data[6] = CELL_LVL_REPORTED;
    s_hb.data[7] = CELL_JOB_MODE;
    (void)plat_can_send(CAN_BUS1, &s_hb);
}

/* ── the task table (design §6) ────────────────────────────────────────── */
static const sched_ent_t s_tasks[] = {
    {    1u,    0u, task_1ms,         RT_1MS  },
    {    5u,    2u, task_sup_batch,   RT_5MS  },
    /* the monitor runs every 2.5 ms in the stock (0x7EE2E -> 0x873F0, entry
     * 2 of the 2.5 ms task list, all modes). Our tick is 1 ms, so two
     * entries per 5 ms give a 2/3 ms alternation — the 2.5 ms mean, inside
     * the stock's own 25 ms / 9-pass (2.78 ms) supervision ceiling. */
    {    5u,    0u, task_sup_monitor, RT_5MS  },
    {    5u,    3u, task_sup_monitor, RT_5MS  },
    {   10u,    1u, task_10ms,        RT_10MS },
    {  100u,    5u, task_100ms,       RT_100MS },
};

/* ── init (the stock's 19-call order, simplified to our module set) ────── */
int main(void)
{
    /* order 3.1-2: boot mailbox input FIRST (it may be a power-up transient) */
    plat_mb_capture();

    /* order 3.1-5: peripheral bring-up + first WDT feed */
    plat_wdt_early_capture();           /* S11: before wdt_init rewrites WRCR/WTSR */
    plat_init();
    plat_app_init();                    /* S10 pin drives + B2 walk-timer calibration */
    plat_stock_init_mirror();           /* the stock's system writes (spec 03 6.2) */
    plat_wdt_capture_reset_cause();     /* once, before the first feed */
    plat_wdt_kick();                    /* the stock's first feed */

    /* order 3.1-6: RAM self-test + data/bss init (start.S) + mailbox */
    (void)plat_mb_poll_init();

    /* order 3.1-14: C runtime static-init (carried — GCC generates) */

    /* order 3.1-17/20: boot-cause handshake (the mailbox state machine) */

    /* order 3.1-21: SR.I lowered 0xF → 0xE (all maskable ISRs live) */
    plat_intc_setup(&s_ipr_map);       /* IPRs + IBNR=0 first */
    plat_rspi_init();                   /* both buses to idle SPCR=0x08 */
    plat_sr_open();                     /* then open the interrupt mask */

    /* order 3.1-23: init bank */
    sched_init(s_tasks, (uint8_t)(sizeof(s_tasks) / sizeof(s_tasks[0])));
    sup_cells_init();                   /* clear every pinned cell range first */
    sup_jobs_init();
    sup_walk_init();                    /* fault_manager_init → walk init */
    sup_batch_init();                   /* serial_rx_init: request 0, J5 staged+queued */
    drv_jobs_init();
    sol_init();
    bench_init();
    vehicle_init();
    park_init();
    sensors_init(&s_sensors_cfg);
    fca_tx_init();
    meas_table_init();                  /* meas_init with the project index list */
    uds_init(&s_uds_app);
    (void)com_register_hw();
    plat_can_rx_commit();               /* ascending-id mailboxes: the RCAN ID-reorder
                                         * hardware drops ids whose search crosses an
                                         * unsorted inversion (bench 2026-11-03) */

    /* start the 1 ms tick (the scheduler's heartbeat) */
    plat_tick_start();

    /* J5 as the FIRST bus-B job (design A7; stock serial_rx_init) — already
     * staged and queued by sup_batch_init above */
    drv_id_kick();                      /* bus-A ID too (stock 0x921A0) */
    sup_job_engine_pump();              /* launch J5 on bus B */

    /* main_loop (stock 0x67A68): drain the rings forever. plat_tick_poll is
     * the HOST simulator's 1 ms source (the target's tick is the ATU-D1 ISR;
     * its poll always returns false). */
    for (;;) {
        /* S13 (baseline review): the unconditional CMT poll that stood here
         * fed the scheduler a second tick source alongside the ATU — double
         * rate the moment the ATU tick lives. The CMT now runs ONLY as the
         * gated fallback below (the host's 1 ms source takes that path too:
         * its plat_tick_isr_count() is 0, so s_atu_retired stays false). */
        /* r01e/g POLLED FALLBACKS: the superloop harvests every interrupt
         * flag directly — the firmware runs even with a dead INTC, and the
         * ISR counters (s_tick_isr_count, s_vec_count) tell the bench which
         * path carried the traffic. The CMT poll (r01b-proven) carries the
         * tick until the ATU hardware tick proves SUSTAINED and retires it;
         * if the ATU goes quiet again the CMT re-engages (F2: the old
         * single-fire self-retire left the bench with NO tick source at
         * all once TCNT1D1 froze — protocol_audit §1/F2). */
        static uint32_t s_atu_isr_prev;
        static uint32_t s_atu_live_us;
        static bool s_atu_retired;
        if (plat_tick_poll_hw()) {
            sched_tick_isr();                        /* polled ATU harvest */
            s_atu_live_us = plat_frt_us();
            s_atu_retired = true;
        }
        if (plat_tick_isr_count() != s_atu_isr_prev) {
            /* the true ISR is sustaining the tick (it calls
             * sched_tick_isr itself); a single harvested flag is not proof */
            s_atu_isr_prev = plat_tick_isr_count();
            s_atu_live_us = plat_frt_us();
            s_atu_retired = true;
        } else if (s_atu_retired &&
                   (plat_frt_us() - s_atu_live_us) >= ATU_TICK_QUIET_US) {
            s_atu_retired = false;                   /* ATU quiet: resume CMT */
        }
        if (!s_atu_retired && plat_tick_poll()) {
            sched_tick_isr();                        /* CMT fallback (r01b) */
        }
        plat_alive(0x01u);                       /* S11 breadcrumb: superloop */
        (void)plat_walktmr_poll();
        /* S1 (baseline review): NO RSPI polls. With interrupts open the
         * vector could fire between the poll's SPRF check and the body's
         * SPDR read — one frame processed twice, the replies shifted a slot,
         * the burst ended early (r02f: >= 40 frames sent, s_vec_count[1] =
         * 35). Vector delivery on both buses is proven on the bench, and the
         * stock has no polled path. plat_rspi_poll stays for the lab tools. */
        /* r01d DIAGNOSTIC: superloop WDT kick (a dead tick starves the
         * task-side kick; a hung SUPERLOOP still resets) */
        plat_wdt_kick();
        sched_drain();
    }
}

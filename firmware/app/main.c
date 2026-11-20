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
#include "bsw/sup/sup_mode.h"
#include "bsw/sup/sup_ctrl.h"
#include "bsw/sup/sup_letters.h"
#include "bsw/drv/drv_jobs.h"
#include "bsw/drv/drv_odiag.h"
#include "bsw/drv/drv_groupa.h"
#include "bsw/drv/drv_temp.h"
#include "bsw/drv/drv_pksens.h"
#include "bsw/com/com.h"
#include "bsw/meas/meas.h"
#include "bsw/diag/uds.h"
#include "io/sensors/sensors.h"
#include "io/vehicle/vehicle.h"
#include "io/vehicle/fca_tx.h"
#include "io/vehicle/diag_ids.h"
#include "io/vehicle/shadow_sim.h"   /* TCU_SHADOW_SIM: bench-only, feeds the shadows' hooks only */
#include "io/actuators/sol_ctl.h"
#include "app/bench_out.h"
#include "app/bench_dump.h"
#include "safety/park_pair.h"
#include "safety/kl15.h"
#include "app/flash_entry.h"
#include "app/meas_table.h"
#include "app/park.h"
#include "app/rng_chain.h"
#include "app/tmp_chain.h"
#include "app/gs_shadow.h"
#include "app/gs_bridge.h"
#include "bsw/drv/drv_reqlaw.h"
#include "safety/park_mon.h"
#include "app/fb_shadow.h"
#include "app/pa7_shadow.h"
#include "app/tsel_shadow.h"
#include "app/sm_shadow.h"
#include "app/fr_shadow.h"
#include "app/gp_shadow.h"
#include "app/park_law.h"
#if !defined(TCU_RAM_APP)
#include "app/g6a.h"
#include "safety/os_sel.h"
#endif

/* Shaft assignment of the speed channels (spec 02 section 4.1), the stock calibration's routing:
 * channel 0 = TIF12 -> stock signal B (0xFFF93048), channel 1 = TIF13 -> signal A (0xFFF93044).
 * Output = channel 0 [O bench 2026-11-16: the steel test counted on F12 only, and the user
 * identified that sensor as the output speed sensor at the rear of the transmission].
 * Turbine = channel 1 [O bench 2026-11-16: a magnet with alternating poles passed across the
 * input speed sensor (G182, which reads a magnetic ring) counted on F13 only].
 * Before 2026-11-16 this was turbine 0 / output 1, which fed TIF13 into the park arbiter.
 * Pulses per revolution = the stock's own, from the speed computation 0x143686 that feeds D646 /
 * 0xFFF8EE6E: output 40 (cal 0x195DCD = 0x28), turbine 30 (cal 0x195DCE = 0x1E) [O bytes;
 * ~/tcu_sweep/park_position_speed_decode.md]. The user accepts the stock values (2026-11-16); not
 * physically counted (the transmission cannot be opened now). Channel 2 (TIF14) is unused and
 * keeps the Timer F calibration's 60. Before 2026-11-16 all three were 60, which made output rpm
 * read 2/3 of the stock's. */
static const sensors_cfg_t s_sensors_cfg = { 1u, 0u, { 40u, 30u, 60u } };

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
    {  4u, 12u, 0xEu },      /* DMAC4 (ADC_B end — spec 24 had A/B swapped) */
    {  4u,  8u,  4u },       /* DMAC5 (ADC_A end, the 1 ms 0x8F058)        */
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
#if !defined(TCU_RAM_APP)
            /* F1: the read-only fault-reaction shadow takes the RAW frame
             * (app/fr_hook.c) before our com layer judges it (D-FR-8) */
            fr_hook_rx(bus, f.id, f.dlc, f.data);
            shadow_sim_rx(bus, f.id, f.dlc, f.data);   /* bench-only simulated speeds (shadows only) */
#endif
            vehicle_rx((can_bus_t)bus, &f);
        }
    }
    uds_tick_1ms();
    fca_tx_tick_1ms();
    flash_entry_tick_1ms();             /* FE30 handover countdown (spec 16) */
    /* the bus-A diag job 3.8 ms after each cyclic (stock: the DCNTD02
     * one-shot 0x92060 → ISR 0x92078 → 0x9225C; ours polled: drv_odiag.c),
     * queued in the bus-A ring (never dropped for a busy channel) */
    drv_od_diag_poll(plat_frt_us());
    plat_wdt_kick();
}

/* ── 5 ms: the supervisor batch (design §7.5, stock 0x8EE82) — and, in
 *    OS mode 1, the stock's 10 ms supervisor entries on every second pass
 *    (the reply proc, pin cut, F stage and batch move there; the J2 stage
 *    stays at 5 ms): sup_batch.c, ~/tcu_sweep/mode1_port.md ──────────── */
static void task_sup_batch(void)
{
    plat_trace((uint8_t)'B', 0u, 0u, 0u, (const uint16_t *)0, 0u);   /* r02n */
    sup_batch_5ms();
}

/* ── 2.5 ms (2/3 ms alternation): the walk monitor (stock 0x873F0) ────── */
static void task_sup_monitor(void)
{
    plat_trace((uint8_t)'M', 0u, 0u, 0u, (const uint16_t *)0, 0u);   /* r02n */
    sup_monitor_poll();
}

#if !(defined(GS_G6B) && GS_G6B)
/* M5-1: the stock request producer's 10 ms chain (app/rq_prod.c): weak, so
 * the RAM application (no producer) links without it. r04r: the code-flash
 * application (GS_G6B) has no holder any more — the engine (app/gs_cand.c)
 * is the sole producer of ch 0..6 (~/tcu_sweep/r04r_port.md) */
extern void rq_prod_task_10ms(uint8_t os_mode) __attribute__((weak));
#endif

/* ── 10 ms: application tasks ──────────────────────────────────────────── */
#if !defined(TCU_RAM_APP)
/* the garage shadow's speed inputs: a valid, non-negative sensor rpm, else 0 */
static uint16_t gs_rpm(rpm_t r, quality_t q)
{
    return (q == Q_OK && r > 0) ? (uint16_t)r : 0u;
}

/* THE GARAGE ENGINE'S LIVE INPUTS, BY VALUE (app/gs_shadow.h gs_in_t): the
 * shadow, the bridge and the G6a candidate (app/g6a.c) take the same set */
static void gs_in_build(gs_in_t *gi)
{
    const vehicle_in_t *v = vehicle_in();
    const sensors_in_t *sn = sensors_in();
    const gs_in_t g = {
        .os_mode = sup_mode(),
        .rngd = *(const volatile uint8_t *)0xFFF89CDCu,     /* park_law's 0x89CDC */
        .park_model = park_law_state(),
        .park_req = park_law_request(),
        .temp = *(const volatile uint8_t *)0xFFF89C13u,     /* tmp_chain's 0x89C13 */
        .cd2 = *(const volatile uint8_t *)0xFFF89CD2u,      /* tmp_chain's 0x89CD2 */
        .eng_speed = (uint16_t)v->engine_speed,
        .eng_valid = (v->q_engine_speed == Q_OK) ? 1u : 0u,
        /* the shift context's speeds (0x89CB8 / 0x89CBA stand-ins): our
         * live output / turbine sensors, 0 when invalid or negative */
        .out_rpm = gs_rpm(sn->output_speed, sn->q_output_speed),
        .tur_rpm = gs_rpm(sn->turbine_speed, sn->q_turbine_speed),
        /* G6a: park_law's 0xCFFDE direction request (the candidate's AE28) */
        .ae28 = *(const volatile uint8_t *)0xFFF8AE28u,
        /* G5c (~/tcu_sweep/g5c_port.md): slot 26's 0x89C3D (safety/os_sel.c
         * os_sel_flags_10ms, before this in task_10ms; 0 at the boot pass in
         * OS mode 0 — the stock's bss, slot 26 not yet run) */
        .c3d = *(const volatile uint8_t *)(uintptr_t)OS_FLG_C3D,
    };
    *gi = g;
}

/* G6a (app/g6a.c): the candidate's boot-state-1 pass — bsw/sup/sup_mode.c
 * calls it (weak) at row-1 entry 17's slot (r04q: right after app/rq_prod.c's
 * rq_prod_boot_pass; r04r: the holder is retired, this IS the producer's
 * boot pass, and the seam's first value starts the wire's producer content) */
void g6a_boot_pass(void)
{
    gs_in_t gi;
    gs_in_build(&gi);
    g6a_boot(&gi);
    /* r04q (D-G6-13): the monitor evaluates the candidate's boot pass at
     * once (the boot allowance: the seam carries the stock's all-release row
     * from this pass, then A's switch-on fill), then the boot comparison */
    gs_bridge_boot_pass();
    g6a_boot_compare();
}
#endif

static void task_10ms(void)
{
#if !defined(TCU_RAM_APP)
    const uint32_t t_task0 = plat_frt_us();         /* G6a: the task's duration witness */
    /* G5b (safety/os_sel.c, ~/tcu_sweep/g5b_port.md): the OS mode at the
     * task's start selects the stock's 10 ms task object (modes 1..3: the
     * pre-hook 0x68EA8, slot 0's selector 0x1418E2, slot 26's flags, the
     * post-hook 0x68EFE). The mode cannot change inside this task: the
     * apply (0x66E06) runs in the 5 ms batch (bsw/sup/sup_batch.c) */
    const uint8_t os0 = sup_mode();
    os_sel_echo_10ms(os0);                          /* pre-hook 0x6D6E0 → 0x68EA8 */
#endif
    /* M5-1: the stock 10 ms list (modes 1..3) runs the element tick and
     * update (entry 29) and the producer + actuators (entry 43) before
     * the bus-A cyclic below reads 0xFFF84B78 through the law */
    /* the temperature byte (app/tmp_chain.c): mode 0 → the safety layer's
     * ED20 re-initialisation (0x132DCE); modes 1..3 → 0xB986C, the latch of
     * 0x89C13, entry 29 — before the producer (entry 43) reads it */
    tmp_chain_task_10ms(sup_mode());
#if !(defined(GS_G6B) && GS_G6B)
    if (rq_prod_task_10ms) {
        rq_prod_task_10ms(sup_mode());
    }
#endif
    sensors_update_10ms();
    vehicle_update_10ms();
#if !defined(TCU_RAM_APP)
    shadow_sim_tick_10ms();             /* bench-only: ages the simulated-speed frame (100 ms) */
#endif
#if !defined(TCU_RAM_APP)
    {
        /* G5b: THE MODE SELECTOR 0x1418E2 (10 ms slot 0 0x14F34C, after the
         * engine conditioning 0x13BF66 / 0x1507BE and BEFORE the terminal-15
         * machine 0x140B80 — it reads E9FF from the previous pass), on our
         * inputs by value; it writes the request 0x95483 only */
        const vehicle_in_t *v = vehicle_in();
        const sensors_in_t *sn = sensors_in();
        const os_sel_in_t si = {
            .eng = (uint16_t)v->engine_speed,
            .eng_valid = (v->q_engine_speed == Q_OK) ? 1u : 0u,
            .tur_ok = (sn->q_turbine_speed == Q_OK) ? 1u : 0u,
            .tur = gs_rpm(sn->turbine_speed, sn->q_turbine_speed),
            .sup = *(const volatile uint16_t *)0xFFF94F86u,    /* 0x8D8DE's supply (0x96066) */
            .e9ff = kl15_e9ff(),
            .pad = 0u,
        };
        os_sel_tick_10ms(os0, &si);
    }
#endif
    /* THE TERMINAL-15 STATE (safety/kl15.c, 0x140B80): the stock runs it
     * every 10 ms in modes 0..3 (0x133C4E / 0x14F34C, slot 0 of the 10 ms
     * list), after the CAN receive and before the range chain (slot 3) —
     * its E9FF / EA01 / EA03 are the chain's D641 / D642 / D643 */
    kl15_tick_10ms();
    {
        /* THE RANGE: the stock's cyclic chain 0x9B306/0x9B322/0x9B34A, the
         * live owner of D6D6 (app/rng_chain.c), which the stock list
         * 0x4D834 runs before 0xA2EBC (0xB912C, our park_tick); after
         * vehicle_update so it sees this pass's lever, brake and engine
         * speed. It runs in OS modes 1..3 (as the stock's lists; D6D6 keeps
         * the init arbiter's value before) and takes the group-98 verdict
         * of the park sensor (park_pair, the previous pass's report) as a
         * value. */
        park_pair_wit_t pw;

        park_pair_witness(&pw);
        /* a group-98 report reaches the fault model only on the pass after the
         * stock's 100 ms slot made one (0xFF = no report this pass) */
        rng_chain_tick_10ms(sup_mode(), pw.failed,
                            park_pair_take_report() ? pw.tested : 0xFFu);
#if !defined(TCU_RAM_APP)
        /* THE PA7 SHADOW's range part (app/pa7_hook.c, read-only): the
         * stock's 0x9B34A calls 0xA6CF8 (E567 → D713 → D712) after the D6D6
         * chain, modes 1..3 */
        pa7_hook_range(sup_mode());
#endif
    }
#if !defined(TCU_RAM_APP)
    /* G5b: slot 26 (0xA2EAC / 0xA2EA4 / 0xA2EB4) — the one-hot mode flags
     * 0x89C3C..3E / 0x89C53..55, before 0xB912C (slot 27, park_tick). Live
     * cells; G5c: the engine reads 0x89C3D (gs_in_build, below); park_law
     * keeps its own c3c from the mode) */
    os_sel_flags_10ms(os0);
#endif
    park_set_os_mode(sup_mode());   /* 0x89C3C for the fault-reaction park request */
    park_tick_10ms();
#if !defined(TCU_RAM_APP)    /* the RAM application carries no shadow (its 64 KB limit) */
    {
        /* G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): the shadow (instance 1)
         * is deleted (D-G6-7). THE GARAGE ENGINE (app/gs_cand.c, the
         * candidate: the stock engine + holder in the stock 10 ms slot order,
         * on its own RAM and its pinned stock objects) gets the live values
         * BY VALUE after 0xB912C (park_tick: 0x89CDC), as the stock's slots
         * 26..42 follow it, and is the producer of the live A..E currents —
         * its pass BEFORE the monitor below decides what the seam hands the
         * F stage (the 5 ms batch, 1 ms later) */
        const sensors_in_t *sn = sensors_in();
        gs_in_t gi;
        uint32_t t_st;
        gs_in_build(&gi);
        g6a_cand_tick(&gi);
        {
            /* THE MONITOR (app/gs_bridge.c; ours): the permits, the element
             * bounds, the release envelope, the windows (violation → the P
             * content, latched) and the E28E fence. Its live inputs BY
             * VALUE; it writes only its own RAM (.gsb_*) */
            const gsb_in_t bi = {
                .mon_faults = park_mon_faults(),
                .relwin_st = drv_rq_rel_win_state(),
                .eng_valid = gi.eng_valid,
                .tur_ok = (sn->q_turbine_speed == Q_OK) ? 1u : 0u,
                .eng_speed = gi.eng_speed,
                .tur_rpm = gi.tur_rpm,
                .cyc = drv_job_tx(DRV_CYC),
            };
            t_st = plat_frt_us();
            gs_bridge_tick_10ms(&bi);
            g6a_time(G6A_T_MON, plat_frt_us() - t_st);
        }
        /* the wire self-checks and CPU witnesses (app/g6a.c): the law's
         * 0x94ED4 against the replicated seam + guard, the guard replica */
        g6a_compare_10ms(sup_mode());
        /* THE SHIFT-MAP SHADOW's gear feedback chain (app/sm_gear.c, read-
         * only, its own RAM .sm_gear; 2026-11-19): the stock slots 30..33
         * (0x10485E / 0xD15A6 / 0x104A28 / 0x103B36 / 0xD140C / 0xDD954) on
         * its own cells, the engine's power-flow cells BY VALUE after its
         * pass; the shadow's 0x89C28 is the 30 ms pass's gear_in */
        /* F1 (2026-11-19, ~/tcu_sweep/f1_fr_shadow_port.md): THE READ-ONLY
         * FAULT-REACTION / GEAR-CODE SHADOW (app/fr_shadow.c + fr_fm.c, its
         * own RAM .fr_shadow / .fr_fm): the RX status, the comm monitor, the
         * generic fault manager, the code chain, 0x89CF5 and the fault-fed
         * hinge — before the gear chain (the stock's slot 27 precedes 30) */
        fr_hook_10ms();
        sm_hook_10ms();
        /* (2026-11-20, ~/tcu_sweep/gp_shadow_port.md) THE READ-ONLY GEAR-PAIR
         * SHIFT-ENGINE SHADOW (app/gp_shadow.c + the coupled gear chain
         * app/gp_gear.c, their own RAM .gp_shadow / .gp_gear): the stock
         * slots 30..35 with the shift object 0xFFF8BD14 as a shadow */
        gp_hook_10ms();
    }
#endif
    bench_tick_10ms();
    sol_tick_10ms();        /* channel bookkeeping; bus A is the F stage's (sup_batch.c) */
    meas_table_update();
    meas_tick_10ms();
    park_pair_tick_10ms();  /* park position sensor: the gather + the pair
                             * object 0x141B42, every 10 ms as the stock
                             * (group 98 runs in the 100 ms slot below) */
    /* the 30 ms (0xB9646) and 100 ms (the safety slice 0x13204A: gather,
     * slew limit → 0xFFF8E9C8) tasks of modes 1..3, on this tick when due,
     * after the 10 ms task (lower priority) */
    tmp_chain_task_slow(sup_mode());
#if !defined(TCU_RAM_APP)
    /* G5b: the post-hook 0x6D6EC → 0x68EFE — 0x93B5E = the request; the
     * next 5 ms batch's 0x6F0B4 takes it, 0x66E06 applies it at its tail */
    os_sel_handoff_10ms(os0);
    g6a_task_time(sup_mode(), plat_frt_us() - t_task0);
#endif
}

/* the stock's 100 ms safety slice 0x13204A → 0x14F7EA also runs the park
 * sensor's group-98 verdict 0x141E06 (modes 1..3) */
void tmp_chain_hook_100ms(void)
{
    park_pair_g98_100ms();
}

#if !defined(TCU_RAM_APP)
/* the stock's 30 ms list 0x4D940 (modes 1..3): the shift target, entries 5 /
 * 14 / 15 / 16, before 0xB9646 — the READ-ONLY shift-map shadow
 * (app/sm_shadow.c, its own RAM .sm_shadow; it writes nothing live) */
void tmp_chain_hook_30ms(void)
{
    sm_hook_30ms();
}
#endif

/* ── 100 ms: heartbeat ─────────────────────────────────────────────────── */
static can_frame_t s_hb;
static void task_100ms(void)
{
    s_hb.id = DIAG_ID_HEARTBEAT;
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
    (void)plat_can_send(DIAG_BUS_HEARTBEAT, &s_hb);
}

/* L0-sense: the AN0 multiplexer 5 ms finisher (stock 0x8F62C via 0x8EDEA)
 * and the descriptor status ramp (0x8F5A0 part) */
static void task_adc_5ms(void)
{
    /* r02l discriminator: the chain's clock (Timer G) starts 500 ms in —
     * well after the opening, the ask and the gate (r02j: C7 by ~15 ms) */
    static bool s_adc_started;
    if (!s_adc_started && sched_now_ms() >= 500u) {
        plat_adcmux_start();
        s_adc_started = true;
    }
    plat_adcmux_finish_5ms();
    plat_adcmux_status_5ms();
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
    {    5u,    4u, task_adc_5ms,     RT_5MS  },   /* L0-sense: 0x8F62C + 0x8F5A0 */
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
    drv_temp_init();                    /* M4: 0x88A50 (row-9 entry 0x7CB5C) */
    tmp_chain_init();                   /* M5: the temperature byte's safety objects
                                         * (startup 0x15A67C, row-9 0x132980) */
    drv_pksens_init();                  /* park sensor: 0x88EB0 (row-9 list's last) */
    drv_ga_init();                      /* M4: group-A law (0x9136E shadows) */
    sol_init();
    bench_init();
    vehicle_init();
    park_init();
    kl15_init();                        /* the terminal-15 object 0xFFF8E9D4 (0x140B30) */
#if !defined(TCU_RAM_APP)
    os_sel_init();                      /* G5b: the selector object, 0x93B56, the mode flags */
#endif
    rng_chain_init();                   /* the range: the 0x9B228 one-shot (init arbiter → D6D6) */
#if !defined(TCU_RAM_APP)
    /* the garage bridge, G1 (its own RAM; its windows survive a warm reset) */
    gs_bridge_init(plat_boot_index() <= 1u);
    /* G6a: the candidate (its RAM cleared) and the comparator, before the
     * boot-state-1 pass (sup_mode_background) runs the candidate's boot */
    g6a_init();
    /* the read-only PWM-feedback shadow (app/fb_shadow.c, its own RAM):
     * cells cleared, then the Timer-D0 window sampler armed (vector 216,
     * level 6) — the stock arms it in its init batch (0x90010) */
    fb_shadow_init();
    fb_shadow_start();
    /* the read-only PA7 shadow (app/pa7_shadow.c, its own RAM): cleared as
     * 0x90D12 leaves PA7 (0), then the 0x9B228 one-shot's 0xA6CF8 on the
     * init arbiter's range */
    pa7_shadow_init();
    pa7_hook_range_init();
    /* the read-only ECU-temperature shadow (app/tsel_shadow.c, its own RAM):
     * the stock's init hub 0x832AC (0x88BAC, 0x88D98) */
    tsel_hook_init();
    /* the read-only shift-map shadow (app/sm_shadow.c, its own RAM) */
    sm_hook_init();
    /* F1: the read-only fault-reaction shadow (its RAM cleared; the fault
     * manager's masks 0 — nothing stored) */
    fr_hook_init();
    /* the read-only gear-pair shadow (its RAM cleared; the coupled chain's
     * init = 0xCFFDE(1)'s, as the r04w chain) */
    gp_hook_init();
    /* BENCH-ONLY simulated speeds for the shadows (io/vehicle/shadow_sim.c): registers
     * 0x7F6 on bus 2 before plat_can_rx_commit below */
    shadow_sim_init();
#endif
    park_pair_init();                   /* park sensor: 0x141B1C (safety layer) */
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
        /* the stock's background task 0x6A104 (activated by every main-loop
         * pass, 0x67A68 → 0x9A674): the boot-state machine and the startup
         * modules 1/2 that lead to the mode-1 switch (sup_mode.c) */
        sup_mode_background();
    }
}

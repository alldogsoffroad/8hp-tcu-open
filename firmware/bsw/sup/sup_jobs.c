/* sup_jobs.c — the bus-B job engine: 14 descriptors, queues, one-in-flight
 * (design §7.1-7.3, spec 22 §2 validated).
 *
 * The descriptor table is flash-resident (stock-faithful). RAM staging
 * buffers are OR'd by the stagers; the engine copies template→staging,
 * launches via plat_rspi, and the ISR-context completion handler runs.
 * Queues chain gaplessly: the ISR's done() pumps the next queued job.
 */
#include "bsw/sup/sup_jobs.h"
#include "bsw/sup/sup_cells.h"
#include "platform.h"
#include <string.h>

/* ── select-config tables (flash const, design §7.1) ───────────────────── */

const uint16_t SEL_B1[]    = { 0xAF11u };
const uint16_t SEL_B1_B0[] = { 0xAF11u, 0xAF01u };
const uint16_t SEL_B0[]    = { 0xAF01u };
const uint16_t SEL_B2[]    = { 0xAF20u };
/* SEL_A5 (bus A) is defined in bsw/drv/drv_jobs.c — the bus-A table's home */

/* ── TX templates (flash const, spec 22 §2.1 validated) ────────────────── */

static const uint16_t TMPL_CTRL[4]  = { 0x8E00u, 0xC900u, 0x2200u, 0x0900u };
static const uint16_t TMPL_EN[2]    = { 0x9000u, 0x2400u };
static const uint16_t TMPL_POS[2]   = { 0x2800u, 0x2E00u };
static const uint16_t TMPL_SSLB2[1] = { 0x00FFu };
static const uint16_t TMPL_ID[4]    = { 0x0600u, 0x0300u, 0x0000u, 0x0000u };
static const uint16_t TMPL_STAT[5]  = { 0x1800u, 0x0C00u, 0x1E00u, 0x0600u, 0x1400u };
static const uint16_t TMPL_NIB[1]   = { 0x0000u };
static const uint16_t TMPL_DUTY[7]  = { 0x8800u, 0x8800u, 0x8800u, 0x8800u,
                                        0x1200u, 0x0C00u, 0x0A00u };
static const uint16_t TMPL_LEVEL[4] = { 0x8400u, 0x0A00u, 0x1200u, 0x0C00u };
static const uint16_t TMPL_FLT[2]   = { 0x9000u, 0x2400u };

/* ── RAM staging buffers (linker-pinned, design §9) ───────────────────────
 * The four bench-read staging words are pinned AT their stock cell
 * addresses: the byte cell inside a pair (e.g. CELL_J1_C9_STAGED at
 * 0xFFF88426) is the big-endian high byte of the 16-bit staged word, so
 * pinning makes the cell and the buffer the SAME storage on the target.
 * The remaining buffers land in the reserved staging windows; none of them
 * is bench-read by address. */
static uint16_t stage_ctrl[4]  __attribute__((section(".sup_j1")));    /* 0x8E+C9 @ 0xFFF88424 */
static uint16_t stage_en[2]    __attribute__((section(".sup_j2")));    /* 90|en  @ 0xFFF88434 */
static uint16_t stage_pos[2]   __attribute__((section(".sup_j3")));    /* 28,2E  @ 0xFFF88444 */
static uint16_t stage_nib[1]   __attribute__((section(".sup_j7")));    /* 0000   @ 0xFFF885E0 */

static uint16_t stage_sslb2[1] __attribute__((section(".sup_stage"))); /* window 0xFFF88400 */
static uint16_t stage_id[4]    __attribute__((section(".sup_stage")));
static uint16_t stage_stat[5]  __attribute__((section(".sup_stage")));
static uint16_t stage_duty[7]  __attribute__((section(".sup_stage2"))); /* window 0xFFF88450 */
static uint16_t stage_level[4] __attribute__((section(".sup_stage2")));
static uint16_t stage_flt[2]   __attribute__((section(".sup_stage2")));

/* ── RAM reply buffers (ordinary .bss; bench reads via staging cells) ──── */

static uint16_t rx_ctrl[4], rx_en[2], rx_pos[2], rx_sslb2[1];
static uint16_t rx_id[4], rx_stat[5], rx_nib[1];
static uint16_t rx_duty[7], rx_level[4], rx_flt[2];

/* ── the descriptor table (flash const; class routing per the validation) ─ */

const sup_job_desc_t sup_job_table[JOB_COUNT] = {
    [JOB_CTRL]  = { .class_field=0x0100, .frames=4, .n_sel=2, .tx_tmpl=TMPL_CTRL,
                    .tx_stage=stage_ctrl, .rx=rx_ctrl, .sel_cfg=SEL_B1_B0,
                    .queue=JOBQ_CYCLIC, .bus=1 },
    [JOB_EN]    = { .class_field=0x0100, .frames=2, .n_sel=1, .tx_tmpl=TMPL_EN,
                    .tx_stage=stage_en, .rx=rx_en, .sel_cfg=SEL_B1,
                    .queue=JOBQ_CYCLIC, .bus=1 },
    [JOB_POS]   = { .class_field=0x0100, .frames=2, .n_sel=1, .tx_tmpl=TMPL_POS,
                    .tx_stage=stage_pos, .rx=rx_pos, .sel_cfg=SEL_B1,
                    .queue=JOBQ_CYCLIC, .bus=1 },
    [JOB_SSLB2] = { .class_field=0x0100, .frames=1, .n_sel=1, .tx_tmpl=TMPL_SSLB2,
                    .tx_stage=stage_sslb2, .rx=rx_sslb2, .sel_cfg=SEL_B2,
                    .queue=JOBQ_CYCLIC, .bus=1 },
    [JOB_ID]    = { .class_field=0x0100, .frames=4, .n_sel=2, .tx_tmpl=TMPL_ID,
                    .tx_stage=stage_id, .rx=rx_id, .sel_cfg=SEL_B1_B0,
                    .queue=JOBQ_CYCLIC, .bus=1 },
    [JOB_STAT]  = { .class_field=0x0100, .frames=5, .n_sel=2, .tx_tmpl=TMPL_STAT,
                    .tx_stage=stage_stat, .rx=rx_stat, .sel_cfg=SEL_B1_B0,
                    .queue=JOBQ_CYCLIC, .bus=1 },
    [JOB_NIB]   = { .class_field=0x0100, .frames=1, .n_sel=1, .tx_tmpl=TMPL_NIB,
                    .tx_stage=stage_nib, .rx=rx_nib, .sel_cfg=SEL_B0,
                    .queue=JOBQ_CYCLIC, .bus=1 },
    [JOB_DUTY]  = { .class_field=0x0101, .frames=7, .n_sel=1, .tx_tmpl=TMPL_DUTY,
                    .tx_stage=stage_duty, .rx=rx_duty, .sel_cfg=SEL_B1,
                    .queue=JOBQ_WALK, .bus=1 },
    [JOB_LEVEL] = { .class_field=0x0101, .frames=4, .n_sel=1, .tx_tmpl=TMPL_LEVEL,
                    .tx_stage=stage_level, .rx=rx_level, .sel_cfg=SEL_B1,
                    .queue=JOBQ_WALK, .bus=1 },
    [JOB_FLT]   = { .class_field=0x0100, .frames=2, .n_sel=1, .tx_tmpl=TMPL_FLT,
                    .tx_stage=stage_flt, .rx=rx_flt, .sel_cfg=SEL_B1,
                    .queue=JOBQ_CYCLIC, .bus=1 },
};

/* the per-job RAM status array (design §7.1): the in-flight flag is
 * runtime state — the flash table must never be written (it lands in
 * flash on the target). Set by launch_job, cleared by sup_job_release. */
static volatile uint8_t s_in_flight[JOB_COUNT];

/* ── queues (design §7.2) ───────────────────────────────────────────────── */

#define CYCLIC_SLOTS 10
#define WALK_SLOTS 5

typedef struct {
    uint8_t ids[CYCLIC_SLOTS];
    uint8_t head, tail, count;
} jobq_cyclic_t;

typedef struct {
    uint8_t ids[WALK_SLOTS];
    uint8_t head, tail, count;
} jobq_walk_t;

static jobq_cyclic_t s_q_cyclic;
static jobq_walk_t s_q_walk;

/* queued flag per job (fast check to prevent double-queueing) */
static uint8_t s_queued[JOB_COUNT];

/* forward: the reply handlers (sup_handlers.c) */
extern void sup_resp_ctrl(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_en(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_pos(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_sslb2(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_id(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_stat(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_nib(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_duty(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_level(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);
extern void sup_resp_flt(uint8_t bus, uint8_t job_id, uint16_t *rx, uint8_t n);

static const rspi_done_fn s_handlers[JOB_COUNT] = {
    [JOB_CTRL]  = sup_resp_ctrl,
    [JOB_EN]    = sup_resp_en,
    [JOB_POS]   = sup_resp_pos,
    [JOB_SSLB2] = sup_resp_sslb2,
    [JOB_ID]    = sup_resp_id,
    [JOB_STAT]  = sup_resp_stat,
    [JOB_NIB]   = sup_resp_nib,
    [JOB_DUTY]  = sup_resp_duty,
    [JOB_LEVEL] = sup_resp_level,
    [JOB_FLT]   = sup_resp_flt,
};

/* ── the engine ─────────────────────────────────────────────────────────── */

/* THE ENGINE LOCK (2026-11-11 decode): the stock's queue 0x937F8, engine
 * 0x93902 and check-free 0x93B8E all bracket their work with 0x9665C(0x60)
 * / 0x96674 — `stc sr; and #0xF0; cmp/ge r4; ... ldc r3,sr`: raise SR.I to
 * 6 (never lower it), restore on exit. That masks the walk timer (IPR 6)
 * and RSPI-B (IPR 4) while the rings are touched. Ours ran the same ring
 * code from task context, the walk ISR and the RSPI done ISR with no lock:
 * an ISR preempting a dequeue could double-take a slot and skip the next
 * (the r02e witness: J1's handler never set an OK/ECHOFAIL bit in 5/5
 * reads while J2/J3 completed [O]; the race as its cause is [I]). */
static uint32_t q_lock(void)
{
    uint32_t prev = plat_sr_i();

    if (prev < 6u) {
        plat_sr_set(6u);
    }
    return prev;
}

static void q_unlock(uint32_t prev)
{
    plat_sr_set(prev);
}

bool sup_job_check_free(uint8_t id)
{
    bool free_now;
    uint32_t lk;

    if (id >= JOB_COUNT) {
        return false;
    }
    lk = q_lock();
    if (s_in_flight[id] != 0u && !s_queued[id] &&
        !plat_rspi_busy(sup_job_table[id].bus)) {
        /* STALE-BURST RECOVERY — the stock's busy-find law (0x93B8E, the
         * shared check-free every builder calls: 0x91086 duty, 0x91190 level,
         * all cyclic builders). The stock does NOT silently skip a busy job:
         * state != armed ('@') means the job's completion never came, and the
         * check-free CALLS the job's release function (desc+0x14: W1 0x910EE,
         * W2 0x911D8), which counts the discard in the per-job error cell
         * (0x90541 bit0/bit1) and clears the state byte — so a stuck burst
         * can never wedge its builder: the next pass relaunches fresh. The
         * armed state is exempt (the burst-end handoff); its port is the
         * engine's handoff flag, held across plat_rspi_busy for exactly that
         * window (plat_rspi.c). Ours additionally requires the channel idle:
         * in-flight with the channel ARMED/FEEDING is a live burst and waits
         * (stock '@' behavior). The r01r duty investigation proved the
         * one-launch wedge class is real in this port: without this law an
         * orphaned s_in_flight is permanent and the builder dies silently
         * forever (the stock's 0x93B8E is why it never could). */
        sup_job_err_bump(id);
        s_in_flight[id] = 0u;
    }
    free_now = (s_in_flight[id] == 0u && !s_queued[id]);
    q_unlock(lk);
    return free_now;
}

bool sup_job_queue(uint8_t id)
{
    uint32_t lk;

    if (id >= JOB_COUNT) {
        return false;
    }
    lk = q_lock();
    if (s_in_flight[id] != 0u || s_queued[id]) {
        q_unlock(lk);
        return false;                           /* one-in-flight (NR-6) */
    }
    const sup_job_desc_t *d = &sup_job_table[id];
    if (d->queue == JOBQ_WALK) {
        if (s_q_walk.count >= WALK_SLOTS) { q_unlock(lk); return false; }
        s_q_walk.ids[s_q_walk.head] = id;
        s_q_walk.head = (s_q_walk.head + 1u) % WALK_SLOTS;
        s_q_walk.count++;
    } else {
        if (s_q_cyclic.count >= CYCLIC_SLOTS) { q_unlock(lk); return false; }
        s_q_cyclic.ids[s_q_cyclic.head] = id;
        s_q_cyclic.head = (s_q_cyclic.head + 1u) % CYCLIC_SLOTS;
        s_q_cyclic.count++;
    }
    s_queued[id] = 1u;
    q_unlock(lk);
    return true;
}

/* launch one job on its bus (internal) */
static bool launch_job(uint8_t id)
{
    const sup_job_desc_t *d = &sup_job_table[id];
    /* Staging model (design §7.1): the stagers (batch composers, letters
     * stager, walk builders) write tx_stage directly BEFORE queueing and
     * the engine sends tx_stage AS-IS. The flash templates are the
     * reference/initial images only — copying them over the staging here
     * would clobber every composed word. */
    (void)d->tx_tmpl;
    if (!plat_rspi_launch(d->bus, id, d->tx_stage, d->rx, d->frames,
                          d->sel_cfg, d->n_sel, s_handlers[id])) {
        return false;
    }
    s_in_flight[id] = 1u;
    s_queued[id] = 0u;
    CELL_USE_COUNTER(id)++;
    plat_rspi_pump(d->bus);
    return true;
}

/* clear the in-flight flag — the reply handlers call this from ISR context
 * before pumping the next queued job */
void sup_job_release(uint8_t id)
{
    if (id < JOB_COUNT) {
        s_in_flight[id] = 0u;               /* single byte store: atomic */
    }
}

void sup_job_engine_pump(void)
{
    /* the whole dispatch runs under the engine lock, as the stock's 0x93902
     * does (raise to 6; the walk ISR already sits at 6, the RSPI ISR at 4) */
    uint32_t lk = q_lock();

    /* walk queue first (higher priority — the stock's walk timer has
     * priority over the 5 ms batch in the shared job engine) */
    while (s_q_walk.count > 0u && !plat_rspi_busy(1u)) {
        uint8_t id = s_q_walk.ids[s_q_walk.tail];
        s_q_walk.tail = (s_q_walk.tail + 1u) % WALK_SLOTS;
        s_q_walk.count--;
        if (!launch_job(id)) {
            s_queued[id] = 0u;                  /* can't launch — drop */
            break;
        }
    }
    while (s_q_cyclic.count > 0u && !plat_rspi_busy(1u)) {
        uint8_t id = s_q_cyclic.ids[s_q_cyclic.tail];
        s_q_cyclic.tail = (s_q_cyclic.tail + 1u) % CYCLIC_SLOTS;
        s_q_cyclic.count--;
        if (!launch_job(id)) {
            s_queued[id] = 0u;
            break;
        }
    }
    /* bus A if we add it here later */
    q_unlock(lk);
}

/* ── the ISR-context done handler wrapper ────────────────────────────────── */
/* Each sup_resp_* handler clears in_flight and pumps — defined in
 * sup_handlers.c. The plat_rspi ISR calls them directly via the
 * s_handlers function-pointer table above. */

/* ── lab job (FE13 / magic-frame experiments; design §7.2) ─────────────── */

#define SUP_LAB_MAX 100u                /* the FE13 contract: 2..200 bytes per one-shot */
static uint16_t s_lab_tx[SUP_LAB_MAX], s_lab_rx[SUP_LAB_MAX];
static uint8_t s_lab_active;

bool sup_lab_job_queue(uint16_t *tx, uint16_t *rx, uint8_t frames,
                       const uint16_t *sel_cfg, uint8_t n_sel)
{
    if (s_lab_active || frames > SUP_LAB_MAX) {
        return false;
    }
    memcpy(s_lab_tx, tx, frames * sizeof(uint16_t));
    s_lab_active = 1u;
    /* use the lab path directly (blocking for FE13; non-blocking via
     * the caller checking s_lab_active) */
    bool ok = plat_rspi_lab(1u, s_lab_tx, s_lab_rx, frames, sel_cfg, n_sel, 100u);
    s_lab_active = 0u;
    if (ok && rx) {
        memcpy(rx, s_lab_rx, frames * sizeof(uint16_t));
    }
    return ok;
}

const uint16_t *sup_job_rx(uint8_t id)
{
    return id < JOB_COUNT ? sup_job_table[id].rx : 0;
}

/* ── init ────────────────────────────────────────────────────────────────── */

/* ── cell-range init (design §10) ──────────────────────────────────────────
 * The pinned cell ranges are NOLOAD: start.S does not clear them, so a
 * warm watchdog reset would leave stale cell values for the bench and the
 * walk. Clear every reserved range up front (the stock's own RAM clear,
 * spec 21 order 3.1-6) — the module inits then overlay their initial
 * values. Does NOT touch 0xFFF800E8/0xFFF800EC (the boot-record gates
 * below our RAM image, which survive resets by design). */
void sup_cells_init(void)
{
    static const struct { uint32_t addr; uint16_t len; } rng[] = {
        { 0xFFF84A40u, 0x10u },   /* .sup_gate  handover-ready cell            */
        { 0xFFF88400u, 0x24u },   /* .sup_stage staging window (J5/J6/J4)      */
        { 0xFFF88424u, 0x08u },   /* .sup_j1    0x8E + C9 staged pair          */
        { 0xFFF88434u, 0x04u },   /* .sup_j2    90|en staged                   */
        { 0xFFF88444u, 0x04u },   /* .sup_j3    28/2E staged                   */
        { 0xFFF88450u, 0x30u },   /* .sup_stage2 W1/W2/fault-twin staging      */
        { 0xFFF885E0u, 0x04u },   /* .sup_j7    0000 staged                   */
        { 0xFFF90540u, 0x14u },   /* .sup_walk  walk reply cells               */
        { 0xFFF90740u, 0x240u },  /* .sup_cells protocol cells                 */
        { 0xFFF9240Cu, 0x0Cu },   /* .sup_era   FRT stamps + staged token      */
        { 0xFFF9383Cu, 0x10u },   /* .sup_period guard cells + era period      */
        { 0xFFF9417Cu, 0x04u },   /* .sup_s8    state-8 budget cells           */
        { 0xFFF93B40u, 0x68u },   /* .sup_fsm   walk FSM cells                 */
        { 0xFFF94218u, 0x1Cu },   /* .sup_use   job use counters (14 × u16)    */
        { 0xFFF94994u, 0x04u },   /* .sup_latch max-latch cell                 */
        { 0xFFF94E5Cu, 0x1B8u },  /* .sup_value value cells .. 0xFFF95010      */
    };
    for (unsigned i = 0u; i < sizeof rng / sizeof rng[0]; i++) {
        memset((void *)(uintptr_t)rng[i].addr, 0, rng[i].len);
    }
}

void sup_jobs_init(void)
{
    /* staging = the templates' initial images: NOLOAD windows are cleared
     * by sup_cells_init, so copy the reference words in for determinism
     * (every word the engine can send before its stager runs) */
    memcpy(stage_ctrl, TMPL_CTRL, sizeof TMPL_CTRL);
    memcpy(stage_en, TMPL_EN, sizeof TMPL_EN);
    memcpy(stage_pos, TMPL_POS, sizeof TMPL_POS);
    memcpy(stage_sslb2, TMPL_SSLB2, sizeof TMPL_SSLB2);
    memcpy(stage_id, TMPL_ID, sizeof TMPL_ID);
    memcpy(stage_stat, TMPL_STAT, sizeof TMPL_STAT);
    memcpy(stage_nib, TMPL_NIB, sizeof TMPL_NIB);
    memcpy(stage_duty, TMPL_DUTY, sizeof TMPL_DUTY);
    memcpy(stage_level, TMPL_LEVEL, sizeof TMPL_LEVEL);
    memcpy(stage_flt, TMPL_FLT, sizeof TMPL_FLT);

    memset(&s_q_cyclic, 0, sizeof s_q_cyclic);
    memset(&s_q_walk, 0, sizeof s_q_walk);
    memset(s_queued, 0, sizeof s_queued);
    memset((void *)s_in_flight, 0, sizeof s_in_flight);
    s_lab_active = 0u;
}

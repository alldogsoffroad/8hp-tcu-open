/* fr_shadow.h — a READ-ONLY shadow of the stock's FAULT-REACTION / GEAR-CODE
 * CHAIN (F1, 2026-11-19; plan ~/tcu_sweep/fault_reaction_gearcode_plan.md
 * "F1", decode ~/tcu_sweep/f0_fault_chain_decode.md, the port
 * ~/tcu_sweep/f1_fr_shadow_port.md). It computes, every 10 ms pass, what the
 * stock would decide on this unit — the CAN receive status, the comm
 * monitor's verdicts, the generic fault manager's reaction masks, the
 * substitute-gear code and the Sys-mode flag — and writes NOTHING live.
 *
 * PORTED from the bytes (pristine image, literal pools resolved):
 *   0x6EC80 / 0x6EEC0 / 0x69F00  the per-message RX status (timeout, the CRC
 *                 / counter / DLC flags after the 4th consecutive bad frame,
 *                 counter step 1..cal 3, a repeat is an error, the first
 *                 frame accepted) — fed by RAW frames (D-FR-8), never by our
 *                 com layer's verdict; the 53 comm-entry messages
 *   0x1513F8 / 0x13626A / 0x1364C6 / 0x136506 / 0x136952 / 0x1362B0 /
 *   0x13631A / 0x13661C / 0x136966  the comm monitor (object 0xFFF95750's
 *                 fields, ours — not at the stock address) with cfg 0x642BC
 *   0x9BE50 / 0x9B918 / 0xABDFA / 0xAAC0E / 0xAACE4 / 0xA031C / 0x9F718 /
 *   0xA0FC4 / 0xA0FA0  THE GENERIC FAULT MANAGER, RAM ONLY, over all 195
 *                 group configs (app/fr_fm.c; no record / aging / UDS status /
 *                 event log / fatal path exists in it)
 *   0x13A616 / 0x13A9D4 / 0x13BA5C  the substitute-gear selector
 *   0xB90B2 / 0xDD16C / 0x14E0B6 / 0x14E0FC / 0xDD118 / 0xDB63C  the code
 *                 writer → 0x89C41..44, 0x89CED, 0x89C43; 0xD0262's AE2D
 *   0xB986C's 0x89CF5 (four-term OR: reaction 50, 0xFFF8B10C, 0xFFF85195,
 *                 0xDCFDA(E6C6, 6))
 *   0x10485E / 0xD15A6 / 0xD1B10 / 0xD1922 / 0xD1990 / 0xD16BA / 0xD1854 /
 *   0xDBCA8 / 0xDBCF8  A SECOND EVALUATION of the r04w gear chain's hinge with
 *                 the shadow's code: the FAULT-FED TARGET (E4A4' / E6A1'),
 *                 on the healthy chain's state (app/sm_gear.c untouched)
 *
 * STOOD IN (compile-time, the driving dump; each use counted):
 *   0xFFF8E57D = 0xA5 (fault memory ready), 0xFFF8E579 = 0x10, 0xFFF8DEE6 = 0,
 *   0xFFF898D6 = 0x17 (operating condition); the dependency master's
 *   0x9AA00 rule bit taken as clear (S b3 tested; groups 97 / 149 / 150 are
 *   never reported here); the bus-controller error state 0x761CE = 0;
 *   0xFFF8EA21 = 0, 0xFFF9582E = 0 (b2 = 0), 0xFFF9535B / 5C / 64 = 0; the RX
 *   driver state bit 1 = 0; p1 / p2 (0xFFF8B274 / 75) = 0; 0xFFF8C310 = 0;
 *   0xEB216 (the shift object's query in 0xDB63C) = 0; 0xB8396 (codes 6 / 8
 *   / 0x1A, 0x89CED 1 / 2 / 7) not ported (counted, 0); 0xFFF8B10C =
 *   0xFFF85195 = 0x89CEE = 0x89D11 = 0; AE2B = AE2C = 0; EF12 / EF08 = 0 / 0,
 *   0xFFF95592 / 90 = 0 / 1 (dump); E298 = AE70 = 0.
 *   CONTENT-PLAUSIBILITY MONITORS (groups 148 / 149 / 150 / 173 / 159 / 120 /
 *   127 / 117 / 154 / 155 / 105 / 107 / 108, rules undecoded): reported
 *   UNTESTED every pass. Groups 27 / 28 (P1D8D / P1DCA / P1D8E): the TOE32
 *   off / on-state test, ported 2026-11-20 (fr_t3_t below; D-FR-9 retired).
 *   Group 122 (0x13B74E): not ported. Messages our CAN layer has no mailbox
 *   for (25 of the 34 coded bus-1 IDs) are never received: honestly "timed
 *   out" on this bench, but a sender of those IDs would not be seen.
 *
 * INPUTS BY VALUE (fr_in_t, app/fr_hook.c) and RAW FRAMES (fr_shadow_rx,
 * app/main.c's 1 ms CAN drain, before vehicle_rx). The reports of groups 109
 * (U1267, rng_chain's 0xA6AB0) and 98 (P0850, park_pair's 0x141E06) come in
 * by value when rng_chain processed one (the same pass).
 *
 * READ-ONLY, ENFORCED: every output TU defines TCU_OUTPUT_PATH — this header
 * #errors under it; app/fr_shadow.c / fr_fm.c / fr_cal.c define
 * TCU_FR_SHADOW_TU and the live headers #error under it; fr_shadow_scan.py
 * (outputs / shadow / hook, negative fixtures); g6a_scan.py nvm (no EEPROM /
 * NVM / DSM-record / event-log / fatal symbol, literal or DSM-image address
 * in the shadow's objects); sh-elf-gcc pass/fail checks; the live identity
 * (test_g6b with the shadow ticking, against the goldens).
 */
#ifndef TCU_APP_FR_SHADOW_H
#define TCU_APP_FR_SHADOW_H

#if defined(TCU_OUTPUT_PATH)
#error "the fault-reaction shadow is read-only: an output path may not include fr_shadow.h"
#endif

#include <stdint.h>
#include <stddef.h>

#define FR_NGRP 195u
#define FR_NENT 53u
#define FR_REC_N 64u

/* ── the inputs, by value ─────────────────────────────────────────────── */
typedef struct {
    uint32_t t_us;      /* plat_frt_us at the gather (the witness) */
    uint8_t  os_mode;   /* 0x93B33 */
    uint8_t  e9fe;      /* kl15 0xFFF8989C (b1) */
    uint8_t  e9ff;      /* kl15 E9FF (state 1 = start of drive cycle) */
    uint8_t  d6d6;      /* the live range 0xFFF8D6D6 (0x14FC5C → obj+0x19) */
    uint8_t  d6da;      /* 0xFFF8D6DA (obj+0x1A): read by nothing ported — the hook passes 0 */
    uint8_t  e69b;      /* the engine's committed power flow (0x89C30) */
    uint8_t  e6ee;      /* the engine's completed power flow (0x89C2F) */
    uint8_t  e6ef;      /* E6EF */
    uint8_t  c3d;       /* 0x89C3D (slot 26) */
    uint8_t  u_failed;  /* rng_chain's last group-109 report (0xA6AB0: r6 / r7) */
    uint8_t  u_tested;
    uint8_t  rng_g109;  /* rng_chain's model: U1267 in the masks (the comparator) */
    uint8_t  rng_g98;   /* P0850 in the masks */
    uint8_t  eng_st;    /* 0x108 engine status (E910): 0 valid, 2 invalid */
    uint8_t  out_st;    /* the output speed status (EE76): 0 valid, 2 invalid */
    uint8_t  gf_e4a5;   /* the healthy chain's E4A4[1] (app/sm_gear.c) */
    uint8_t  gf_e69c1;  /* its E69C[1] */
    int8_t   gf_e69f;   /* its gear code E69F */
    int8_t   gf_e343;   /* its E33C+7 */
    int8_t   gf_cce4;   /* its CCE4[0] */
    int8_t   gf_c25;    /* its 0x89C25 */
    int8_t   gf_c28;    /* its 0x89C28 */
    int8_t   gf_c2b;    /* its 0x89C2B */
    int8_t   gf_e6a1;   /* its (healthy) gear target E6A1 — the comparator */
    uint8_t  ce8;       /* the shift-map shadow's 0x89CE8 */
    uint8_t  cd2;       /* 0x89CD2 */
    uint8_t  g98_failed;/* park_pair's last group-98 report (0x141E06) */
    uint8_t  g98_tested;
    uint8_t  pad[3];
    uint32_t rep109_n;  /* rng_chain's group-109 reports processed (a change = one this pass) */
    uint32_t rep98_n;   /* rng_chain's group-98 reports processed */
    uint16_t sup_mv;    /* supply 0x94F86 (+0xAC's 0xFFF93920 stand-in) */
    uint16_t d646;      /* the output speed (rpm, 0 when invalid) */
    uint16_t eng;       /* the 0x108 engine speed (E90E) */
    uint16_t cb8;       /* the output / turbine / engine speeds the hinge's */
    uint16_t cba;       /* limiters read (as app/sm_hook.c gathers them) */
    uint16_t cd0;
} fr_in_t;

/* ── one RX message (0x6EC80's RAM state, ours) ───────────────────────── */
typedef struct {
    uint16_t timer;     /* s+4 */
    uint8_t  st;        /* s+8 the status byte */
    uint8_t  cnt[3];    /* s+0xB..D the CRC / counter / DLC error counts */
    uint8_t  nib;       /* s+0xA the last counter nibble | 0x80 */
    uint8_t  newf;      /* a raw frame since the last pass (s+7 b1) */
    uint8_t  rdlc;      /* its received DLC */
    uint8_t  pad;
    uint8_t  data[8];   /* the frame (0xFFF9131C) */
} fr_rx_t;
_Static_assert(sizeof(fr_rx_t) == 18u, "fr_rx_t is 18 bytes (witness_fr.py)");

/* ── the comm monitor (the 0xFFF95750 object's fields; ours) ──────────── */
typedef struct {
    uint8_t  st[FR_NENT];       /* +4 + n the status copies */
    uint8_t  pad35;
    uint16_t t3a, t3c, ta8, taa;/* the bus timers */
    uint16_t cnt[FR_NENT];      /* +0x3E the per-entry lost countdowns */
    uint16_t sup;               /* +0xAC */
    uint8_t  ae, bd;            /* the bus-1 / bus-2 controller states (0) */
    uint8_t  af, b0, b1, b2;
    uint8_t  b6, b7, b8, b9, ba, bb, bc;
    uint8_t  be, bf, c0, c1, c2, c3, c4;
    uint8_t  prev[5];           /* 0xFFF8532A */
    uint8_t  arprev, arseen;    /* 0xFFF85328 / 29 */
    uint8_t  pad2[2];
} fr_comm_t;

/* ── the 0x13A9D4 object (0xFFF95858's fields, ours) and the code cells ── */
typedef struct {
    uint8_t  fwd;       /* +4 */
    uint8_t  o18, o19, o1a; /* the gatherer: 0x89C25, D6D6, D6DA */
    uint8_t  reinit;    /* +0x1C */
    uint8_t  l1d, l1e, l1f;
    uint8_t  a2b;       /* +0x20 */
    uint8_t  latch[6];  /* +0x2C..+0x31 */
    uint8_t  l39, l3a;
    uint8_t  res;       /* +0x4A (mirror +0x1B) */
    uint8_t  o34, o38;  /* +0x34 / +0x38 the output / vehicle status */
    uint8_t  icnt;      /* 0xFFF85332 the first-101-calls counter */
    uint8_t  pad;
    uint16_t t10, t12, t14, t16;
    uint16_t o32, o36;  /* +0x32 output, +0x36 vehicle */
    uint16_t x48;
    uint16_t lim[6];    /* +0x4C..+0x56 */
    uint32_t m24, m28;  /* the stored masked banks */
    /* 0x14E0FC's object 0xFFF95838 (+8 timer, +0x11 result, +0x15 latch) */
    uint16_t e14_t;
    uint8_t  e14_r, e14_l;
    /* the code cells (0x89C3C block, ours) */
    uint8_t  c41, c42, c44, ced;
    int8_t   c43;
    uint8_t  ae2d;      /* AE2D = (0x89C43 != 0) (0xD0262) */
    uint8_t  x519d, x519e;  /* 0x8519D / 0x8519E (the previous code) */
    uint8_t  cf5;       /* 0x89CF5 */
    int8_t   ff_e4a4;   /* the fault-fed requested gear (0x10485E') */
    int8_t   ff_e6a1;   /* THE FAULT-FED TARGET (0xD15A6') */
    uint8_t  pad3;
} fr_code_t;

/* ── the comparators, witnesses ───────────────────────────────────────── */
typedef struct {
    uint32_t passes;            /* 10 ms ticks (every mode) */
    uint32_t mon_passes;        /* comm-monitor / code passes (modes 1..3) */
    uint32_t rx_frames;         /* raw frames matched to an entry */
    uint32_t rx_other;          /* raw frames for no entry */
    uint32_t c109_dis_n;        /* passes: our U1267 in the masks != rng_chain's g109 */
    uint32_t c98_dis_n;         /* passes: our P0850 in the masks != rng_chain's g98 */
    uint32_t d661_dis_n;        /* passes: our reaction 16 != rng_chain's D661 (= its g109) */
    uint32_t tgt_dis_n;         /* passes: the fault-fed target != the healthy chain's E6A1 */
    uint32_t cf5_n;             /* passes with 0x89CF5 = 1 */
    uint32_t cf5_off_n;         /* mode-1..3 passes with 0x89CF5 = 0 (the healthy Sys law's window) */
    uint32_t c109_first_ms, c98_first_ms, tgt_first_ms;   /* the first disagreement (0 = none) */
    uint32_t unp_n;             /* an unported branch reached (0xB8396, the 0xDBCA8-gated gear-g
                                 * rescale, DD16C / DD118 events) */
    uint32_t ev49c_n, ev49a_n;  /* the event-log calls of 0xDD16C / 0xDD118 and 0xDB63C (witness) */
    uint32_t ff_lim_n, ff_err_n;/* the fault-fed hinge's limiter changes / table errors */
    uint32_t t_us_last, t_us_max;   /* the tick's own duration */
    uint32_t plaus_untested_n;  /* untested reports made for the content monitors */
    uint32_t rep109_seen, rep98_seen;   /* the last rng_chain counters taken */
    uint32_t rep109_n, rep98_n; /* reports forwarded to the fault manager */
} fr_wit_t;

typedef struct {
    fr_rx_t   rx[FR_NENT];
    fr_comm_t cm;
    fr_code_t cd;
    fr_in_t   in;               /* the last inputs */
    fr_wit_t  w;
} fr_t;

/* ── the fault manager (app/fr_fm.c; its RAM is written ONLY by fr_fm.c) ── */
typedef struct {
    uint32_t ms;                /* the time base (our 0xFFF89898: +10 per 10 ms task) */
    uint8_t  kind;              /* FR_EV_* */
    uint8_t  g;                 /* the group (or the code / target for the shadow's kinds) */
    uint16_t v;                 /* the group's T, or a value */
} fr_ev_t;
#define FR_EV_START    1u       /* a fail debounce started (T 0 → 1) */
#define FR_EV_CONFIRM  2u
#define FR_EV_HEAL     3u
#define FR_EV_PEND_ON  4u
#define FR_EV_PEND_OFF 5u
#define FR_EV_DIRECT   6u       /* 0x9B918 */
#define FR_EV_RECYCLE  7u       /* 0xAACE4 (terminal re-cycle) */
#define FR_EV_CODE     0x10u    /* g = 0x89C41, v = 0x89C43 */
#define FR_EV_TARGET   0x11u    /* g = the fault-fed E6A1' (as u8), v = E4A4' */
#define FR_EV_CF5      0x12u    /* g = 0x89CF5 */
#define FR_EV_MASKS    0x13u    /* the masks changed: v = the low 16 of M0 ^ M1 (see the masks) */

typedef struct {
    uint16_t s[FR_NGRP];        /* 0xFFF8DBA6 the status */
    uint16_t t[FR_NGRP];        /* 0xFFF8D714 the up/down timer */
    uint16_t d[FR_NGRP];        /* 0xFFF8D89A the step stamp */
    uint16_t tmax[FR_NGRP];     /* 0xFFF8DD2C the timer maximum */
    uint8_t  r[FR_NGRP];        /* the in-RAM record: b0 tested-pass, b1 failed, b3 active,
                                 * b4 in the masks, b6 (0x40 as 0x9CE52), b7 exists — never
                                 * persisted (no record API exists) */
    uint8_t  occ[FR_NGRP];      /* confirmations (the rule's occurrence count, sat 255) */
    uint32_t m0, m1;            /* 0xFFF897D0 / D8 the reaction masks */
    uint32_t p0, p1;            /* 0xFFF897E0 / E4 the pending masks */
    uint8_t  rc[64];            /* 0xFFF89818 / 58 the pending refcounts */
    uint32_t now;               /* the time base (ms) */
    uint8_t  cyc;               /* 0xAE890 the operating cycle (1 / 2 / 3) */
    uint8_t  rec_n;             /* events recorded (<= FR_REC_N) */
    uint16_t pad;
    uint32_t rec_all;           /* events seen (incl. the ones past the recorder) */
    uint32_t rep_n, conf_n, heal_n, direct_n, recycle_n;
    uint32_t dep_n;             /* a dependency master test ran (0x9CDA8) */
    uint32_t rule_c2_n;         /* a rebuild rule's occurrence condition (0x200 / 0x40 / 4) met
                                 * (its cal not decoded: treated as "drop") */
    uint32_t bad_g_n;           /* a report for a group outside 1..195 */
    fr_ev_t  rec[FR_REC_N];     /* the first FR_REC_N events */
} fr_fm_t;

/* ── groups 27 / 28 (2026-11-20, ~/tcu_sweep/group28_decode.md, the port
 * ~/tcu_sweep/group28_port.md): THE TOE32 (k = 3, pin PL7) OFF / ON-STATE
 * TEST — 0x849D4's call 0x8B284(3) with its gate 0x8B5DE(3), the reporter
 * 0x7B358(3) (the DSM report slot 0x7ABC8 → 0x7B9B8), the conversion
 * 0x9140A / 0x93658 of the AN0 mux ring's slots 35 / 40 (mux input 4).
 * Inputs BY VALUE (fr_shadow_t3_in, app/fr_hook.c); the supply is fr_in_t's
 * sup_mv (0x94F86). OUR copies of the stock cells (never the cells
 * themselves): the countdowns 0x94828[3] / 0x9482D[3] (in 5 ms units: the
 * stock's 0x7C7E4 service runs at 5 ms, ours steps 2 per 10 ms pass), the
 * first-call flag 0x94148[3], the duty category 0x93A7A[3], the tested mask
 * 0x93900[3], the fault bits 0x938F6[3], the mask copy 0x94988[3], the code
 * 0x940D4[3], the cut flag 0x94147. Section .fr_toe (0xFFF8915C, right
 * after .fr_fm in the dump-zero gap). */
typedef struct {
    uint16_t r35[2];            /* +0x00 the ring slot 35: 0xFFF885E8 + 0x46 / 0xFFF88688 + 0x46 (raw) */
    uint16_t r40[2];            /* +0x04 slot 40: + 0x50 */
    uint16_t duty;              /* +0x08 0xFFF90874[3]: TOE32's applied duty (‰, 0x91500) */
    uint8_t  x95005;            /* +0x0A the sequencer's pre-'L' window (1 = before 'L') */
    uint8_t  x94110;            /* +0x0B 0x849D4's first-pass flag (0x8B284 runs only once it is set) */
} fr_t3_in_t;
_Static_assert(sizeof(fr_t3_in_t) == 12u, "fr_t3_in_t (witness_fr.py)");

typedef struct {
    fr_t3_in_t in;              /* +0x00 the last inputs */
    uint8_t  fed;               /* +0x0C inputs latched since the last pass (none: the pass skips) */
    uint8_t  gate;              /* +0x0D 0x8B5DE's last result */
    uint8_t  c28, c2d;          /* +0x0E / 0F our 0x94828[3] / 0x9482D[3] (x 5 ms) */
    uint8_t  first;             /* +0x10 our 0x94148[3] */
    uint8_t  code;              /* +0x11 our 0x940D4[3] (0x84 = group 28 mon 4: P1DCA) */
    uint8_t  rep27, rep28;      /* +0x12 / 13 the last report: b0 failed, b1 tested */
    uint16_t v1, v2;            /* +0x14 / 16 our 0x908A6[3] (slot 40) / 0x9089C[3] (slot 35), mV */
    uint16_t prev;              /* +0x18 our 0x93A7A[3] (the duty last seen) */
    uint16_t en;                /* +0x1A our 0x93900[3]: the bits tested */
    uint16_t fl;                /* +0x1C our 0x938F6[3]: the fault bits (latched per tested bit) */
    uint16_t m88;               /* +0x1E our 0x94988[3] */
    uint8_t  x94147;            /* +0x20 our 0x94147 (the d >= 1000 cut flag) */
    uint8_t  pad[3];
    uint32_t passes;            /* +0x24 passes run (inputs fed) */
    uint32_t gate_n;            /* +0x28 passes with the gate open (the test ran) */
    uint32_t fail_n;            /* +0x2C passes reporting a failed + tested group (27 or 28) */
    uint32_t pass_n;            /* +0x30 passes reporting both passed */
    uint32_t untest_n;          /* +0x34 passes reporting both untested */
    uint32_t gate_ms;           /* +0x38 the fault manager's ms at the first open gate (0 = never) */
    uint32_t fail_ms;           /* +0x3C ... at the first failed report */
} fr_t3_t;
_Static_assert(sizeof(fr_t3_t) == 0x40u && offsetof(fr_t3_t, v1) == 0x14u && offsetof(fr_t3_t, passes) == 0x24u,
               "fr_t3_t layout (witness_fr.py)");

/* the layout tools/bench/witness_fr.py reads (host and SH alike) */
_Static_assert(sizeof(fr_t) == 0x568u && offsetof(fr_t, cm) == 0x3BAu && offsetof(fr_t, cd) == 0x484u &&
               offsetof(fr_t, in) == 0x4CCu && offsetof(fr_t, w) == 0x504u, "fr_t layout (witness_fr.py)");
_Static_assert(sizeof(fr_comm_t) == 0xC8u && offsetof(fr_comm_t, t3a) == 0x36u && offsetof(fr_comm_t, ae) == 0xAAu,
               "fr_comm_t layout (witness_fr.py)");
_Static_assert(sizeof(fr_code_t) == 0x48u && offsetof(fr_code_t, c41) == 0x3Cu && offsetof(fr_code_t, cf5) == 0x44u,
               "fr_code_t layout (witness_fr.py)");
_Static_assert(sizeof(fr_wit_t) == 0x64u, "fr_wit_t layout (witness_fr.py)");
_Static_assert(sizeof(fr_fm_t) == 0xA1Cu && offsetof(fr_fm_t, r) == 0x618u && offsetof(fr_fm_t, m0) == 0x7A0u &&
               offsetof(fr_fm_t, now) == 0x7F0u && offsetof(fr_fm_t, rec) == 0x81Cu, "fr_fm_t layout (witness_fr.py)");

/* ── the API ──────────────────────────────────────────────────────────── */
void fr_shadow_init(void);
void fr_shadow_rx(uint8_t bus, uint16_t id, uint8_t dlc, const uint8_t *data);  /* bus: 0 / 1 */
void fr_shadow_tick_10ms(const fr_in_t *in);
void fr_shadow_time(uint32_t us);                   /* the tick's duration (the hook's witness) */
const volatile fr_t *fr_shadow_state(void);
void fr_shadow_t3_in(const fr_t3_in_t *in);         /* latch the TOE32 test's inputs (before the tick) */
const volatile fr_t3_t *fr_shadow_t3_state(void);
uint16_t fr_shadow_t3_conv(uint32_t sum, uint32_t n);  /* 0x93658: trunc(sum x 25000/65536 / n), 0 -> 1 */

void fr_fm_init(void);
void fr_fm_tick(uint8_t e9fe);                          /* +10 ms, the operating cycle */
void fr_fm_report(uint8_t g, uint8_t failed, uint8_t tested);   /* 0x9BE50 */
void fr_fm_direct(uint8_t g);                           /* 0x9B918 */
uint8_t  fr_fm_react(uint8_t id);                       /* 0xA031C */
uint32_t fr_fm_bank(uint8_t k);                         /* 0x9F718 */
uint8_t  fr_fm_failing(uint8_t g);                      /* 0xA0FC4 */
uint8_t  fr_fm_confirmed(uint8_t g);                    /* 0xA0FA0 */
uint8_t  fr_fm_in_masks(uint8_t g);                     /* the record's b4 */
void fr_fm_note(uint8_t kind, uint8_t g, uint16_t v);   /* the shadow's own events */
const volatile fr_fm_t *fr_fm_state(void);

/* app/fr_hook.c */
void fr_hook_init(void);
void fr_hook_rx(uint8_t bus, uint16_t id, uint8_t dlc, const uint8_t *data);
void fr_hook_10ms(void);
void fr_hook_feed(const fr_in_t *in);                   /* the part after the gather (host harness) */
void fr_hook_feed_t3(const fr_t3_in_t *in);             /* the TOE32 inputs (before fr_hook_feed) */

#endif /* TCU_APP_FR_SHADOW_H */

/* gs_shadow.c — a READ-ONLY shadow of the stock's GARAGE ENGAGEMENT, ported
 * from the bytes of the pristine image (sh-elf-objdump -b binary -m sh2a
 * -EB; flash address = file offset; literal pools resolved) with the Ghidra
 * corpus as an aid, checked against the driving dump stockram_driving_
 * 20261104.bin. app/gs_shadow.h for the frame; ~/tcu_sweep/
 * garage_shadow_port.md for the function table, stand-ins and grades.
 *
 * It writes ONLY its own RAM (.gs_obj / .gs_rec here, .gs_req and the fill
 * cells through app/gs_fill.c). No live header is included (TCU_GS_SHADOW_TU
 * makes them #error); every live value arrives by value in gs_in_t.
 *
 * PORTED (stock order per 10 ms pass, list 0x4DA2C slots):
 *   26  0xB983C/B9854 the mode flag → the shadow's 0x89C3D (GS_FORCED: 0)
 *   27  0xB912C subset: 0x89CDC, 0x89CD0 = E90E, 0x89D06 = E910 (0x13BF66)
 *   28  timers: 0x106848 (E4AC+6, sub +0x32/+0x34/+0xE/+0x3C), 0xD0C66
 *       (E28E +6/+8), 0xDBEEC/0xDC35E (E6C5), the 0x89CF8 time base, the
 *       element tick / update 0xE5624 / 0xE566C (shadow fill instance)
 *   29  0xB986C subset: 0xD4DC2 (+0xD4F02/D4EC0/D4E9E/D4F24) → E6A8,
 *       0x89D0F / 0x89D10 / 0x89D12
 *   30  0xCFFDE: AE28 (0x63445), 0xD093A (E28E), 0x103F56, 0xD15A6 (E6A1)
 *   31  0xD01C4: 0x103DE2, 0xCFD1A (+0xDC37E) → 0xBAABC → 0xDDCA2 (0xDE2AC/
 *       0xDC282, 0xDE2FA, 0xEB900, 0xED988/ED9B2, 0x1066D6, 0x106AE6,
 *       0x10D130 (+0x10E780), 0x106866), 0x103DE2, 0x104A28
 *   32  0xD01E6: 0x103DE2, 0x103CEC strobe → 0xD140C, 0x103DE2, 0xD0BAA
 *   35  0x106698 → 0x10CE88 (0x10EB76, 0x10E780, 0x10E75E, 0x10D070 with
 *       0x10D5C4 / 0x10D5E4 / 0x10D9B6, 0x10DE46) ×3, 0x106834 → 0x10CECA ×3
 *   42  0xDC4C4: Sys (0x15), WK (0x16), the garage sources ids 5..10
 *       (0xDC812..0xDCA06), the source-0 holder (0xDC1E2), the +6 clear;
 *       the actuators 0x107BD2 / 0x107FB8 (→ the shadow fill's 0x107C3E)
 *   completion 0xBAB6C → 0xEAD9A → 0xEDA48 → 0x106918 → 0x10CDFA, then
 *       0xBAAA6 → 0xDDC38 → 0x1067EC / 0x10D0B4
 *   helpers 0x10E52C 0x10E5EE 0x10E71A 0x10E7D8 0x10E872 0x10E958 0x10E994
 *       0x10E9E0 0x10EA1C 0x10EA4C 0x10EA72 0x10EABE 0x10EB02 0x10EB10
 *       0x10EC40 0x10EC78 0x10ECC6 0x10ED1A 0x10ED4A 0x10ED6C 0x10EDB2
 *       0x10EDFC 0x10EE4C 0x10EE84 0x10EF04 0x10EF48 0x10EF92 0x10EFB4
 *       0x10EFCA 0x10F06A 0x10E486 0x10CF80 0x112A30/0x112A4A/0x1147C8,
 *       the ramp object (vtable 0x49FC0: +0x14 = 0xD1D8E → 0xD1C4C reset,
 *       +0x20 = 0xD1D96 → 0xD1C70 step, float, ÷1000.0 at 0xD1DC4),
 *       0xD1B94 / 0xD1BBA, 0xB6FD6, the saturators 0xB7768 0xB7776 0xB77C8
 *       0xB7888 0xB785C 0xB78AC 0xB77BC
 *
 * NOT PORTED / STAND-INS (each counted where reachable; report §3):
 *   the release phases 0x12..0x17 (types 3/4; D→N, R→N): the release is
 *   completed on its first pass (wit rel_n); 0x10E896's 0xE9464/0xE945E
 *   term (0); 0xBA178 / 0xEAD94 / 0xE9446 notifications; BC74 (ids
 *   0x10/0x11: not started, E6C1/E6C2 dump 0/3); the kinds 1/2/5/6/7 (the
 *   flags B0FE/B10D/B10E/E60F dump 0 → kind 0); 0xD1B10/0xD1990 (E6A1 =
 *   the gear target); 0x104A28's undecoded branch (wit seq_n); 0x103CEC's
 *   shift-object checks (accept); AE2A..AE2E, AE70, E298, E618, 0x89C8A
 *   (dump 0, AE2D dump 1); 0x89C04 block (dump: 0x89C06 0, 0x89C08 0,
 *   0x89C10 0, 0x89C1A 0, 0x89C1C 0x22, 0x89C1D 0x17), BD78 0; the line
 *   pressure (reaction 50: Sys 20000 binary, 0x89CF5 = 1); WK lockup
 *   (0xDCD38.., not ported).
 *
 * 2026-11-18 (~/tcu_sweep/garage_shadow_v2_port.md) — PORTED, replacing the
 * dump stand-ins BD76 0x7FFF / BD30 0xFF / BD6C 1: the synchronisation
 * progress of the shift context block A — slot 28 0xEC0E0 (BD2E / BD30 ++),
 * slot 34 0xEBDBC → 0x1175D6 (0x117F48 sync + clears, 0x117A74 slip =
 * BD7C, 0x1179D8 → 0xD2058 rate, 0x117AEC BD76), the engagement start
 * 0xEDC5A → 0x11793E / 0xD2046 and the end 0xEBB6C → 0x11755E; the garage
 * fill-time term 0xE469C (app/gs_fill.c); reaction 37 in 0xD9CBC (stand-in
 * input GS_R37 = 0). Stand-ins of this part: 0x89CB8 / 0x89CBA = our live
 * output / turbine sensor rpm (the stock's conditioning not ported; 0 on
 * the dry bench); the slip signs 0x89D5E / 0x89D60 = 1 (forced by 0x89D63 /
 * 0x89D65 = 01, dump); block +5 (0x11779C) = 0 (read only by a release
 * context); BD14+0x34 = 0 (no queued shift); block B not ported (+3 = 0:
 * never touches the counters, never read by the engine). STILL a stand-in:
 * the 0x10E896 term (the E4BC turbine-gradient PI output; its target D238
 * is undecoded) = 0.
 */
#define TCU_GS_SHADOW_TU 1
#include "app/gs_shadow.h"
#include "app/gs_fill.h"

#if defined(TCU_RAM_APP)

void gs_shadow_init(void) { }
void gs_shadow_tick_10ms(const gs_in_t *in) { (void)in; }

#else

/* ── the dump-held inputs (stockram_driving_20261104 [O dump]) ─────────── */
#define GS_C06      0u          /* 0xFFF89C06 cal index (×4 into blk58 tables) */
#define GS_C08      0u          /* 0xFFF89C08 (u16) */
#define GS_C10      0u          /* 0xFFF89C10 (u16) */
#define GS_C1A      0           /* 0xFFF89C1A (s16, [I] input torque) */
#define GS_C1C      0x22u       /* 0xFFF89C1C */
#define GS_C1D      0x17u       /* 0xFFF89C1D */
#define GS_C0A      0u          /* 0xFFF89C0A */
#define GS_D09      0u          /* 0xFFF89D09 */
#define GS_BD78     0u          /* 0xFFF8BD78 */
#define GS_BD28     0u          /* 0xFFF8BD28 (u16) */
#define GS_C25      3           /* 0xFFF89C25 engaged gear (s8) */
/* the shift context's idle values (dump, long-idle synchronised): BD6C
 * 01, BD76 0x7FFF, BD2E 0xFFFF, BD30 0xFF, +0x27 01, +0x2C 00, target gear
 * +1 = 3, active context +3 = 01 [O dump 0xFFF8BD6C..] — the INITIAL values
 * of the ported producer (it starts each engagement from 0x11793E) */
#define GS_IDLE_BD76  0x7FFF
#define GS_IDLE_BD2E  0xFFFFu
#define GS_IDLE_BD30  0xFFu
#define GS_D5E      1           /* 0xFFF89D5E / 0xFFF89D60 slip signs: forced 1
                                 * by 0xFFF89D63 / 0xFFF89D65 = 01 [O dump] */
#define GS_E4B1     1u          /* 0xFFF8E4B1 = E4AC+5 at boot */
#define GS_E6C1     0u          /* 0xFFF8E6C1 */
#define GS_E6C2     3           /* 0xFFF8E6C2 (s8) */
#define GS_AE2D     1u          /* 0xFFF8AE2D */
#define GS_LINEP    20000u      /* 0xFFF8BCB0 (reaction 50, forced maximum) */
#define GS_SYSMODE  1u          /* 0xFFF89CF5 (reaction 50) */
#define GS_E6C3     0u          /* 0xFFF8E6C3 */
#define GS_E298     0u          /* 0xFFF8E298 */
#define GS_E1F_SETTLE 0x18u     /* cal 0x1FE780 (via 0xFFF84C94 → 0x537F8 + 8) */
#define GS_E28E_T8  3u          /* cal 0x1FE766 */
_Static_assert(GS_C06 == 0u && GS_C08 == 0u && GS_C10 == 0u && GS_C1A == 0 &&
               GS_C1C == 0x22u && GS_C1D == 0x17u && GS_C0A == 0u &&
               GS_D09 == 0u && GS_BD78 == 0u && GS_D5E == 1 &&
               GS_IDLE_BD76 == 0x7FFF && GS_IDLE_BD2E == 0xFFFFu && GS_IDLE_BD30 == 0xFFu &&
               GS_E4B1 == 1u &&
               GS_E6C1 == 0u && GS_E6C2 == 3 && GS_C25 == 3 && GS_AE2D == 1u && GS_LINEP == 20000u &&
               GS_SYSMODE == 1u && GS_E6C3 == 0u && GS_E298 == 0u,
               "gs_shadow: every dump-held input equals the stock driving dump");
/* the gear target: forced = the fault-free strategy's 1st (0xFFF8A697 = 1);
 * stock-faithful = the bench's code 0x20 → 0xDB63C → gear 3 */
#if GS_FORCED
#  define GS_TARGET   1
#else
#  define GS_TARGET   3
#endif

/* the RAM pointer cells (dump = ROM init 0x65110 + (cell − 0xFFF84000)) */
#define GS_P84D44   0x00056F98u
#define GS_P84D4C(e) (0x00057474u + 0x40u * (uint32_t)((e) - 1u))   /* e = 1..5 */

/* ── RAM: OUR block (not stock cells) ─────────────────────────────────── */
#if defined(__sh__)
#  define GS_OBJ __attribute__((section(".gs_obj")))
#  define GS_REC __attribute__((section(".gs_rec")))
#  define GS_PRG __attribute__((section(".gs_prog")))   /* the tail of .gs_obj */
#  define GS_RC1 __attribute__((section(".gs_rec1")))   /* the tail of .gs_obj */
#else
#  define GS_OBJ
#  define GS_REC
#  define GS_PRG
#  define GS_RC1
#endif

typedef struct {        /* the ramp object at sub +0x40 (vtable 0x49FC0) */
    float    v;         /* +0  */
    uint16_t ts;        /* +4  0x89CF8 at the last step */
    float    lo;        /* +0x10 */
    float    hi;        /* +0x18 */
} gs_ramp_t;

typedef struct {        /* one element sub-object 0xFFF8D0DC/D148/D1B4 (0x6C) */
    uint8_t  act;       /* +0    */
    uint8_t  past;      /* +1    */
    uint8_t  f2, f3;    /* +2 +3 */
    uint8_t  f4;        /* +4    */
    uint8_t  f5, f6;    /* +5 +6 */
    uint8_t  f7, f8, f9;/* +7 +8 +9 */
    int16_t  out_p;     /* +0xA  */
    int16_t  calc_p;    /* +0xC  */
    uint16_t t_e;       /* +0xE  */
    uint8_t  out_mode;  /* +0x10 */
    uint8_t  phase;     /* +0x11 */
    uint16_t lim12;     /* +0x12 */
    uint8_t  tmo;       /* +0x14 */
    uint8_t  type;      /* +0x15 */
    uint8_t  f16, f17;  /* +0x16 +0x17 */
    uint8_t  f18;       /* +0x18 */
    uint16_t to1a;      /* +0x1A */
    uint8_t  cls;       /* +0x1C */
    uint8_t  f1d, f1e, f1f;
    uint8_t  eng, rel;  /* +0x20 +0x21 */
    uint8_t  step;      /* +0x22 */
    int16_t  base24;    /* +0x24 */
    int16_t  p26;       /* +0x26 */
    int16_t  r28, r2a, r2c;
    uint16_t p2e;       /* +0x2E */
    int16_t  dur30;     /* +0x30 */
    uint16_t t32;       /* +0x32 */
    uint8_t  t34;       /* +0x34 */
    int16_t  d36;       /* +0x36 */
    uint16_t f38;       /* +0x38 */
    int16_t  f3a;       /* +0x3A */
    uint8_t  f3b;       /* +0x3B */
    uint8_t  t3c;       /* +0x3C */
    uint8_t  f3d;       /* +0x3D */
    int16_t  f3e;       /* +0x3E */
    gs_ramp_t ramp;     /* +0x40 */
    int16_t  tgt54;     /* +0x54 */
    uint8_t  tgt56;     /* +0x56 */
    uint32_t idx58, blk58, idx5c, blk5c, idx60, blk60;  /* +0x58/+0x5C/+0x60 cells */
    uint32_t p64;       /* +0x64 */
} gs_sub_t;

typedef struct {
    /* 0x89C3C block (shadow copies) */
    uint8_t  c3d;
    uint8_t  rngd;          /* 0x89CDC */
    uint16_t cd0;           /* 0x89CD0 (E90E) */
    uint8_t  d06;           /* 0x89D06 (E910) */
    uint8_t  d0f, d10, d12;
    uint8_t  pf;            /* 0x89C30 */
    uint8_t  c2f;           /* 0x89C2F */
    uint16_t tb;            /* 0x89CF8 time base */
    uint8_t  c13;           /* 0x89C13 (live temperature byte, latched) */
    uint8_t  cd2_now;       /* 0x89CD2 (live, latched) */
    uint8_t  e6a8[5];       /* engine-state object */
    /* the power-flow module */
    uint8_t  ae28, e6ef, e69b, e6ee;
    int8_t   e69f, e6a1, e4a7;
    uint8_t  e69c0;
    uint8_t  e6c5;
    uint8_t  e28e, e28e_f;  /* E28E state, flags [4] */
    uint16_t e28e_t6, e28e_t8;
    /* the shift object 0xFFF8BD14 (fields used) */
    uint8_t  bd15, bd16, bd17, bd22, bd30;
    int16_t  bd76;
    /* 0xFFF8E33C */
    uint8_t  e33c[3];       /* +2 +3 +4 */
    int16_t  e33c_n, e33c_t;/* +0x10 / +0xA snapshots */
    /* 0xFFF8E4AC */
    uint16_t e4ac2;
    uint8_t  e4ac4, e4ac5, e4ac6, e4ac7, kind, e4ac9;
    gs_sub_t sub[3];        /* 0 D0DC (first, class 2), 1 D148 (class 3), 2 D1B4 (class 4) */
    uint16_t cur[7];        /* shadow currents ch 0..6 */
    uint8_t  done_now, commit_now;
    uint8_t  rec_on;        /* 0xFF idle, else the slot being written */
    uint32_t rec_t0;
} gs_t;

/* the shift context block A (0xFFF8BD6C = BD14+0x58) and the pass counters
 * BD2E / BD30 — the synchronisation-progress producer (2026-11-18,
 * ~/tcu_sweep/garage_shadow_v2_port.md); block B (0xFFF8BEB4, the queued
 * context, +3 = 0) never touches the counters and the engine reads only
 * block A: not ported */
typedef struct {
    uint8_t  sync;          /* +0x00 BD6C the sync flag */
    int8_t   tgear;         /* +0x01 target gear (0x1176C4) */
    uint8_t  act3;          /* +0x03 active context (1) */
    uint8_t  p5;            /* +0x05 f(gears) (0x11779C: not ported, 0 — read
                             * only by a BD16 != 3 context, the release) */
    uint8_t  p27;           /* +0x27 force sync */
    uint8_t  hold28;        /* +0x28 the hold of this pass */
    uint8_t  dir;           /* +0x2C sync direction (0xFF = −1) */
    uint8_t  p31;           /* +0x31 the hold outside a garage */
    int16_t  slip;          /* +0x10 BD7C */
    int16_t  rate;          /* +0x12 BD7E (DT1 of −slip) */
    int32_t  acc;           /* +0x140 the DT1 state ×1000 */
    uint16_t ts;            /* +0x144 the DT1 time stamp (0x89CF8) */
    uint16_t bd2e;          /* 0xFFF8BD2E passes since sync */
    uint16_t cb8, cba;      /* the shadow's 0x89CB8 / 0x89CBA (live, by value) */
    uint8_t  sync_pass_ok;  /* ours: a sync seen since the last commit */
    uint8_t  r37;           /* the reaction-37 input (stand-in) */
    uint32_t sync_pass;     /* ours: shadow pass of the last 0 → 1 sync */
    uint32_t hold_n;        /* ours: passes with the hold set */
} gs_prog_t;

static volatile gs_t g_s GS_OBJ;
static volatile gs_wit_t g_w GS_OBJ;
static volatile gs_prog_t g_p GS_PRG;
/* the recorder: slot 0 fills .gs_rec, slot 1 the tail of .gs_obj (each
 * region alone is too small for 2 × 112 × 28 bytes) */
static volatile gs_rec_t g_rec0[GS_REC_N] GS_REC;
static volatile gs_rec_t g_rec1[GS_REC_N] GS_RC1;
#define G   (*(gs_t *)&g_s)
#define W   (*(gs_wit_t *)&g_w)
#define P   (*(gs_prog_t *)&g_p)
static volatile gs_rec_t *rec_at(uint8_t slot, uint8_t i)
{
    return slot == 0u ? &g_rec0[i] : &g_rec1[i];
}

#if !defined(__sh__)
static uint8_t s_host_mask;         /* gs_shadow_host_standins */
static int16_t s_host_bd76;
static uint8_t s_host_bd30;
#else
#  define s_host_mask 0u
#endif
static uint8_t s_park_model;      /* 0xFFF8BC70[0], this pass (live, by value) */

/* ── saturators (0xB7xxx, from the bytes / corpus) ──────────────────── */
static int16_t b7888(int32_t v)  { return (int16_t)(v < 0x8000 ? (v > -0x8001 ? v : -0x8000) : 0x7FFF); }
static uint16_t b785c(int32_t v) { return (uint16_t)(v >= 0x10000 ? 0xFFFF : (v < 0 ? 0 : v)); }
static int16_t b7768(uint16_t v) { return (int16_t)(v < 0x8000u ? (int16_t)v : 0x7FFF); }
static uint8_t b7776(uint16_t v) { return (uint8_t)(v < 0x100u ? v : 0xFFu); }
static uint8_t b77c8(int16_t v)  { return (uint8_t)(v < 0x100 ? (v < 0 ? 0 : v) : 0xFF); }
static int32_t b78ac(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (hi < v ? hi : v); }
static int16_t b77bc(int16_t v)  { return v < 0 ? (int16_t)0 : v; }
static uint16_t sat16p1(uint16_t v) { return (uint16_t)(v == 0xFFFFu ? v : v + 1u); }   /* 0xBAE5E, step 1 */
static uint8_t sat8p1(uint8_t v)    { return (uint8_t)(v == 0xFFu ? v : v + 1u); }      /* 0xBAE46, step 1 */

/* 0xD1B94 (u8 t, u8 T) / 0xD1BBA (u16): t < T ? a + t·(b − a)/T : b → 0xB7888 */
static int16_t d1b94(uint8_t t, uint8_t T, int16_t a, int16_t b)
{
    int32_t v = b;
    if ((uint32_t)t < (uint32_t)T) {
        v = ((int32_t)t * ((int32_t)b - (int32_t)a)) / (int32_t)T + (int32_t)a;
    }
    return b7888(v);
}

/* 0xB6FD6: the u8 2-D map (float interpolation as the stock) */
static uint8_t b6fd6(uint32_t m, uint8_t px, uint8_t py)
{
    const uint32_t nx = gsf_fl32(m), ny = gsf_fl32(m + 4u);
    const uint32_t ax = m + 8u, ay = ax + nx;
    uint32_t sx = 0u, sy = 0u;
    int32_t fx = 0, dx = 0, fy = 0, dy = 0;
    uint32_t u6, u7, u3;

    if (nx < 2u || ny < 2u) {
        W.unp_n++;
        return 0u;
    }
    if (gsf_fl8(ax) < px) {
        if (px < gsf_fl8(ax + nx - 1u)) {
            uint32_t i = 0u;
            while (gsf_fl8(ax + i) < px) { i++; }
            sx = i - 1u;
            fx = (int32_t)px - (int32_t)gsf_fl8(ax + sx);
            dx = (int32_t)gsf_fl8(ax + i) - (int32_t)gsf_fl8(ax + sx);
        } else {
            fx = 1; dx = 1; sx = nx - 2u;
        }
    }
    if (gsf_fl8(ay) < py) {
        if (py < gsf_fl8(ay + ny - 1u)) {
            uint32_t i = 0u;
            while (gsf_fl8(ay + i) < py) { i++; }
            sy = i - 1u;
            fy = (int32_t)py - (int32_t)gsf_fl8(ay + sy);
            dy = (int32_t)gsf_fl8(ay + i) - (int32_t)gsf_fl8(ay + sy);
        } else {
            fy = 1; dy = 1; sy = ny - 2u;
        }
    } else {
        dy = (int32_t)gsf_fl8(ay + 1u) - (int32_t)gsf_fl8(ay);
    }
    {
        const uint32_t z = ax + nx * sy + nx + ny + sx;
        u6 = gsf_fl8(z);
        if (fx == 0) {
            u7 = (uint32_t)(int32_t)(int8_t)gsf_fl8(z + nx);
        } else {
            const float f = (float)dx;
            int32_t d;
            u7 = gsf_fl8(z + 1u);
            if (u7 < u6) {
                d = -(int32_t)((float)(fx * (int32_t)(u6 - u7)) / f);
            } else {
                d = (int32_t)((float)(fx * (int32_t)(u7 - u6)) / f);
            }
            u6 = (uint32_t)((int32_t)u6 + d);
            u7 = gsf_fl8(z + nx);
            u3 = gsf_fl8(z + nx + 1u);
            if (u3 < u7) {
                u7 = u7 - (uint32_t)(int32_t)((float)(fx * (int32_t)(u7 - u3)) / f);
            } else {
                u7 = (uint32_t)(int32_t)((float)(fx * (int32_t)(u3 - u7)) / f) + u7;
            }
        }
        if (fy != 0) {
            u3 = u6 & 0xFFu;
            if ((u7 & 0xFFu) < u3) {
                u6 = (u3 - ((uint32_t)(int32_t)((float)(fy * (int32_t)(u3 - (u7 & 0xFFu))) / (float)dy) & 0xFFu)) & 0xFFu;
            } else {
                u6 = ((uint32_t)(int32_t)((float)(fy * (int32_t)((u7 & 0xFFu) - (u6 & 0xFFu))) / (float)dy) + u3) & 0xFFu;
            }
        }
    }
    return (uint8_t)u6;
}

/* the ramp object (vtable 0x49FC0) */
static void ramp_reset(gs_ramp_t *r, int16_t init, int16_t lo, int16_t hi)     /* 0xD1C4C */
{
    r->lo = (float)lo;
    r->hi = (float)hi;
    r->v = (float)init;
    r->ts = G.tb;
}
static int16_t ramp_step(gs_ramp_t *r, int16_t rate)                            /* 0xD1C70(r, rate·100) */
{
    const uint16_t dt = (uint16_t)(G.tb - r->ts);
    float v = (float)dt * (float)((int32_t)rate * 100) / 1000.0f + r->v;
    if (r->lo > v) {
        v = r->lo;
    } else if (v > r->hi) {
        v = r->hi;
    }
    r->v = v;
    r->ts = G.tb;
    if (!(v > 32767.0f) && !(-32768.0f > v)) {                                  /* 0xB7910 */
        return (int16_t)(int32_t)v;
    }
    return (v > 32767.0f) ? (int16_t)0x7FFF : (int16_t)-0x8000;
}

/* ── the shadow request objects (0xE55DC / 0xE513C: element 1..5) ──────── */
static uint32_t req_obj(uint8_t e)
{
    static const uint32_t T[6] = { 0u, GS_OBJ_A, GS_OBJ_B, GS_OBJ_C, GS_OBJ_D, GS_OBJ_E };
    if (e == 0u || e > 5u) {
        W.unp_n++;                  /* the stock returns element 2 after a report */
        return GS_OBJ_B;
    }
    return T[e];
}
#define RU8(o, off)    (*(volatile uint8_t  *)(uintptr_t)((uint32_t)(o) + (off)))
#define RU16(o, off)   (*(volatile uint16_t *)(uintptr_t)((uint32_t)(o) + (off)))
/* rq_fill.h's field offsets, repeated (the shadow may not include it) */
#define RO_P   0x00u
#define RO_RES 0x02u
#define RO_SET 0x06u
#define RO_MODE 0x07u
#define RO_SRC 0x08u
#define RO_PRIO 0x09u
#define RO_ST  0x34u
#define RO_54  0x54u
#define RO_55  0x55u
#define RO_56  0x56u
#define RO_57  0x57u
#define RO_5A  0x5Au

static bool b822c(uint8_t e) { return gsf_e4dba(req_obj(e)); }             /* +0x46 == 10000 */
static bool b8246(uint8_t e) { return RU8(req_obj(e), RO_54) != 0u; }      /* vtable +0x5C */
static uint8_t b8260(uint8_t e) { return RU8(req_obj(e), RO_55); }        /* vtable +0x50 */
static uint8_t b827a(uint8_t e)
{
    const uint32_t o = req_obj(e);
    return (RU8(o, RO_55) != 0u || RU8(o, RO_56) != 0u || RU8(o, RO_57) != 0u) ? 1u : 0u;
}

/* ── the per-sub-object helpers (cal: blk58 / blk60 by stock address) ─── */
static uint32_t b58(const gs_sub_t *s, uint32_t off) { return gsf_fl32(s->blk58 + off); }
static uint32_t b60(const gs_sub_t *s, uint32_t off) { return gsf_fl32(s->blk60 + off); }

/* every helper below is the kind-0 path (E4AC+8 = 0); the other kinds are
 * not ported (unreachable with the stand-in flags, counted if reached) */
static void kind_check(void) { if (G.kind != 0u) { W.unp_n++; } }

static int16_t e994(const gs_sub_t *s) { kind_check(); return (int16_t)gsf_b6e10(b58(s, 0x78u), (int16_t)G.bd22); }
static int16_t ea72(const gs_sub_t *s) { return (int16_t)gsf_b7164(b58(s, 0x14u + 4u * GS_C06), P.slip, (int16_t)GS_C1A); }
static int16_t edb2(const gs_sub_t *s) { return (int16_t)gsf_b6e10(b58(s, 0x0Cu), (int16_t)GS_C1A); }
static int16_t eb10(const gs_sub_t *s) { return (int16_t)gsf_b7164(b58(s, 0x08u), (int16_t)GS_C1D, b7768(GS_C10)); }
static int16_t eabe(const gs_sub_t *s) { return (int16_t)gsf_b7164(b58(s, 0xA4u), (int16_t)GS_C1D, b7768(GS_C10)); }
static int16_t ef92(const gs_sub_t *s, int16_t x) { return (int16_t)gsf_b6e10(b58(s, 0x9Cu + 4u * GS_C06), x); }
static int16_t edfc(const gs_sub_t *s) { return (int16_t)gsf_b6e10(b58(s, 0x1Cu + 4u * GS_C06), (int16_t)GS_C1A); }
static int16_t ee4c(const gs_sub_t *s) { return (int16_t)gsf_b6e10(b58(s, 0x24u + 4u * GS_C06), (int16_t)GS_C1A); }
static int16_t efb4(const gs_sub_t *s, int16_t x) { return (int16_t)gsf_b6e10(b58(s, 0x2Cu), x); }
static int16_t efca(const gs_sub_t *s, int16_t x) { return (int16_t)gsf_b6e10(b58(s, 0x88u), x); }
static int16_t ef04(const gs_sub_t *s) { return (int16_t)gsf_b7164(b58(s, 0x3Cu + 4u * GS_C06), (int16_t)GS_C1A, (int16_t)GS_BD78); }
static int16_t ed6c(const gs_sub_t *s) { return (int16_t)gsf_b7164(b58(s, 0x8Cu + 4u * GS_C06), (int16_t)GS_BD78, (int16_t)GS_C1A); }
static int16_t ee84(const gs_sub_t *s) { return (int16_t)gsf_b6e10(b58(s, 0x30u + 4u * GS_C06), (int16_t)GS_C1A); }
static uint8_t ef48(const gs_sub_t *s) { return gsf_b6d6c(b58(s, 0x04u), G.c13); }
static uint8_t eb02(const gs_sub_t *s) { return gsf_fl8(b60(s, 0x04u)); }
static int16_t ea1c(const gs_sub_t *s) { return (int16_t)gsf_b6e10(b58(s, 0x80u), (int16_t)G.bd22); }
static int16_t e9e0(const gs_sub_t *s)
{
    int16_t v = (int16_t)gsf_b6e10(b58(s, 0x7Cu), (int16_t)G.bd22);
    if (G.e4ac5 == 1u) {
        v = (int16_t)gsf_b6e10(b58(s, 0xACu), (int16_t)G.c13);
    }
    return v;
}
static int16_t ea4c(const gs_sub_t *s, uint8_t f5)          /* 0x10EA4C */
{
    if (f5 != 0u && G.bd16 == 3u) {
        return (int16_t)gsf_b6e10(gsf_fl32(s->p64 + 0x20u), (int16_t)G.bd22);
    }
    return 0;
}
static uint16_t ed1a(const gs_sub_t *s) { return gsf_b6eae(b58(s, 0x50u), (uint16_t)GS_C08); }
static uint8_t ed4a(const gs_sub_t *s)                       /* 0x10ED4A: D148 +0x16 / +0x20 */
{
    if (G.sub[1].f16 == 0u && G.sub[1].eng != 0u) {
        return gsf_fl8(b58(s, 0x58u));
    }
    return 0u;
}
static int16_t e52c(const gs_sub_t *s)                       /* 0x10E52C */
{
    int16_t v = 0;
    if (G.bd16 == 3u) {
        if (s->cls == 4u) {
            v = (int16_t)ed4a(s);                            /* BD1C / BD1D: dump 0 */
        } else {
            v = (int16_t)ed1a(s);
            kind_check();
        }
    }
    return v;
}
static uint8_t ec40(const gs_sub_t *s) { kind_check(); return gsf_b6d6c(b58(s, 0x64u + 4u * GS_C06), G.bd22); }
static uint16_t ec78(const gs_sub_t *s)
{
    uint16_t v = gsf_b6eae(b58(s, 0x6Cu), (uint16_t)GS_C08);
    if (G.e4ac5 == 1u) {
        v = gsf_b6eae(b58(s, 0xB4u), (uint16_t)GS_C08);
    }
    if (s->f18 != 0u) {
        W.unp_n++;                                           /* 0x10F1A2 */
    }
    return v;
}
static uint8_t ecc6(const gs_sub_t *s) { kind_check(); return b6fd6(b58(s, 0x70u), G.bd22, (uint8_t)GS_C1D); }
static int16_t e71a(const gs_sub_t *s)                       /* 0x10E71A */
{
    if (s->cls == 3u) {
        return (int16_t)ec78(s);
    }
    if (s->cls == 4u) {
        return (int16_t)ecc6(s);
    }
    return (int16_t)ec40(s);
}
static int16_t e5ee(const gs_sub_t *s)                       /* 0x10E5EE */
{
    int32_t v = 0;
    if (s->cls == 3u) {
        v = e9e0(s);
    } else if (s->cls == 4u) {
        v = ea1c(s);
    }
    if (s->f1e != 0u && s->cls == 2u) {
        W.unp_n++;                                           /* +0x1E: never set here */
    }
    /* cls 4: E982A(BC74) = BC74+5 != 0 — BC74 never starts here (stand-in) */
    v += ea4c(s, s->f5);
    return b7888(v);
}
static int16_t e7d8(gs_sub_t *s)                             /* 0x10E7D8 */
{
    if (s->past == 0u) {
        s->d36 = (int16_t)gsf_b7164(b58(s, 0x94u), (int16_t)GS_C1A, (int16_t)G.c13);
    }
    return s->d36;
}
static bool e872(const gs_sub_t *s)                          /* 0x10E872 */
{
    const int32_t c = (int32_t)(int8_t)(uint8_t)(gsf_fl8(b58(s, 0x98u)) - 0x80u);
    return c <= (int32_t)G.bd76 && s->f4 == 0u;
}
static bool cf80(const gs_sub_t *o, int16_t p)               /* 0x10CF80 */
{
    if (o->eng == 0u) {
        return true;
    }
    return (int32_t)p <= (int32_t)gsf_e4bec(req_obj(o->eng)) && o->f16 != 0u;
}
static int16_t e486(const gs_sub_t *s, int16_t x)            /* 0x10E486 */
{
    int32_t v = (int32_t)eb10(s) + (int32_t)gsf_b6e10(b58(s, 0x10u), x) + (int32_t)ed6c(s);
    kind_check();
    v += (int32_t)gsf_b6e10(b58(s, 0x84u), (int16_t)G.bd22);
    if (s->f3d != 0u) {
        W.unp_n++;                                           /* 0x10EF76 (+0x3D, kind 1 only) */
    }
    return b7888(v);
}
static int16_t e958(const gs_sub_t *s, int16_t v)            /* 0x10E958 */
{
    return d1b94(b7776(s->t32), b77c8(s->dur30), s->tgt54, v);
}
/* 0x112A30(BE1C, p, e) = 0x112A4A (≥ 0) then 0x1147C8: p > 0xE4C18(e, 0) → that */
static int16_t a30(int16_t p, uint8_t e)
{
    int32_t v = p < 0 ? 0 : p;
    const uint16_t u = gsf_e4c18(req_obj(e), 0u);
    if ((int32_t)u < v) {
        W.lim_n++;
        v = b7888((int32_t)u);
    }
    return (int16_t)v;
}

/* 0x10E780: timeout +0x1A and completion limit +0x12 */
static void e780(gs_sub_t *s)
{
    if (G.bd16 == 4u) {
        if (G.kind == 2u) {
            W.unp_n++;
            s->lim12 = 0u;
        } else {
            s->lim12 = gsf_b6eae(b60(s, 0x08u), G.bd22);     /* 0x10F06A */
        }
    } else {
        const uint16_t u = gsf_b6eae(b60(s, 0x00u), G.bd22);
        s->to1a = u;
        s->lim12 = b785c((int32_t)eb02(s) + (int32_t)u);
    }
}

/* 0x10E75E */
static void e75e(gs_sub_t *s)
{
    if (s->tmo == 0u) {
        s->tmo = (s->past != 0u && s->to1a <= s->t_e) ? 1u : 0u;
    }
}

/* 0x10D5C4: the next phase from table 0x63176 (step-major, 6 types) */
static uint8_t d5c4(const gs_sub_t *s)
{
    if (s->tmo != 0u) {
        return 0x0Eu;
    }
    return gsf_fl8(0x00063176u + (uint32_t)s->type + (uint32_t)s->step * 6u);
}

/* 0x10D5E4: phase entry */
static void d5e4(gs_sub_t *s)
{
    int32_t dur = 0x7FFFFFFF;
    int32_t base = s->base24;

    switch (s->phase) {
    case 0x00: case 0x0F: case 0x13:
        break;
    case 0x01: case 0x11:
        dur = e52c(s);
        break;
    case 0x02:
        s->f17 = 1u;
        break;
    case 0x03:
        s->f16 = 1u;
        base = e994(s);
        break;
    case 0x04:
        /* cls 2: 0xBA178(E60F) — fill-object notification, not ported */
        ramp_reset(&s->ramp, 0, -20000, 20000);
        s->f16 = 1u;
        s->f8 = 1u;
        s->t34 = 0u;
        if (s->cls == 2u) {
            s->f3d = 0u;                    /* kind == 1 only */
        }
        s->f4 = 0u;                         /* 0xB8054(E5C2) = B0FE = 0 */
        base = 0;
        dur = e71a(s);
        if (s->f18 == 0u) {
            base = e5ee(s);
        }
        break;                              /* L72A: kind 5 only */
    case 0x09:
        base = 0;
        dur = e7d8(s);
        s->past = 1u;
        s->t_e = 0u;
        if (s->f1e != 0u && s->cls == 2u) {
            W.unp_n++;
        }
        break;
    case 0x0A:
        s->f4 = 0u;
        s->r2c = ef92(s, (int16_t)P.cb8);
        s->r28 = 0;
        ramp_reset(&s->ramp, 0, -20000, 20000);
        s->f2 = 1u;
        break;
    case 0x0B:
        s->past = 1u;
        s->f2 = 1u;
        s->f3 = 1u;
        break;
    case 0x0C: case 0x0E:
        s->f4 = 0u;
        /* cls 2: 0xEAD94(BD14, 1) — shift-object notification, not ported */
        dur = (int32_t)eb02(s);
        base = s->calc_p;
        break;
    case 0x10:
        s->past = 1u;
        s->t_e = 0u;
        if (s->cls == 4u) {
            s->r2a = eabe(s);
            ramp_reset(&s->ramp, b7888((int32_t)s->tgt54 - (int32_t)s->r2a), -20000, 20000);
        } else {
            ramp_reset(&s->ramp, s->tgt54, -20000, 20000);
        }
        break;
    case 0x14:
        s->f7 = 1u;
        break;
    default:                                /* release 0x12/0x15..0x17: not ported */
        W.unp_n++;
        break;
    }
    s->dur30 = b7888(dur);
    s->base24 = b7888(base);
}

/* 0x10D9B6: phase done? */
static bool d9b6(gs_sub_t *s)
{
    if (s->tmo != 0u && s->phase != 0x0Eu) {
        return true;
    }
    switch (s->phase) {
    case 0x00:
        return true;
    case 0x01:
        if (s->cls == 2u) {
            if (b822c(s->eng)) {
                return true;
            }
            if (cf80(&G.sub[2], s->dur30)) {
                return true;
            }
        }
        if (s->cls == 3u && b822c(s->eng)) {
            return true;
        }
        break;
    case 0x02:
        return b822c(s->eng);
    case 0x03:
        if (s->f1d == 0u) {
            return true;
        }
        if (G.sub[2].past != 0u) {          /* D1B5 */
            return true;
        }
        return G.sub[2].eng == 0u;          /* D1D4 */
    case 0x04:
        if (s->tgt56 == 0u) {
            return false;
        }
        /* 0xE9832(BC74, e): BC74 never holds an element here */
        if (s->cls == 2u && e872(s)) {
            return true;
        }
        if (s->cls == 2u && G.kind == 1u) {
            W.unp_n++;
            return false;
        }
        /* cls 3 with *0xFFF8E349 != 0 → +0x1D: E349 is 0 here (dump) */
        break;
    case 0x09: case 0x0A:
        if (e872(s)) {
            return true;
        }
        return !((int32_t)s->t32 < (int32_t)s->dur30);
    case 0x0B:
        return gsf_fl8(b58(s, 0x74u)) <= G.bd30;
    case 0x0C: case 0x0E: case 0x10: case 0x14:
        return false;
    case 0x0F: {
        const bool wait = G.sub[2].f16 == 0u && G.sub[2].eng != 0u;   /* D1CA / D1D4 */
        if (!wait) {
            return true;
        }
        if (s->cls != 2u) {
            return false;
        }
        return b822c(s->eng);
    }
    case 0x11:
        /* BD1C / BD1D == 0 (dump): the second sub-object's fill condition */
        return cf80(&G.sub[1], s->dur30);
    default:
        W.unp_n++;                          /* release phases */
        break;
    }
    return (int32_t)s->dur30 <= (int32_t)s->t32;
}

/* 0x10DE46: the phase pressure */
static int16_t de46(gs_sub_t *s)
{
    int32_t p;

    switch (s->phase) {
    case 0x01: case 0x02: case 0x03: case 0x0F: case 0x11:
        p = s->base24;
        break;
    case 0x04: {
        int16_t rate;
        p = s->base24;
        if (s->cls == 3u || s->cls == 4u) {
            if (s->cls == 4u) {
                rate = (int16_t)ef48(s);
                p += eabe(s);
            } else if (s->f18 == 0u) {
                rate = (int16_t)gsf_fl8(G.e4ac5 == 1u ? b58(s, 0xB8u) : b58(s, 0x00u));
            } else {
                rate = 0;
            }
        } else {
            p += (int32_t)ea72(s) + (int32_t)e994(s);
            rate = edb2(s);
        }
        p += ramp_step(&s->ramp, rate);
        if (s->cls == 2u) {
            p += eb10(s);
            if (s->f3d != 0u) {
                W.unp_n++;
            }
        }
        if (p < 0) {
            p = 0;
        }
        s->tgt54 = b7888(p);
        s->tgt56 = 1u;
        break;
    }
    case 0x09: {
        const int16_t v = e486(s, 0);
        p = a30(b7888(e958(s, b7888((int32_t)v + (int32_t)s->base24))), s->eng);
        break;
    }
    case 0x0A: {
        const int16_t r = edfc(s);
        s->r28 = ramp_step(&s->ramp, b7888((int32_t)r + (int32_t)s->r2c));
        p = a30(b7888((int32_t)e486(s, b7768(s->t32)) + (int32_t)s->r28 + (int32_t)s->base24), s->eng);
        break;
    }
    case 0x0B: {
        const int16_t a = ee4c(s);
        const int16_t b = efb4(s, (int16_t)P.cb8);
        const int16_t sum = b7888((int32_t)a + (int32_t)b);
        int32_t v;
        s->r28 = ramp_step(&s->ramp, sum);
        v = (int32_t)e486(s, b7888((int32_t)s->t32 + (int32_t)(int16_t)s->p2e));
        v += (int32_t)s->base24 + (int32_t)s->r28 + (int32_t)efca(s, b7768(s->t32));
        v = b7888(v);
        p = v < 0 ? 0 : v;                   /* 0x112A4A */
        break;
    }
    case 0x0C: case 0x0E:
        p = d1b94(b7776(s->t32), b77c8(s->dur30), s->base24, 20000);
        break;
    case 0x10: {
        int32_t i16, u;
        if (s->cls == 4u) {
            const int16_t v = ramp_step(&s->ramp, ef04(s));
            const int16_t m = (int16_t)gsf_b6e10(b58(s, 0x44u + 4u * GS_C06), b7768(s->t32));
            const int16_t e = eabe(s);
            i16 = (int32_t)v + (int32_t)m;
            if (s->r2a < e) {
                s->r2a = e;
            }
            u = s->r2a;
        } else if (s->f18 == 0u && G.e4ac5 == 1u) {
            const int16_t a = (int16_t)gsf_b6e10(b58(s, 0xA8u), (int16_t)GS_C1A);
            i16 = ramp_step(&s->ramp, a);
            u = (int16_t)gsf_b6e10(b58(s, 0xB0u), b7768(s->t32));
        } else if (s->f18 == 0u) {
            i16 = ramp_step(&s->ramp, ee84(s));
            u = (int16_t)gsf_b6e10(b58(s, 0x38u), b7768(s->t32));
        } else {
            W.unp_n++;                       /* +0x18 (0x10F16C / 0x10F188) */
            i16 = 0;
            u = 0;
        }
        p = b78ac(i16 + u, 0, 20000);
        break;
    }
    case 0x14:
        p = (s->cls == 3u && GS_C25 != 6) ? 20000 : 0;      /* *0x89C25 */
        break;
    default:
        W.unp_n++;                           /* release phases 0x12/0x13/0x15..0x17 */
        p = 0;
        break;
    }
    if (p < 0) {
        p = 0;
    }
    return b7888(p);
}

/* 0x10D070: advance through every completed phase (the stock loops
 * unbounded; the cap is ours: a table walk past its terminal phases) */
static void d070(gs_sub_t *s)
{
    unsigned guard = 0u;
    while (d9b6(s)) {
        uint8_t ph;
        do {
            s->step = (uint8_t)(s->step + 1u);
            ph = d5c4(s);
            if (++guard > 64u) {
                W.loop_cap_n++;
                return;
            }
        } while (ph == s->phase);
        s->phase = ph;
        d5e4(s);
        s->p2e = (uint16_t)b7768(s->t32);
        s->t32 = 0u;
    }
}

/* 0x10EB76 */
static void eb76(gs_sub_t *s)
{
    if ((uint16_t)GS_C10 < gsf_b6eae(gsf_fl32(GS_P84D44), (uint16_t)GS_C1D)) {
        s->t3c = 0u;
    }
}

/* 0x10CE88: one step */
static void ce88(gs_sub_t *s)
{
    eb76(s);
    if (G.kind != 5u) {
        s->f3e = 0;
    }
    if (s->eng != 0u || s->rel != 0u) {
        e780(s);
        e75e(s);
        d070(s);
        s->calc_p = de46(s);
    }
}

/* 0x10CECA: the output */
static void ceca(gs_sub_t *s)
{
    s->out_mode = 1u;
    if (s->eng == 0u && s->rel == 0u) {
        s->out_p = 0;
        return;
    }
    {
        int16_t v = s->calc_p;
        if (s->cls == 2u) {
            /* 0x10E896: + the 0xFFF8E6CC term (0xE9464 / 0xE945E) — NOT
             * DECODED, stand-in 0 → 0xB7888(p) */
            W.e896_n++;
            v = b7888(v);
        }
        s->out_p = v;
    }
    if (s->eng != 0u) {
        const uint8_t ph = s->phase;
        s->out_mode = (ph == 0x01u || ph == 0x0Fu || ph == 0x11u) ? 1u : 0u;
    }
    if (s->rel != 0u) {
        if (s->cls == 2u) {
            s->out_mode = 0u;
        } else {
            s->out_mode = (s->f7 != 0u && s->out_p == 0) ? 1u : 0u;
        }
    }
}

/* 0x10CF4E: the timers (+1 per pass, saturating) */
static void cf4e(gs_sub_t *s)
{
    s->t32 = sat16p1(s->t32);
    s->t34 = sat8p1(s->t34);
    s->t_e = sat16p1(s->t_e);
    s->t3c = sat8p1(s->t3c);
}

/* 0x10D0B4: stop one sub-object */
static void d0b4(gs_sub_t *s)
{
    s->f3e = s->out_p;
    s->calc_p = 0;
    s->out_p = 0;
    s->eng = 0u;
    s->rel = 0u;
    s->phase = 0x1Eu;
    s->act = 0u;
    s->past = 0u;
    s->f2 = 0u;
    s->f3 = 0u;
    s->f8 = 0u;
    s->tgt56 = 0u;
    s->base24 = 0;
    s->f16 = 0u;
    s->tmo = 0u;
    s->f17 = 0u;
    s->f9 = 0u;
    s->f4 = 0u;                 /* (*0xFFF8E349 = 0: not ours to hold) */
    s->f3a = 0;                 /* 0xE9446(E6CC, 1, 1): not ported */
    s->f18 = 0u;
}

/* 0x1067EC: stop */
static void e67ec(uint8_t a, uint8_t b, uint8_t c)
{
    if (a == 0u && b == 0u && c == 0u) {
        G.e4ac9 = (G.kind == 2u) ? 1u : 0u;
        G.kind = 0u;
    }
    if (a == 0u) {
        d0b4(&G.sub[0]);
    }
    if (b == 0u) {
        d0b4(&G.sub[1]);
    }
    if (c == 0u) {
        d0b4(&G.sub[2]);
    }
    G.e4ac5 = 0u;
}

/* 0xD11EC: E298 (neutral idle) "slip release active" */
static uint8_t d11ec(uint8_t st)
{
    return (st == 0u || st == 1u || st == 2u || st == 11u || st == 12u) ? (uint8_t)0u : (uint8_t)1u;
}

/* 0x10D130: start one sub-object (pf = the new power flow) */
static void d130(gs_sub_t *s, uint8_t pf, uint8_t elem, int8_t gear, uint8_t flag)
{
    const uint32_t gi = (uint32_t)(uint8_t)(gear + 1) * 4u;
    uint32_t v;

    v = gsf_fl32(0x0005DDE0u + gi);                      /* +0x60 */
    s->idx60 = v;
    s->blk60 = ((int32_t)v < 0 || (int32_t)v > 8) ? (W.unp_n++, s->blk60)
             : gsf_fl32(0x00057354u + 4u * 0u /* *0x89C04 = 0 */ + v * 8u);
    v = gsf_fl32(0x0005DD90u + gi);                      /* +0x58 */
    s->idx58 = v;
    s->blk58 = ((int32_t)v < 0 || (int32_t)v > 8) ? (W.unp_n++, s->blk58)
             : gsf_fl32(0x0005ADE0u + v * 8u);
    v = gsf_fl32(0x0005DDB8u + gi);                      /* +0x5C */
    s->idx5c = v;
    s->blk5c = ((int32_t)v < 0 || (int32_t)v > 8) ? (W.unp_n++, s->blk5c)
             : gsf_fl32(0x000590ECu + v * 8u);
    s->f3d = 0u;
    if (pf == 0u) {
        s->rel = elem;
    } else if (s->cls == 2u && d11ec(GS_E298) == 1u) {      /* 0xB6A98 = 0xD11EC(E298) */
        s->eng = 0u;
    } else {
        s->eng = elem;
    }
    if (pf == 0u) {
        if (s->rel != 0u) {
            s->f38 = (uint16_t)GS_BD28;                  /* *0xFFF8BD28 (dump) */
            s->act = 1u;
        } else {
            s->act = 0u;
        }
    } else {
        s->act = (s->eng != 0u) ? 1u : 0u;
    }
    if (!(s->cls == 3u && s->f9 != 0u)) {
        if (s->act != 0u) {
            if (pf == 0u) {
                s->f5 = 0u;
                s->f6 = 0u;
            } else {
                s->f5 = b8260(s->eng);
                s->f6 = b827a(s->eng);
                s->f1d = b822c(s->eng) ? 1u : 0u;
            }
            s->f1e = 0u;                                 /* flag (param_5) is 0 here */
            (void)flag;
            s->f3a = 0;
            s->f3b = 0u;
            s->f4 = 1u;
        }
        s->step = 0u;
        s->phase = 0u;
    }
    s->f9 = 0u;
    if (s->rel != 0u) {
        s->f7 = 0u;
        s->f1f = 0u;
    }
    if (s->eng != 0u && s->eng < 6u) {
        s->p64 = GS_P84D4C(s->eng);                      /* **(0x5DE08 + 4e) */
    }
    /* kinds 1/2: +0x68 = 0x125B6C(..) — not ported (kind 0 here) */
    e780(s);
}

/* 0x106866: the sequence types */
static void e6866(uint8_t p2, uint8_t p3)
{
    if (p2 == 4u) {
        if (p3 == 3u) {
            G.sub[0].type = 0u;
            G.sub[1].type = 1u;
            G.sub[2].type = 2u;
            return;
        }
        G.sub[0].type = (G.kind == 2u) ? 4u : 3u;
        G.sub[1].type = 3u;
        G.sub[2].type = 3u;
        return;
    }
    G.sub[0].type = 6u;
    G.sub[1].type = 6u;
    G.sub[2].type = 6u;
}

/* 0x106AE6: the kind (flags B10E / B0FE / B10D / E60F: stand-in 0) */
static uint8_t e6ae6(void)
{
    if (G.c2f == 0u) {
        return (G.e4ac9 != 0u) ? 5u : 0u;
    }
    return 0u;
}

/* 0x1066D6: start the engine */
static void e66d6(uint8_t pf, uint8_t e1, uint8_t e2, uint8_t e3, int8_t gear)
{
    uint8_t flag = 0u;
    if (pf == 0u) {
        G.e4ac6 = 0u;
    }
    if (GS_E6C1 != pf && GS_E6C1 != 0u) {
        flag = 1u;
    }
    G.kind = e6ae6();
    d130(&G.sub[0], pf, e1, gear, flag);
    d130(&G.sub[1], pf, e2, gear, flag);
    d130(&G.sub[2], pf, e3, gear, flag);
    G.e4ac4 = 0u;
    G.e4ac7 = 0u;
    G.e4ac2 = (uint16_t)P.slip;                          /* BD7C */
}

/* 0xDC282: the k-th element of the target gear (role tables 0x6312F /
 * 0x63139 / 0x63143), dropped when already applied and confirmed */
static uint8_t dc282(uint8_t k, uint8_t pf, int8_t gear)
{
    static const uint32_t T[4] = { 0u, 0x0006312Fu, 0x00063139u, 0x00063143u };
    uint8_t e;
    if (k < 1u || k > 3u) {
        W.unp_n++;
        return 0u;
    }
    e = gsf_fl8((uint32_t)((int32_t)T[k] + (int32_t)gear));
    if (k >= 2u && pf != 0u && e != 0u && b8246(e)) {
        e = 0u;
    }
    /* the fill object E60F's active element: E60F inactive here */
    return e;
}

/* ── the synchronisation progress of the shift context (2026-11-18) ─────
 * ~/tcu_sweep/garage_progress_decode.md, re-derived from the bytes:
 *   0xD7C8C  ratio(gear) = *(u16)(*0xFFF84B2C + 2·(gear + 1)), *0xFFF84B2C =
 *            0x4FAA4 (0xD7DDE) → 3306 0 4714 3143 2106 1667 1285 1000 839 667
 *   0x117A74 slip = sat16(ratio(+1)·0x89CB8·s1/1000 − s2·0x89CBA)  (s1/s2:
 *            0x89D5E/0x89D60, forced 1 while 0x89D63/0x89D65 != 0 — dump 01)
 *   0x117F48 sync (+0) / direction (+0x2C) / the BD2E and BD30 clears,
 *            threshold *(*0xFFF84D48 + 0xB0) = cal 0x1FF1F6 = 20 rpm
 *   0x1179D8 → 0xD2058 rate (+0x12) = DT1 of −slip, T = 0xB6EAE(*(*0xFFF84D48
 *            + 0x28) = 0x1B606C, 0x89C08) (250 at 0x89C08 = 0); 0xD2046 the reset
 *   0x117AEC BD76 (+0x0A): synced → BD2E; else rate 0 or diverging → −32768,
 *            else −slip·100/rate; then floored at −500 (0x117B3C..0x117B46:
 *            −32768.0 is floored too — BD76 reads −500, not −32768) → 0xB7910
 *   0x1175D6 the pass (slot 34, 0xEBDBC, unconditional): hold +0x28 = BD17 == 4
 *            ? *0xFFF8D0E0 (D0DC+4, 0xEEA80) : +0x31
 *   0x11793E engagement start (0xEDC5A, BD18 == 0): +0x31 = +0x28 = 1, +0x27 =
 *            (BD1A ∈ {0x1E, 0x0A}) — BD1A = 0 from 0xEB900 → 0; 0x117F48(1, 0);
 *            0x1179D8(block, 1) → 0xD2046
 *   0x11755E end (0xEBB6C ← 0xDDC38): +0x31 = 0, +0x27 = 1
 *   0xEC0E0  slot 28: BD30 / BD2E += *(u16)0xFFF8E220 (= 1), saturating */
static int16_t b7910(float v)
{
    if (!(v > 32767.0f) && !(-32768.0f > v)) {
        return (int16_t)(int32_t)v;
    }
    return (v > 32767.0f) ? (int16_t)0x7FFF : (int16_t)-0x8000;
}

static uint16_t d7c8c(int8_t gear)                       /* the gear ratio ×1000 */
{
    if ((int)gear < -1 || (int)gear > 8) {
        W.unp_n++;                                       /* report (0x66) */
        return 0u;
    }
    return gsf_fl16(0x0004FAA4u + 2u * (uint32_t)((int)gear + 1));
}

static int16_t sync_thr(void)                            /* 0xEB4A4: 20 rpm */
{
    return (int16_t)gsf_fl16(gsf_fl32(0x0005713Cu + 0xB0u));
}

static int16_t a74(void)                                 /* 0x117A74 */
{
    const int32_t r = (int32_t)d7c8c(P.tgear);
    const int32_t v = ((int32_t)GS_D5E * (int32_t)P.cb8 * r) / 1000 - (int32_t)GS_D5E * (int32_t)P.cba;
    return b7888(v);
}

static void f48(uint8_t hold, uint8_t p3)                /* 0x117F48 */
{
    const int16_t thr = sync_thr();
    const uint8_t was = P.sync;
    P.slip = a74();
    if (hold != 0u) {
        P.sync = 0u;
        P.dir = 0xFFu;
    } else if (P.sync == 0u) {
        uint8_t d = P.dir;
        if (d == 0xFFu) {
            d = (G.bd16 == 3u) ? (P.slip >= 0 ? 3u : 1u) : P.p5;
            P.dir = d;
        }
        if (((d == 1u || d == 2u) && (int32_t)P.slip >= -(int32_t)thr) ||
            ((d == 3u || d == 4u) && (int32_t)P.slip <= (int32_t)thr) || p3 != 0u) {
            P.sync = 1u;
        }
    }
    if (P.act3 == 1u) {
        const int32_t a = (P.slip == -0x8000) ? 0x7FFF : (P.slip < 0 ? -(int32_t)P.slip : P.slip);
        if (P.sync == 0u) {
            P.bd2e = 0u;
        }
        if (P.sync == 0u || a > (int32_t)thr) {          /* 0xB77A0 */
            G.bd30 = 0u;
        }
    }
    if (was == 0u && P.sync != 0u) {
        P.sync_pass = W.passes;
        P.sync_pass_ok = 1u;
    }
}

static void dt1_reset(void)                              /* 0xD2046(+0x140, −slip) */
{
    P.ts = G.tb;
    P.acc = (int32_t)(int16_t)(-(int32_t)P.slip) * 1000;
}

static void dt1(void)                                    /* 0x1179D8(p, 0) → 0xD2058 */
{
    const int16_t x = (int16_t)(-(int32_t)P.slip);
    uint16_t t = gsf_b6eae(gsf_fl32(0x0005713Cu + 0x28u), (uint16_t)GS_C08);
    uint16_t dt;
    int16_t y;
    if (t == 0u) {
        t = 1u;
    }
    dt = (uint16_t)(G.tb - P.ts);
    if (dt > t) {
        dt = t;
    }
    y = b7888(((int32_t)x * 1000 - P.acc) / (int32_t)t);
    P.acc += (int32_t)y * (int32_t)dt;
    P.ts = G.tb;
    P.rate = y;
}

static int16_t aec(void)                                 /* 0x117AEC (p4 = 0) → 0xB7910 */
{
    float r;
    const float fr = (float)P.rate;
    if (P.sync != 0u) {
        r = (float)P.bd2e * 1.0f;
    } else if (fr == 0.0f || (fr > 0.0f && P.slip < 0) || (fr < 0.0f && P.slip > 0)) {
        r = -32768.0f;
    } else {
        r = ((float)(-(int32_t)P.slip) * 100.0f) / fr;
    }
    if (-500.0f > r) {
        r = -500.0f;
    }
    return b7910(r);
}

/* slot 34: 0xEBDBC → 0x1175D6 on block A (BD14+0x34 = 0: stand-in) */
static void prog_pass(void)
{
    const uint8_t hold = (G.bd17 == 4u) ? G.sub[0].f4 : P.p31;
    P.hold28 = hold;
    if (hold != 0u) {
        P.hold_n++;
    }
    f48(hold, P.p27);
    dt1();
    G.bd76 = aec();
#if !defined(__sh__)
    if ((s_host_mask & GS_HOST_OLD_PROGRESS) != 0u) {   /* the old stand-ins */
        G.bd76 = 0x7FFF;
        G.bd30 = 0xFFu;
        P.sync = 1u;
    } else if ((s_host_mask & GS_HOST_FORCE_PROGRESS) != 0u) {
        G.bd76 = s_host_bd76;
        G.bd30 = s_host_bd30;
        P.sync = 0u;
    }
#endif
}

/* the engagement start: 0xEDC5A's block-A branch (0x11793E, 0x1179D8(p, 1)) */
static void prog_start(int8_t gear)
{
    P.tgear = gear;                                      /* 0x1176C4 (+1, +2) */
    P.p5 = 0u;
    P.p31 = 1u;
    P.hold28 = 1u;
    P.p27 = 0u;                                          /* BD1A = 0 (0xEB900) */
    P.sync_pass_ok = 0u;
    f48(1u, 0u);
    dt1_reset();
}

/* the end: 0xEBB6C → 0x11755E(block A, 1, …) */
static void prog_end(void)
{
    P.p31 = 0u;
    P.p27 = 1u;
}

/* 0xEB900 + 0xEDC5A (BD14): the shift object enters the garage engagement */
static void eb900(uint8_t pf, uint8_t e1, uint8_t e2, uint8_t e3, int8_t gear)
{
    G.bd16 = (e1 == 0u) ? 0u : (pf == 0u ? 4u : 3u);
    gsf_set_bd16(G.bd16);
    e66d6(pf, e1, e2, e3, gear);
    G.bd15 = (G.bd16 != 0u) ? 1u : 0u;
    /* 0xEDC5A → 0xED906 → 0xED988 */
    if (G.bd16 == 3u || G.bd16 == 4u) {
        G.bd17 = 4u;
    }
    G.bd22 = G.cd2_now;                                  /* 0xED9B2: *0x89CD2 */
    if (G.bd17 == 4u) {
        e6866(G.bd17, G.bd16);
    }
    if (G.bd16 != 0u) {
        prog_start(gear);                                /* 0xEDC5A → 0x11793E */
    }
}

/* 0xDDCA2: the engagement start (from the commit) */
static void ddca2(uint8_t old, uint8_t pf, int8_t gear)
{
    G.e33c[0] = dc282(1u, pf, gear);
    G.e33c[1] = dc282(2u, pf, gear);
    G.e33c[2] = dc282(3u, pf, gear);
    if (old == pf) {
        W.unp_n++;                                       /* report 0x373 */
    }
    G.e33c_n = (int16_t)G.cd0;                           /* +0x10 = 0x89CD0 */
    G.e33c_t = (int16_t)P.cba;                           /* +0xA  = 0x89CBA */
    eb900(pf, G.e33c[0], G.e33c[1], G.e33c[2], gear);
    /* 0xEEDFC (0xFFF8C190) and 0xE984A (BC74: E6C2 = 3 → not started) */
}

/* 0xDDC38: the engagement end */
static void ddc38(void)
{
    G.e33c[0] = 0u;
    G.e33c[1] = 0u;
    G.e33c[2] = 0u;
    e67ec(0u, 0u, 0u);
    prog_end();                                          /* 0xEBB6C → 0x11755E */
    G.bd16 = 0u;                                         /* 0xEBB6C [stand-in] */
    G.bd17 = 0u;
    gsf_set_bd16(0u);
}

/* 0x10CDFA */
static bool cdfa(const gs_sub_t *s, uint8_t p2)
{
    if (s->eng == 0u) {
        return true;
    }
    if (s->act != 0u && (s->tmo == 0u || s->t_e < s->lim12)) {
        return b8246(s->eng) && p2 != 0u;
    }
    return true;
}

/* 0xBAB6C → 0xEAD9A → 0xEDA48 → 0x106918 */
static bool bab6c(uint8_t *cause)
{
    if (G.bd16 == 3u) {
        bool d;
        if (G.sub[0].act == 0u) {
            d = cdfa(&G.sub[1], 1u) && cdfa(&G.sub[2], 1u);
        } else {
            d = cdfa(&G.sub[0], P.sync);         /* BD14+0x58 = BD6C (last pass) */
        }
        if (d) {
            /* 2: after the timeout flag (+0x14: the 0x0E ramp, then the
             * +0x54 test or the +0x12 limit), 1: the normal +0x54 test */
            *cause = (G.sub[0].tmo != 0u) ? 2u : 1u;
        }
        return d;
    }
    /* BD16 == 4 (release, phases 0x12..0x17 not ported) or idle */
    W.rel_n++;
    *cause = 3u;
    return true;
}

/* ── the engine-state FSM 0xD4DC2 (object E6A8) and 0xB986C's flags ───── */
static void d4dc2(void)
{
    uint8_t *e = (uint8_t *)G.e6a8;
    const uint16_t n = G.cd0;                            /* 0x89CD0 */
    const uint8_t q = G.d06;                             /* 0x89D06 */
    const uint8_t r = G.rngd;                            /* 0x89CDC */
    uint8_t st;

    e[2] = (430u < P.cba || GS_D09 != 0u) ? 1u : 0u;                         /* 0xD4F02 */
    e[1] = ((int32_t)50 + 380 < (int32_t)n || (q != 0u && (r == 5u || r == 7u))) ? 1u : 0u;  /* 0xD4EC0 */
    e[0] = (380u < n || q != 0u) ? 1u : 0u;                                  /* 0xD4E9E */
    e[3] = 0u;                                  /* 0xD4F24: 0xB80A4(E5C2) == 1 — stand-in 0 */
    st = e[4];
    switch (st) {
    case 0u:
        if (e[1] == 1u || e[3] == 1u) {
            st = 3u;
        } else if (e[0] == 1u || e[2] == 1u) {
            st = 2u;
        } else {
            st = 1u;
        }
        break;
    case 1u:
        if (e[0] == 1u || e[2] == 1u || e[3] == 1u) {
            st = 2u;
        }
        break;
    case 2u:
        if (e[0] == 0u && e[2] == 0u && e[3] == 0u) {
            st = 1u;
        }
        if (e[1] == 1u || e[3] == 1u) {
            st = 3u;
        }
        break;
    case 3u:
        if (e[0] == 0u && e[2] == 0u && e[3] == 0u) {
            st = 1u;
        }
        break;
    default:
        W.unp_n++;                                       /* report 0x11A */
        st = 3u;
        break;
    }
    e[4] = st;
    G.d0f = (st == 3u) ? 1u : 0u;                        /* 0xB7C7C → +0xD3 */
    G.d10 = (st == 1u) ? 1u : 0u;                        /* 0xB7C6E → +0xD4 */
    G.d12 = (G.pf == 0u && G.c2f == 0u && G.d10 != 0u) ? 1u : 0u;   /* +0xD6 */
}

/* ── E28E, the exception machine (0xD093A slot 30, 0xD0BAA slot 32) ──────
 * D0D6A = (AE2C == 1) = 0; E618 (0xBA744) = 0; 0x89C8A = 0 [stand-ins] */
#define D0D6A   0u
#define E618    0u
#define C8A     0u
static void d093a(void)
{
    switch (G.e28e) {
    case 0u:
        G.e28e_f = 0u;
        if (D0D6A) { G.e28e = 1u; }
        else if (G.d12) { G.e28e = 4u; G.e28e_f |= 8u; }
        else if (E618) { G.e28e = 4u; G.e28e_f |= 0x80u; }
        else if (C8A == 1u && GS_AE2D == 0u) { G.e28e = 7u; G.e28e_f |= 0x20u; }
        break;
    case 2u:
        G.e28e_f = 2u;
        if (G.e28e_t6 > 0u || D0D6A) { /* wait */ }
        else if (G.d12) { G.e28e = 4u; G.e28e_f |= 8u; }
        else if (E618) { G.e28e = 4u; G.e28e_f |= 0x80u; }
        else { G.e28e = 3u; }
        break;
    case 5u:
        G.e28e_f = 2u;
        if (D0D6A) { G.e28e = 1u; }
        else if (G.d12 || E618) { /* stay */ }
        else { G.e28e = 6u; }
        break;
    case 7u:
        if (D0D6A) { G.e28e = 1u; }
        else if (G.d12 || E618) { G.e28e = 4u; }
        else if (C8A == 0u || GS_AE2D == 1u) { G.e28e = 0xBu; G.e28e_t8 = GS_E28E_T8; }
        else if (G.e6ee != 0u || G.e69b != 0u) { /* stay */ }
        else { G.e28e = 8u; }
        break;
    case 9u:
        if (D0D6A) { G.e28e = 1u; }
        else if (G.d12 || E618) { G.e28e = 4u; }
        else if (C8A != 0u && GS_AE2D != 1u) { /* stay */ }
        else { G.e28e = 10u; G.e28e_t8 = GS_E28E_T8; }
        break;
    case 0xBu:
        if (D0D6A) { G.e28e = 1u; }
        else if (G.d12 || E618) { G.e28e = 4u; }
        else if (G.e28e_t8 > 0u && GS_AE2D != 1u) { /* stay */ }
        else { G.e28e = 0u; }
        break;
    case 1u: case 3u: case 4u: case 6u: case 8u: case 10u:
        break;
    default:
        W.unp_n++;                                       /* report 0x89 */
        G.e28e = 0u;
        break;
    }
}
static void d0baa(void)
{
    switch (G.e28e) {
    case 1u:  G.e28e = 2u;  G.e28e_f = 2u; break;
    case 3u:  G.e28e = 0u;  break;
    case 4u:  G.e28e = 5u;  G.e28e_f = 2u; break;
    case 6u:  G.e28e = 0u;  break;
    case 8u:  G.e28e = 9u;  G.e28e_f = 5u; break;
    case 10u: G.e28e = 0xBu; break;
    default: break;
    }
}
static bool d0d52(void) { return G.e28e != 0u && G.e28e != 7u; }

/* 0xD0C94: the holder's override code */
static uint8_t d0c94(uint8_t st)
{
    if (st > 0x11u) { return 0u; }
    if (st >= 1u && st <= 6u) { return 2u; }
    if ((st >= 8u && st <= 10u) || st >= 15u) { return 5u; }
    if (st >= 12u && st <= 14u) { return 4u; }
    return 0u;
}

/* ── the power-flow module (0xCFFDE / 0xD01C4 / 0xD01E6) ───────────────── */
static bool pending(void) { return G.e6ee != G.e69b; }            /* 0xCFE62 */

/* 0x103F56 (AE70[1] = 0, AE2C = 0, E298 = 0: stand-ins) */
static void r3f56(void)
{
    if (!d0d52() && G.e28e != 7u && G.d0f != 0u /* && D11A6(E298) == 0 && AE70[1] != 3 */) {
        const uint32_t i = (uint32_t)G.ae28 + 3u * (uint32_t)G.e69b;
        if (i < 9u) {
            G.e6ef = gsf_fl8(0x0006354Cu + i);
        } else {
            W.unp_n++;
            G.e6ef = 0u;
        }
    } else {
        G.e6ef = 0u;
    }
}

/* 0xD15A6 (0xD1B10 / 0xD1990: the gear target, stand-in) */
static void d15a6(void)
{
    if (G.c3d != 0u && G.e69b == 1u) {
        G.e6a1 = G.e69f;
    }
    if (G.e6ef == 2u || G.e69b == 2u) {
        G.e6a1 = -1;
    } else {
        G.e6a1 = (int8_t)GS_TARGET;
    }
}

/* 0xDC37E: the gate of a nonzero commit */
static bool dc37e(void)
{
    return s_park_model == 1u && G.c3d == 0u && GS_E1F_SETTLE <= G.e6c5;
}

/* 0xCFD1A: the commit */
static void cfd1a(void)
{
    bool ok;
    if (G.e6ef == 0u) {
        ok = true;
    } else if (G.e6ef == 1u) {
        ok = G.e69f != -1;
    } else if (G.e6ef == 2u) {
        ok = G.e6a1 == -1 && G.e69f == -1;
    } else {
        W.unp_n++;                                       /* report 0x90 */
        ok = false;
    }
    if (G.e69c0 == 1u) {
        ok = false;
    }
    if (ok && !pending() && G.e6ef != G.e69b && (G.e6ef == 0u || dc37e())) {
        const uint8_t old = G.e69b;
        G.e69b = G.e6ef;
        G.pf = G.e6ef;                                   /* 0x89C30 */
        G.commit_now = 1u;
        W.commits++;
        W.commit_pass = W.passes;
        W.commit_old = old;
        W.commit_new = G.e69b;
        if (G.e69b != 0u && W.rec_slot < GS_REC_SLOTS) {  /* the recorder (ours) */
            G.rec_on = W.rec_slot;
            W.rec_n[W.rec_slot] = 0u;
            W.rec_slot = (uint8_t)(W.rec_slot + 1u);
            G.rec_t0 = W.passes;
        }
        ddca2(old, G.e69b, G.e69f);                      /* 0xBAABC */
    }
}

/* 0x103DE2: completion of a pending change */
static void r3de2(void)
{
    if (pending()) {
        uint8_t cause = 4u;
        const bool done = bab6c(&cause);
        if (done || G.e6ef != G.e69b || d0d52()) {
            if (!done) {
                cause = 4u;
            }
            G.e6ee = G.e69b;
            G.c2f = G.e69b;                              /* 0x89C2F */
            ddc38();                                     /* 0xBAAA6 */
            G.done_now = 1u;
            W.done_cause = cause;
            W.done_pass = W.passes;
        }
    }
}

/* 0x104A28 (+ OUR stand-in for its undecoded sequencing branch: with no
 * change pending in power flow 0, E4A7 follows E6A1, so a gear code of −1
 * left by an R engagement returns to the target in N/P) */
static void r4a28(void)
{
    if (pending() || (G.e4a7 != G.e6a1 && G.e6a1 == -1)) {
        G.e4a7 = G.e6a1;
    } else if (G.e69b == 0u && G.e4a7 != G.e6a1) {
        W.seq_n++;
        G.e4a7 = G.e6a1;
    }
}

/* 0x103CEC strobe (shift-object checks: accept) → 0xD140C */
static void r3cec(void)
{
    if (G.e4a7 != G.e69f && !pending()) {
        G.e69f = G.e4a7;                                 /* 0x89C2B */
        G.e69c0 = 1u;
    }
}

/* ── the producer 0xDC4C4 into the SHADOW request objects ──────────────── */
static uint8_t s_park_req_in;

static void e54a4(uint32_t o)                            /* Sys / WK method */
{
    uint16_t r;
    if (RU8(o, RO_MODE) != 0u) {
        const uint16_t p = RU16(o, RO_P);
        if (p >= GS_LINEP || p == 0u) {
            r = p;
        } else {
            r = (o == GS_OBJ_WK) ? 0u : (uint16_t)GS_LINEP;
        }
    } else {
        r = RU16(o, RO_P);
    }
    RU16(o, RO_RES) = r;
    RU16(o, 0x04u) = r;
    if ((uint32_t)(int32_t)(int16_t)r > (uint32_t)(int32_t)(int16_t)RU16(GS_OBJ_SYS, RO_RES)) {
        RU16(o, 0x04u) = RU16(GS_OBJ_SYS, RO_RES);
    }
}

static void e53da(uint32_t o, uint16_t p, uint8_t mode, uint8_t prio, uint8_t src)
{
    RU8(o, RO_SET) = 1u;
    RU16(o, RO_P) = p;
    RU8(o, RO_MODE) = mode;
    RU8(o, RO_SRC) = src;
    RU8(o, RO_PRIO) = prio;
    if (o == GS_OBJ_SYS || o == GS_OBJ_WK) {
        e54a4(o);
    } else {
        gsf_e4e5c(o);
    }
}

static uint8_t dc1e2(uint8_t block, int8_t gear, uint8_t e)
{
    static const uint8_t T63122[6] = { 0, 1, 0, 0, 0, 0 };
    const uint8_t m = d0c94(G.e28e);
    if (s_park_req_in != 0u && GS_E6C3 != 0u) {
        return 0u;                                       /* 0x63128: all 0 */
    }
    if (m == 5u) {
        return T63122[e];
    }
    if (m == 2u) {
        return 0u;                                       /* 0x6314C: all 0 */
    }
    if ((int)gear < -1 || (int)gear > 8) {
        W.unp_n++;
        return 0u;
    }
    /* 0x505A4[e + (gear + 1)·6 + block·0x3C] (the shadow's flash copy) */
    return gsf_fl8(0x000505A4u + (uint32_t)e + (uint32_t)((int)gear + 1) * 6u + (uint32_t)block * 0x3Cu);
}

static bool e68ac(void)                                  /* 0x1068AC */
{
    return G.sub[0].act != 0u || (G.sub[1].act != 0u && G.sub[1].f9 == 0u) || G.sub[2].act != 0u;
}

static void dc4c4(void)
{
    static const uint32_t EO[6] = { 0u, GS_OBJ_A, GS_OBJ_B, GS_OBJ_C, GS_OBJ_D, GS_OBJ_E };
    const uint8_t block = (G.pf != 0u) ? 1u : 0u;

    e53da(GS_OBJ_SYS, (uint16_t)GS_LINEP, (uint8_t)GS_SYSMODE, 4u, 0x15u);
    if ((G.pf == 1u && G.c2f == G.pf)) {
        W.lockup_n++;                                    /* 0xDCD38.. lockup branch: not ported */
    }
    e53da(GS_OBJ_WK, 0u, 1u, 4u, 0x16u);
    if (e68ac()) {                                       /* 0xDC812 */
        for (uint8_t k = 0u; k < 3u; k++) {
            const uint8_t e = G.e33c[k];
            if (e != 0u && RU8(req_obj(e), RO_SET) == 0u) {
                const gs_sub_t *s = &G.sub[k];
                const uint8_t pf0 = (G.pf == 0u) ? 1u : 0u;
                const uint8_t id = (uint8_t)(5u + 2u * k + (pf0 ? 0u : 1u));
                const uint8_t prio = pf0 ? 1u : 0u;
                if (pf0 && !b822c(e)) {
                    continue;
                }
                e53da(req_obj(e), (uint16_t)b77bc(s->out_p), s->out_mode, prio, id);
            }
        }
    }
    {
        const uint8_t m = d0c94(G.e28e);
        for (uint8_t e = 1u; e <= 5u; e++) {
            const uint32_t o = EO[e];
            bool hold = RU8(o, RO_SET) == 0u;
            if (!hold && m != 0u) {
                hold = !(m == 5u && e == 1u);
            }
            if (hold) {
                e53da(o, dc1e2(block, G.e69f, e) != 0u ? 20000u : 0u, 1u, 4u, 0u);
            }
        }
    }
    for (uint8_t e = 1u; e <= 5u; e++) {
        RU8(EO[e], RO_SET) = 0u;
    }
    RU8(GS_OBJ_SYS, RO_SET) = 0u;
    RU8(GS_OBJ_WK, RO_SET) = 0u;
}

/* the actuators (0x107BD2 / 0x107FB8 as app/rq_prod.c ports them) */
#define GS_IMIN 500u                                     /* cal 0x1FEF6C (**0xFFF84C94) */
#define GS_IMAX 8500u                                    /* cal 0x1FEF6E */
/* 0xD9CBC: i_min = 0 while reaction 37 is active (0xA031C(*0x632CB = 0x25))
 * for every actuator except WK / Sys (id 5 / 6) [O bytes 0xD9CCC..0xD9CE2] */
static uint16_t d9cbc(uint8_t id)
{
    if (P.r37 != 0u && id != 6u && id != 5u) {
        return 0u;
    }
    return GS_IMIN;
}
static uint16_t law1(uint8_t id, uint8_t type, uint16_t p)
{
    if (p == 0u) {
        return type != 0u ? GS_IMAX : d9cbc(id);
    }
    if (p < GS_LINEP && p < 12750u) {
        return d9cbc(id);                                /* fault 0x1C4 / 0x1C2 */
    }
    return type == 0u ? GS_IMAX : d9cbc(id);
}
static void actuators(void)
{
    static const struct { uint32_t o; uint8_t id, type, supply; } A[7] = {
        { GS_OBJ_A, 0u, 0u, 0u }, { GS_OBJ_B, 2u, 0u, 0u }, { GS_OBJ_C, 4u, 1u, 0u },
        { GS_OBJ_D, 1u, 1u, 0u }, { GS_OBJ_E, 3u, 1u, 0u }, { GS_OBJ_WK, 5u, 0u, 1u },
        { GS_OBJ_SYS, 6u, 1u, 1u },
    };
    for (unsigned k = 0u; k < 7u; k++) {
        const uint32_t o = A[k].o;
        const uint16_t p = RU16(o, RO_RES);
        uint16_t i;
        if (A[k].supply != 0u) {
            if (RU8(o, RO_MODE) == 0u) {
                W.unp_n++;                               /* 0x107FE0: not ported */
                i = 0u;
            } else {
                i = law1(A[k].id, A[k].type, p);
            }
        } else if (RU8(o, RO_MODE) == 1u) {
            if (p == 0u && RU8(o, RO_5A) == 1u) {
                W.unp_n++;                               /* 0x107D90: not ported */
                i = 0u;
            } else {
                i = law1(A[k].id, A[k].type, p);
            }
        } else {
            i = gsf_107c3e(A[k].id, A[k].type, p, d9cbc(A[k].id), GS_IMAX);
        }
        G.cur[A[k].id] = i;
    }
}

/* ── the recorder (ours) ───────────────────────────────────────────────── */
static void rec_pass(void)
{
    static const uint32_t EO[5] = { GS_OBJ_A, GS_OBJ_B, GS_OBJ_C, GS_OBJ_D, GS_OBJ_E };
    const uint8_t slot = G.rec_on;
    uint8_t n;
    volatile gs_rec_t *r;
    uint8_t modes = 0u, set = 0u;

    if (slot >= GS_REC_SLOTS) {
        return;
    }
    n = W.rec_n[slot];
    if (n >= GS_REC_N) {
        G.rec_on = 0xFFu;
        return;
    }
    r = rec_at(slot, n);
    for (unsigned e = 0u; e < 5u; e++) {
        if (RU8(EO[e], RO_MODE) != 0u) { modes = (uint8_t)(modes | (1u << e)); }
        if (RU16(EO[e], RO_P) != 0u)   { set = (uint8_t)(set | (1u << e)); }
    }
    for (unsigned k = 0u; k < 3u; k++) {
        if (G.sub[k].out_mode != 0u) { modes = (uint8_t)(modes | (1u << (5u + k))); }
    }
    r->pass = (uint16_t)(W.passes - G.rec_t0);
    r->pf = G.pf;
    r->flags = (uint8_t)((G.e6ef & 3u) | (pending() ? 4u : 0u) | (e68ac() ? 8u : 0u) |
                         (G.sub[0].tmo != 0u ? 0x10u : 0u) | (G.done_now ? 0x20u : 0u) |
                         (G.commit_now ? 0x40u : 0u) | (G.c3d != 0u ? 0x80u : 0u));
    r->ph[0] = G.sub[0].phase;
    r->ph[1] = G.sub[1].phase;
    r->ph[2] = G.sub[2].phase;
    r->modes = modes;
    r->reqset = set;
    r->gear = G.e69f;
    for (unsigned c = 0u; c < 7u; c++) {
        r->cur[c] = G.cur[c];
    }
    r->bd30 = G.bd30;
    r->sync = (uint8_t)((P.sync != 0u ? 1u : 0u) | (P.hold28 != 0u ? 2u : 0u) |
                        (P.bd2e == 0xFFFFu ? 4u : 0u));
    r->bd76 = G.bd76;
    W.rec_n[slot] = (uint8_t)(n + 1u);
}

/* ── the fill inputs of the shadow instance ────────────────────────────── */
static void fill_bind(void)
{
    gs_fill_in_t f = {
        .temp = G.c13, .c1c = (uint8_t)GS_C1C, .c0a = (uint8_t)GS_C0A,
        .eng_stop = G.d10,                   /* the SHADOW's 0x89D10 (live engine sim) */
        .c3d = G.c3d, .cd2 = G.cd2_now, .e5c2 = 0u, .b0fe = 0u, .e3ed = 0u, .ba79c = 0u,
        .e6a3 = 0x22u, .rec1fd = 1u, .lp = (uint16_t)GS_LINEP, .lp2 = 20000u,
        /* the adaptations, EEPROM block 0x0304 [O dump, adaptation_decode]:
         * fill time +0x13E (ms) and kiss +0x42 (= +0x11A offset + the
         * temperature curve at the dump's 26 °C; the curve is not ported) */
        .adapt38 = { 4, 16, 10, -6, -5 },
        .adapt42 = { 600u, 482u, 993u, 838u, 611u },
        .ba7dc = { 1, 1, 0, 0, 0 },          /* as app/rq_prod.c (0xBA7DC stand-in) */
        .v10cadc = { 0u, 0u, 0u, 0u, 0u },
        .k4b = { 3u, 4u, 6u, 8u, 9u },       /* dump +0x4B (A..E) */
        .rec_frac = { 0u, 0x2710u, 0u, 0u, 0u },   /* record 0x1FD +0xC0 (B 10000) */
    };
    gsf_bind_in(&f);
}

static bool s_booted;

void gs_shadow_init(void)
{
    volatile uint8_t *b = (volatile uint8_t *)&g_s;
    for (unsigned i = 0u; i < sizeof g_s; i++) { b[i] = 0u; }
    b = (volatile uint8_t *)&g_w;
    for (unsigned i = 0u; i < sizeof g_w; i++) { b[i] = 0u; }
    b = (volatile uint8_t *)g_rec0;
    for (unsigned i = 0u; i < sizeof g_rec0; i++) { b[i] = 0u; }
    b = (volatile uint8_t *)g_rec1;
    for (unsigned i = 0u; i < sizeof g_rec1; i++) { b[i] = 0u; }
    b = (volatile uint8_t *)&g_p;
    for (unsigned i = 0u; i < sizeof g_p; i++) { b[i] = 0u; }
    for (uint32_t a = GS_REQ_BASE; a < GS_REQ_BASE + GS_REQ_SIZE; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    gs_fill_cells.c851a2 = 0u;
    gs_fill_cells.c851a3 = 0u;
    gs_fill_cells.c851a5 = 0u;
    gs_fill_cells.e384 = 0u;
    G.e28e = 1u;                                         /* 0xD0C5A */
    G.e28e_f = 4u;
    G.e69f = (int8_t)GS_TARGET;
    G.e6a1 = (int8_t)GS_TARGET;
    G.e4a7 = (int8_t)GS_TARGET;
    G.sub[0].cls = 2u;
    G.sub[1].cls = 3u;
    G.sub[2].cls = 4u;
    for (unsigned k = 0u; k < 3u; k++) {
        d0b4(&G.sub[k]);
        G.sub[k].type = 6u;
    }
    G.e4ac5 = GS_E4B1;
    /* the shift context at its idle values (dump: long-idle synchronised) */
    G.bd30 = (uint8_t)GS_IDLE_BD30;
    G.bd76 = (int16_t)GS_IDLE_BD76;
    P.bd2e = (uint16_t)GS_IDLE_BD2E;
    P.sync = 1u;
    P.tgear = 3;
    P.act3 = 1u;
    P.p27 = 1u;
    P.p31 = 0u;
    P.dir = 0u;
    P.r37 = (uint8_t)GS_R37;
#if !defined(__sh__)
    if ((s_host_mask & GS_HOST_R37) != 0u) {
        P.r37 = 1u;
    }
#endif
    G.rec_on = 0xFFu;
    G.c3d = GS_FORCED ? 0u : 1u;
    fill_bind();
    for (uint8_t e = 1u; e <= 5u; e++) {
        gsf_construct(req_obj(e), 0xFFF84D20u + 4u * (uint32_t)(e - 1u), e);
    }
    s_booted = false;
}

void gs_shadow_tick_10ms(const gs_in_t *in)
{
    if (in->os_mode < 1u || in->os_mode > 3u) {
        return;                                          /* the stock's lists: modes 1..3 */
    }
    s_park_model = in->park_model;
    s_park_req_in = in->park_req;
    G.done_now = 0u;
    G.commit_now = 0u;
    /* slot 26: the one-hot mode flag */
    G.c3d = GS_FORCED ? 0u : ((in->os_mode == 1u) ? 1u : 0u);
    /* slot 27: 0xB912C (0x13BF66: E90E held while 0x108 is invalid) */
    G.rngd = in->rngd;
    if (in->eng_valid != 0u && in->eng_speed != 0xFFFFu) {
        G.cd0 = in->eng_speed;
        G.d06 = 0u;
    } else {
        G.d06 = 2u;
    }
    G.c13 = in->temp;
    G.cd2_now = in->cd2;
    P.cb8 = in->out_rpm;                                 /* 0x89CB8 (stand-in: live sensor) */
    P.cba = in->tur_rpm;                                 /* 0x89CBA (stand-in: live sensor) */
    fill_bind();
    gsf_set_pf(G.pf, G.c2f);
    if (!s_booted) {
        /* the boot-state-1 pass: 0xE5602, then the producer + actuators */
        s_booted = true;
        gsf_e5602();
        d4dc2();
        dc4c4();
        actuators();
        W.passes++;
        return;
    }
    /* slot 28: 0xEC0E0 — BD30 / BD2E (+= *0xFFF8E220 = 1), then 0x106848 */
    G.bd30 = sat8p1(G.bd30);
    P.bd2e = sat16p1(P.bd2e);
    /* the timers, the time base, the element tick / update */
    G.e4ac6 = sat8p1(G.e4ac6);
    cf4e(&G.sub[0]);
    cf4e(&G.sub[1]);
    cf4e(&G.sub[2]);
    if (G.e28e_t6 > 0u) { G.e28e_t6--; }
    if (G.e28e_t8 > 0u) { G.e28e_t8--; }
    if (G.c3d != 0u) {
        G.e6c5 = 0u;                                     /* 0xDC35E */
    } else {
        G.e6c5 = sat8p1(G.e6c5);                         /* 0xDBEEC */
    }
    G.tb = (uint16_t)(G.tb + 10u);
    gsf_e5624();
    gsf_e566c();
    /* slot 29: 0xB986C */
    d4dc2();
    /* slot 30: 0xCFFDE */
    if (G.rngd <= 8u) {
        const uint8_t x = gsf_fl8(0x00063445u + G.rngd);
        if (x != 0xFFu) {
            G.ae28 = x;
        }
    } else {
        W.unp_n++;                                       /* report 0x7E */
    }
    d093a();
    r3f56();
    d15a6();
    /* slot 31: 0xD01C4 */
    r3de2();
    cfd1a();
    r3de2();
    r4a28();
    /* slot 32: 0xD01E6 */
    r3de2();
    r3cec();
    r3de2();
    d0baa();
    /* slot 34: 0xEBDBC → 0x1175D6 (block A): sync, rate, BD76 */
    prog_pass();
    /* slot 35: 0xEB348 → step (D148, D1B4, D0DC), output (same order) */
    ce88(&G.sub[1]);
    ce88(&G.sub[2]);
    ce88(&G.sub[0]);
    ceca(&G.sub[1]);
    ceca(&G.sub[2]);
    ceca(&G.sub[0]);
    /* slot 42: the producer + actuators */
    gsf_set_pf(G.pf, G.c2f);
    dc4c4();
    actuators();
    rec_pass();
    G.e69c0 = 0u;                                        /* [I] one pass */
    W.e28e = G.e28e;
    W.e6a8 = G.e6a8[4];
    W.now_pf = G.pf;
    W.now_e6ef = G.e6ef;
    W.now_ae28 = G.ae28;
    W.now_flags = (uint8_t)((G.d0f != 0u ? 1u : 0u) | (pending() ? 2u : 0u) |
                            (G.c3d != 0u ? 4u : 0u) | (e68ac() ? 8u : 0u));
    W.now_gear = G.e69f;
    for (unsigned k = 0u; k < 3u; k++) {
        W.now_ph[k] = G.sub[k].phase;
    }
    for (unsigned c = 0u; c < 7u; c++) {
        W.now_cur[c] = G.cur[c];
    }
    W.passes++;
}

const volatile gs_wit_t *gs_shadow_wit(void) { return &g_w; }
const volatile gs_rec_t *gs_shadow_rec(uint8_t slot, uint8_t i)
{
    return (slot < GS_REC_SLOTS && i < GS_REC_N) ? rec_at(slot, i) : 0;
}
int16_t  gs_shadow_bd76(void)        { return g_s.bd76; }
uint8_t  gs_shadow_bd30(void)        { return g_s.bd30; }
uint8_t  gs_shadow_sync(void)        { return g_p.sync; }
int16_t  gs_shadow_slip(void)        { return g_p.slip; }
uint16_t gs_shadow_cur(uint8_t ch)   { return ch < 7u ? g_s.cur[ch] : 0xFFFFu; }
uint8_t  gs_shadow_pf(void)          { return g_s.pf; }
int8_t   gs_shadow_gear(void)        { return g_s.e69f; }
uint8_t  gs_shadow_phase(uint8_t k)  { return k < 3u ? g_s.sub[k].phase : 0xFFu; }
uint8_t  gs_shadow_pending(void)     { return g_s.e6ee != g_s.e69b ? 1u : 0u; }
uint8_t  gs_shadow_e6ef(void)        { return g_s.e6ef; }
uint8_t  gs_shadow_ae28(void)        { return g_s.ae28; }
uint8_t  gs_shadow_d0f(void)         { return g_s.d0f; }

#if !defined(__sh__)
void gs_shadow_host_sub(uint8_t k, int16_t v[6])
{
    const gs_sub_t *s = &G.sub[k < 3u ? k : 0u];
    v[0] = s->calc_p;
    v[1] = s->base24;
    v[2] = s->tgt54;
    v[3] = s->dur30;
    v[4] = (int16_t)s->t32;
    v[5] = (int16_t)RU16(req_obj(s->eng != 0u ? s->eng : 2u), RO_RES);
}

void gs_shadow_host_standins(uint8_t mask, int16_t bd76, uint8_t bd30)
{
    s_host_mask = mask;
    s_host_bd76 = bd76;
    s_host_bd30 = bd30;
    gsf_host_e469c_off((mask & GS_HOST_OLD_E469C) != 0u);
}
#endif

#endif /* !TCU_RAM_APP */

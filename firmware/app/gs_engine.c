/* gs_engine.c — THE GARAGE ENGINE TEMPLATE (G6-0, 2026-11-19, ~/tcu_sweep/
 * g6_0_port.md). NOT a translation unit of its own: a BINDING includes it.
 * G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): the image carries ONE binding,
 * app/gs_cand.c (the engine, the producer of the live A..E currents under
 * the monitor); instance 1 (the shadow) is a host-only test reference
 * (tests/unit/gs_ref_shadow.c). The engine's stock objects (E28E, E33C, E4A7,
 * E4AC, E69B, E69F, E6A1, E6A8, E6C5, E6EE, E6EF, the shift object BD14 with
 * its context block A) are their own objects at their stock offsets, pinned
 * at their stock addresses on SH (gse_* below, linker.ld .gse_*: D-G6-9).
 *   instance 1  app/gs_shadow.c — the read-only shadow: today's RAM
 *               (.gs_obj / .gs_rec / .gs_prog / .gs_rec1 / .gs_rel, the
 *               request objects at gs_fill.h GS_REQ_BASE), today's symbols
 *               (gs_shadow_*, the fill instance gsf_* of app/gs_fill.c);
 *   instance 2  app/gs_cand.c + app/gs_cand_fill.c (binding
 *               app/gs_cand_bind.h) — the future G6a CANDIDATE, HOST-ONLY in
 *               G6-0: its own request objects (GSE_REQ_BASE), its own fill
 *               instance (gcf_*), its exports renamed gs_cand_*.
 * The binding supplies, BEFORE including this file: GSE_BINDING; for a
 * second instance also GSE_REQ_BASE (gs_fill.h: GSE_OBJ_* follow it) and
 * the renames of every exported name (the shadow's API and the fill's
 * gsf_* names). Everything the engine owns lives in this file's statics
 * (one set per binding TU) and in the GSE_OBJ_* request objects.
 * The text below this note is r04l's app/gs_shadow.c, unchanged except
 * that the request objects are named GSE_OBJ_* (gs_fill.h) and a host-only
 * state accessor (gs_shadow_host_state) is appended.
 *
 * G6a (2026-11-19, ~/tcu_sweep/g6a_port.md): the binding also chooses
 *   GSE_SEC_OBJ/REC/PRG/RC1/REL  the SH sections of the statics (default:
 *                                instance 1's .gs_obj / .gs_rec / .gs_prog /
 *                                .gs_rec1 / .gs_rel — unchanged strings, so
 *                                instance 1's object code is r04m's);
 *   GSE_REC 0                    no recorders (no g_rec0/1, the release
 *                                object reduced to its header gs_rel_t up to
 *                                .rec): the candidate's RAM budget;
 *   GSE_AE28_IN                  the direction request AE28 is the LIVE
 *                                0xFFF8AE28 by value (gs_in_t.ae28; the G6b
 *                                form: one 0xCFFDE, park_law's), the
 *                                template's own table lookup kept as a
 *                                cross-check counter;
 *   GSE_BOOT_ENTRY               an explicit boot-state-1 pass entry
 *                                (gs_shadow_boot_pass, the G6b slot: row-1
 *                                list entries 12 / 17 in OS mode 0, as
 *                                app/rq_prod.c's rq_prod_boot_pass);
 *   GSE_CLEAR_BEGIN/END          linker symbols of the binding's whole RAM
 *                                block, cleared by init (NOLOAD RAM is not
 *                                cleared by start.S: the fill instance's
 *                                statics — 0x89CF8 time base, rec_frac,
 *                                counters, the host-negative flag
 *                                s_e469c_off — start defined).
 *
 * (r04l) a READ-ONLY shadow of the stock's GARAGE ENGAGEMENT, ported
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
 *   [G4a 2026-11-19: the release phases 0x12/0x13/0x14 ARE ported — see
 *   below; only kind 2's 0x15..0x17 remain counted] 0x10E896's 0xE9464/
 *   0xE945E term (0); 0xBA178 / 0xEAD94 / 0xE9446 notifications; BC74 (ids
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
 *
 * 2026-11-19 (~/tcu_sweep/g4a_release_port.md, decode garage_release_
 * decode.md, re-derived from the bytes) — G4a, THE RELEASE, replacing the
 * one-pass stand-in: a power-flow-0 commit (lever P/N, D↔R through N)
 * starts BD16 = 4 through the same 0xDDCA2 → 0xEB900 → 0x1066D6 path,
 * the three sub-objects type 3 (0x106866) → phases 0x12 (entry 0x10D824:
 * +0x26 = 0xB82E2 = elem +2 − +0x42, the class-3 / element-C case
 * 0x10D836; done 0x10DC4A: classes 2/4 at once, class 3 on 0x10E850),
 * 0x13 (pressure 0x10E25A: class 2 min(+0x26, map_u8(blk5C+0x28)·0x10F00C
 * /100 + 0x10F030 + map(blk5C+0x30)), class 4 kind 0 min(+0x26,
 * map2(blk5C+8) + map2(blk5C+0)), class 3 0; done 0x10DCE8: the vent
 * flags +0x1F with the partner, the speed exit |BD7C| >= 0x10F26C), 0x14;
 * the output 0x10CECA's release mode on the ELEMENT (+0x21 == 2), not the
 * class; the pass counter BD8E (block A +0x22: 0 at every start by
 * 0x117970, +1 in slot 28 by 0x117538); completion 0xEDA48 (+2 == 4) →
 * 0x106918 (cause 5) or BD8E >= D0DC +0x12 (0x10F06A, cause 6). Stand-in:
 * the release's 0x10E896 term D252 (zeroed at the start, no writer found)
 * = 0 (counted, gs_rel_t.e896_n). Kind 2 (0x15..0x17, the class-4 kind
 * 2/7 maps, lim12 kind 2) is unreachable here (kind 0): counted unp_n.
 */
#if !defined(GSE_BINDING)
#error "gs_engine.c is the garage engine TEMPLATE: compile a binding (app/gs_shadow.c), not the template"
#endif
#define TCU_GS_SHADOW_TU 1
#include "app/gs_shadow.h"
#include "app/gs_fill.h"
#include <stddef.h>
#ifndef GSE_REC
#  define GSE_REC 1
#endif
/* G6b: a binding may time the tick's slot groups (the candidate: app/g6a.c
 * g6a_stage, the CPU witnesses); instance 1 compiles nothing for it */
#ifndef GSE_STAGE
#  define GSE_STAGE(k) ((void)0)
#endif
#if defined(GSE_BOOT_ENTRY) && (GS_FORCED != 1)
#error "gs_cand: the G6a candidate runs D-G6-2 (0x89C3D forced 0) and D-G6-3 (gear target 1st): GS_FORCED must be 1"
#endif

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
/* r04s: E6C1 / E6C2 (0xFFF8E6C1 / C2) are no longer stand-ins (0 / 3):
 * 0xDC0F8 is ported (slot 42, gse_e6c1_t below) */
/* r04s: the accelerator pedal 0x89CD8 (0xB912C's copy of 0xFFF9545E, the
 * safety object 0xFFF95458's conditioned 0x120 byte 2: raw · 127 / 125, the
 * no-frame default *(0x195DA8) = 23 [O cal]). We receive 0x120 (io/vehicle
 * profile_fca.c, permil) but do NOT run the stock's conditioning (0x13C99C /
 * 0x13C9BA): the stock's no-frame value stands in, by value — the bench
 * sends no 0x120, so the stock reads exactly this */
#define GS_PEDAL    23u         /* 0xFFF89CD8 (stand-in = cal 0x195DA8) */
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
               GS_PEDAL == 23u && GS_C25 == 3 && GS_AE2D == 1u && GS_LINEP == 20000u &&
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
#  ifndef GSE_SEC_OBJ                                   /* instance 1 (G6a: binding-chosen) */
#    define GSE_SEC_OBJ ".gs_obj"
#    define GSE_SEC_REC ".gs_rec"
#    define GSE_SEC_PRG ".gs_prog"
#    define GSE_SEC_RC1 ".gs_rec1"
#    define GSE_SEC_REL ".gs_rel"
#  endif
#  define GS_OBJ __attribute__((section(GSE_SEC_OBJ)))
#  define GS_REC __attribute__((section(GSE_SEC_REC)))
#  define GS_PRG __attribute__((section(GSE_SEC_PRG)))  /* the tail of .gs_obj */
#  define GS_RC1 __attribute__((section(GSE_SEC_RC1)))  /* the tail of .gs_obj */
#  define GS_REL __attribute__((section(GSE_SEC_REL)))  /* G4a: the release (0xFFF8872C) */
#else
#  define GS_OBJ
#  define GS_REC
#  define GS_PRG
#  define GS_RC1
#  define GS_REL
#endif

/* r04s (2026-11-19, ~/tcu_sweep/r04s_port.md; layout b2_slots_subobj_decode.md
 * §B, re-checked against the constructor 0x10D010 and the driving dump's D148
 * `+0x40 43D98000 07D0 0000 C69C4000 469C4000 | 00049FC0`): the ramp object
 * at sub +0x40 — 0x14 bytes, its vtable word 0x49FC0 at its +0x10 (= sub
 * +0x50, written by 0x10D010 at 0x10D02E/0x10D030: base 0x49F9C, then the
 * class 0x49FC0). The vtable word is stored as the value only: our port
 * calls 0xD1C4C / 0xD1C70 directly, never through it. */
typedef struct {        /* the ramp object at sub +0x40 (vtable 0x49FC0) */
    float    v;         /* +0x00 */
    uint16_t ts;        /* +0x04 0x89CF8 at the last step */
    uint16_t r6;        /* +0x06 */
    float    lo;        /* +0x08 */
    float    hi;        /* +0x0C */
    uint32_t vptr;      /* +0x10 (sub +0x50) 0x00049FC0 */
} gs_ramp_t;
_Static_assert(offsetof(gs_ramp_t, ts) == 0x04u && offsetof(gs_ramp_t, lo) == 0x08u &&
               offsetof(gs_ramp_t, hi) == 0x0Cu && offsetof(gs_ramp_t, vptr) == 0x10u &&
               sizeof(gs_ramp_t) == 0x14u, "the ramp object at its stock offsets (0x14 bytes)");
#define GSE_RAMP_VTBL   0x00049FC0u

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
    int16_t  f3a;       /* +0x3A (u16: word accesses only; r04s: the write-only
                         * f3b at +0x3C is gone — 0x10D130's +0x3A/+0x3B byte
                         * stores are this word's two halves) */
    uint8_t  t3c;       /* +0x3C */
    uint8_t  f3d;       /* +0x3D */
    int16_t  f3e;       /* +0x3E */
    gs_ramp_t ramp;     /* +0x40 .. +0x53 (vtable word at +0x50) */
    int16_t  tgt54;     /* +0x54 */
    uint8_t  tgt56;     /* +0x56 */
    uint8_t  r57;       /* +0x57 */
    uint32_t p58;       /* +0x58 pointer to its {idx, blk} cell 0xFFF8CF94 + 8·(class − 2) */
    uint32_t p5c;       /* +0x5C                                0xFFF8CF0C + 8·(class − 2) */
    uint32_t p60;       /* +0x60                                0xFFF8CE7C + 8·(class − 2) */
    uint32_t p64;       /* +0x64 **(0x5DE08 + 4e) */
    uint32_t r68;       /* +0x68 kinds 1/2: 0x125B6C(..) — 0 here */
} gs_sub_t;
_Static_assert(offsetof(gs_sub_t, out_p) == 0x0Au && offsetof(gs_sub_t, out_mode) == 0x10u &&
               offsetof(gs_sub_t, phase) == 0x11u && offsetof(gs_sub_t, lim12) == 0x12u &&
               offsetof(gs_sub_t, tmo) == 0x14u && offsetof(gs_sub_t, type) == 0x15u &&
               offsetof(gs_sub_t, f18) == 0x18u && offsetof(gs_sub_t, to1a) == 0x1Au &&
               offsetof(gs_sub_t, cls) == 0x1Cu && offsetof(gs_sub_t, f1e) == 0x1Eu &&
               offsetof(gs_sub_t, eng) == 0x20u && offsetof(gs_sub_t, rel) == 0x21u &&
               offsetof(gs_sub_t, step) == 0x22u && offsetof(gs_sub_t, base24) == 0x24u &&
               offsetof(gs_sub_t, p26) == 0x26u && offsetof(gs_sub_t, p2e) == 0x2Eu &&
               offsetof(gs_sub_t, dur30) == 0x30u && offsetof(gs_sub_t, t32) == 0x32u &&
               offsetof(gs_sub_t, t34) == 0x34u && offsetof(gs_sub_t, d36) == 0x36u &&
               offsetof(gs_sub_t, f38) == 0x38u && offsetof(gs_sub_t, f3a) == 0x3Au &&
               offsetof(gs_sub_t, t3c) == 0x3Cu && offsetof(gs_sub_t, f3d) == 0x3Du &&
               offsetof(gs_sub_t, f3e) == 0x3Eu && offsetof(gs_sub_t, ramp) == 0x40u &&
               offsetof(gs_sub_t, ramp.vptr) == 0x50u && offsetof(gs_sub_t, tgt54) == 0x54u &&
               offsetof(gs_sub_t, tgt56) == 0x56u && offsetof(gs_sub_t, p58) == 0x58u &&
               offsetof(gs_sub_t, p5c) == 0x5Cu && offsetof(gs_sub_t, p60) == 0x60u &&
               offsetof(gs_sub_t, p64) == 0x64u && offsetof(gs_sub_t, r68) == 0x68u &&
               sizeof(gs_sub_t) == 0x6Cu, "the element sub-object at its stock offsets (0x6C, 0x10D010)");
/* r04s: one {idx, blk} cell behind a sub-object's +0x58 / +0x5C / +0x60
 * pointer (0x10D130 writes `*p = idx; p[1] = blk`) */
typedef struct {
    uint32_t idx;
    uint32_t blk;
} gse_pair_t;
_Static_assert(sizeof(gse_pair_t) == 8u, "the {idx, blk} cell");
#define GSE_PR58_STOCK  0xFFF8CF94u     /* table 0x5AE28[class − 2] */
#define GSE_PR5C_STOCK  0xFFF8CF0Cu     /* table 0x59134[class − 2] */
#define GSE_PR60_STOCK  0xFFF8CE7Cu     /* table 0x5739C[class − 2] */

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
    /* G6c: the engine-state object E6A8, the power-flow cells E69B / E69F /
     * E6A1 / E4A7 / E6C5 / E6EE / E6EF, E28E, the shift object BD14 (+ its
     * context block A), E33C and E4AC live in their own objects at the
     * stock addresses (below, D-G6-9) */
    uint8_t  ae28;          /* 0xFFF8AE28 (the live cell is park_law's: not pinned) */
    uint8_t  e69c0;         /* the 0x103CEC strobe flag (its stock cell not decoded: not pinned) */
    /* r04s: the three element sub-objects left gs_t — they are pinned at
     * 0xFFF8D0DC / D148 / D1B4 (g_sub below, SUB[k]) */
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
    /* G6c: block A's cells and BD2E moved into the pinned shift object
     * gse_bd14_t (below); what stays here is ours */
    uint16_t cb8, cba;      /* the shadow's 0x89CB8 / 0x89CBA (live, by value) */
    uint8_t  sync_pass_ok;  /* ours: a sync seen since the last commit */
    uint8_t  r37;           /* the reaction-37 input (stand-in) */
    uint32_t sync_pass;     /* ours: shadow pass of the last 0 → 1 sync */
    uint32_t hold_n;        /* ours: passes with the hold set */
} gs_prog_t;

/* ── G6c (2026-11-19, ~/tcu_sweep/g6c_port.md, D-G6-9): THE ENGINE'S STOCK
 * OBJECTS AT THE STOCK ADDRESSES. Every field below sits at the offset its
 * stock cell has (the port named each field by its stock address; checked
 * against the driving dump stockram_driving_20261104 in the report); the
 * bytes between them are padding of ours (0) — the stock's other cells of
 * these objects are not ported. On SH the binding's GSE_PIN puts each object
 * in its own NOLOAD section at its stock address (linker.ld .gse_*, ASSERTed
 * against every reservation); on the host they are plain statics. NOT
 * pinned (stated in the report): the element sub-objects D0DC / D148 / D1B4
 * (gs_sub_t is 0x74 bytes with a float model of the +0x40 ramp object and
 * index + block pairs for the +0x58.. pointer cells — not the stock's
 * 0x6C-byte layout), the request objects (0xFFF8B7C8.. are the live
 * holder's, kept as the E28E fence source), the 0x89C3C block copies
 * (0x89C13 / 0x89C30 / 0x89CD2 / 0x89CDC / 0x89D10 are live cells of ours).
 */
typedef struct {            /* 0xFFF8E28E, the exception machine (0xD093A / 0xD0BAA) */
    uint8_t  st;            /* +0 the state */
    uint8_t  r1[3];
    uint8_t  f4;            /* +4 the flags */
    uint8_t  r5;
    uint16_t t6;            /* +6 timer (0xD0C66) */
    uint16_t t8;            /* +8 timer (0xD0C66; cal 0x1FE766) */
} gse_e28e_t;
_Static_assert(offsetof(gse_e28e_t, f4) == 4u && offsetof(gse_e28e_t, t6) == 6u &&
               offsetof(gse_e28e_t, t8) == 8u && sizeof(gse_e28e_t) == 0x0Au, "E28E at its stock offsets");
typedef struct {            /* 0xFFF8E33C, the engagement object (0xDDCA2) */
    uint8_t  r0[2];
    uint8_t  b[3];          /* +2 +3 +4 the first / second / third element */
    uint8_t  r5[5];
    int16_t  ta;            /* +0xA snapshot of 0x89CBA */
    uint8_t  rc[4];
    int16_t  n10;           /* +0x10 snapshot of 0x89CD0 */
    uint16_t t12;           /* +0x12 (0xFFF8E34E, r04s) passes since 0x89C2F last read 0:
                             * +1 sat. in slot 28 (0xDE0FE → 0xBAE5E(E21E, +0x12), step
                             * *0xFFF8E220 = 1), 0 while 0x89C2F != 0 (0xDE2FA: slot 34
                             * 0xDDFD8 and the start 0xDDCA2); read by 0x10D130 (+0x1E) */
} gse_e33c_t;
_Static_assert(offsetof(gse_e33c_t, b) == 2u && offsetof(gse_e33c_t, ta) == 0x0Au &&
               offsetof(gse_e33c_t, n10) == 0x10u && offsetof(gse_e33c_t, t12) == 0x12u &&
               sizeof(gse_e33c_t) == 0x14u, "E33C at its stock offsets");
/* r04s: 0xFFF8E6C1 / E6C2 = E6BF +2 / +3 (0xDC0F8, slot 42): the last power
 * flow whose gear's first element (role table 0x6312F) reported applied
 * (0xE55DC(e) → vtable +0x38 = 0xE4DBA, +0x46 == 10000), and the gear code
 * while 0x89C30 == E6C1 */
typedef struct {
    uint8_t  c1;            /* 0xFFF8E6C1 */
    int8_t   c2;            /* 0xFFF8E6C2 */
} gse_e6c1_t;
_Static_assert(sizeof(gse_e6c1_t) == 2u, "E6C1 / E6C2");
typedef struct {            /* 0xFFF8E4AC, the three-element engine (0x1066D6 ..) */
    uint8_t  e4ac0;         /* +0 (r04s) 0x106AC4: D0DC's phase in {0x0B, 0x0C, 0x0E} */
    uint8_t  r1;
    uint16_t e4ac2;         /* +2 */
    uint8_t  e4ac4, e4ac5, e4ac6, e4ac7;    /* +4 .. +7 */
    uint8_t  kind;          /* +8 the shift kind (0x106AE6) */
    uint8_t  e4ac9;         /* +9 */
} gse_e4ac_t;
_Static_assert(offsetof(gse_e4ac_t, e4ac0) == 0u && offsetof(gse_e4ac_t, e4ac2) == 2u &&
               offsetof(gse_e4ac_t, e4ac7) == 7u && offsetof(gse_e4ac_t, e4ac4) == 4u &&
               offsetof(gse_e4ac_t, e4ac5) == 5u && offsetof(gse_e4ac_t, kind) == 8u &&
               offsetof(gse_e4ac_t, e4ac9) == 9u && sizeof(gse_e4ac_t) == 0x0Au, "E4AC at its stock offsets");
typedef struct {            /* 0xFFF8BD14, the shift object, with its context block A
                             * (+0x58 = 0xFFF8BD6C, the synchronisation progress) */
    uint8_t  r0;
    uint8_t  bd15;          /* +0x01 */
    uint8_t  bd16;          /* +0x02 the shift kind (3 garage, 4 release) */
    uint8_t  bd17;          /* +0x03 */
    uint8_t  r4[4];
    uint8_t  bd1c;          /* +0x08 (r04s) 0xEC6FE: a forward reversal's flag */
    uint8_t  bd1d;          /* +0x09 (r04s) 0xEC6FE: a reverse reversal's flag */
    uint8_t  ra[4];
    uint8_t  bd22;          /* +0x0E 0x89CD2 at the start (0xED9B2) */
    uint8_t  rf[0x0Bu];
    uint16_t bd2e;          /* +0x1A BD2E passes since sync */
    uint8_t  bd30;          /* +0x1C BD30 locked passes */
    uint8_t  r1d[0x13u];
    uint8_t  bd44;          /* +0x30 (r04s) 0xEB900: the third element of the last release */
    uint8_t  r31[0x27u];
    uint8_t  sync;          /* +0x58 BD6C (A +0x00) the sync flag */
    int8_t   tgear;         /* +0x59 BD6D (A +0x01) target gear (0x1176C4) */
    uint8_t  r5a;
    uint8_t  act3;          /* +0x5B BD6F (A +0x03) active context (1) */
    uint8_t  r5c;
    uint8_t  p5;            /* +0x5D BD71 (A +0x05) f(gears) (0x11779C: not ported, 0 — read
                             * only by a BD16 != 3 context, the release) */
    uint8_t  r5e[4];
    int16_t  bd76;          /* +0x62 BD76 (A +0x0A) progress */
    uint8_t  r64[4];
    int16_t  slip;          /* +0x68 BD7C (A +0x10) */
    int16_t  rate;          /* +0x6A BD7E (A +0x12) DT1 of −slip */
    uint8_t  r6c[0x0Eu];
    uint16_t bd8e;          /* +0x7A BD8E (A +0x22) release passes since the start */
    uint8_t  r7c[3];
    uint8_t  p27;           /* +0x7F BD93 (A +0x27) force sync */
    uint8_t  hold28;        /* +0x80 BD94 (A +0x28) the hold of this pass */
    uint8_t  r81[3];
    uint8_t  dir;           /* +0x84 BD98 (A +0x2C) sync direction (0xFF = −1) */
    uint8_t  r85[4];
    uint8_t  p31;           /* +0x89 BD9D (A +0x31) the hold outside a garage */
    uint8_t  r8a[0x10Eu];
    int32_t  acc;           /* +0x198 BEAC (A +0x140) the DT1 state ×1000 */
    uint16_t ts;            /* +0x19C BEB0 (A +0x144) the DT1 time stamp (0x89CF8) */
} gse_bd14_t;
_Static_assert(offsetof(gse_bd14_t, bd15) == 0x01u && offsetof(gse_bd14_t, bd16) == 0x02u &&
               offsetof(gse_bd14_t, bd17) == 0x03u && offsetof(gse_bd14_t, bd22) == 0x0Eu &&
               offsetof(gse_bd14_t, bd1c) == 0x08u && offsetof(gse_bd14_t, bd1d) == 0x09u &&
               offsetof(gse_bd14_t, bd44) == 0x30u &&
               offsetof(gse_bd14_t, bd2e) == 0x1Au && offsetof(gse_bd14_t, bd30) == 0x1Cu &&
               offsetof(gse_bd14_t, sync) == 0x58u && offsetof(gse_bd14_t, tgear) == 0x59u &&
               offsetof(gse_bd14_t, act3) == 0x5Bu && offsetof(gse_bd14_t, p5) == 0x5Du &&
               offsetof(gse_bd14_t, bd76) == 0x62u && offsetof(gse_bd14_t, slip) == 0x68u &&
               offsetof(gse_bd14_t, rate) == 0x6Au && offsetof(gse_bd14_t, bd8e) == 0x7Au &&
               offsetof(gse_bd14_t, p27) == 0x7Fu && offsetof(gse_bd14_t, hold28) == 0x80u &&
               offsetof(gse_bd14_t, dir) == 0x84u && offsetof(gse_bd14_t, p31) == 0x89u &&
               offsetof(gse_bd14_t, acc) == 0x198u && offsetof(gse_bd14_t, ts) == 0x19Cu &&
               sizeof(gse_bd14_t) == 0x1A0u, "BD14 + block A at their stock offsets (ends at block B, BEB4)");
#if defined(__sh__)
#  define GSE_PIN(sec) __attribute__((section(sec)))
#else
#  define GSE_PIN(sec)
#endif
static volatile gse_e28e_t g_e28e GSE_PIN(".gse_e28e");     /* 0xFFF8E28E */
static volatile gse_e33c_t g_e33c GSE_PIN(".gse_e33c");     /* 0xFFF8E33C */
static volatile gse_e4ac_t g_e4ac GSE_PIN(".gse_e4ac");     /* 0xFFF8E4AC */
static volatile gse_bd14_t g_bd14 GSE_PIN(".gse_bd14");     /* 0xFFF8BD14 */
static volatile int8_t  g_e4a7 GSE_PIN(".gse_e4a7");        /* 0xFFF8E4A7 (E4A6[1]) the gear target, committed */
static volatile uint8_t g_e69b GSE_PIN(".gse_e69b");        /* 0xFFF8E69B committed power flow */
static volatile int8_t  g_e69f GSE_PIN(".gse_e69f");        /* 0xFFF8E69F the current gear code */
static volatile int8_t  g_e6a1 GSE_PIN(".gse_e6a1");        /* 0xFFF8E6A1 the gear target */
static volatile uint8_t g_e6a8[5] GSE_PIN(".gse_e6a8");     /* 0xFFF8E6A8 the engine-state object (0xD4DC2) */
static volatile gse_e6c1_t g_e6c1 GSE_PIN(".gse_e6c1");    /* 0xFFF8E6C1 / C2 (r04s, 0xDC0F8) */
static volatile uint8_t g_e6c5 GSE_PIN(".gse_e6c5");        /* 0xFFF8E6C5 the settle counter */
static volatile uint8_t g_e6ee GSE_PIN(".gse_e6ee");        /* 0xFFF8E6EE */
static volatile uint8_t g_e6ef GSE_PIN(".gse_e6ef");        /* 0xFFF8E6EF the power-flow request */
/* r04s (~/tcu_sweep/r04s_port.md, B2 §B): the three element sub-objects
 * 0xFFF8D0DC / D148 / D1B4 (classes 2 / 3 / 4, 0x6C each, contiguous) and
 * the nine {idx, blk} cells their +0x58 / +0x5C / +0x60 point to, at the
 * stock addresses (constructor 0x10D010 from 0x15CEC0; tables 0x5AE28 /
 * 0x59134 / 0x5739C [O bytes]) */
static volatile gs_sub_t   g_sub[3]  GSE_PIN(".gse_d0dc");  /* 0xFFF8D0DC, D148, D1B4 */
static volatile gse_pair_t g_pr58[3] GSE_PIN(".gse_pr58");  /* 0xFFF8CF94 / 9C / A4 */
static volatile gse_pair_t g_pr5c[3] GSE_PIN(".gse_pr5c");  /* 0xFFF8CF0C / 14 / 1C */
static volatile gse_pair_t g_pr60[3] GSE_PIN(".gse_pr60");  /* 0xFFF8CE7C / 84 / 8C */
#define E28  (*(gse_e28e_t *)&g_e28e)
#define E33  (*(gse_e33c_t *)&g_e33c)
#define EAC  (*(gse_e4ac_t *)&g_e4ac)
#define KB   (*(gse_bd14_t *)&g_bd14)
#define V_E4A7 (*(int8_t *)&g_e4a7)
#define V_E69B (*(uint8_t *)&g_e69b)
#define V_E69F (*(int8_t *)&g_e69f)
#define V_E6A1 (*(int8_t *)&g_e6a1)
#define V_E6A8 (*(uint8_t (*)[5])&g_e6a8)
#define V_E6C5 (*(uint8_t *)&g_e6c5)
#define E6C   (*(gse_e6c1_t *)&g_e6c1)
#define V_E6EE (*(uint8_t *)&g_e6ee)
#define V_E6EF (*(uint8_t *)&g_e6ef)
#define SUB    (*(gs_sub_t (*)[3])&g_sub)
/* the {idx, blk} cell a sub-object pointer names. On SH the pointer IS the
 * cell's address (the cells are pinned at the stock addresses the
 * constructor stores — linker.ld ASSERTs ADDR == the stock value); on the
 * host the pointer holds the same stock value and is mapped onto this
 * instance's cells (any other value: a zero cell — only the host-only
 * gs_shadow_host_ceca probe, whose local sub-object has no pointers) */
static gse_pair_t *pr(uint32_t a)
{
#if defined(__sh__)
    return (gse_pair_t *)(uintptr_t)a;
#else
    static gse_pair_t z;
    if (a - GSE_PR58_STOCK < 24u) { return (gse_pair_t *)&g_pr58[(a - GSE_PR58_STOCK) / 8u]; }
    if (a - GSE_PR5C_STOCK < 24u) { return (gse_pair_t *)&g_pr5c[(a - GSE_PR5C_STOCK) / 8u]; }
    if (a - GSE_PR60_STOCK < 24u) { return (gse_pair_t *)&g_pr60[(a - GSE_PR60_STOCK) / 8u]; }
    z.idx = 0u;
    z.blk = 0u;
    return &z;
#endif
}
/* the pointer value the constructor stores for cell c of a group */
static uint32_t pr_addr(volatile gse_pair_t *grp, uint32_t stock, uint8_t c)
{
#if defined(__sh__)
    (void)stock;
    return (uint32_t)(uintptr_t)&grp[c];
#else
    (void)grp;
    return stock + 8u * (uint32_t)c;
#endif
}
static void gse_zero(volatile void *p, uint32_t n)
{
    volatile uint8_t *b = (volatile uint8_t *)p;
    for (uint32_t i = 0u; i < n; i++) { b[i] = 0u; }
}

static volatile gs_t g_s GS_OBJ;
static volatile gs_wit_t g_w GS_OBJ;
static volatile gs_prog_t g_p GS_PRG;
#if GSE_REC
/* the recorder: slot 0 fills .gs_rec, slot 1 the tail of .gs_obj (each
 * region alone is too small for 2 × 112 × 28 bytes) */
static volatile gs_rec_t g_rec0[GS_REC_N] GS_REC;
static volatile gs_rec_t g_rec1[GS_REC_N] GS_RC1;
/* G4a: the release's state, witnesses and recorder — its own object (no
 * zero-initialised static may be added to this TU: its .bss lies in
 * .gs_obj BEFORE .gs_prog / .gs_rec1, which must not move) */
static volatile gs_rel_t g_rel GS_REL;
#define GSE_REL_SIZE ((uint32_t)sizeof g_rel)
#define G   (*(gs_t *)&g_s)
#define R   (*(gs_rel_t *)&g_rel)
#else
/* GSE_REC 0 (G6a, the candidate's RAM budget): no recorders; the release
 * object is its header only — gse_relh_t = gs_rel_t up to .rec (BD8E, the
 * counters, the two slot headers the completion writes), asserted field for
 * field; .rec is never touched. */
typedef struct {                /* gs_rel_t without .rec — field for field */
    uint16_t bd8e;
    uint8_t  on;
    uint8_t  last;
    uint32_t rel_passes;
    uint32_t starts;
    uint32_t unp_n;
    uint32_t c3_n;
    uint32_t e896_n;
    uint32_t idle_n;
    gs_rslot_t slot[2];
} gse_relh_t;
_Static_assert(sizeof(gse_relh_t) == offsetof(gs_rel_t, rec) &&
               offsetof(gse_relh_t, on) == offsetof(gs_rel_t, on) &&
               offsetof(gse_relh_t, last) == offsetof(gs_rel_t, last) &&
               offsetof(gse_relh_t, rel_passes) == offsetof(gs_rel_t, rel_passes) &&
               offsetof(gse_relh_t, starts) == offsetof(gs_rel_t, starts) &&
               offsetof(gse_relh_t, unp_n) == offsetof(gs_rel_t, unp_n) &&
               offsetof(gse_relh_t, c3_n) == offsetof(gs_rel_t, c3_n) &&
               offsetof(gse_relh_t, e896_n) == offsetof(gs_rel_t, e896_n) &&
               offsetof(gse_relh_t, idle_n) == offsetof(gs_rel_t, idle_n) &&
               offsetof(gse_relh_t, slot) == offsetof(gs_rel_t, slot),
               "gs engine: gse_relh_t is gs_rel_t up to .rec");
static volatile gse_relh_t g_rel GS_REL;
#define GSE_REL_SIZE ((uint32_t)sizeof g_rel)
#define G   (*(gs_t *)&g_s)
#define R   (*(gse_relh_t *)&g_rel)
#endif
#define W   (*(gs_wit_t *)&g_w)
#define P   (*(gs_prog_t *)&g_p)
#if GSE_REC
static volatile gs_rec_t *rec_at(uint8_t slot, uint8_t i)
{
    return slot == 0u ? &g_rec0[i] : &g_rec1[i];
}
#endif

#if !defined(__sh__)
static uint8_t s_host_r04s[8][12];  /* r04s: the last 8 starts' reversal inputs */
static uint8_t s_host_r04s_n;
static uint8_t s_host_mask;         /* gs_shadow_host_standins */
static uint8_t s_host_ceca_cls;     /* gs_shadow_host_ceca: the old class test */
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
    static const uint32_t T[6] = { 0u, GSE_OBJ_A, GSE_OBJ_B, GSE_OBJ_C, GSE_OBJ_D, GSE_OBJ_E };
    if (e == 0u || e > 5u) {
        W.unp_n++;                  /* the stock returns element 2 after a report */
        return GSE_OBJ_B;
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
static uint32_t b58(const gs_sub_t *s, uint32_t off) { return gsf_fl32(pr(s->p58)->blk + off); }
static uint32_t b60(const gs_sub_t *s, uint32_t off) { return gsf_fl32(pr(s->p60)->blk + off); }
static uint32_t b5c(const gs_sub_t *s, uint32_t off) { return gsf_fl32(pr(s->p5c)->blk + off); }

/* ── THE RELEASE helpers (G4a, 2026-11-19; ~/tcu_sweep/g4a_release_port.md,
 * from the bytes) ──────────────────────────────────────────────────────── */
/* 0xB82E2(E5C4, e) = 0xE513C(e) → 0xE4C00: b7888((u16)+2 − (u16)+0x42) —
 * the element's result minus its kiss pressure [O 0xE4C00..0xE4C16] */
static int16_t b82e2(uint8_t e)
{
    const uint32_t o = req_obj(e);
    return b7888((int32_t)(uint32_t)RU16(o, RO_RES) - (int32_t)(uint32_t)RU16(o, 0x42u));
}
/* 0xB82F6(E5C4, e): the element's +0x42 (kiss) */
static uint16_t b82f6(uint8_t e) { return RU16(req_obj(e), 0x42u); }
/* 0x10F00C: map2(blk5C+0x14; x = +0x38 (s16), y = b7768(+0x32)) */
static int16_t f00c(const gs_sub_t *s)
{
    return (int16_t)gsf_b7164(b5c(s, 0x14u), (int16_t)s->f38, b7768(s->t32));
}
/* 0x10F030: map2(blk5C+0x1C+4·0x89C06; x = BD22 (u8), y = b7768(+0x32)) */
static int16_t f030(const gs_sub_t *s)
{
    return (int16_t)gsf_b7164(b5c(s, 0x1Cu + 4u * GS_C06), (int16_t)KB.bd22, b7768(s->t32));
}
/* 0x10F26C(*0xFFF84D44, 0x89C2F): the release speed exit, a word through
 * *(set + (0x89C2F == 1 ? 0x3C : 0x44) + 4·0x89C06) (cal 0x1FF1D2 / 0x1FF1D6
 * = 2000 rpm) */
static int16_t f26c(void)
{
    return (int16_t)gsf_fl16(gsf_fl32(GS_P84D44 + (G.c2f == 1u ? 0x3Cu : 0x44u) + 4u * GS_C06));
}
/* 0x10E850: (+0x21 == 2) ? b7768(element +0x42) : 0 [O 0x10E850..0x10E870] */
static int16_t e850(const gs_sub_t *s)
{
    return (s->rel == 2u) ? b7768(b82f6(s->rel)) : (int16_t)0;
}
/* 0xB77A0: |v| saturated (−32768 → 32767) */
static int16_t b77a0(int16_t v)
{
    return (v == (int16_t)-0x8000) ? (int16_t)0x7FFF : (int16_t)(v < 0 ? -v : v);
}

/* every helper below is the kind-0 path (E4AC+8 = 0); the other kinds are
 * not ported (unreachable with the stand-in flags, counted if reached) */
static void kind_check(void) { if (EAC.kind != 0u) { W.unp_n++; } }

static int16_t e994(const gs_sub_t *s) { kind_check(); return (int16_t)gsf_b6e10(b58(s, 0x78u), (int16_t)KB.bd22); }
static int16_t ea72(const gs_sub_t *s) { return (int16_t)gsf_b7164(b58(s, 0x14u + 4u * GS_C06), KB.slip, (int16_t)GS_C1A); }
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
static int16_t ea1c(const gs_sub_t *s) { return (int16_t)gsf_b6e10(b58(s, 0x80u), (int16_t)KB.bd22); }
static int16_t e9e0(const gs_sub_t *s)
{
    int16_t v = (int16_t)gsf_b6e10(b58(s, 0x7Cu), (int16_t)KB.bd22);
    if (EAC.e4ac5 == 1u) {
        v = (int16_t)gsf_b6e10(b58(s, 0xACu), (int16_t)G.c13);
    }
    return v;
}
static int16_t ea4c(const gs_sub_t *s, uint8_t f5)          /* 0x10EA4C */
{
    if (f5 != 0u && KB.bd16 == 3u) {
        return (int16_t)gsf_b6e10(gsf_fl32(s->p64 + 0x20u), (int16_t)KB.bd22);
    }
    return 0;
}
static uint16_t ed1a(const gs_sub_t *s) { return gsf_b6eae(b58(s, 0x50u), (uint16_t)GS_C08); }
static uint8_t ed4a(const gs_sub_t *s)                       /* 0x10ED4A: D148 +0x16 / +0x20 */
{
    if (SUB[1].f16 == 0u && SUB[1].eng != 0u) {
        return gsf_fl8(b58(s, 0x58u));
    }
    return 0u;
}
static int16_t e52c(const gs_sub_t *s)                       /* 0x10E52C */
{
    int16_t v = 0;
    if (KB.bd16 == 3u) {
        if (s->cls == 4u) {
            v = (int16_t)ed4a(s);
            if (KB.bd1c != 0u || KB.bd1d != 0u) {           /* r04s: 0x10E54E..0x10E574 */
                const int16_t m = (int16_t)gsf_b6d6c(b58(s, 0x54u), KB.bd22);
                v = (v > m) ? v : m;                         /* 0x104D4C: the larger */
            }
        } else {
            v = (int16_t)ed1a(s);
            kind_check();
        }
    }
    return v;
}
static uint8_t ec40(const gs_sub_t *s) { kind_check(); return gsf_b6d6c(b58(s, 0x64u + 4u * GS_C06), KB.bd22); }
static uint16_t ec78(const gs_sub_t *s)
{
    uint16_t v = gsf_b6eae(b58(s, 0x6Cu), (uint16_t)GS_C08);
    if (EAC.e4ac5 == 1u) {
        v = gsf_b6eae(b58(s, 0xB4u), (uint16_t)GS_C08);
    }
    if (s->f18 != 0u) {
        W.unp_n++;                                           /* 0x10F1A2 */
    }
    return v;
}
static uint8_t ecc6(const gs_sub_t *s) { kind_check(); return b6fd6(b58(s, 0x70u), KB.bd22, (uint8_t)GS_C1D); }
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
        /* r04s (0x10E62C..0x10E64E): a fast reversal's base offset —
         * *(*(*0xFFF84D44) + 20) = cal 0x1FF1BE = −100 mbar if the gear code
         * reads −1 (R), else +24 → cal 0x1FF1C0 = 0 */
        v += (int16_t)gsf_fl16(gsf_fl32(GS_P84D44 + (V_E69F == -1 ? 20u : 24u)));
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
    return c <= (int32_t)KB.bd76 && s->f4 == 0u;
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
    v += (int32_t)gsf_b6e10(b58(s, 0x84u), (int16_t)KB.bd22);
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
    if (KB.bd16 == 4u) {
        if (EAC.kind == 2u) {
            /* 0x10E794: map(blk60 +0x0C, 0x89C13) — kind 2, unreachable */
            R.unp_n++;
            W.unp_n++;
            s->lim12 = gsf_b6eae(b60(s, 0x0Cu), G.c13);
        } else {
            s->lim12 = gsf_b6eae(b60(s, 0x08u), KB.bd22);     /* 0x10F06A */
        }
#if !defined(__sh__)
        if ((s_host_mask & GS_HOST_LIM12_0) != 0u) {
            s->lim12 = 0u;                               /* host negative */
        }
#endif
    } else {
        const uint16_t u = gsf_b6eae(b60(s, 0x00u), KB.bd22);
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
            /* r04s (0x10D706..0x10D728): cal *(*0xFFF84D44 + 44) (gear code
             * −1) / + 48 — both 0 in this calibration (0x1FF1CA / 0x1FF1CC) */
            base = (int16_t)gsf_fl16(gsf_fl32(GS_P84D44 + (V_E69F == -1 ? 44u : 48u)));
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
    case 0x12:                              /* 0x10D824 (G4a) */
        if (!b822c(s->rel)) {
            s->p26 = 0;                     /* 0x10D8A8: the element is not applied */
        } else if (s->cls == 3u && s->rel == 3u) {
            /* the class-3 / element-C case: +0x26 = *(*(*0xFFF84D44 + (E4B4
             * == 2 ? 0x38 : 0x34))) − b7768(element +0x42) [O 0x10D836..
             * 0x10D876]: C as the second element (a 6th-gear release, not a
             * garage) */
            const int16_t w = (int16_t)gsf_fl16(gsf_fl32(GS_P84D44 + (EAC.kind == 2u ? 0x38u : 0x34u)));
            R.c3_n++;
            s->p26 = (int16_t)(w - b7768(b82f6(s->rel)));
        } else {
            s->p26 = b82e2(s->rel);         /* 0x10D896 */
        }
        break;
    case 0x15:                              /* 0x10D884: as 0x12 without the C case (kind 2) */
        R.unp_n++;
        s->p26 = b822c(s->rel) ? b82e2(s->rel) : (int16_t)0;
        break;
    default:                                /* 0x16 / 0x17 (kind 2 only): not ported */
        R.unp_n++;
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
            if (cf80(&SUB[2], s->dur30)) {
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
        if (SUB[2].past != 0u) {          /* D1B5 */
            return true;
        }
        return SUB[2].eng == 0u;          /* D1D4 */
    case 0x04:
        if (s->tgt56 == 0u) {
            return false;
        }
        /* 0xE9832(BC74, e): BC74 never holds an element here */
        if (s->cls == 2u && e872(s)) {
            return true;
        }
        if (s->cls == 2u && EAC.kind == 1u) {
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
        return gsf_fl8(b58(s, 0x74u)) <= KB.bd30;
    case 0x0C: case 0x0E: case 0x10: case 0x14:
        return false;
    case 0x0F: {
        const bool wait = SUB[2].f16 == 0u && SUB[2].eng != 0u;   /* D1CA / D1D4 */
        if (!wait) {
            return true;
        }
        if (s->cls != 2u) {
            return false;
        }
        return b822c(s->eng);
    }
    case 0x11:
        /* r04s (0x10DA40..0x10DAA4): BD1C / BD1D (0xEC6FE, a reversal) */
        if (KB.bd1c == 0u && KB.bd1d == 0u) {
            return cf80(&SUB[1], s->dur30);              /* the second sub-object's fill condition */
        }
        if ((int32_t)P.cba >= (int32_t)(int16_t)gsf_b6e10(b58(s, 0x5Cu), (int16_t)GS_C1A)) {
            return true;
        }
        if ((int32_t)P.cba >= (int32_t)(uint16_t)gsf_b6eae(b58(s, 0x60u), (uint16_t)GS_C1D)) {
            return true;
        }
        return (int32_t)s->t32 >= (int32_t)s->dur30;   /* 0x10DE34: the timer */
    case 0x12:                              /* 0x10DC4A (G4a) */
        if (KB.bd16 != 4u) {
            break;                          /* 0x10DE34: the timer */
        }
        if (s->cls == 2u || s->cls == 4u) {
            return true;
        }
        /* class 3 (GBR 0xFFF8D0E6): D0E3 (D0DC +7), D0E6 (+0xA) == 0x10E850,
         * D1BB (D1B4 +7), D1BE (+0xA) == 0x10E850 — 0x10E850 of THIS sub */
        if (SUB[0].f7 == 0u || SUB[0].out_p != e850(s)) {
            return false;
        }
        return SUB[2].f7 != 0u && SUB[2].out_p == e850(s);
    case 0x13: {                            /* 0x10DCE8 (G4a) */
        uint8_t part;
        if (s->cls == 3u && GS_C25 == 6) {
            return true;
        }
        if (b822c(s->rel)) {
            s->f1f = (uint8_t)((b82e2(s->rel) > 0) ? 0u : 1u);   /* cmp/pl; movrt */
        }
        if (s->cls == 4u) {
            part = (s->f1f != 0u && SUB[0].f1f != 0u) ? 1u : 0u;   /* D0FB */
        } else {
            part = (s->f1f != 0u && SUB[2].f1f != 0u) ? 1u : 0u;   /* D1D3 */
        }
        if ((int32_t)b77a0(KB.slip) >= (int32_t)(uint16_t)f26c()) {  /* BD7C */
            return true;
        }
        return part != 0u;
    }
    case 0x15: case 0x16: case 0x17:
        R.unp_n++;                          /* release kind 2: not ported */
        W.unp_n++;
        break;
    default:
        W.unp_n++;
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
                rate = (int16_t)gsf_fl8(EAC.e4ac5 == 1u ? b58(s, 0xB8u) : b58(s, 0x00u));
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
        } else if (s->f18 == 0u && EAC.e4ac5 == 1u) {
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
    case 0x12:                               /* 0x10E3E0 (G4a) */
        p = s->p26;
        break;
    case 0x13:                               /* 0x10E25A (G4a) */
        if (s->cls == 4u) {
            if (EAC.kind == 2u || EAC.kind == 7u) {
                R.unp_n++;                   /* 0x10E27E / 0x10E2BE: not ported */
                W.unp_n++;
                p = 0;
            } else {
                p = (int32_t)(int16_t)gsf_b7164(b5c(s, 0x08u + 4u * GS_C06), (int16_t)KB.bd22, b7768(s->t32))
                  + (int32_t)(int16_t)gsf_b7164(b5c(s, 0x00u), (int16_t)s->f38, b7768(s->t32));
            }
        } else if (s->cls == 2u) {
            const uint8_t fac = gsf_b6d6c(b5c(s, 0x28u), G.c13);       /* 0x89C13 */
            p = ((int32_t)fac * (int32_t)f00c(s)) / 100 + (int32_t)f030(s)
              + (int32_t)(int16_t)gsf_b6e10(b5c(s, 0x30u), (int16_t)GS_C10);
        } else {
            p = 0;                           /* class 3: 0x10E46C */
            break;
        }
        if ((int32_t)s->p26 < p) {           /* never above +0x26 */
            p = s->p26;
        }
        break;
    case 0x15: case 0x16: case 0x17:
        R.unp_n++;                           /* release kind 2: not ported */
        W.unp_n++;
        p = 0;
        break;
    default:
        W.unp_n++;
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

/* r04s: 0x106698's prologue and epilogue (slot 35, 0xEB348 → 0xEB382 →
 * 0xEEB56 → 0x106698(E4AC), every pass whatever BD17 [O bytes]):
 *   0x89C3D != 0 → +5 = 1 (D-G6-2 forces our 0x89C3D to 0: never — +5 keeps
 *   GS_E4B1 from boot until the first stop, as before);
 *   0x106AB6 → 0x106C16 (+7 / +2) and 0x106C4A (+4);
 *   +9 = 0;
 *   0x10D10C(D148) (class 3 with +9 and 0x10CDFA(D148, 1)) → 0x1067D6 (+8 =
 *   0, 0x10D0B4(D148), +5 = 0) — our D148 +9 is never set (the A-only
 *   engine 0x10D3F4 is not ported): unreachable, ported;
 *   [0x10CE88 × 3]; then 0x106AC4: +0 = D0DC phase in {0x0B, 0x0C, 0x0E}. */
static void e6c16(void)                                  /* 0x106C16 */
{
    uint8_t s7 = EAC.e4ac7;
    const int16_t bd7c = KB.slip;
    if (s7 == 0u &&
        (uint32_t)(int32_t)(int8_t)GS_PEDAL >= (uint32_t)(int32_t)(int8_t)gsf_fl8(gsf_fl32(GS_P84D44 + 12u))) {
        s7 = 1u;                                         /* the pedal 0x89CD8 >= cal 0x1FE8D7 (13) */
        EAC.e4ac7 = 1u;
    }
    if (s7 == 0u || bd7c < (int16_t)EAC.e4ac2) {
        EAC.e4ac2 = (uint16_t)bd7c;                      /* BD7C, then its running minimum */
    }
}
static void e6c4a(void)                                  /* 0x106C4A: kind 1 only */
{
    if (EAC.kind == 1u && EAC.e4ac4 == 0u) {
        W.unp_n++;                                       /* 0x89D1D vs cal (kind 1: unreachable) */
    }
}
static bool d10c(void);
static void e67d6(void);
static void e6698_pro(void)
{
    if (G.c3d != 0u) {
        EAC.e4ac5 = 1u;
    }
    e6c16();
    e6c4a();
    EAC.e4ac9 = 0u;
    if (d10c()) {
        e67d6();
    }
}
static void e6698_epi(void)                              /* 0x106AC4 */
{
    const uint8_t ph = SUB[0].phase;
    EAC.e4ac0 = (ph == 0x0Bu || ph == 0x0Cu || ph == 0x0Eu) ? 1u : 0u;
}

/* 0x10CE88: one step */
static void ce88(gs_sub_t *s)
{
    eb76(s);
    if (EAC.kind != 5u) {
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
        if (s->cls == 2u) {                 /* +0x1C == 2 [O 0x10CEEE] */
            /* 0x10E896: + the 0xFFF8E6CC term (0xE9464 / 0xE945E) — NOT
             * DECODED, stand-in 0 → 0xB7888(p); in a release (BD16 = 4)
             * the term is D252 (0xE945E), zeroed at the start, no writer
             * found: stand-in 0, counted apart */
            W.e896_n++;
            if (KB.bd16 == 4u) {
                R.e896_n++;
            }
            v = b7888(v);
        }
        s->out_p = v;
    }
    if (s->eng != 0u) {
        const uint8_t ph = s->phase;
        s->out_mode = (ph == 0x01u || ph == 0x0Fu || ph == 0x11u) ? 1u : 0u;
    }
    if (s->rel != 0u) {
        /* the ELEMENT +0x21 == 2 (B) selects mode 0, not the class [O
         * 0x10CF20..0x10CF2E: movu.b @(33,r14) cmp/eq #2] (G4a) */
#if !defined(__sh__)
        if ((s_host_ceca_cls != 0u || (s_host_mask & GS_HOST_CECA_CLASS) != 0u)
                ? (s->cls == 2u) : (s->rel == 2u)) {     /* host negative: the old test */
#else
        if (s->rel == 2u) {
#endif
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
        EAC.e4ac9 = (EAC.kind == 2u) ? 1u : 0u;
        EAC.kind = 0u;
    }
    if (a == 0u) {
        d0b4(&SUB[0]);
    }
    if (b == 0u) {
        d0b4(&SUB[1]);
    }
    if (c == 0u) {
        d0b4(&SUB[2]);
    }
    EAC.e4ac5 = 0u;
}

/* r04s: 0x10D10C(D148): class 3 with +9 set and 0x10CDFA(D148, 1) */
static bool cdfa(const gs_sub_t *s, uint8_t p2);
static bool d10c(void)
{
    const gs_sub_t *s = &SUB[1];
    return s->cls == 3u && s->f9 != 0u && cdfa(s, 1u);
}
/* r04s: 0x1067D6(E4AC): the A-only engagement's stop */
static void e67d6(void)
{
    EAC.kind = 0u;
    d0b4(&SUB[1]);
    EAC.e4ac5 = 0u;
}

/* 0xD11EC: E298 (neutral idle) "slip release active" */
static uint8_t d11ec(uint8_t st)
{
    return (st == 0u || st == 1u || st == 2u || st == 11u || st == 12u) ? (uint8_t)0u : (uint8_t)1u;
}

/* r04s: 0x10F084 — the third element's offset after a reversal: by E6C2
 * (1 → *(P+0x58), 2 → +0x54, −1 → +0x50; P = *0xFFF84D44 = 0x56F98) a map
 * over the temperature byte 0x89C13 (cal 0x1B5324 = −10 at every
 * temperature for E6C2 = 1; 0x1B5310 / 0x1B52FC = 0), else 0. Read through
 * 0x10CFB6 (the element's fill time, 0xBA7DC in 0xE47CA) */
static int16_t f084(void)
{
    uint32_t off;
    switch (E6C.c2) {
    case 1:  off = 0x58u; break;
    case 2:  off = 0x54u; break;
    case -1: off = 0x50u; break;
    default: return 0;
    }
    return (int16_t)gsf_b6e10(gsf_fl32(GS_P84D44 + off), (int16_t)(uint16_t)G.c13);
}

/* 0x10D130: start one sub-object (pf = the new power flow) */
static void d130(gs_sub_t *s, uint8_t pf, uint8_t elem, int8_t gear, uint8_t flag)
{
    const uint32_t gi = (uint32_t)(uint8_t)(gear + 1) * 4u;
    uint32_t v;

    gse_pair_t *q;

    /* r04s: through the sub-object's pointers (0x10D144 `mov.l @(96,r4)`,
     * 0x10D19C @(88), 0x10D1EA @(92)): `*p = idx` always, `p[1]` = the block
     * (out of range: the old block kept, after a report) */
    v = gsf_fl32(0x0005DDE0u + gi);                      /* +0x60 */
    q = pr(s->p60);
    q->idx = v;
    q->blk = ((int32_t)v < 0 || (int32_t)v > 8) ? (W.unp_n++, q->blk)
           : gsf_fl32(0x00057354u + 4u * 0u /* *0x89C04 = 0 */ + v * 8u);
    v = gsf_fl32(0x0005DD90u + gi);                      /* +0x58 */
    q = pr(s->p58);
    q->idx = v;
    q->blk = ((int32_t)v < 0 || (int32_t)v > 8) ? (W.unp_n++, q->blk)
           : gsf_fl32(0x0005ADE0u + v * 8u);
    v = gsf_fl32(0x0005DDB8u + gi);                      /* +0x5C */
    q = pr(s->p5c);
    q->idx = v;
    q->blk = ((int32_t)v < 0 || (int32_t)v > 8) ? (W.unp_n++, q->blk)
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
            /* r04s (0x10D338..0x10D38E): +0x1E = flag && E33C+0x12 <=
             * *(*(*0xFFF84D44) + 16) (cal 0x1FE8D8 = 10; `cmp/hi` on the
             * sign-extended word); a class-4 sub-object with +0x1E, E6C2 <=
             * 2, the gear <= 2 and a nonzero power flow gets +0x3A from
             * 0x10F084, every other start +0x3A = 0 (the word: +0x3B is
             * its low byte, not a field) */
            s->f1e = (flag != 0u &&
                      (uint32_t)(int32_t)(int16_t)E33.t12 <= (uint32_t)gsf_fl8(gsf_fl32(GS_P84D44 + 16u)))
                     ? 1u : 0u;
            if (s->f1e != 0u && s->cls == 4u && (int32_t)E6C.c2 <= 2 && (int32_t)gear <= 2 && pf != 0u) {
                s->f3a = f084();
            } else {
                s->f3a = 0;
            }
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
            SUB[0].type = 0u;
            SUB[1].type = 1u;
            SUB[2].type = 2u;
            return;
        }
        SUB[0].type = (EAC.kind == 2u) ? 4u : 3u;
        SUB[1].type = 3u;
        SUB[2].type = 3u;
        return;
    }
    SUB[0].type = 6u;
    SUB[1].type = 6u;
    SUB[2].type = 6u;
}

/* 0x106AE6(E4AC, gear): the kind (r04s: the whole decision from the bytes
 * 0x106AE6..0x106C14). +8 = 0; gear > 2 → 0. 0x89C2F == 0: 0xB80D0(E5C2)
 * (B10E) → 6, else 0xB8054(E5C2) (B0FE) → 1, else +9 → 5, else 0;
 * 0x89C2F != 0: 0xB80C6(E5C2) (B10D) → 2, else 0xBA19E(E60F) → 7, else 0.
 * The fill-object flags B10E / B0FE / B10D and E60F are stand-ins 0 (dump
 * 0); +9 is 0 (cleared by 0x106698's prologue every slot-35 pass, and our
 * 0x1067EC writes it only for kind 2): the kind is 0 in every reachable
 * start. Kinds 1 / 2 / 5 / 7 then rewrite the {idx, blk} cell
 * **0x57138 (0xFFF8CE74) from table 0x530FC[gear + 1] / 0x57120 — a cell
 * we do not hold (0x106BB6..0x106C0E): counted, not ported */
static uint8_t e6ae6(int8_t gear)
{
    uint8_t k = 0u;
    if ((int32_t)gear > 2) {
        return 0u;
    }
    if (G.c2f == 0u) {
        /* 0xB80D0 (B10E) / 0xB8054 (B0FE): stand-ins 0 */
        if (EAC.e4ac9 != 0u) {
            k = 5u;
        }
    } else {
        /* 0xB80C6 (B10D) / 0xBA19E (E60F): stand-ins 0 */
    }
    if (k == 1u || k == 2u || k == 5u || k == 7u) {
        W.unp_n++;                                       /* the **0x57138 cell (0x106BB6..) */
    }
    return k;
}

/* 0x1066D6: start the engine */
static void e66d6(uint8_t pf, uint8_t e1, uint8_t e2, uint8_t e3, int8_t gear)
{
    uint8_t flag = 0u;
    if (pf == 0u) {
        EAC.e4ac6 = 0u;
    }
    if (E6C.c1 != pf && E6C.c1 != 0u) {                   /* 0xB83B4(E5C4) = E6C1 */
        flag = 1u;
    }
    EAC.kind = e6ae6(gear);
    d130(&SUB[0], pf, e1, gear, flag);
    d130(&SUB[1], pf, e2, gear, flag);
    d130(&SUB[2], pf, e3, gear, flag);
    EAC.e4ac4 = 0u;
    EAC.e4ac7 = 0u;
    EAC.e4ac2 = (uint16_t)KB.slip;                          /* BD7C */
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
    const int32_t r = (int32_t)d7c8c(KB.tgear);
    const int32_t v = ((int32_t)GS_D5E * (int32_t)P.cb8 * r) / 1000 - (int32_t)GS_D5E * (int32_t)P.cba;
    return b7888(v);
}

static void f48(uint8_t hold, uint8_t p3)                /* 0x117F48 */
{
    const int16_t thr = sync_thr();
    const uint8_t was = KB.sync;
    KB.slip = a74();
    if (hold != 0u) {
        KB.sync = 0u;
        KB.dir = 0xFFu;
    } else if (KB.sync == 0u) {
        uint8_t d = KB.dir;
        if (d == 0xFFu) {
            d = (KB.bd16 == 3u) ? (KB.slip >= 0 ? 3u : 1u) : KB.p5;
            KB.dir = d;
        }
        if (((d == 1u || d == 2u) && (int32_t)KB.slip >= -(int32_t)thr) ||
            ((d == 3u || d == 4u) && (int32_t)KB.slip <= (int32_t)thr) || p3 != 0u) {
            KB.sync = 1u;
        }
    }
    if (KB.act3 == 1u) {
        const int32_t a = (KB.slip == -0x8000) ? 0x7FFF : (KB.slip < 0 ? -(int32_t)KB.slip : KB.slip);
        if (KB.sync == 0u) {
            KB.bd2e = 0u;
        }
        if (KB.sync == 0u || a > (int32_t)thr) {          /* 0xB77A0 */
            KB.bd30 = 0u;
        }
    }
    if (was == 0u && KB.sync != 0u) {
        P.sync_pass = W.passes;
        P.sync_pass_ok = 1u;
    }
}

static void dt1_reset(void)                              /* 0xD2046(+0x140, −slip) */
{
    KB.ts = G.tb;
    KB.acc = (int32_t)(int16_t)(-(int32_t)KB.slip) * 1000;
}

static void dt1(void)                                    /* 0x1179D8(p, 0) → 0xD2058 */
{
    const int16_t x = (int16_t)(-(int32_t)KB.slip);
    uint16_t t = gsf_b6eae(gsf_fl32(0x0005713Cu + 0x28u), (uint16_t)GS_C08);
    uint16_t dt;
    int16_t y;
    if (t == 0u) {
        t = 1u;
    }
    dt = (uint16_t)(G.tb - KB.ts);
    if (dt > t) {
        dt = t;
    }
    y = b7888(((int32_t)x * 1000 - KB.acc) / (int32_t)t);
    KB.acc += (int32_t)y * (int32_t)dt;
    KB.ts = G.tb;
    KB.rate = y;
}

static int16_t aec(void)                                 /* 0x117AEC (p4 = 0) → 0xB7910 */
{
    float r;
    const float fr = (float)KB.rate;
    if (KB.sync != 0u) {
        r = (float)KB.bd2e * 1.0f;
    } else if (fr == 0.0f || (fr > 0.0f && KB.slip < 0) || (fr < 0.0f && KB.slip > 0)) {
        r = -32768.0f;
    } else {
        r = ((float)(-(int32_t)KB.slip) * 100.0f) / fr;
    }
    if (-500.0f > r) {
        r = -500.0f;
    }
    return b7910(r);
}

/* slot 34: 0xEBDBC → 0x1175D6 on block A (BD14+0x34 = 0: stand-in) */
static void prog_pass(void)
{
    const uint8_t hold = (KB.bd17 == 4u) ? SUB[0].f4 : KB.p31;
    KB.hold28 = hold;
    if (hold != 0u) {
        P.hold_n++;
    }
    f48(hold, KB.p27);
    dt1();
    KB.bd76 = aec();
#if !defined(__sh__)
    if ((s_host_mask & GS_HOST_OLD_PROGRESS) != 0u) {   /* the old stand-ins */
        KB.bd76 = 0x7FFF;
        KB.bd30 = 0xFFu;
        KB.sync = 1u;
    } else if ((s_host_mask & GS_HOST_FORCE_PROGRESS) != 0u) {
        KB.bd76 = s_host_bd76;
        KB.bd30 = s_host_bd30;
        KB.sync = 0u;
    }
#endif
}

/* the engagement start: 0xEDC5A's block-A branch (0x11793E, 0x1179D8(p, 1)) */
static void prog_start(int8_t gear)
{
    KB.tgear = gear;                                      /* 0x1176C4 (+1, +2) */
    KB.p5 = 0u;
    KB.p31 = 1u;
    KB.hold28 = 1u;
    KB.p27 = 0u;                                          /* BD1A = 0 (0xEB900) */
    P.sync_pass_ok = 0u;
    f48(1u, 0u);
    dt1_reset();
}

/* the end: 0xEBB6C → 0x11755E(block A, 1, …) */
static void prog_end(void)
{
    KB.p31 = 0u;
    KB.p27 = 1u;
}

/* 0xEB900 + 0xEDC5A (BD14): the shift object enters the garage engagement */
/* r04s: 0xEC6FE(BD14, pf, flag) — BD1C / BD1D = 0; with the start's flag
 * (0xDDCA2: E6C1 != pf && E6C1 != 0), the last release's third element
 * BD44 still applied (0xB822C) and 0xB8054(E5C2) = 0 (B0FE: stand-in 0):
 * BD1C = 1 for forward, BD1D = 1 for reverse */
static void ec6fe(uint8_t pf, uint8_t flag)
{
    KB.bd1c = 0u;
    KB.bd1d = 0u;
    if (flag != 0u && KB.bd44 != 0u && b822c(KB.bd44)) {
        if (pf == 1u) {
            KB.bd1c = 1u;
        } else if (pf == 2u) {
            KB.bd1d = 1u;
        }
    }
}

static void eb900(uint8_t pf, uint8_t e1, uint8_t e2, uint8_t e3, int8_t gear, uint8_t flag)
{
    KB.bd16 = (e1 == 0u) ? 0u : (pf == 0u ? 4u : 3u);
    if (e3 != 0u && pf == 0u) {
        KB.bd44 = e3;                                    /* r04s: 0xEB962..0xEB96E */
    }
    gsf_set_bd16(KB.bd16);
    e66d6(pf, e1, e2, e3, gear);
    KB.bd15 = (KB.bd16 != 0u) ? 1u : 0u;
    ec6fe(pf, flag);                                     /* r04s: 0xEB9C2 */
#if !defined(__sh__)
    {   /* r04s host witness: every start's reversal inputs (gs_shadow_host_r04s) */
        uint8_t *h = s_host_r04s[s_host_r04s_n & 7u];
        h[0] = pf; h[1] = flag; h[2] = E6C.c1; h[3] = (uint8_t)E6C.c2;
        h[4] = (uint8_t)(E33.t12 > 0xFFu ? 0xFFu : E33.t12); h[5] = KB.bd44;
        h[6] = (uint8_t)(KB.bd1c | (uint8_t)(KB.bd1d << 1)); h[7] = SUB[0].f1e;
        h[8] = SUB[2].f1e; h[9] = (uint8_t)SUB[2].f3a; h[10] = (uint8_t)W.passes; h[11] = (uint8_t)(W.passes >> 8);
        s_host_r04s_n++;
    }
#endif
    /* 0xEDC5A → 0xED906 → 0xED988 */
    if (KB.bd16 == 3u || KB.bd16 == 4u) {
        KB.bd17 = 4u;
    }
    KB.bd22 = G.cd2_now;                                  /* 0xED9B2: *0x89CD2 */
    if (KB.bd17 == 4u) {
        e6866(KB.bd17, KB.bd16);
    }
    if (KB.bd16 != 0u) {
        KB.bd8e = 0u;                                     /* 0xEDC5A → 0xED9B2 → 0x117970: +0x22 */
        prog_start(gear);                                /* 0xEDC5A → 0x11793E */
    }
}

/* 0xDDCA2: the engagement start (from the commit) */
static void ddca2(uint8_t old, uint8_t pf, int8_t gear)
{
    E33.b[0] = dc282(1u, pf, gear);
    E33.b[1] = dc282(2u, pf, gear);
    E33.b[2] = dc282(3u, pf, gear);
    if (old == pf) {
        W.unp_n++;                                       /* report 0x373 */
    }
    if (G.c2f != 0u) {
        E33.t12 = 0u;                                    /* r04s: 0xDE2FA (0xDDCF6) */
    }
    E33.n10 = (int16_t)G.cd0;                           /* +0x10 = 0x89CD0 */
    E33.ta = (int16_t)P.cba;                           /* +0xA  = 0x89CBA */
    /* r04s: the start's flag (0xDDD0A..0xDDD24: 0xB83B4(E5C4) = E6C1) → 0xEB900 → 0xEC6FE */
    eb900(pf, E33.b[0], E33.b[1], E33.b[2], gear, (E6C.c1 != pf && E6C.c1 != 0u) ? 1u : 0u);
    /* 0xEEDFC (0xFFF8C190) and 0xE984A (BC74: E6C2 = 3 → not started) */
}

/* 0xDDC38: the engagement end */
static void ddc38(void)
{
    E33.b[0] = 0u;
    E33.b[1] = 0u;
    E33.b[2] = 0u;
    e67ec(0u, 0u, 0u);
    prog_end();                                          /* 0xEBB6C → 0x11755E */
    /* r04q (B2, b2_slots_subobj_decode.md §C.2; 0xEBB6C clears only BD15):
     * BD17 is NOT cleared any more (it keeps 4 — "the three-element engine
     * owns the shift" — as the stock dump shows after an engagement).
     * BD16's clear is OURS, not the stock's (the stock leaves BD16 at the
     * last kind until the next start): kept because our completion test
     * (bab6c's BD16 branches), the releasing() accessor the monitor reads
     * (gs_shadow_releasing: BD16 == 4) and the release recorder need it */
    KB.bd16 = 0u;                                         /* ours (see above) */
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
    if (KB.bd16 == 3u) {
        bool d;
        if (SUB[0].act == 0u) {
            d = cdfa(&SUB[1], 1u) && cdfa(&SUB[2], 1u);
        } else {
            d = cdfa(&SUB[0], KB.sync);         /* BD14+0x58 = BD6C (last pass) */
        }
        if (d) {
            /* 2: after the timeout flag (+0x14: the 0x0E ramp, then the
             * +0x54 test or the +0x12 limit), 1: the normal +0x54 test */
            *cause = (SUB[0].tmo != 0u) ? 2u : 1u;
        }
        return d;
    }
#if !defined(__sh__)
    if (KB.bd16 == 4u && (s_host_mask & GS_HOST_OLD_RELEASE) != 0u) {
        W.unp_n++;                                       /* host negative: the old stand-in */
        *cause = 3u;
        return true;
    }
#endif
    if (KB.bd16 == 4u) {
        /* 0xEDA48 (+2 == 4): +0x2D = 0x106918 (BD16 == 4 branch, 0x10691A..
         * 0x10694C): done unless (D0E3 == 0 or the role-4 pressure 0x1069DA
         * = D0DC +0xA != 0) and 0xB822C(0x10CDDC(D0DC)); else done iff
         * (u16) BD8E >= (u16) D0EE (D0DC +0x12) [O 0xEDA64..0xEDA96] (G4a) */
        bool d = true;
        if (SUB[0].f7 == 0u || SUB[0].out_p != 0) {
            const uint8_t e = (SUB[0].eng != 0u) ? SUB[0].eng : SUB[0].rel;   /* 0x10CDDC */
            if (b822c(e)) {
                d = false;
            }
        }
        if (d) {
            *cause = 5u;
            return true;
        }
        if (KB.bd8e >= SUB[0].lim12) {
            *cause = 6u;
            return true;
        }
        return false;
    }
    /* BD16 == 0 (a change without elements): the stand-in, kept */
    R.idle_n++;
    *cause = 3u;
    return true;
}

/* ── the engine-state FSM 0xD4DC2 (object E6A8) and 0xB986C's flags ───── */
static void d4dc2(void)
{
    uint8_t *e = (uint8_t *)V_E6A8;
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
    switch (E28.st) {
    case 0u:
        E28.f4 = 0u;
        if (D0D6A) { E28.st = 1u; }
        else if (G.d12) { E28.st = 4u; E28.f4 |= 8u; }
        else if (E618) { E28.st = 4u; E28.f4 |= 0x80u; }
        else if (C8A == 1u && GS_AE2D == 0u) { E28.st = 7u; E28.f4 |= 0x20u; }
        break;
    case 2u:
        E28.f4 = 2u;
        if (E28.t6 > 0u || D0D6A) { /* wait */ }
        else if (G.d12) { E28.st = 4u; E28.f4 |= 8u; }
        else if (E618) { E28.st = 4u; E28.f4 |= 0x80u; }
        else { E28.st = 3u; }
        break;
    case 5u:
        E28.f4 = 2u;
        if (D0D6A) { E28.st = 1u; }
        else if (G.d12 || E618) { /* stay */ }
        else { E28.st = 6u; }
        break;
    case 7u:
        if (D0D6A) { E28.st = 1u; }
        else if (G.d12 || E618) { E28.st = 4u; }
        else if (C8A == 0u || GS_AE2D == 1u) { E28.st = 0xBu; E28.t8 = GS_E28E_T8; }
        else if (V_E6EE != 0u || V_E69B != 0u) { /* stay */ }
        else { E28.st = 8u; }
        break;
    case 9u:
        if (D0D6A) { E28.st = 1u; }
        else if (G.d12 || E618) { E28.st = 4u; }
        else if (C8A != 0u && GS_AE2D != 1u) { /* stay */ }
        else { E28.st = 10u; E28.t8 = GS_E28E_T8; }
        break;
    case 0xBu:
        if (D0D6A) { E28.st = 1u; }
        else if (G.d12 || E618) { E28.st = 4u; }
        else if (E28.t8 > 0u && GS_AE2D != 1u) { /* stay */ }
        else { E28.st = 0u; }
        break;
    case 1u: case 3u: case 4u: case 6u: case 8u: case 10u:
        break;
    default:
        W.unp_n++;                                       /* report 0x89 */
        E28.st = 0u;
        break;
    }
}
static void d0baa(void)
{
    switch (E28.st) {
    case 1u:  E28.st = 2u;  E28.f4 = 2u; break;
    case 3u:  E28.st = 0u;  break;
    case 4u:  E28.st = 5u;  E28.f4 = 2u; break;
    case 6u:  E28.st = 0u;  break;
    case 8u:  E28.st = 9u;  E28.f4 = 5u; break;
    case 10u: E28.st = 0xBu; break;
    default: break;
    }
}
static bool d0d52(void) { return E28.st != 0u && E28.st != 7u; }

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
static bool pending(void) { return V_E6EE != V_E69B; }            /* 0xCFE62 */

/* 0x103F56 (AE70[1] = 0, AE2C = 0, E298 = 0: stand-ins) */
static void r3f56(void)
{
    if (!d0d52() && E28.st != 7u && G.d0f != 0u /* && D11A6(E298) == 0 && AE70[1] != 3 */) {
        const uint32_t i = (uint32_t)G.ae28 + 3u * (uint32_t)V_E69B;
        if (i < 9u) {
            V_E6EF = gsf_fl8(0x0006354Cu + i);
        } else {
            W.unp_n++;
            V_E6EF = 0u;
        }
    } else {
        V_E6EF = 0u;
    }
}

/* 0xD15A6 (0xD1B10 / 0xD1990: the gear target, stand-in) */
static void d15a6(void)
{
    if (G.c3d != 0u && V_E69B == 1u) {
        V_E6A1 = V_E69F;
    }
    if (V_E6EF == 2u || V_E69B == 2u) {
        V_E6A1 = -1;
    } else {
        V_E6A1 = (int8_t)GS_TARGET;
    }
}

/* 0xDC37E: the gate of a nonzero commit */
static bool dc37e(void)
{
    return s_park_model == 1u && G.c3d == 0u && GS_E1F_SETTLE <= V_E6C5;
}

/* 0xCFD1A: the commit */
static void cfd1a(void)
{
    bool ok;
    if (V_E6EF == 0u) {
        ok = true;
    } else if (V_E6EF == 1u) {
        ok = V_E69F != -1;
    } else if (V_E6EF == 2u) {
        ok = V_E6A1 == -1 && V_E69F == -1;
    } else {
        W.unp_n++;                                       /* report 0x90 */
        ok = false;
    }
    if (G.e69c0 == 1u) {
        ok = false;
    }
    if (ok && !pending() && V_E6EF != V_E69B && (V_E6EF == 0u || dc37e())) {
        const uint8_t old = V_E69B;
        V_E69B = V_E6EF;
        G.pf = V_E6EF;                                   /* 0x89C30 */
        G.commit_now = 1u;
        W.commits++;
        W.commit_pass = W.passes;
        W.commit_old = old;
        W.commit_new = V_E69B;
        if (V_E69B != 0u && W.rec_slot < GS_REC_SLOTS) {  /* the recorder (ours) */
            G.rec_on = W.rec_slot;
            W.rec_n[W.rec_slot] = 0u;
            W.rec_slot = (uint8_t)(W.rec_slot + 1u);
            G.rec_t0 = W.passes;
        }
        ddca2(old, V_E69B, V_E69F);                      /* 0xBAABC */
        if (KB.bd16 == 4u) {                              /* G4a: a release started */
            const uint8_t k = (old == 2u) ? 1u : 0u;     /* slot 0 from D, 1 from R */
            volatile gs_rslot_t *h = &g_rel.slot[k];
            R.starts++;
            R.on = k;
            R.last = k;
            h->start_pass = W.passes;
            h->len = 0xFFFFu;
            h->bd8e_end = 0u;
            h->lim12 = SUB[0].lim12;
            h->n = 0u;
            h->cause = 0u;
            h->gear = V_E69F;
            for (unsigned j = 0u; j < 3u; j++) {
                h->el[j] = SUB[j].rel;
                h->p26[j] = 0;
            }
        } else if (V_E69B != 0u) {
            R.on = 0xFFu;                                /* a nonzero commit ends the release slot */
        }
    }
}

/* 0x103DE2: completion of a pending change */
static void r3de2(void)
{
    if (pending()) {
        uint8_t cause = 4u;
        const bool done = bab6c(&cause);
        if (done || V_E6EF != V_E69B || d0d52()) {
            if (!done) {
                cause = 4u;
            }
            if (KB.bd16 == 4u && R.last < 2u) {           /* G4a: the release completes */
                volatile gs_rslot_t *h = &g_rel.slot[R.last];
                const uint32_t d = W.passes - h->start_pass;
                h->len = (uint16_t)(d > 0xFFFEu ? 0xFFFEu : d);
                h->bd8e_end = KB.bd8e;
                h->cause = cause;
            }
            V_E6EE = V_E69B;
            G.c2f = V_E69B;                              /* 0x89C2F */
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
    if (pending() || (V_E4A7 != V_E6A1 && V_E6A1 == -1)) {
        V_E4A7 = V_E6A1;
    } else if (V_E69B == 0u && V_E4A7 != V_E6A1) {
        W.seq_n++;
        V_E4A7 = V_E6A1;
    }
}

/* 0x103CEC strobe (shift-object checks: accept) → 0xD140C */
static void r3cec(void)
{
    if (V_E4A7 != V_E69F && !pending()) {
        V_E69F = V_E4A7;                                 /* 0x89C2B */
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
            r = (o == GSE_OBJ_WK) ? 0u : (uint16_t)GS_LINEP;
        }
    } else {
        r = RU16(o, RO_P);
    }
    RU16(o, RO_RES) = r;
    RU16(o, 0x04u) = r;
    if ((uint32_t)(int32_t)(int16_t)r > (uint32_t)(int32_t)(int16_t)RU16(GSE_OBJ_SYS, RO_RES)) {
        RU16(o, 0x04u) = RU16(GSE_OBJ_SYS, RO_RES);
    }
}

static void e53da(uint32_t o, uint16_t p, uint8_t mode, uint8_t prio, uint8_t src)
{
    RU8(o, RO_SET) = 1u;
    RU16(o, RO_P) = p;
    RU8(o, RO_MODE) = mode;
    RU8(o, RO_SRC) = src;
    RU8(o, RO_PRIO) = prio;
    if (o == GSE_OBJ_SYS || o == GSE_OBJ_WK) {
        e54a4(o);
    } else {
        gsf_e4e5c(o);
    }
}

static uint8_t dc1e2(uint8_t block, int8_t gear, uint8_t e)
{
    static const uint8_t T63122[6] = { 0, 1, 0, 0, 0, 0 };
    const uint8_t m = d0c94(E28.st);
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
    return SUB[0].act != 0u || (SUB[1].act != 0u && SUB[1].f9 == 0u) || SUB[2].act != 0u;
}

static void dc4c4(void)
{
    static const uint32_t EO[6] = { 0u, GSE_OBJ_A, GSE_OBJ_B, GSE_OBJ_C, GSE_OBJ_D, GSE_OBJ_E };
    const uint8_t block = (G.pf != 0u) ? 1u : 0u;

    e53da(GSE_OBJ_SYS, (uint16_t)GS_LINEP, (uint8_t)GS_SYSMODE, 4u, 0x15u);
    if ((G.pf == 1u && G.c2f == G.pf)) {
        W.lockup_n++;                                    /* 0xDCD38.. lockup branch: not ported */
    }
    e53da(GSE_OBJ_WK, 0u, 1u, 4u, 0x16u);
    if (e68ac()) {                                       /* 0xDC812 */
        for (uint8_t k = 0u; k < 3u; k++) {
            const uint8_t e = E33.b[k];
            if (e != 0u && RU8(req_obj(e), RO_SET) == 0u) {
                const gs_sub_t *s = &SUB[k];
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
        const uint8_t m = d0c94(E28.st);
        for (uint8_t e = 1u; e <= 5u; e++) {
            const uint32_t o = EO[e];
            bool hold = RU8(o, RO_SET) == 0u;
            if (!hold && m != 0u) {
                hold = !(m == 5u && e == 1u);
            }
            if (hold) {
                e53da(o, dc1e2(block, V_E69F, e) != 0u ? 20000u : 0u, 1u, 4u, 0u);
            }
        }
    }
    for (uint8_t e = 1u; e <= 5u; e++) {
        RU8(EO[e], RO_SET) = 0u;
    }
    RU8(GSE_OBJ_SYS, RO_SET) = 0u;
    RU8(GSE_OBJ_WK, RO_SET) = 0u;
}

/* r04s: 0xDC0F8(E6BF) — slot 42, right after the producer (0xB85C8) and
 * before the actuators (0xB85C0 → 0xDC076), list 0xA30E2 [O bytes]:
 *   0x89C30 == E6C1        → E6C2 = the gear code 0x89C2B;
 *   else if 0x89C30 != 0   → E6C1 = 0x89C30 once the gear's first element
 *                            (0xDC1C4: role table 0x6312F[gear code])
 *                            reports applied: 0xE55DC(e) → vtable +0x38 =
 *                            0xE4DBA (+0x46 == 10000) = our b822c.
 * E6C1 is never written back to 0 by power flow 0: after a completed D it
 * holds 1 / 1 through P, after a completed R 2 / −1. Readers: 0x1066D6 and
 * 0xDDCA2 (the start's flag), 0x10D130 / 0x10F084 (E6C2), BC74's start
 * 0xE993A (not ported: it needs the pedal 0x89CD8 < 13, GS_PEDAL is 23) */
static void dc0f8(void)
{
    if (G.pf == E6C.c1) {
        E6C.c2 = V_E69F;                                 /* 0x89C2B */
    } else if (G.pf != 0u) {
        const uint8_t e = gsf_fl8((uint32_t)((int32_t)0x0006312F + (int32_t)V_E69F));
        if (b822c(e)) {
            E6C.c1 = G.pf;
        }
    }
}

/* r04s: 0x10CFB6(0xFFF8D1B4, e) via 0xBA7DC → 0xEEB22 — D1B4's +0x3A while
 * D1B4's element (+0x20) is e, else 0. The fill time's term (app/gs_fill.c
 * RQF_BA7DC_HOOK) */
int16_t gs_shadow_cfb6(uint8_t elem)
{
    return (SUB[2].eng == elem) ? SUB[2].f3a : (int16_t)0;
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
        { GSE_OBJ_A, 0u, 0u, 0u }, { GSE_OBJ_B, 2u, 0u, 0u }, { GSE_OBJ_C, 4u, 1u, 0u },
        { GSE_OBJ_D, 1u, 1u, 0u }, { GSE_OBJ_E, 3u, 1u, 0u }, { GSE_OBJ_WK, 5u, 0u, 1u },
        { GSE_OBJ_SYS, 6u, 1u, 1u },
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
#if !GSE_REC
static void rec_pass(void) { }
#else
static void rec_pass(void)
{
    static const uint32_t EO[5] = { GSE_OBJ_A, GSE_OBJ_B, GSE_OBJ_C, GSE_OBJ_D, GSE_OBJ_E };
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
        if (SUB[k].out_mode != 0u) { modes = (uint8_t)(modes | (1u << (5u + k))); }
    }
    r->pass = (uint16_t)(W.passes - G.rec_t0);
    r->pf = G.pf;
    r->flags = (uint8_t)((V_E6EF & 3u) | (pending() ? 4u : 0u) | (e68ac() ? 8u : 0u) |
                         (SUB[0].tmo != 0u ? 0x10u : 0u) | (G.done_now ? 0x20u : 0u) |
                         (G.commit_now ? 0x40u : 0u) | (G.c3d != 0u ? 0x80u : 0u));
    r->ph[0] = SUB[0].phase;
    r->ph[1] = SUB[1].phase;
    r->ph[2] = SUB[2].phase;
    r->modes = modes;
    r->reqset = set;
    r->gear = V_E69F;
    for (unsigned c = 0u; c < 7u; c++) {
        r->cur[c] = G.cur[c];
    }
    r->bd30 = KB.bd30;
    r->sync = (uint8_t)((KB.sync != 0u ? 1u : 0u) | (KB.hold28 != 0u ? 2u : 0u) |
                        (KB.bd2e == 0xFFFFu ? 4u : 0u));
    r->bd76 = KB.bd76;
    W.rec_n[slot] = (uint8_t)(n + 1u);
}
#endif

/* ── the release recorder (G4a, ours) ──────────────────────────────────── */
#if !GSE_REC
static void rel_pass(void)                  /* the counters only (no recorder) */
{
    if (KB.bd16 == 4u) {
        R.rel_passes++;
        W.rel_n++;
    }
}
#else
static uint8_t ph2(uint8_t ph)
{
    return (ph >= 0x12u && ph <= 0x14u) ? (uint8_t)(ph - 0x12u) : 3u;
}

static void rel_pass(void)
{
    static const uint8_t SH[5] = { 0u, 1u, 2u, 3u, 4u };    /* A D B E C */
    const uint8_t k = R.on;
    volatile gs_rslot_t *h;
    volatile gs_rrec_t *r;
    uint8_t n;

    if (KB.bd16 == 4u) {
        R.rel_passes++;
        W.rel_n++;
    }
    if (k >= 2u) {
        return;
    }
    h = &g_rel.slot[k];
    n = h->n;
    if (n >= GS_RREC_N) {
        R.on = 0xFFu;
        return;
    }
    if (n == 0u) {
        for (unsigned j = 0u; j < 3u; j++) {
            h->p26[j] = SUB[j].p26;
        }
    }
    r = &g_rel.rec[k][n];
    for (unsigned c = 0u; c < 5u; c++) {
        r->cur[c] = G.cur[SH[c]];
    }
    r->ph = (uint8_t)(ph2(SUB[0].phase) | (uint8_t)(ph2(SUB[1].phase) << 2) |
                      (uint8_t)(ph2(SUB[2].phase) << 4));
    r->fl = (uint8_t)((KB.bd16 == 4u ? 0x01u : 0u) | (G.done_now ? 0x02u : 0u) |
                      (G.commit_now ? 0x04u : 0u) | (SUB[0].f1f != 0u ? 0x08u : 0u) |
                      (SUB[2].f1f != 0u ? 0x10u : 0u) |
                      ((int32_t)b77a0(KB.slip) >= (int32_t)(uint16_t)f26c() ? 0x20u : 0u) |
                      (SUB[0].out_mode != 0u ? 0x40u : 0u) | (SUB[2].out_mode != 0u ? 0x80u : 0u));
    h->n = (uint8_t)(n + 1u);
}
#endif

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
    volatile uint8_t *b;
#if defined(GSE_CLEAR_BEGIN) && defined(__sh__)
    {
        /* G6a: the binding's whole NOLOAD block (this TU's and its fill
         * instance's statics), which start.S does not clear */
        extern uint8_t GSE_CLEAR_BEGIN[], GSE_CLEAR_END[];
        for (volatile uint8_t *q = GSE_CLEAR_BEGIN; q < (volatile uint8_t *)GSE_CLEAR_END; q++) {
            *q = 0u;
        }
    }
#endif
    b = (volatile uint8_t *)&g_s;
    for (unsigned i = 0u; i < sizeof g_s; i++) { b[i] = 0u; }
    /* G6c: the pinned stock objects (outside the binding's cleared block) */
    gse_zero(&g_e28e, sizeof g_e28e);
    gse_zero(&g_e33c, sizeof g_e33c);
    gse_zero(&g_e4ac, sizeof g_e4ac);
    gse_zero(&g_bd14, sizeof g_bd14);
    gse_zero(&g_e4a7, sizeof g_e4a7);
    gse_zero(&g_e69b, sizeof g_e69b);
    gse_zero(&g_e69f, sizeof g_e69f);
    gse_zero(&g_e6a1, sizeof g_e6a1);
    gse_zero(g_e6a8, sizeof g_e6a8);
    gse_zero(&g_e6c1, sizeof g_e6c1);                    /* r04s */
    gse_zero(&g_e6c5, sizeof g_e6c5);
    gse_zero(&g_e6ee, sizeof g_e6ee);
    gse_zero(&g_e6ef, sizeof g_e6ef);
    gse_zero(g_sub, sizeof g_sub);                       /* r04s: the pinned sub-objects */
    gse_zero(g_pr58, sizeof g_pr58);                     /*       and their {idx, blk} cells */
    gse_zero(g_pr5c, sizeof g_pr5c);
    gse_zero(g_pr60, sizeof g_pr60);
    b = (volatile uint8_t *)&g_w;
    for (unsigned i = 0u; i < sizeof g_w; i++) { b[i] = 0u; }
#if GSE_REC
    b = (volatile uint8_t *)g_rec0;
    for (unsigned i = 0u; i < sizeof g_rec0; i++) { b[i] = 0u; }
    b = (volatile uint8_t *)g_rec1;
    for (unsigned i = 0u; i < sizeof g_rec1; i++) { b[i] = 0u; }
#endif
    b = (volatile uint8_t *)&g_p;
    for (unsigned i = 0u; i < sizeof g_p; i++) { b[i] = 0u; }
    b = (volatile uint8_t *)&g_rel;
    for (unsigned i = 0u; i < GSE_REL_SIZE; i++) { b[i] = 0u; }
    R.on = 0xFFu;
    R.last = 0xFFu;
    KB.bd8e = 0xFFFFu;                                    /* dump: long idle */
    for (uint32_t a = GSE_REQ_BASE; a < GSE_REQ_BASE + GS_REQ_SIZE; a++) {
        *(volatile uint8_t *)(uintptr_t)a = 0u;
    }
    gs_fill_cells.c851a2 = 0u;
    gs_fill_cells.c851a3 = 0u;
    gs_fill_cells.c851a5 = 0u;
    gs_fill_cells.e384 = 0u;
    E28.st = 1u;                                         /* 0xD0C5A */
    E28.f4 = 4u;
    V_E69F = (int8_t)GS_TARGET;
    V_E6A1 = (int8_t)GS_TARGET;
    V_E4A7 = (int8_t)GS_TARGET;
    SUB[0].cls = 2u;
    SUB[1].cls = 3u;
    SUB[2].cls = 4u;
    for (uint8_t k = 0u; k < 3u; k++) {
        /* r04s: the constructor 0x10D010's vtable word and pointers (the
         * cells stay 0 until the first 0x10D130 writes them, as before —
         * the stock's 0x15CA38 defaults are not ported: nothing of ours
         * reads a cell before its sub-object's first start) */
        SUB[k].ramp.vptr = GSE_RAMP_VTBL;
        SUB[k].f5 = 1u;                                  /* 0x10D064 / 0x10D066: +5 = +6 = 1 */
        SUB[k].f6 = 1u;
        SUB[k].p58 = pr_addr(g_pr58, GSE_PR58_STOCK, k);
        SUB[k].p5c = pr_addr(g_pr5c, GSE_PR5C_STOCK, k);
        SUB[k].p60 = pr_addr(g_pr60, GSE_PR60_STOCK, k);
    }
    for (unsigned k = 0u; k < 3u; k++) {
        d0b4(&SUB[k]);
        SUB[k].type = 6u;
    }
    EAC.e4ac5 = GS_E4B1;
    /* the shift context at its idle values (dump: long-idle synchronised) */
    KB.bd30 = (uint8_t)GS_IDLE_BD30;
    KB.bd76 = (int16_t)GS_IDLE_BD76;
    KB.bd2e = (uint16_t)GS_IDLE_BD2E;
    KB.sync = 1u;
    KB.tgear = 3;
    KB.act3 = 1u;
    KB.p27 = 1u;
    KB.p31 = 0u;
    KB.dir = 0u;
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
#if !defined(__sh__)
    s_host_r04s_n = 0u;
#endif
}

/* the pass head (the live values by value) and the boot-state-1 pass —
 * macros, so that instance 1's tick compiles to r04m's object code and the
 * G6a boot entry (GSE_BOOT_ENTRY) runs exactly the same text */
#define GSE_TICK_HEAD(in)                                                       \
    s_park_model = (in)->park_model;                                            \
    s_park_req_in = (in)->park_req;                                             \
    G.done_now = 0u;                                                            \
    G.commit_now = 0u;                                                          \
    /* slot 26: the one-hot mode flag */                                        \
    G.c3d = GS_FORCED ? 0u : (((in)->os_mode == 1u) ? 1u : 0u);                 \
    /* slot 27: 0xB912C (0x13BF66: E90E held while 0x108 is invalid) */         \
    G.rngd = (in)->rngd;                                                        \
    if ((in)->eng_valid != 0u && (in)->eng_speed != 0xFFFFu) {                  \
        G.cd0 = (in)->eng_speed;                                                \
        G.d06 = 0u;                                                             \
    } else {                                                                    \
        G.d06 = 2u;                                                             \
    }                                                                           \
    G.c13 = (in)->temp;                                                         \
    G.cd2_now = (in)->cd2;                                                      \
    P.cb8 = (in)->out_rpm;                  /* 0x89CB8 (stand-in: live sensor) */ \
    P.cba = (in)->tur_rpm;                  /* 0x89CBA (stand-in: live sensor) */ \
    GSE_NEG_C3D_HOOK();                                                         \
    fill_bind();                                                                \
    gsf_set_pf(G.pf, G.c2f)
#ifndef GSE_NEG_C3D_HOOK
#  define GSE_NEG_C3D_HOOK() ((void)0)      /* G6a host negative only (app/gs_cand_bind.h) */
#endif
#if defined(GSE_BOOT_ENTRY)
/* r04q (D-G6-13, ~/tcu_sweep/r04q_port.md; decode b1_e28e_timing_decode.md
 * §1.1, bytes re-read 0xA2E30..0xA2E72, 0xD01C4..0xD0230, 0x103FD6): the
 * row-1 list's sub-list 0x4D7B0 ENTRY 16 = 0xA2E30, run by the boot pass
 * before entry 17 (the producer). Its body, in the stock order:
 *   0xB6BDC → 0xCFF0E(AE28)        NOT PORTED: the E6F0 object's init
 *                                  (0x104568 / 0x104712), the 0xBA6F4(E617,
 *                                  45) hook (E617 0: rts), a consistency
 *                                  check of E6EE/E69B/CCE4/E69F that only
 *                                  reports (0xB79E4 id 0x7E) and 0xD12E8
 *                                  (E2A2) — none of them is read by the
 *                                  holder / producer / engine we port;
 *   0xB6BE4(1) → 0xCFFDE(1)        param 1: 0xD0C5A (E28E = 1, flags |= 4)
 *                                  INSTEAD OF the step 0xD093A; its other
 *                                  init calls (0x104008, 0xCFE56, 0x103EC0,
 *                                  0x104A0E, 0xD13C6, 0x103B0E) re-initialise
 *                                  the power-flow cells our gse_init has
 *                                  just set — no candidate code runs between
 *                                  init and this pass (the tick returns in
 *                                  OS mode 0), so they are at their init
 *                                  values here: only 0xD0C5A is ported;
 *   0xB6BEC → 0xD01C4 (slot 31)    0x103DE2(E6EE); r = 0xCFD1A(E69B); if r:
 *                                  0x103DE2(E6EE); 0x104A28(E4A6) — ported
 *                                  as the bytes (the conditional second
 *                                  0x103DE2, unlike our cyclic slot 31);
 *   0xB6BF4 → 0xD01E6 (slot 32)    0x103B36(CCE4, 1) + 0xD140C (+ the
 *                                  conditional 0x103B36(CCE4, 0)) = our
 *                                  r3cec; 0x103FD6(E6EF) (ported below:
 *                                  E69C 0 and E28E not idle → E6EF 0);
 *                                  0x103DE2; r = 0xCFD1A; if r: 0x103DE2;
 *                                  **0xD0BAA (E28E 1 → 2)**; 0xBA6F4(E617,
 *                                  47): E617 0 → rts, not ported;
 *   0xBAC0E → 0xEAC2C(E3EC), 0xBAC1E → 0xDD954(E33C), 0xB85D0 → 0xDBEE8,
 *   0xB85B0 → 0xDC086, 0xB85B8(0) → 0xDC35E(E6BF, 0)
 *                                  NOT PORTED (b2_slots_subobj_decode.md:
 *                                  > 1300 rpm forward-drive enable — 0
 *                                  here; the gear-pair coordinator — idle
 *                                  with a constant gear code; the E6BF
 *                                  object's inits; 0xDC35E(0) clears the
 *                                  settle counter E6C5, already 0 from
 *                                  gse_init): no effect at the boot pass.
 * At the boot pass E6EE = E69B = 0 (nothing pending), E6EF = 0, E4A7 =
 * E6A1 = E69F = the target: every call but 0xD0C5A and 0xD0BAA is a no-op
 * here; they are ported so the composition is the stock's. Result: E28E is
 * 2 when entry 17's producer runs (ONE released producer pass, the stock's
 * count; r04p held two). */
static void r3fd6(void)                                  /* 0x103FD6 (E6EF) */
{
    if (G.e69c0 == 0u) {                                 /* E69C == 0 */
        if (d0d52()) {
            V_E6EF = 0u;                                 /* E28E not idle (0xD0D52) */
        } else {
            W.unp_n++;                                   /* 0xD0D66 → 0x103F56: not ported
                                                          * (unreachable here: E28E = 1) */
        }
    }
}
static void row1_e16(void)                               /* 0xA2E30 */
{
    E28.st = 1u;                                         /* 0xCFFDE(1) → 0xD0C5A */
    E28.f4 = (uint8_t)(E28.f4 | 4u);
    /* 0xD01C4 */
    r3de2();
    G.commit_now = 0u;
    cfd1a();
    if (G.commit_now != 0u) {
        r3de2();
    }
    r4a28();
    /* 0xD01E6 */
    r3cec();
    r3fd6();
    r3de2();
    G.commit_now = 0u;
    cfd1a();
    if (G.commit_now != 0u) {
        r3de2();
    }
    d0baa();                                             /* E28E 1 → 2 */
}
#  define GSE_ROW1_E16()   row1_e16()
#  define GSE_BOOT_WIT()   (W.e28e = E28.st)             /* the monitor reads it from this pass on */
#else
#  define GSE_ROW1_E16()   ((void)0)                     /* instance 1 (host reference): r04l's */
#  define GSE_BOOT_WIT()   ((void)0)
#endif
#define GSE_BOOT_PASS()                                                         \
    /* the boot-state-1 pass: 0xE5602, (r04q) row-1 entry 16, then the        \
     * producer + actuators (entry 17) */                                       \
    s_booted = true;                                                            \
    gsf_e5602();                                                                \
    d4dc2();                                                                    \
    GSE_ROW1_E16();                                                             \
    dc4c4();                                                                    \
    actuators();                                                                \
    GSE_BOOT_WIT();                                                             \
    W.passes++

#if defined(GSE_AE28_IN)
static volatile uint32_t s_ae28_x;      /* live AE28 != the template's own 0x63445 lookup */
#endif

#if defined(GSE_BOOT_ENTRY)
/* G6a: the boot-state-1 pass at ITS slot (app/main.c → bsw/sup/sup_mode.c,
 * row-1 list entries 12 / 17, OS mode 0 — right after app/rq_prod.c's
 * rq_prod_boot_pass), not at the first mode-1 tick as instance 1 does */
void gs_shadow_boot_pass(const gs_in_t *in)
{
    if (s_booted) {
        return;
    }
    GSE_TICK_HEAD(in);
    GSE_BOOT_PASS();
}
#endif

void gs_shadow_tick_10ms(const gs_in_t *in)
{
    if (in->os_mode < 1u || in->os_mode > 3u) {
        return;                                          /* the stock's lists: modes 1..3 */
    }
    GSE_TICK_HEAD(in);
    if (!s_booted) {
        GSE_BOOT_PASS();
        return;
    }
    GSE_STAGE(0u);                                       /* the head (inputs, fill_bind) */
    /* slot 28: 0xEC0E0 — BD30 / BD2E (+= *0xFFF8E220 = 1), then 0x106848 */
    KB.bd30 = sat8p1(KB.bd30);
    KB.bd2e = sat16p1(KB.bd2e);
    KB.bd8e = sat16p1(KB.bd8e);                            /* 0x117538(BD6C) → 0xBAE5E(E21E, +0x22) */
    E33.t12 = sat16p1(E33.t12);                          /* r04s: 0xDE0FE → 0xBAE5E(E21E, E33C+0x12) */
    /* the timers, the time base, the element tick / update */
    EAC.e4ac6 = sat8p1(EAC.e4ac6);
    cf4e(&SUB[0]);
    cf4e(&SUB[1]);
    cf4e(&SUB[2]);
    if (E28.t6 > 0u) { E28.t6--; }
    if (E28.t8 > 0u) { E28.t8--; }
    if (G.c3d != 0u) {
        V_E6C5 = 0u;                                     /* 0xDC35E */
    } else {
        V_E6C5 = sat8p1(V_E6C5);                         /* 0xDBEEC */
    }
    G.tb = (uint16_t)(G.tb + 10u);
    gsf_e5624();
    gsf_e566c();
    GSE_STAGE(1u);                                       /* slot 28: timers, element tick / update */
    /* slot 29: 0xB986C */
    d4dc2();
    GSE_STAGE(2u);                                       /* slot 29 */
    /* slot 30: 0xCFFDE */
#if defined(GSE_AE28_IN)
    {
        /* G6a (the G6b form): the LIVE direction request (park_law's
         * 0xCFFDE, one 0xFFF8AE28), by value; the template's own lookup on
         * the same 0x89CDC kept only as a cross-check */
        uint8_t own = G.ae28;
        if (G.rngd <= 8u) {
            const uint8_t x = gsf_fl8(0x00063445u + G.rngd);
            if (x != 0xFFu) {
                own = x;
            }
        }
        if (own != in->ae28) {
            s_ae28_x++;
        }
        G.ae28 = in->ae28;
    }
#else
    if (G.rngd <= 8u) {
        const uint8_t x = gsf_fl8(0x00063445u + G.rngd);
        if (x != 0xFFu) {
            G.ae28 = x;
        }
    } else {
        W.unp_n++;                                       /* report 0x7E */
    }
#endif
    d093a();
    r3f56();
    d15a6();
    GSE_STAGE(3u);                                       /* slot 30 */
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
    GSE_STAGE(4u);                                       /* slots 31, 32 */
    /* slot 34: 0xEBDBC → 0x1175D6 (block A): sync, rate, BD76 */
    prog_pass();
    /* slot 34: 0xBAC26 → 0xDDFD8(E33C) — r04s: only its 0xDE2FA (E33C+0x12
     * = 0 while 0x89C2F != 0); the peaks +0xA / +0x10 and the torque term
     * +0xE (read by nothing of ours) and E33C+0 not ported */
    if (G.c2f != 0u) {
        E33.t12 = 0u;
    }
    GSE_STAGE(5u);                                       /* slot 34 */
    /* slot 35: 0xEB348 → step (D148, D1B4, D0DC), output (same order) */
    e6698_pro();                                         /* r04s: 0x106698's prologue */
    ce88(&SUB[1]);
    ce88(&SUB[2]);
    ce88(&SUB[0]);
    e6698_epi();                                         /* r04s: 0x106AC4 */
    ceca(&SUB[1]);
    ceca(&SUB[2]);
    ceca(&SUB[0]);
    GSE_STAGE(6u);                                       /* slot 35 */
    /* slot 42: the producer + actuators */
    gsf_set_pf(G.pf, G.c2f);
    dc4c4();
    dc0f8();                                             /* r04s: 0xB85D8 (between 0xB85C8 and 0xB85C0) */
    actuators();
    GSE_STAGE(7u);                                       /* slot 42 */
    rec_pass();
    rel_pass();
    G.e69c0 = 0u;                                        /* [I] one pass */
    W.e28e = E28.st;
    W.e6a8 = V_E6A8[4];
    W.now_pf = G.pf;
    W.now_e6ef = V_E6EF;
    W.now_ae28 = G.ae28;
    W.now_flags = (uint8_t)((G.d0f != 0u ? 1u : 0u) | (pending() ? 2u : 0u) |
                            (G.c3d != 0u ? 4u : 0u) | (e68ac() ? 8u : 0u));
    W.now_gear = V_E69F;
    for (unsigned k = 0u; k < 3u; k++) {
        W.now_ph[k] = SUB[k].phase;
    }
    for (unsigned c = 0u; c < 7u; c++) {
        W.now_cur[c] = G.cur[c];
    }
    W.passes++;
    GSE_STAGE(8u);                                       /* the tail (recorders, witnesses) */
}

const volatile gs_wit_t *gs_shadow_wit(void) { return &g_w; }
const volatile gs_rel_t *gs_shadow_rel(void) { return (const volatile gs_rel_t *)(const volatile void *)&g_rel; }
uint8_t gs_shadow_releasing(void)    { return g_bd14.bd16 == 4u ? 1u : 0u; }
const volatile gs_rec_t *gs_shadow_rec(uint8_t slot, uint8_t i)
{
#if GSE_REC
    return (slot < GS_REC_SLOTS && i < GS_REC_N) ? rec_at(slot, i) : 0;
#else
    (void)slot;
    (void)i;
    return 0;                                            /* GSE_REC 0: no recorder */
#endif
}
int16_t  gs_shadow_bd76(void)        { return g_bd14.bd76; }
uint8_t  gs_shadow_bd30(void)        { return g_bd14.bd30; }
uint8_t  gs_shadow_sync(void)        { return g_bd14.sync; }
int16_t  gs_shadow_slip(void)        { return g_bd14.slip; }
uint16_t gs_shadow_cur(uint8_t ch)   { return ch < 7u ? g_s.cur[ch] : 0xFFFFu; }
uint8_t  gs_shadow_pf(void)          { return g_s.pf; }
int8_t   gs_shadow_gear(void)        { return g_e69f; }
uint8_t  gs_shadow_phase(uint8_t k)  { return k < 3u ? g_sub[k].phase : 0xFFu; }
uint8_t  gs_shadow_pending(void)     { return g_e6ee != g_e69b ? 1u : 0u; }
uint8_t  gs_shadow_e6ef(void)        { return g_e6ef; }
uint8_t  gs_shadow_ae28(void)        { return g_s.ae28; }
uint8_t  gs_shadow_d0f(void)         { return g_s.d0f; }
/* the request mode +7 of the shadow's request object behind channel ch
 * (0..4 = A D B E C): 1 = the on/off law (500 / 8500), 0 = the curve law
 * (the garage bridge's mode-1 guard, app/gs_bridge.c); 0xFF otherwise */
uint8_t  gs_shadow_reqmode(uint8_t ch)
{
    static const uint32_t EO[5] = { GSE_OBJ_A, GSE_OBJ_D, GSE_OBJ_B, GSE_OBJ_E, GSE_OBJ_C };
    return ch < 5u ? RU8(EO[ch], RO_MODE) : 0xFFu;
}
/* G6a: the fill-FSM state +0x34 of the request object behind channel ch
 * (0..4 = A D B E C); 0xFF otherwise (the comparator, app/g6a.c) */
uint8_t  gs_shadow_fillst(uint8_t ch)
{
    static const uint32_t EO[5] = { GSE_OBJ_A, GSE_OBJ_D, GSE_OBJ_B, GSE_OBJ_E, GSE_OBJ_C };
    return ch < 5u ? RU8(EO[ch], RO_ST) : 0xFFu;
}
#if defined(GSE_AE28_IN)
uint32_t gs_shadow_ae28_x(void) { return s_ae28_x; }
#endif

#if !defined(__sh__)
void gs_shadow_host_sub(uint8_t k, int16_t v[6])
{
    const gs_sub_t *s = &SUB[k < 3u ? k : 0u];
    v[0] = s->calc_p;
    v[1] = s->base24;
    v[2] = s->tgt54;
    v[3] = s->dur30;
    v[4] = (int16_t)s->t32;
    v[5] = (int16_t)RU16(req_obj(s->eng != 0u ? s->eng : 2u), RO_RES);
}

uint8_t gs_shadow_host_ceca(uint8_t cls, uint8_t eng, uint8_t rel, uint8_t f7, int16_t calc_p,
                            uint8_t ceca_cls)
{
    gs_sub_t t = { 0 };
    t.cls = cls;
    t.eng = eng;
    t.rel = rel;
    t.f7 = f7;
    t.calc_p = calc_p;
    t.phase = 0x13u;
    s_host_ceca_cls = ceca_cls;
    ceca(&t);
    s_host_ceca_cls = 0u;
    return t.out_mode;
}

void gs_shadow_host_rel(uint8_t k, int16_t v[4])
{
    const gs_sub_t *s = &SUB[k < 3u ? k : 0u];
    v[0] = s->p26;
    v[1] = s->out_p;
    v[2] = (int16_t)s->f1f;
    v[3] = (int16_t)s->lim12;
}

/* r04s (host only): start k (0 = the oldest kept) of the last 8: pf, the
 * start's flag, E6C1, E6C2, E33C+0x12 (sat. 255), BD44, BD1C | BD1D << 1,
 * D0DC +0x1E, D1B4 +0x1E, D1B4 +0x3A (low byte), the pass (2 bytes);
 * returns the number of starts so far */
uint8_t gs_shadow_host_r04s(uint8_t k, uint8_t v[12])
{
    const uint8_t n = s_host_r04s_n;
    const uint8_t have = n < 8u ? n : 8u;
    if (k < have) {
        const uint8_t *h = s_host_r04s[(uint8_t)(n - have + k) & 7u];
        for (unsigned i = 0u; i < 12u; i++) { v[i] = h[i]; }
    }
    return n;
}

void gs_shadow_host_standins(uint8_t mask, int16_t bd76, uint8_t bd30)
{
    s_host_mask = mask;
    s_host_bd76 = bd76;
    s_host_bd30 = bd30;
    gsf_host_e469c_off((mask & GS_HOST_OLD_E469C) != 0u);
}

/* G6-0 (host only): this instance's state, block by block, for the
 * two-instance identity test (tests/unit/gs_dual.c): every byte the engine
 * owns — its statics and its request objects (relative to GSE_REQ_BASE) —
 * and the fill cells of its fill instance. NULL past the last block. */
const volatile void *gs_shadow_host_state(uint8_t k, uint32_t *n)
{
    switch (k) {
    case 0u:  *n = (uint32_t)sizeof g_s;     return &g_s;
    case 1u:  *n = (uint32_t)sizeof g_w;     return &g_w;
    case 2u:  *n = (uint32_t)sizeof g_p;     return &g_p;
#if GSE_REC
    case 3u:  *n = (uint32_t)sizeof g_rec0;  return g_rec0;
    case 4u:  *n = (uint32_t)sizeof g_rec1;  return g_rec1;
#else
    case 3u:  case 4u: *n = 0u;              return &g_s;   /* GSE_REC 0: none */
#endif
    case 5u:  *n = GSE_REL_SIZE;             return &g_rel;
    case 6u:  *n = GS_REQ_SIZE;              return (const volatile void *)(uintptr_t)GSE_REQ_BASE;
    case 7u:  *n = (uint32_t)sizeof gs_fill_cells; return &gs_fill_cells;
    case 8u:  *n = 1u;                       return &s_park_model;
    case 9u:  *n = 1u;                       return &s_park_req_in;
    case 10u: *n = (uint32_t)sizeof s_booted; return &s_booted;
    /* G6c: the pinned stock objects */
    case 11u: *n = (uint32_t)sizeof g_e28e;  return &g_e28e;
    case 12u: *n = (uint32_t)sizeof g_e33c;  return &g_e33c;
    case 13u: *n = (uint32_t)sizeof g_e4ac;  return &g_e4ac;
    case 14u: *n = (uint32_t)sizeof g_bd14;  return &g_bd14;
    case 15u: *n = 1u;                       return &g_e4a7;
    case 16u: *n = 1u;                       return &g_e69b;
    case 17u: *n = 1u;                       return &g_e69f;
    case 18u: *n = 1u;                       return &g_e6a1;
    case 19u: *n = (uint32_t)sizeof g_e6a8;  return g_e6a8;
    case 20u: *n = 1u;                       return &g_e6c5;
    case 21u: *n = 1u;                       return &g_e6ee;
    case 22u: *n = 1u;                       return &g_e6ef;
    /* r04s: the pinned sub-objects and their cells */
    case 23u: *n = (uint32_t)sizeof g_sub;   return g_sub;
    case 24u: *n = (uint32_t)sizeof g_pr58;  return g_pr58;
    case 25u: *n = (uint32_t)sizeof g_pr5c;  return g_pr5c;
    case 26u: *n = (uint32_t)sizeof g_pr60;  return g_pr60;
    case 27u: *n = (uint32_t)sizeof g_e6c1;  return &g_e6c1;
    default:  *n = 0u;                       return 0;
    }
}
#endif

#endif /* !TCU_RAM_APP */

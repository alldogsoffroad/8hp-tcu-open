/* gs_fill.c — the READ-ONLY garage-engagement shadow's SECOND INSTANCE of
 * the stock element fill machinery and actuator law: app/rq_fill.c (the
 * port of 0xE3710..0xE566C, 0x107C3E, the interpolators) compiled once more,
 * every request object and cell redirected to the shadow's own RAM
 * (gs_fill.h GS_OBJ_*, gs_fill_cells), every exported name renamed (gsf_*),
 * so that no instruction of this translation unit can address a live cell.
 * The live instance (app/rq_fill.c, linked once as itself) is unchanged:
 * the three hooks it gained are empty preprocessor branches there.
 *
 * Hooks taken here (rq_fill.c):
 *   RQF_FL_EXTRA_LOOKUP  the shadow's own flash copies (app/gs_cal.c)
 *   RQF_SHIFT_SRC_HOOK   0xE40DA cases 1/3/6/8/10 (the garage sources):
 *     +0x4C = 0xBA8EC(E61A) = BD14+2 (the shadow's shift kind, 3 engage)
 *       [O bytes 0xE4164..0xE416A, 0xBA8EC = movu.b @BD16];
 *     +0x4D = 0xBA90A(E61A) = 0xEB118(BD14) — NOT DECODED: STAND-IN 0
 *       (witness gsf_shift_n); with +0x4C != 0 the fill-time term 0xE469C
 *       is not evaluated (rq_fill.c e47ca counts it in unp) — the decode's
 *       "garage fill-time term" is therefore 0 in the shadow [stand-in];
 *     +0x5B |= 0xE4E38 (= the 0xB8054 / B0FE flag, as rq_fill.c's 0xB/0xC case);
 *     +0x4C == 0 → fault 0x19F (as the stock's 0xE43xx report).
 */
#define RQF_SHADOW_INSTANCE 1
#include "app/gs_fill.h"
#include "app/gs_cal.h"

#define RQ_OBJ_A    GS_OBJ_A
#define RQ_OBJ_B    GS_OBJ_B
#define RQ_OBJ_C    GS_OBJ_C
#define RQ_OBJ_D    GS_OBJ_D
#define RQ_OBJ_E    GS_OBJ_E
#define RQ_OBJ_SYS  GS_OBJ_SYS
#define RQ_OBJ_WK   GS_OBJ_WK

/* the shadow's fill cells (.gs_obj) */
#if defined(__sh__) && !defined(TCU_RAM_APP)
volatile gs_fill_cells_t gs_fill_cells __attribute__((section(".gs_obj")));
#else
volatile gs_fill_cells_t gs_fill_cells;
#endif
#define RQF_CELL_851A2  (gs_fill_cells.c851a2)
#define RQF_CELL_851A3  (gs_fill_cells.c851a3)
#define RQF_CELL_851A5  (gs_fill_cells.c851a5)
#define RQF_CELL_E384   (gs_fill_cells.e384)

/* every exported name of rq_fill.c, renamed */
#define rqf_bind        gsf_bind
#define rqf_fl8         gsf_fl8
#define rqf_fl16        gsf_fl16
#define rqf_fl32        gsf_fl32
#define rqf_cell_ptr    gsf_cell_ptr
#define rqf_construct   gsf_construct
#define rqf_e4e5c       gsf_e4e5c
#define rqf_e5602       gsf_e5602
#define rqf_e5624       gsf_e5624
#define rqf_e566c       gsf_e566c
#define rqf_107c3e      gsf_107c3e
#define rqf_b6eae       gsf_b6eae
#define rqf_b6e10       gsf_b6e10
#define rqf_b6d6c       gsf_b6d6c
#define rqf_b7164       gsf_b7164
#define rqf_rec_frac    gsf_rec_frac

#define RQF_FL_EXTRA_LOOKUP(a, n)   gs_cal_lookup((a), (n))

static void gsf_shift_src(uint32_t o);
#define RQF_SHIFT_SRC_HOOK(o)       gsf_shift_src(o)

#include "app/rq_fill.c"

#if !defined(TCU_RAM_APP)

static rqf_in_t s_gin;
static volatile uint32_t s_gunp;
static volatile uint32_t s_gf19f;
static volatile uint32_t s_gshift;
static uint8_t s_bd16;

void gsf_bind_in(const gs_fill_in_t *in)
{
    s_gin.temp = in->temp;
    s_gin.c1c = in->c1c;
    s_gin.c0a = in->c0a;
    s_gin.eng_stop = in->eng_stop;
    s_gin.c3d = in->c3d;
    s_gin.cd2 = in->cd2;
    s_gin.e5c2 = in->e5c2;
    s_gin.b0fe = in->b0fe;
    s_gin.e3ed = in->e3ed;
    s_gin.ba79c = in->ba79c;
    s_gin.e6a3 = in->e6a3;
    s_gin.rec1fd = in->rec1fd;
    s_gin.lp = in->lp;
    s_gin.lp2 = in->lp2;
    for (unsigned i = 0u; i < 5u; i++) {
        s_gin.adapt38[i] = in->adapt38[i];
        s_gin.adapt42[i] = in->adapt42[i];
        s_gin.ba7dc[i] = in->ba7dc[i];
        s_gin.v10cadc[i] = in->v10cadc[i];
        s_gin.k4b[i] = in->k4b[i];
        s_gin.rec_frac[i] = in->rec_frac[i];
    }
    gsf_bind(&s_gin, &s_gunp, &s_gf19f);
}

void gsf_set_bd16(uint8_t v) { s_bd16 = v; }

static void gsf_shift_src(uint32_t o)
{
    s_gshift++;
    RQF_U8(o, RO_4C) = s_bd16;                  /* 0xBA8EC(E61A) = BD14+2 */
    RQF_U8(o, RO_4D) = 0u;                      /* 0xBA90A → 0xEB118: stand-in 0 */
    RQF_U8(o, RO_5B) = (uint8_t)(RQF_U8(o, RO_5B) | (s_in->b0fe != 0u ? 1u : 0u));
    if (RQF_U8(o, RO_4C) == 0u) {
        f19f();                                 /* 0x19F (0xB, 0xB, 0xB) */
    }
}

/* 0xE4C18(obj, p2) [O bytes 0xE4C18..0xE4C86]: p2 ∉ {0,1} → fault 0x19F, 0;
 * else (+0x46 == 10000) ? 0xE4CCC − kiss (− 0xE4C8E when p2 == 0) : 0 →
 * 0xB785C */
uint16_t gsf_e4c18(uint32_t o, uint8_t p2)
{
    int32_t v = 0;
    if (p2 != 0u && p2 != 1u) {
        f19f();
    } else if (e4dba(o)) {
        v = (int32_t)e4ccc(o) - (int32_t)RQF_U16(o, RO_PKISS);
        if (p2 == 0u) {
            v -= (int32_t)e4c8e(o);
        }
    }
    return b785c(v);
}

/* 0xE4BEC [O decompile]: state 3 ? +0x2C / 10 : 0 */
uint16_t gsf_e4bec(uint32_t o)
{
    return (RQF_U8(o, RO_ST) == RQF_ST_APPLIED) ? (uint16_t)(RQF_U16(o, RO_T) / 10u) : (uint16_t)0u;
}

bool gsf_e4dba(uint32_t o) { return e4dba(o); }

uint32_t gsf_unp_n(void)   { return s_gunp; }
uint32_t gsf_f19f_n(void)  { return s_gf19f; }
uint32_t gsf_shift_n(void) { return s_gshift; }

#endif /* !TCU_RAM_APP */

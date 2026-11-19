/* gs_fill.h — the READ-ONLY garage-engagement shadow's own instance of the
 * element fill machinery and actuator law (app/gs_fill.c compiles
 * app/rq_fill.c a second time against SHADOW request objects and cells).
 * Shadow-internal: app/gs_shadow.c and the host tests include it; an
 * output path may not (TCU_OUTPUT_PATH refuses it). */
#ifndef TCU_APP_GS_FILL_H
#define TCU_APP_GS_FILL_H

#if defined(TCU_OUTPUT_PATH) && !defined(RQF_SHADOW_INSTANCE)
#error "the garage shadow is read-only: an output path may not include gs_fill.h"
#endif

#include <stdint.h>
#include <stdbool.h>

/* the shadow request objects: OUR RAM (linker.ld .gs_req), 0x60 bytes each,
 * laid out exactly as the stock objects 0xFFF8B7C8 + 0x60·n (rq_fill.h RO_*),
 * so the second instance runs the stock bytes' code unchanged */
#define GS_REQ_BASE     0xFFF911A0u
#define GS_REQ_SIZE     0x2A0u
#define GS_OBJ_A        (GS_REQ_BASE + 0x000u)
#define GS_OBJ_B        (GS_REQ_BASE + 0x060u)
#define GS_OBJ_C        (GS_REQ_BASE + 0x0C0u)
#define GS_OBJ_D        (GS_REQ_BASE + 0x120u)
#define GS_OBJ_E        (GS_REQ_BASE + 0x180u)
#define GS_OBJ_SYS      (GS_REQ_BASE + 0x1E0u)
#define GS_OBJ_WK       (GS_REQ_BASE + 0x240u)

/* THE ENGINE TEMPLATE'S OBJECT BINDING (G6-0, 2026-11-19, ~/tcu_sweep/
 * g6_0_port.md). app/gs_engine.c (the garage engine) and app/gs_fill.c (its
 * fill instance) address the request objects ONLY through GSE_OBJ_*; the
 * instance-1 binding (app/gs_shadow.c, app/gs_fill.c compiled as
 * themselves) leaves GSE_REQ_BASE undefined here and gets the shadow's
 * objects above, value for value. A second binding (app/gs_cand_bind.h)
 * defines GSE_REQ_BASE to its own block before including. */
#ifndef GSE_REQ_BASE
#  define GSE_REQ_BASE  GS_REQ_BASE
#endif
#define GSE_OBJ_A       (GSE_REQ_BASE + 0x000u)
#define GSE_OBJ_B       (GSE_REQ_BASE + 0x060u)
#define GSE_OBJ_C       (GSE_REQ_BASE + 0x0C0u)
#define GSE_OBJ_D       (GSE_REQ_BASE + 0x120u)
#define GSE_OBJ_E       (GSE_REQ_BASE + 0x180u)
#define GSE_OBJ_SYS     (GSE_REQ_BASE + 0x1E0u)
#define GSE_OBJ_WK      (GSE_REQ_BASE + 0x240u)

/* the fill inputs (mirror of rq_fill.h rqf_in_t, without its header) */
typedef struct {
    uint8_t  temp, c1c, c0a, eng_stop, c3d, cd2, e5c2, b0fe, e3ed, ba79c, e6a3, rec1fd;
    uint16_t lp, lp2;
    int16_t  adapt38[5];
    uint16_t adapt42[5];
    int16_t  ba7dc[5];
    uint16_t v10cadc[5];
    uint8_t  k4b[5];
    uint16_t rec_frac[5];
} gs_fill_in_t;

/* the shadow's fill cells (0x851A2/3/5, 0xFFF8E384 in the stock) */
typedef struct {
    uint8_t c851a2, c851a3, c851a5, e384;
} gs_fill_cells_t;
extern volatile gs_fill_cells_t gs_fill_cells;

void     gsf_bind_in(const gs_fill_in_t *in);
void     gsf_construct(uint32_t o, uint32_t cal_cell, uint8_t elem);    /* 0xE3710 */
void     gsf_e4e5c(uint32_t o);                                         /* 0xE4E5C */
void     gsf_e5602(void);
void     gsf_e5624(void);
void     gsf_e566c(void);
uint16_t gsf_107c3e(uint8_t id, uint8_t type, uint16_t p, uint16_t imin, uint16_t imax);
uint8_t  gsf_fl8(uint32_t a);
uint16_t gsf_fl16(uint32_t a);
uint32_t gsf_fl32(uint32_t a);
uint16_t gsf_b6eae(uint32_t map, uint16_t x);
int32_t  gsf_b6e10(uint32_t map, int16_t x);
uint8_t  gsf_b6d6c(uint32_t map, uint8_t x);
/* r04s: 0xBA7DC → 0x10CFB6(0xFFF8D1B4, e) — the engine's (renamed per
 * binding, app/gs_cand_bind.h): D1B4's +0x3A while D1B4's element is e */
int16_t  gs_shadow_cfb6(uint8_t elem);
int32_t  gsf_b7164(uint32_t map, int16_t x, int16_t y);
uint16_t gsf_e4c18(uint32_t o, uint8_t p2);     /* 0xE4C18 (via 0xB8356 / 0x1147C8) */
uint16_t gsf_e4bec(uint32_t o);                 /* 0xE4BEC (via 0xB836E) */
bool     gsf_e4dba(uint32_t o);                 /* vtable +0x38: +0x46 == 10000 */
/* the engagement's shift kind BD14+2 as the shadow's shift object holds it
 * (0xBA8EC, read by 0xE40DA for sources 1/3/6/8/10) */
void     gsf_set_bd16(uint8_t v);
/* the shadow's power flow 0x89C30 and 0x89C2F (0xE47CA's term gate) */
void     gsf_set_pf(uint8_t pf, uint8_t c2f);
#if !defined(__sh__)
void     gsf_host_e469c_off(bool off);          /* host negative test only */
#endif
/* counters of THIS instance (never the live ones) */
uint32_t gsf_unp_n(void);       /* unported branches reached */
uint32_t gsf_f19f_n(void);      /* stock fault 0x19F reports */
uint32_t gsf_shift_n(void);     /* the 0xE40DA shift-source stand-in taken */
uint32_t gsf_e469c_n(void);     /* the garage fill-time term evaluated nonzero */

#endif /* TCU_APP_GS_FILL_H */

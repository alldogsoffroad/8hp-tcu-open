/* gs_cand.h — the API of INSTANCE 2 of the garage engine template (the G6a
 * CANDIDATE, app/gs_cand.c; ~/tcu_sweep/g6_0_port.md, g6a_port.md).
 * The same functions as app/gs_shadow.h's, renamed gs_cand_*; the types are
 * gs_shadow.h's. */
#ifndef TCU_APP_GS_CAND_H
#define TCU_APP_GS_CAND_H

#if defined(TCU_OUTPUT_PATH)
#error "the garage shadow is read-only: an output path may not include gs_cand.h"
#endif
#if defined(TCU_RAM_APP) || defined(TCU_LOADER)
#error "gs_cand: the G6a candidate is for the code-flash application only (not the RAM application or the loader)"
#endif

#include "app/gs_shadow.h"

void gs_cand_init(void);
void gs_cand_tick_10ms(const gs_in_t *in);
const volatile gs_rel_t *gs_cand_rel(void);
uint8_t  gs_cand_releasing(void);
const volatile gs_wit_t *gs_cand_wit(void);
const volatile gs_rec_t *gs_cand_rec(uint8_t slot, uint8_t i);
uint16_t gs_cand_cur(uint8_t ch);
uint8_t  gs_cand_pf(void);
int8_t   gs_cand_gear(void);
uint8_t  gs_cand_phase(uint8_t k);
uint8_t  gs_cand_pending(void);
uint8_t  gs_cand_e6ef(void);
uint8_t  gs_cand_ae28(void);
uint8_t  gs_cand_d0f(void);
uint8_t  gs_cand_reqmode(uint8_t ch);
uint8_t  gs_cand_fillst(uint8_t ch);
/* G6a (the SH form, app/gs_cand_bind.h GSC_SH_FORM): the boot-state-1 pass
 * at its own slot, and the live-AE28 cross-check counter */
void     gs_cand_boot_pass(const gs_in_t *in);
uint32_t gs_cand_ae28_x(void);
int16_t  gs_cand_bd76(void);
uint8_t  gs_cand_bd30(void);
uint8_t  gs_cand_sync(void);
int16_t  gs_cand_slip(void);
uint8_t  gs_cand_host_ceca(uint8_t cls, uint8_t eng, uint8_t rel, uint8_t f7, int16_t calc_p,
                           uint8_t ceca_cls);
void     gs_cand_host_rel(uint8_t k, int16_t v[4]);
void     gs_cand_host_standins(uint8_t mask, int16_t bd76, uint8_t bd30);
void     gs_cand_host_sub(uint8_t k, int16_t v[6]);
uint8_t  gs_cand_host_r04s(uint8_t k, uint8_t v[12]);
const volatile void *gs_cand_host_state(uint8_t k, uint32_t *n);
/* the candidate fill instance's own counters (app/gs_cand_fill.c) */
uint32_t gcf_unp_n(void);
uint32_t gcf_f19f_n(void);
uint32_t gcf_shift_n(void);
uint32_t gcf_e469c_n(void);
uint16_t gcf_rec_frac(uint8_t idx);

#endif /* TCU_APP_GS_CAND_H */

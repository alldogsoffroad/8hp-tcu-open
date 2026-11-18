/* drv_tsel.h — the SELECTED ECU TEMPERATURE made live (stage 2a, 2026-11-18;
 * ~/tcu_sweep/diag_temp_shadow_port.md §7, ~/tcu_sweep/temp_stage2a_port.md).
 * Output path: it may not name the read-only temperature shadow — the values
 * arrive BY VALUE (drv_tsel_val_t, filled by app/tsel_live.c from the
 * shadow's results of this hub pass), the drv_pa7 pattern.
 *
 * The cells (the stock writers [O bytes, the shadow's port]):
 *   0x88DA8 → 0x8DD7C  0x9393E (board, 1/16 °C), 0x94100 (its status),
 *                      0x94101 (its byte)
 *   0x88BD0 → 0x8DB5A  0x93ABA (die, 1/16 °C), 0x9415E (its status)
 *   0x88BD0            0x9393C / 0x940FD (the die copies), 0x940FE (die byte),
 *                      0x9393A / 0x940FC (THE SELECTED TEMPERATURE / status),
 *                      0x94170 (source), 0x940FF (its byte)
 * NOT written (stage 2b, separate): the fault flags 0x940E1 / E2 / E4 / E5,
 * the fault word 0x93916, the countdown words 0x9477A / 0x9477C / 0x94823,
 * the producers' internal cells 0x93AB2..C3 / 0x9415C..D / 0x94730. */
#ifndef DRV_TSEL_H
#define DRV_TSEL_H
#include <stdint.h>

/* TSEL_LIVE: 1 = the temperature hub hook writes the shadow's results into
 * the live cells (SH application only); 0 = the shadow stays read-only
 * (host tests by default, RAM app, loader). A host test may build with
 * -DTSEL_LIVE=1 to run the live semantics. */
#if !defined(TSEL_LIVE)
#if defined(__sh__) && !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
#define TSEL_LIVE 1
#else
#define TSEL_LIVE 0
#endif
#endif
#if TSEL_LIVE && (defined(TCU_RAM_APP) || defined(TCU_LOADER))
#error "TSEL_LIVE is for the code-flash application only"
#endif

typedef struct {
    int16_t t_sel;      /* 0x9393A the selected ECU temperature (1/16 °C) */
    int16_t t_die;      /* 0x93ABA the die temperature */
    int16_t t_die_cp;   /* 0x9393C the selection's die copy */
    int16_t t_brd;      /* 0x9393E the board temperature */
    uint8_t st_sel;     /* 0x940FC its status */
    uint8_t src;        /* 0x94170 its source (1 fluid, 2 die, 4 board, 0 default) */
    uint8_t b_sel;      /* 0x940FF the selected byte */
    uint8_t b_die;      /* 0x940FE the die byte */
    uint8_t b_brd;      /* 0x94101 the board byte */
    uint8_t st_die;     /* 0x9415E die status */
    uint8_t st_die_cp;  /* 0x940FD the selection's die status copy */
    uint8_t st_brd;     /* 0x94100 board status */
} drv_tsel_val_t;

/* write the values into the stock cells, in the stock's hub order (board
 * 0x88DA8 first, then die + selection 0x88BD0) */
void drv_tsel_apply(drv_tsel_val_t v);
#endif

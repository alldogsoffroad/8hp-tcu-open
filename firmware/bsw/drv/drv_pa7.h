/* drv_pa7.h — the PA7 "TRS Park Signal" output (stock 0x90CF6), live
 * (2026-11-18, r04e; ~/tcu_sweep/pa7_decode.md). Output path: it may not
 * name the read-only PA7 shadow. */
#ifndef DRV_PA7_H
#define DRV_PA7_H
#include <stdint.h>

/* PA7_LIVE: 1 = the PA7 hook drives the real pin from the shadow's 0x8CE48
 * result (SH application only); 0 = the shadow stays read-only (host tests,
 * RAM app, loader). The C9 bit-7 request (0x9095A) is NOT made live here. */
#if !defined(PA7_LIVE)
#if defined(__sh__) && !defined(TCU_RAM_APP) && !defined(TCU_LOADER)
#define PA7_LIVE 1
#else
#define PA7_LIVE 0
#endif
#endif

/* 0x90CF6(v): 0xFFF90920 = v; v != 0 → bset.b #7,@(3,0xFFFE3800), else bclr */
void drv_pa7_apply(uint8_t v);
#endif

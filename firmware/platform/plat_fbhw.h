/* plat_fbhw.h — the register accessors of the READ-ONLY PWM-feedback shadow
 * (app/fb_shadow.c; ~/tcu_sweep/feedback_shadow_port.md).
 *
 * On SH they are inline volatile accesses at the stock's addresses, with the
 * stock's access widths (0x97648, 0x90844, 0x90A72):
 *   Timer F subblock n at 0xFFFFFA80 + 0x20n: TSRF +3 (byte, ICFF bit 0),
 *     ECNTBF +8 (16), GRBF +0xA (16), ECNTCF +0xC, GRAF +0x10, GRCF +0x18
 *     (32-bit, bits 7:0 fraction)
 *   PHPR 0xFFFFC852, PFPR 0xFFFFC822 (16-bit pin levels)
 *   Timer D0: TCNT1D0 0xFFFFF520 (32-bit, bits 31:8 count at 1 MHz),
 *     OCRD00 0xFFFFF590 (32-bit), TSRD0 low byte 0xFFFFF58D (CMFAD00 bit 4,
 *     cleared by a byte bclr as 0x90A7A does) — BYTE accesses only: the
 *     walk timer owns 0xF58C / 0xF58E / 0xF585
 *   TCNTA 0xFFFFF220 (the FRT, 1 µs) for our ISR-duration witness.
 * On the host they are the Timer F / Timer D model (tests/unit/fb_model.c).
 *
 * Nothing here writes an output: Timer F channels are inputs; OCRD00 has
 * OSSD00 = 00 (no pin output). The arming (TIOR1D0, IPR10, TIERD0) is
 * plat_fbsamp_start() — application only, called once by fb_shadow_start(). */
#ifndef TCU_PLATFORM_PLAT_FBHW_H
#define TCU_PLATFORM_PLAT_FBHW_H

#include <stdint.h>
#include "platform/platform.h"

#if defined(__sh__)
#define FBHW_TF(n)      (0xFFFFFA80u + 0x20u * (uint32_t)(n))
#define FBHW_R8(a)      (*(volatile uint8_t  *)(uintptr_t)(a))
#define FBHW_R16(a)     (*(volatile uint16_t *)(uintptr_t)(a))
#define FBHW_R32(a)     (*(volatile uint32_t *)(uintptr_t)(a))

static inline uint8_t  fbhw_tsrf(uint8_t n)   { return FBHW_R8(FBHW_TF(n) + 3u); }
static inline void     fbhw_icff_clear(uint8_t n)
{
    FBHW_R8(FBHW_TF(n) + 3u) = (uint8_t)(FBHW_R8(FBHW_TF(n) + 3u) & 0xFEu);   /* bclr.b #0 */
}
static inline uint32_t fbhw_grcf(uint8_t n)   { return FBHW_R32(FBHW_TF(n) + 0x18u); }
static inline uint32_t fbhw_graf(uint8_t n)   { return FBHW_R32(FBHW_TF(n) + 0x10u); }
static inline uint32_t fbhw_ecntcf(uint8_t n) { return FBHW_R32(FBHW_TF(n) + 0x0Cu); }
static inline void     fbhw_grbf_set(uint8_t n, uint16_t v)   { FBHW_R16(FBHW_TF(n) + 0x0Au) = v; }
static inline void     fbhw_ecntbf_set(uint8_t n, uint16_t v) { FBHW_R16(FBHW_TF(n) + 0x08u) = v; }
static inline uint16_t fbhw_phpr(void)        { return FBHW_R16(0xFFFFC852u); }
static inline uint16_t fbhw_pfpr(void)        { return FBHW_R16(0xFFFFC822u); }
static inline uint32_t fbhw_tcnt1d0(void)     { return FBHW_R32(0xFFFFF520u); }
static inline void     fbhw_ocrd00_set(uint32_t v) { FBHW_R32(0xFFFFF590u) = v; }
static inline void     fbhw_cmfad00_clear(void)
{
    FBHW_R8(0xFFFFF58Du) = (uint8_t)(FBHW_R8(0xFFFFF58Du) & 0xEFu);   /* bclr.b #4,@(13,r14) */
}
static inline uint32_t fbhw_frt_us(void)      { return FBHW_R32(0xFFFFF220u); }
#else
uint8_t  fbhw_tsrf(uint8_t n);
void     fbhw_icff_clear(uint8_t n);
uint32_t fbhw_grcf(uint8_t n);
uint32_t fbhw_graf(uint8_t n);
uint32_t fbhw_ecntcf(uint8_t n);
void     fbhw_grbf_set(uint8_t n, uint16_t v);
void     fbhw_ecntbf_set(uint8_t n, uint16_t v);
uint16_t fbhw_phpr(void);
uint16_t fbhw_pfpr(void);
uint32_t fbhw_tcnt1d0(void);
void     fbhw_ocrd00_set(uint32_t v);
void     fbhw_cmfad00_clear(void);
uint32_t fbhw_frt_us(void);
#endif

/* the stock's lock 0x9665C(0x60) / 0x96674: SR.I raised to at least 6 */
static inline uint32_t fbhw_lock6(void)
{
    const uint32_t lk = plat_sr_i();
    if (lk < 6u) {
        plat_sr_set(6u);
    }
    return lk;
}
static inline void fbhw_unlock(uint32_t lk) { plat_sr_set(lk); }

/* arm the Timer-D0 CMID00 window sampler (vector 216): TIOR1D0 IOAD00 = 11
 * (byte 0xFFFFF581), OCRD00 = first << 8, clear CMFAD00, IPR10 bits 15-12
 * = 6, then TIERD0 CMEAD00 (byte 0xFFFFF58F |= 0x10) — under SR.I = 15.
 * The vector is wired in the ROM table (header.c) at link time. */
void plat_fbsamp_start(uint32_t first);

#endif /* TCU_PLATFORM_PLAT_FBHW_H */

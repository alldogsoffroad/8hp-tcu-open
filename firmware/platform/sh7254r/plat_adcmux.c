/* plat_adcmux.c — L0-sense (2026-11-12): the stock's AN0 external-multiplexer
 * acquisition, ported from the flash bytes of the September image
 * (sh-elf-objdump -b binary -m sh2a -EB on slices; NOT the Ghidra text).
 *
 * Stock routines, in stock call order:
 *   0x96010  DMAC / A-DMAC global reset + enable      (init slot 14, 0x8EEDC)
 *   0x97744  Timer G init (inside ATU init 0x91FA4)   (init slot 14)
 *   0x8F9EC  ADC chain init                           (init batch 0x8EF1A, 1st)
 *     0x967F4  ADC_A/ADC_B control + DMAC5/DMAC4 (0x969E8, descs 0x40E8C/0x40EAC)
 *     0x96900  A-DMAC ch0 (Timer G2 -> PGDR[15:8]) / ch1 (ADR0 -> ring)
 *     0x9696A  mux sequence copy ROM 0x40ECC -> RAM
 *     0x936DC  x6 smoothing-descriptor init
 *     0x95F68 / 0x95F7A  CHCR5/4 IE + ADCSR0/1 TRGE
 *     Timer G1/G2/G4/G5 phase + period registers
 *   0x95FC4  DEI5 ISR (vector 128, ADC_A scan end, 1 ms) -> 0x8F058 (1 ms)
 *   0x95FEA  DEI4 ISR (vector 124, ADC_B, every 2 scans)
 *   0x8F62C  5 ms finisher (0x936FC x6, mux-13 envelope, 0x907FC)
 *   0x8F5A0  5 ms (J2-stage task 0x8EDF6): descriptor status ramp (part)
 *
 * Decode: ~/tcu_sweep/adc_chain_port.md (subagent; integrated and reviewed
 * in the main session). App-only: never linked into the loader, compiled
 * out of the RAM application (its layout does not reserve these ranges).
 *
 * DEVIATIONS (each deliberate, see adc_chain_port.md §3):
 *   D1  the 80-byte mux sequence lives at 0xFFF88500 (alias offset 0x0500),
 *       not 0xFFF8844C: ours keeps .sup_stage2 (W1/W2/fault-twin staging)
 *       at 0xFFF88450..0x8847F — the stock copy would overwrite it.
 *   D2  0x8F058 is ported only through the mux 13/11/8/10/14 loops
 *       (0x8F058..0x8F1B4); the remaining accumulators (mux 0-7/9/15, the
 *       ADC_A/ADC_B direct-scan sums, the 0x071D DMA-alive sentinels) are
 *       not ported (no consumer in L0).
 *   D3  0x8F5A0: only the status ramp of 0x9002F/0x90017/0x9001F; the
 *       20-call span check's fault-561 report is not ported (no fault
 *       manager) — the span cell 0x910FC is still measured by the ISR.
 *   D4  the profiling hooks 0x68F5C/0x68F66 around 0x8F058 are not ported.
 *   D5  before 0x967F4 our own continuous scan (adc_init, shared plat_init)
 *       is stopped and ADSCACT waited out — the manual forbids changing
 *       ADCS/ADANS while ADSCACT = 1, and the stock never meets a running
 *       continuous scan here.
 *   D6  cal bytes read by the stock from flash are constants here
 *       (0x40870 = 00 -> 0x40D8E[0] = 00; 0x1918B0/B1 = 0A 0A;
 *       0x191484 = 0x0032, 0x191486 = 0x07D0) [O bytes].
 *   D7  IPR20 (0x97744 writes (x & 0xFF) | 0x7700) is zeroed again by
 *       plat_intc_setup; we leave it 0 (CMIG0..5 never reach the CPU).
 */
#include <stdint.h>
#include "platform/platform.h"

#if defined(TCU_RAM_APP)
/* the RAM application's layout does not reserve the stock DMA/ring ranges:
 * the chain is never armed there (empty bodies; vectors 124/128 stay idle) */
void plat_adcmux_dma_global_init(void) {}
void plat_adcmux_timer_g_init(void) {}
void plat_adcmux_init(void) {}

void plat_adcmux_finish_5ms(void) {}
void plat_adcmux_status_5ms(void) {}
void plat_dei5_isr(void) {}
void plat_dei4_isr(void) {}
void plat_adcmux_mux_hold(void) {}
void plat_adcmux_mux_release(void) {}
#else

#define R8(a)  (*(volatile uint8_t  *)(uintptr_t)(a))
#define R16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))

/* ---- registers [O manual tables] ------------------------------------- */
#define ADC_A      0xFFFFE800u        /* ADCSR0 +0, ADREF0 +2, ADTRE0 +4, ADTRF0 +6,
                                         ADTRS0 +8, ADTRD0 +0xC, ADADS0 +0x1C,
                                         ADADC0 +0x1E, ADANS0 +0x20, ADANS1 +0x22,
                                         ADCER0 +0x30, ADR0 +0x40               */
#define ADC_B      0xFFFFEC00u        /* ADCSR1 +0, ADREF1 +2, ADTRE1 +0x10,
                                         ADTRF1 +0x12, ADTRS1 +0x14, ADTRD1 +0x18,
                                         ADADS1 +0x1C, ADADC1 +0x1E, ADANS3 +0x20,
                                         ADCER1 +0x30                            */
#define DMAC       0xFFFE1000u        /* ch n: SAR +0x10n, DAR +4, DMATCR +8, CHCR +0xC;
                                         CHFR +0x8C, TEMSK +0x8E; RSAR/RDAR/RDMATCR
                                         +0x100..; ARCR +0x10C, RARCR +0x10E       */
#define DMAOR      0xFFFE1200u
#define DMAFR      0xFFFE1204u
#define DMARS(n)   (0xFFFE1300u + 4u * (n))
#define ADMA       0xFFFE6000u        /* ADMAOR +0, ADMAABR +2, ADMATE0 +0x30,
                                         ADMADE0 +0x40, ADMATCR0 +0x60,
                                         ADMARTCR0 +0x62, ADMATCR1 +0x64,
                                         ADMARTCR1 +0x66, ADMAAR0 +0x90,
                                         ADMARAR0 +0x92, ADMAAR1 +0x94,
                                         ADMARAR1 +0x96                          */
#define TMRG       0xFFFFFE00u        /* TSTRG +1; G n: TCRG +0x80+0x10n,
                                         TSRG +0x81, TCNTG +0x84, OCRG +0x86     */
#define TCNTA      0xFFFFF220u
#define IPR04      0xFFFE0C02u
#define IPR20      0xFFFE0C22u

/* ---- RAM: stock addresses, alias base ADMAABR = 1 -> 0xFFF88000 ------- */
#define RAM_ADCA_BUF   0xFFF88130u    /* DMAC5 dest, 29 words (ADRD0, ADR0..27) */
#define RAM_ADCB_BUF   0xFFF884BAu    /* DMAC4 dest, 2 x 8 words              */
#define RAM_MUX_SEQ    0xFFF88500u    /* D1: stock 0xFFF8844C                  */
#define RAM_MUX_RING   0xFFF885E8u    /* A-DMAC ch1, 2 x 80 words             */
#define RAM_GUARD_A    0xFFF8816Au
#define RAM_GUARD_RING 0xFFF88728u
#define RAM_GUARD_B    0xFFF884DAu
#define ADMA_ALIAS_SEQ  0x0500u       /* D1: stock 0x044C */
#define ADMA_ALIAS_RING 0x05E8u

/* outputs / working cells (stock addresses) */
#define C_SPAN_US      0xFFF910FCu    /* u32: TCNTA span of the last 20 ISR calls */
#define C_SPAN_STAMP   0xFFF91100u    /* u32 */
#define C_SPAN_CNT     0xFFF91104u    /* u16 */
#define C_MS_CNT       0xFFF90628u    /* u32: +1 per ISR (also the TCNT2H copy) */
#define C_MEAN8_RAW    0xFFF90184u    /* u16 raw 1 ms mean, mux 8 */
#define C_MEAN10_RAW   0xFFF90186u    /* u16 raw 1 ms mean, mux 10 */
#define C_MUX8_MV      0xFFF90188u    /* u16 mV: output node (ladder 0x94ED0[0]) */
#define C_MUX10_MV     0xFFF9018Au
#define C_PREV13       0xFFF907DCu    /* u16 raw: previous mux-13 1 ms mean */
#define C_MUX11_MV     0xFFF907E2u    /* u16 mV: supply (ladder 0x94F86) */
#define C_MUX13_MV     0xFFF907E4u    /* u16 mV: mux 13 mean */
#define C_MUX13_PREV   0xFFF907E6u
#define C_MUX14_MV     0xFFF907EEu
#define C_ENV13        0xFFF907F6u    /* u16: ripple envelope */
#define C_RIP13        0xFFF907F8u    /* u16: ripple (MAD + |delta|) mean */
#define C_MIN13_RAW    0xFFF907FAu    /* u16 raw running minimum (reset 5 ms) */
#define C_MIN13_MV     0xFFF907FCu    /* u16 mV: 5 ms minimum of mux 13 */

/* ---- the smoothing descriptors (ROM 0x40874..0x40903) [O bytes] -------- */
typedef struct {
    uint32_t sum;        /* +0 */
    uint8_t  count;      /* +4 */
    uint8_t  len;        /* +5 */
    uint8_t  idx;        /* +6 */
    uint8_t  status;     /* +7 */
} adc_state_t;

typedef struct {
    uintptr_t state;     /* +0  adc_state_t*            */
    float     scale;     /* +4                          */
    uintptr_t out;       /* +8  u16 output              */
    uintptr_t ring;      /* +12 u16[len]                */
    uint8_t   cal;       /* +16 -> *(u8*)cal (D6 value) */
    uint8_t   size;      /* +20                         */
} adc_desc_t;

#define SCALE_25V  0.3814697265625f      /* 0x3EC35000 = 25000/65536 */
#define SCALE_35V  0.5340576171875f      /* 0x3F08B800 = 35000/65536 */
#define SCALE_M14  0.37288665771484375f  /* 0x3EBEEB00 */

static const adc_desc_t D_40874 = { 0xFFF90030u, SCALE_35V, C_RIP13,    0xFFF9009Cu, 0x0Au, 0x0Au };
static const adc_desc_t D_4088C = { 0xFFF90000u, SCALE_25V, C_MUX11_MV, 0xFFF90038u, 0x0Au, 0x0Au };
static const adc_desc_t D_408A4 = { 0xFFF90008u, SCALE_35V, C_MUX13_MV, 0xFFF9004Cu, 0x0Au, 0x0Au };
static const adc_desc_t D_408BC = { 0xFFF90010u, SCALE_25V, C_MUX8_MV,  0xFFF90060u, 0x0Au, 0x0Au };
static const adc_desc_t D_408D4 = { 0xFFF90018u, SCALE_25V, C_MUX10_MV, 0xFFF90074u, 0x0Au, 0x0Au };
static const adc_desc_t D_408EC = { 0xFFF90028u, SCALE_M14, C_MUX14_MV, 0xFFF90088u, 0x0Au, 0x0Au };

#define ST(d)   ((volatile adc_state_t *)(d)->state)
#define RING(d) ((volatile uint16_t *)(d)->ring)

/* ROM 0x40ECC: the 80-byte mux sequence (address = byte >> 4) [O bytes] */
static const uint8_t s_mux_seq[80] = {
    0xC0, 0x70, 0x80, 0xB0, 0xD0, 0x90, 0x30, 0xA0, 0xE0, 0xD0,
    0x70, 0x50, 0x80, 0xB0, 0xD0, 0x10, 0xF0, 0xA0, 0xE0, 0xD0,
    0x20, 0x70, 0x80, 0xB0, 0xD0, 0x20, 0x30, 0xA0, 0xE0, 0xD0,
    0x20, 0x50, 0x80, 0xB0, 0xD0, 0x40, 0xF0, 0xA0, 0xE0, 0xD0,
    0x40, 0x70, 0x80, 0xB0, 0xD0, 0x40, 0x30, 0xA0, 0xE0, 0xD0,
    0x60, 0x50, 0x80, 0xB0, 0xD0, 0x60, 0xF0, 0xA0, 0xE0, 0xD0,
    0x60, 0x70, 0x80, 0xB0, 0xD0, 0x00, 0x30, 0xA0, 0xE0, 0xD0,
    0x00, 0x50, 0x80, 0xB0, 0xD0, 0x00, 0xF0, 0xA0, 0xE0, 0xD0,
};

/* DMAC descriptors (ROM 0x40E8C ch5 / 0x40EAC ch4) [O bytes]:
 * {ch, ARCR, RARCR, DMARS byte, SAR, DAR, DMATCR, RSAR, RDAR, RDMATCR, CHCR} */
typedef struct {
    uint8_t ch, arcr, rarcr, dmars;
    uint32_t sar, dar, tcr, rsar, rdar, rtcr, chcr;
} dmac_desc_t;
static const dmac_desc_t DM_40E8C = { 5u, 0x1Du, 0x1Du, 0u,
    0xFFFFE83Eu, 0xFFF88130u, 0x1Du, 0xFFFFE83Eu, 0xFFF88130u, 0x1Du, 0x58005C08u };
static const dmac_desc_t DM_40EAC = { 4u, 0x08u, 0x08u, 0u,
    0xFFFFEC3Eu, 0xFFF884BAu, 0x10u, 0xFFFFEC3Eu, 0xFFF884BAu, 0x10u, 0x58005D09u };

/* ---- 0x96010: DMAC / A-DMAC global ------------------------------------ */
void plat_adcmux_dma_global_init(void)
{
    R16(DMAOR) = 0u;                                   /* mov.w r4,@r7        */
    R8(ADMA) = 0u;                                     /* ADMAOR = 0          */
    R32(DMAC + 0x0Cu) = 0u; R32(DMAC + 0x1Cu) = 0u;    /* CHCR0..3 = 0        */
    R32(DMAC + 0x2Cu) = 0u; R32(DMAC + 0x3Cu) = 0u;
    R32(DMAC + 0x4Cu) = 0u; R32(DMAC + 0x5Cu) = 0u;    /* CHCR4..7 = 0        */
    R32(DMAC + 0x6Cu) = 0u; R32(DMAC + 0x7Cu) = 0u;
    if (R8(DMAFR) != 0u) {
        R8(DMAFR) = 0u;
    }
    R8(ADMA + 0x30u) = (uint8_t)(R8(ADMA + 0x30u) & 0u);   /* ADMATE0..2 */
    R8(ADMA + 0x31u) = (uint8_t)(R8(ADMA + 0x31u) & 0u);
    R8(ADMA + 0x32u) = (uint8_t)(R8(ADMA + 0x32u) & 0u);
    R8(ADMA + 0x40u) = 0u; R8(ADMA + 0x41u) = 0u; R8(ADMA + 0x42u) = 0u;   /* ADMADE */
    R8(ADMA + 0x43u) = 0u; R8(ADMA + 0x44u) = 0u; R8(ADMA + 0x45u) = 0u;
    R8(ADMA + 0x47u) = 0u;
    R16(ADMA + 0x60u) = 0u;                            /* ADMATCR0           */
    R16(ADMA + 0x64u) = 0u;                            /* ADMATCR1           */
    for (uint32_t o = 0x70u; o <= 0x8Eu; o += 2u) {    /* ADMATCR56..71      */
        R16(ADMA + o) = 0u;
    }
    R16(DMAOR) = 0x0101u;                              /* PR = 01, DME = 1   */
    R8(ADMA + 0x02u) = 1u;                             /* ADMAABR: 0xFFF88000 */
    R8(ADMA) = 1u;                                     /* ADMAOR: DME = 1    */
}

/* ---- 0x97744: Timer G (table 0x41060 = 0x12 x 6) ----------------------- */
void plat_adcmux_timer_g_init(void)
{
    for (uint32_t n = 0u; n < 6u; n++) {
        const uint32_t g = TMRG + 0x80u + 0x10u * n;
        R8(g) = 0x12u;                                 /* TCRGn: line 1, CMPOEG */
        R16(g + 4u) = 0u;                              /* TCNTGn             */
        R16(g + 6u) = 0xFFFFu;                         /* OCRGn              */
        R8(g + 1u) = (uint8_t)(R8(g + 1u) & 0u);       /* TSRGn              */
    }
    R16(IPR20) = (uint16_t)((R16(IPR20) & 0x00FFu) | 0x7700u);   /* D7 */
    R8(TMRG + 1u) = 0x3Fu;                             /* TSTRG (rts slot)   */
}

/* ---- 0x969E8: one DMAC channel from a descriptor ----------------------- */
static void dmac_load(const dmac_desc_t *d)
{
    const uint32_t b = DMAC + 0x10u * d->ch;
    R32(b + 0x0Cu) = R32(b + 0x0Cu) & ~1u;             /* DE = 0             */
    (void)R8(b + 0x8Cu);                               /* CHFR read          */
    R8(b + 0x8Cu) = 0xFEu;                             /* clear TE           */
    (void)R8(b + 0x8Cu);
    (void)R8(b + 0x8Cu);
    R8(b + 0x8Cu) = 0xEFu;                             /* clear HE           */
    (void)R8(b + 0x8Cu);
    R32(b + 0x00u) = d->sar;
    R32(b + 0x04u) = d->dar;
    R32(b + 0x08u) = d->tcr;
    R32(b + 0x100u) = d->rsar;
    R32(b + 0x104u) = d->rdar;
    R32(b + 0x108u) = d->rtcr;
    R32(b + 0x0Cu) = (d->chcr & 0xDD00FF18u) | (R32(b + 0x0Cu) & 0x00040005u);
    if (d->ch >= 4u) {
        R16(b + 0x10Cu) = d->arcr;                     /* ARCR               */
        R16(b + 0x10Eu) = d->rarcr;                    /* RARCR              */
        R16(b + 0x8Eu) = (uint16_t)(0x5B00u | (d->chcr & 1u));   /* TEMSK, key 5B */
    }
    {
        const uint32_t rs = DMARS(d->ch >> 1);
        if ((d->ch & 1u) != 0u) {
            R16(rs) = (uint16_t)((R16(rs) & 0x00FFu) | ((uint16_t)d->dmars << 8));
        } else {
            R16(rs) = (uint16_t)((R16(rs) & 0xFF00u) | d->dmars);
        }
    }
}

/* ---- 0x967F4: both ADC units + DMAC5/DMAC4 ----------------------------- */
static void adc_dmac_init(void)
{
    R8(ADC_A) = (uint8_t)(R8(ADC_A) & 0x7Du);          /* ADST, TRGE off     */
    R8(ADC_B) = (uint8_t)(R8(ADC_B) & 0x7Du);
    R16(ADC_A + 0x04u) = 0u;                           /* ADTRE0             */
    R8(ADC_B + 0x10u) = 0u;                            /* ADTRE1             */
    R8(ADC_A) = 0x10u;                                 /* ADCSR0 = ADIE      */
    R8(ADC_B) = 0x10u;                                 /* ADCSR1 = ADIE      */
    {
        const uint32_t t0 = R32(TCNTA);
        for (;;) {                                     /* <= 3 us, ADITACT   */
            if ((R8(ADC_A + 2u) & 0x40u) == 0u && (R8(ADC_B + 2u) & 0x40u) == 0u) {
                break;
            }
            if ((uint32_t)(R32(TCNTA) - t0) >= 3u) {
                break;
            }
        }
    }
    R16(ADC_A + 0x30u) = 0x4F01u;                      /* ADCER0             */
    R16(ADC_B + 0x30u) = 0x4F80u;                      /* ADCER1             */
    R32(ADC_A + 0xE0u) = 0xAA550001u;                  /* undocumented       */
    R32(ADC_B + 0xE0u) = 0xAA550001u;                  /* undocumented       */
    R16(ADC_A + 0x20u) = 0xFFFEu;                      /* ADANS0: AN1..15    */
    R16(ADC_A + 0x22u) = 0x0FFFu;                      /* ADANS1: AN16..27   */
    R8(ADC_A + 0x25u) = 0u;                            /* undocumented byte  */
    R16(ADC_B + 0x20u) = 0x007Fu;                      /* ADANS3: AN40..46   */
    R8(ADC_A + 0x02u) = (uint8_t)(R8(ADC_A + 0x02u) & 0u);   /* ADREF0     */
    R8(ADC_B + 0x02u) = (uint8_t)(R8(ADC_B + 0x02u) & 0u);   /* ADREF1     */
    R8(ADC_A + 0x1Cu) = 0u;                            /* ADADS0             */
    R8(ADC_A + 0x1Eu) = 0u;                            /* ADADC0             */
    R8(ADC_B + 0x1Cu) = 0u;                            /* ADADS1             */
    R8(ADC_B + 0x1Eu) = 0u;                            /* ADADC1             */
    R16(ADC_A + 0x08u) = 0u;                           /* ADTRS0             */
    R8(ADC_B + 0x14u) = 0u;                            /* ADTRS1             */
    R16(ADC_A + 0x0Cu) = 0u;                           /* ADTRD0             */
    R8(ADC_B + 0x18u) = 0u;                            /* ADTRD1             */
    R16(ADC_A + 0x06u) = (uint16_t)(R16(ADC_A + 0x06u) & 0u);   /* ADTRF0  */
    R8(ADC_B + 0x12u) = (uint8_t)(R8(ADC_B + 0x12u) & 0u);      /* ADTRF1  */
    dmac_load(&DM_40E8C);                              /* ch5: ADC_A         */
    dmac_load(&DM_40EAC);                              /* ch4: ADC_B         */
    R32(DMAC + 0x5Cu) = R32(DMAC + 0x5Cu) | 1u;        /* CHCR5 DE           */
    R32(DMAC + 0x4Cu) = R32(DMAC + 0x4Cu) | 1u;        /* CHCR4 DE           */
    R16(IPR04) = (uint16_t)((R16(IPR04) & 0xF0FFu) | 0x0400u);   /* DEI5 = 4  */
    R16(IPR04) = (uint16_t)((R16(IPR04) & 0x0FFFu) | 0xE000u);   /* DEI4 = E  */
}

/* ---- r02p DISCRIMINATOR: no PG12-15 edge during a bus-B burst --------
 * r02o proved the ch0 stepping corrupts bus B (deaths/AB replies gone with
 * CMEG2 off). Here the stepping runs, but plat_rspi clears CMEG2 at every
 * bus-B burst start (pump) and sets it again at burst end (SSL negated), so
 * the mux address never changes while an SSLB select is asserted. NOT a
 * stock behaviour (the stock's DMA-fed bus B overlaps the stepping freely).
 * Ring samples taken during a burst keep the previous address [I]. */
static volatile uint8_t s_mux_armed;
volatile uint32_t s_mux_hold_n;           /* diagnostic: holds performed */

void plat_adcmux_mux_hold(void)
{
    if (s_mux_armed != 0u) {
        const uint32_t lk = plat_sr_i();
        plat_sr_set(15u);
        R8(TMRG + 0xA0u) = (uint8_t)(R8(TMRG + 0xA0u) & (uint8_t)~1u);   /* CMEG2 off */
        s_mux_hold_n++;
        plat_sr_set(lk);
    }
}

void plat_adcmux_mux_release(void)
{
    if (s_mux_armed != 0u) {
        const uint32_t lk = plat_sr_i();
        plat_sr_set(15u);
        R8(TMRG + 0xA0u) = (uint8_t)(R8(TMRG + 0xA0u) | 1u);            /* CMEG2 on */
        plat_sr_set(lk);
    }
}

/* ---- 0x96900 (+ 0x9696A): A-DMAC ch0/ch1, AN0 interrupt conversion ----- */
static void adma_init(void)
{
    R16(ADC_A + 0x0Cu) = (uint16_t)(R16(ADC_A + 0x0Cu) | 1u);   /* ADIDE0 */
    for (uint32_t i = 0u; i < 80u; i++) {              /* 0x9696A            */
        R8(RAM_MUX_SEQ + i) = s_mux_seq[i];
    }
    R16(ADMA + 0x92u) = ADMA_ALIAS_SEQ;                /* ADMARAR0           */
    R16(ADMA + 0x60u) = 0u;                            /* ADMATCR0           */
    R16(ADMA + 0x90u) = ADMA_ALIAS_SEQ;                /* ADMAAR0            */
    R16(ADMA + 0x62u) = 80u;                           /* ADMARTCR0          */
    R16(ADMA + 0x60u) = 80u;                           /* ADMATCR0           */
    R16(ADMA + 0x96u) = ADMA_ALIAS_RING;               /* ADMARAR1           */
    R16(ADMA + 0x64u) = 0u;                            /* ADMATCR1           */
    R16(ADMA + 0x94u) = ADMA_ALIAS_RING;               /* ADMAAR1            */
    R16(ADMA + 0x66u) = 160u;                          /* ADMARTCR1          */
    R16(ADMA + 0x64u) = 160u;                          /* ADMATCR1           */
    R8(TMRG + 0xA1u) = (uint8_t)(R8(TMRG + 0xA1u) & (uint8_t)~1u);   /* CMFG2 */
    R8(TMRG + 0xA0u) = (uint8_t)(R8(TMRG + 0xA0u) | 1u);            /* CMEG2 */
    s_mux_armed = 1u;   /* r02p: bus-B bursts may now hold/release it */
    R16(ADC_A + 0x04u) = (uint16_t)(R16(ADC_A + 0x04u) | 1u);   /* ADTRGE0 */
}

/* ---- 0x936DC ------------------------------------------------------------ */
static uint8_t desc_init(const adc_desc_t *d)
{
    volatile adc_state_t *s = ST(d);
    uint8_t n = d->size;
    if (d->cal < d->size) {
        n = d->cal;
    }
    s->count = 0u;
    s->sum = 0u;
    s->idx = 0u;
    s->len = n;
    return n;
}

/* D5: stop OUR continuous scan (adc_init in the shared plat_init) */
static void adc_quiesce_ours(void)
{
    R8(ADC_A) = 0u;
    R8(ADC_B) = 0u;
    const uint32_t t0 = R32(TCNTA);
    while (((R8(ADC_A + 2u) | R8(ADC_B + 2u)) & 0x80u) != 0u &&
           (uint32_t)(R32(TCNTA) - t0) < 200u) {
    }
}

/* ---- 0x8F9EC ------------------------------------------------------------ */
void plat_adcmux_init(void)
{
    adc_quiesce_ours();                                /* D5                 */
    adc_dmac_init();                                   /* 0x967F4            */
    adma_init();                                       /* 0x96900            */
    R16(C_MIN13_RAW) = 0xFFFFu;
    (void)desc_init(&D_4088C);
    (void)desc_init(&D_408A4);
    (void)desc_init(&D_40874);
    (void)desc_init(&D_408EC);
    (void)desc_init(&D_408BC);
    (void)desc_init(&D_408D4);
    R8(0xFFF90007u) = 2u;                              /* status 4088C       */
    R8(0xFFF9000Fu) = 2u;                              /* status 408A4       */
    R8(0xFFF90037u) = 2u;                              /* status 40874       */
    R8(0xFFF9002Fu) = 2u;                              /* status 408EC       */
    R16(0xFFF907FEu) = 0u;
    R8(0xFFF908F6u) = 0u;
    R16(RAM_GUARD_A) = 0x5B97u;
    R16(RAM_GUARD_RING) = 0x5B97u;
    R16(RAM_GUARD_B) = 0x5B97u;
    /* D6: *(u8*)0x40870 = 0 -> 0x40D8E[0] = 0 -> DIAGVAL = 0+1 */
    R16(ADC_A + 0x30u) = (uint16_t)((R16(ADC_A + 0x30u) & 0xFCFFu) | 0x0100u | 0x4000u);
    R16(ADC_B + 0x30u) = (uint16_t)((R16(ADC_B + 0x30u) & 0xFCFFu) | 0x0100u | 0x4000u);
    R32(DMAC + 0x5Cu) = R32(DMAC + 0x5Cu) | 4u;        /* 0x95F68: CHCR5 IE  */
    R8(ADC_A) = (uint8_t)(R8(ADC_A) | 2u);             /*          TRGE      */
    R32(DMAC + 0x4Cu) = R32(DMAC + 0x4Cu) | 4u;        /* 0x95F7A: CHCR4 IE  */
    R8(ADC_B) = (uint8_t)(R8(ADC_B) | 2u);             /*          TRGE      */
    R32(C_SPAN_US) = 20000u;
    R16(C_SPAN_CNT) = 20u;
    R16(TMRG + 0x96u) = 50u;      R16(TMRG + 0x94u) = 0xFFD8u;   /* G1 */
    R16(TMRG + 0xA6u) = 50u;      R16(TMRG + 0xA4u) = 0u;        /* G2 */
    R16(TMRG + 0xC6u) = 4000u;    R16(TMRG + 0xC4u) = 0xFFB0u;   /* G4 */
    R16(TMRG + 0xD6u) = 4000u;    R16(TMRG + 0xD4u) = 3940u;     /* G5 */
}

/* ---- 0x93740: push one 1 ms mean into a 10-slot window ----------------- */
static void desc_push(const adc_desc_t *d, uint16_t s)
{
    volatile adc_state_t *st = ST(d);
    volatile uint16_t *ring = RING(d);
    uint32_t i;

    if (st->status < 2u) {
        st->count = 1u;
        i = (st->len > 1u) ? 1u : 0u;
        ring[i] = s;
        st->sum = s;
    } else {
        uint32_t c = st->count;
        if (c < st->len) {
            c++;
            st->count = (uint8_t)c;
            i = (c < st->len) ? c : 0u;
            ring[i] = s;
            st->sum = st->sum + s;
        } else {
            i = st->idx;
            const uint16_t old = ring[i];
            st->sum = st->sum - old + s;
            ring[i] = s;
            i++;
            if (i >= st->len) {
                i = 0u;
            }
        }
    }
    st->idx = (uint8_t)i;
}

/* ---- 0x936FC: publish one window (5 ms) -------------------------------- */
static void desc_finish(const adc_desc_t *d)
{
    volatile adc_state_t *st = ST(d);
    const uint32_t c = st->count;

    if (c != 0u) {
        float f = (float)(int32_t)st->sum;
        f = d->scale * f;
        f = f / (float)(int32_t)c;
        f = f + 0.5f;
        int32_t t = (int32_t)f;                        /* ftrc               */
        if ((uint32_t)t > 0xFFFFu) {                   /* clipu.w            */
            t = 0xFFFF;
        }
        R16(d->out) = (uint16_t)t;
    }
    if (st->status < 2u && st->status != 0u) {         /* (s8)<2 && >0       */
        st->status = (uint8_t)(st->status + 1u);
    }
}

/* ---- 0x9699E: the completed half of the AN0 ring ----------------------- */
static volatile const uint16_t *ring_done_half(void)
{
    if (R16(ADMA + 0x64u) > 80u) {
        return (volatile const uint16_t *)0xFFF88688u;
    }
    return (volatile const uint16_t *)RAM_MUX_RING;
}

static uint32_t sum_stride(volatile const uint16_t *p, uint32_t step, uint32_t n)
{
    uint32_t s = 0u;
    for (; n != 0u; n--) {
        s += *p;
        p += step;
    }
    return s;
}

/* ---- 0x8F058 (D2: through 0x8F1B4) — called from DEI5 at 1 ms ---------- */
static void adcmux_1ms(void)
{
    {   /* the 20-call span */
        const uint16_t k = (uint16_t)(R16(C_SPAN_CNT) + 1u);
        R16(C_SPAN_CNT) = k;
        R32(C_MS_CNT) = R32(C_MS_CNT) + 1u;
        if (k >= 20u) {
            const uint32_t prev = R32(C_SPAN_STAMP);
            const uint32_t now = R32(TCNTA);
            R16(C_SPAN_CNT) = 0u;
            if (prev != 0u) {
                R32(C_SPAN_US) = now - prev;
            }
            R32(C_SPAN_STAMP) = now;
        }
    }
    volatile const uint16_t *h = ring_done_half();

    /* mux 13: slots 4, 9, .. 79 (16) */
    const uint32_t m13 = sum_stride(h + 4, 5u, 16u) >> 4;
    desc_push(&D_408A4, (uint16_t)m13);
    uint32_t mad = 0u;
    for (uint32_t i = 0u; i < 16u; i++) {
        const uint32_t x = h[4u + 5u * i];
        if (x > m13) {
            mad += x - m13;
        } else {
            mad += m13 - x;
        }
    }
    mad >>= 4;
    {
        const uint32_t prev = R16(C_PREV13);
        uint32_t r = mad;
        if (prev != 0u) {
            r += (prev > m13) ? (prev - m13) : (m13 - prev);
            if (r > 0xFFFFu) {                         /* clipu.w            */
                r = 0xFFFFu;
            }
        }
        R16(C_PREV13) = (uint16_t)m13;
        desc_push(&D_40874, (uint16_t)r);
    }
    {
        uint32_t mn = R16(C_MIN13_RAW);
        for (uint32_t i = 0u; i < 16u; i++) {
            const uint32_t x = h[4u + 5u * i];
            if (x < mn) {
                mn = x;
            }
        }
        R16(C_MIN13_RAW) = (uint16_t)mn;
    }
    /* mux 11: slots 3, 13, .. 73 (8) */
    desc_push(&D_4088C, (uint16_t)(sum_stride(h + 3, 10u, 8u) >> 3));
    /* mux 8: slots 2, 12, .. 72 */
    {
        const uint16_t m8 = (uint16_t)(sum_stride(h + 2, 10u, 8u) >> 3);
        R16(C_MEAN8_RAW) = m8;
        desc_push(&D_408BC, m8);
    }
    /* mux 10: slots 7, 17, .. 77 */
    {
        const uint16_t m10 = (uint16_t)(sum_stride(h + 7, 10u, 8u) >> 3);
        R16(C_MEAN10_RAW) = m10;
        desc_push(&D_408D4, m10);
    }
    /* mux 14: slots 8, 18, .. 78 */
    {
        const uint32_t m14 = sum_stride(h + 8, 10u, 8u) >> 3;
        if (m14 < 0x3333u) {
            R8(0xFFF9002Fu) = 0u;
        }
        desc_push(&D_408EC, (uint16_t)m14);
    }
}

/* ---- 0x95FC4 (DEI5, vector 128) / 0x95FEA (DEI4, vector 124) ----------- */
void plat_dei5_isr(void)
{
    (void)R8(DMAC + 0x50u + 0x8Cu);
    R8(DMAC + 0x50u + 0x8Cu) = 0xFEu;
    (void)R8(DMAC + 0x50u + 0x8Cu);
    adcmux_1ms();                                      /* D4: no 0x68F5C/66  */
}

void plat_dei4_isr(void)
{
    (void)R8(DMAC + 0x40u + 0x8Cu);
    R8(DMAC + 0x40u + 0x8Cu) = 0xFEu;
    (void)R8(DMAC + 0x40u + 0x8Cu);
}

/* ---- 0x8F62C: 5 ms finisher, under SR.I >= 4 (0x9665C(0x40)) ----------- */
static inline uint32_t sr_get(void)
{
    uint32_t sr;
    __asm__ volatile ("stc sr, %0" : "=r"(sr) : : "memory");
    return sr;
}
static inline void sr_put(uint32_t sr)
{
    __asm__ volatile ("ldc %0, sr" : : "r"(sr) : "memory");
}

void plat_adcmux_finish_5ms(void)
{
    const uint32_t old = sr_get();                     /* 0x9665C(0x40)      */
    if ((old & 0xF0u) < 0x40u) {
        sr_put((old & ~0xF0u) | 0x40u);
    }

    desc_finish(&D_408A4);                             /* 0x907E4            */
    desc_finish(&D_40874);                             /* 0x907F8            */
    {
        const uint32_t prev = R16(C_MUX13_PREV);
        const int32_t now = (int16_t)R16(C_MUX13_MV);
        int32_t d;
        if (prev == 0u) {
            d = now;
        } else {
            d = ((int32_t)(uint16_t)now >= (int32_t)prev) ? now - (int32_t)prev
                                                          : (int32_t)prev - now;
            if ((int32_t)(uint16_t)d >= (int32_t)(uint16_t)0x07D0) {   /* 0x191486 */
                d = (int16_t)0x07D0;
            }
        }
        R16(C_MUX13_PREV) = (uint16_t)now;
        d += (int16_t)R16(C_RIP13);
        const int32_t env = (int16_t)R16(C_ENV13);
        const int32_t a = (int32_t)(uint16_t)d;
        const int32_t e = (int32_t)(uint16_t)env;
        int32_t v = env;
        if (a >= e) {
            v = d;
        } else {
            int32_t dec = 0;
            if (e > (int32_t)(uint16_t)0x0032) {       /* 0x191484 */
                dec = env - (int16_t)0x0032;
            }
            v = (a >= (int32_t)(uint16_t)dec) ? d : dec;
        }
        const uint32_t mn = R16(C_MIN13_RAW);
        R16(C_ENV13) = (uint16_t)v;
        R16(C_MIN13_RAW) = 0xFFFFu;
        R16(C_MIN13_MV) = (uint16_t)(int32_t)((float)(int32_t)mn * SCALE_35V);
    }
    desc_finish(&D_4088C);                             /* 0x907E2            */
    desc_finish(&D_408BC);                             /* 0x90188            */
    desc_finish(&D_408D4);                             /* 0x9018A            */
    desc_finish(&D_408EC);                             /* 0x907EE            */

    {                                                  /* 0x96674(old)       */
        const uint32_t sr = sr_get();
        sr_put(((old | ~0xF0u) & (sr | 0xF0u)));
    }
}

/* ---- 0x8F5A0 (D3: status ramp only), 5 ms ------------------------------ */
void plat_adcmux_status_5ms(void)
{
    if (R8(0xFFF9002Fu) < 2u) { R8(0xFFF9002Fu) = (uint8_t)(R8(0xFFF9002Fu) + 1u); }
    if (R8(0xFFF90017u) < 2u) { R8(0xFFF90017u) = (uint8_t)(R8(0xFFF90017u) + 1u); }
    if (R8(0xFFF9001Fu) < 2u) { R8(0xFFF9001Fu) = (uint8_t)(R8(0xFFF9001Fu) + 1u); }
}

#endif /* !TCU_RAM_APP */

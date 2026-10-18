/* SH7254R peripheral registers used by the project firmware.
 * Addresses from the SH7254R Group hardware manual (R01UH0480EJ0400 Rev. 4.00), section 31.
 * Only registers in use are listed; add more as modules are implemented. */
#ifndef TCU_SH7254R_REGS_H
#define TCU_SH7254R_REGS_H

#include <stdint.h>

#define REG8(a)  (*(volatile uint8_t  *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

/* ---- Watchdog (section 16) -------------------------------------------- */
#define WDT_WTCR    REG16(0xFFFE0000u)  /* write: 0xA5xx */
#define WDT_WTCNT   REG16(0xFFFE0002u)  /* write: 0x5Axx */
#define WDT_WRCR    REG16(0xFFFE0006u)  /* write: 0x5Axx */

/* ---- Compare match timer (section 15) ---------------------------------- */
#define CMT_CMSTR   REG16(0xFFFEC000u)  /* bit0 STR0, bit1 STR1 */
#define CMT_CMCR0   REG8 (0xFFFEC010u)  /* bit6 CMIE, bits1-0 CKS: 00 = P/8 */
#define CMT_CMSR0   REG8 (0xFFFEC011u)  /* bit0 CMF (write 0 after reading 1) */
#define CMT_CMCNT0  REG16(0xFFFEC012u)
#define CMT_CMCOR0  REG16(0xFFFEC014u)

/* ---- Ports and pin function controller (sections 22, 23) -------------- */
#define PADR   REG16(0xFFFE3802u)
#define PAIOR  REG16(0xFFFE3806u)
#define PACR4  REG16(0xFFFE3810u)
#define PACR3  REG16(0xFFFE3812u)
#define PACR2  REG16(0xFFFE3814u)
#define PACR1  REG16(0xFFFE3816u)
#define PAPSR  REG16(0xFFFE3818u)
#define PBDR   REG16(0xFFFE3882u)
#define PBIOR  REG16(0xFFFE3886u)
#define PBCR4  REG16(0xFFFE3890u)
#define PBCR3  REG16(0xFFFE3892u)
#define PBCR2  REG16(0xFFFE3894u)
#define PBCR1  REG16(0xFFFE3896u)
#define PBIR   REG16(0xFFFE3898u)
#define PBDSR  REG16(0xFFFE389Au)
#define PBPSR  REG16(0xFFFE389Cu)
#define PCDR   REG16(0xFFFE3902u)
#define PCIOR  REG16(0xFFFE3906u)
#define PCCR4  REG16(0xFFFE3910u)
#define PCCR3  REG16(0xFFFE3912u)
#define PCCR2  REG16(0xFFFE3914u)
#define PCCR1  REG16(0xFFFE3916u)
#define PCPSR  REG16(0xFFFE3918u)
#define PCIR   REG16(0xFFFE391Au)
#define PCDSR  REG16(0xFFFE391Cu)
/* Ports D-L: block at 0xFFFFC800 (offsets per stock initialisation, spec 01 section 4) */
#define PFC16(off) REG16(0xFFFFC800u + (off))
#define PJDR   REG16(0xFFFFC860u)
#define PJIOR  REG16(0xFFFFC86Cu)
#define PJCR2  REG16(0xFFFFC870u)
#define PJCR1  REG16(0xFFFFC872u)
#define PJPSR  REG16(0xFFFFC868u)
#define CKCR   REG8 (0xFFFFC921u)       /* CK control register (low byte) */

/* ---- RCAN-TL1 (section 18) --------------------------------------------- */
#define RCAN_A_BASE 0xFFFFD000u         /* bus 1 (shared with channel B)      */
#define RCAN_B_BASE 0xFFFFD800u         /* bus 1, receive-only use in stock   */
#define RCAN_C_BASE 0xFFFFE000u         /* bus 2 (shifter / diagnostic bus)   */

#define RCAN_MCR(b)    REG16((b) + 0x000u)
#define RCAN_GSR(b)    REG16((b) + 0x002u)
#define RCAN_BCR1(b)   REG16((b) + 0x004u)
#define RCAN_BCR0(b)   REG16((b) + 0x006u)
#define RCAN_IRR(b)    REG16((b) + 0x008u)
#define RCAN_IMR(b)    REG16((b) + 0x00Au)
#define RCAN_TXPR(b)   REG32((b) + 0x020u)  /* TXPR1:TXPR0, must be written as 32 bits */
#define RCAN_TXCR(b)   REG32((b) + 0x028u)  /* TXCR1:TXCR0, write 1 to cancel a pending transmission */
#define RCAN_TXACK0(b) REG16((b) + 0x032u)
#define RCAN_RXPR(b)   REG32((b) + 0x040u)  /* RXPR1:RXPR0, bit n = mailbox n; write 1 to clear */
#define RCAN_MB_CTRL0H(b, n) REG16((b) + 0x100u + 32u * (n))          /* IDE RTR 0 STDID[10:0] EXTID[17:16] */
#define RCAN_MB_CTRL0L(b, n) REG16((b) + 0x102u + 32u * (n))          /* EXTID[15:0] */
#define RCAN_MB_LAFMH(b, n)  REG16((b) + 0x104u + 32u * (n))
#define RCAN_MB_LAFML(b, n)  REG16((b) + 0x106u + 32u * (n))
#define RCAN_MB_DATA(b, n, i) REG8((b) + 0x108u + 32u * (n) + (i))
#define RCAN_MB_CTRL1(b, n)  REG16((b) + 0x110u + 32u * (n))          /* NMC ATX DART MBC[2:0] DLC[3:0] */

#define RCAN_MCR_IDR     0x8000u        /* MCR15 ID reorder = 1 (reset value): the mailbox layout
                                           IDE RTR 0 STDID[10:0] EXTID[17:16] used below. Clearing
                                           it shifts every identifier by 2 bits (bench 2026-10-17) */
#define RCAN_MCR_RESET   0x0001u        /* MCR0: reset request  */
#define RCAN_MCR_HALT    0x0002u        /* MCR1: halt request   */
#define RCAN_GSR_RESET   0x0008u        /* GSR3: in reset mode  */
#define RCAN_GSR_HALT    0x0010u        /* GSR4: halt/sleep     */
#define RCAN_MBC_TX      (0u << 8)
#define RCAN_MBC_RX      (3u << 8)      /* receive data frames only, LAFM usable */
#define RCAN_MBC_OFF     (7u << 8)
#define RCAN_MAILBOXES   32u


/* ---- RSPI-B (section 17), CPU-driven access to the supervisor --------- */
#define SPCRB    REG8(0xFFFFB800u)
#define SPPCRB   REG8(0xFFFFB802u)
#define SPSRB    REG8(0xFFFFB803u)
#define SPDRB    REG16(0xFFFFB804u)
#define SPSCRB   REG8(0xFFFFB808u)      /* sequence length - 1 */
#define SPBRB    REG8(0xFFFFB80Au)
#define SPCKDB   REG8(0xFFFFB80Cu)
#define SSLNDB   REG8(0xFFFFB80Du)
#define SPNDB    REG8(0xFFFFB80Eu)
#define SPCMDB0  REG16(0xFFFFB810u)
#define RSPIB_DMA_REQ0 REG16(0xFFFE6074u)   /* as cleared by the stock CPU-mode set-up */
#define RSPIB_DMA_REQ1 REG16(0xFFFE6076u)
/* RSPI-A (bus 0, solenoid driver chips) */
#define SPCRA    REG8(0xFFFFB000u)
#define SPPCRA   REG8(0xFFFFB002u)
#define SPSRA    REG8(0xFFFFB003u)
#define SPDRA    REG16(0xFFFFB004u)
#define SPSCRA   REG8(0xFFFFB008u)
#define SPBRA    REG8(0xFFFFB00Au)
#define SPCKDA   REG8(0xFFFFB00Cu)
#define SSLNDA   REG8(0xFFFFB00Du)
#define SPNDA    REG8(0xFFFFB00Eu)
#define SPCMDA(n) REG16(0xFFFFB010u + 2u * (n))
#define RSPIA_DMA_REQ0 REG16(0xFFFE6070u)
#define RSPIA_DMA_REQ1 REG16(0xFFFE6072u)
#define SPSR_SPRF  0x80u
#define SPSR_SPTEF 0x20u

/* ---- Flash MAT selection and ROM cache (sections 25, 27) ---------------- */
#define ROMMAT   REG16(0xFFFFA820u)      /* write 0x3B00 | sel: 0 user MAT, 1 user boot MAT */
#define RCCR     REG32(0xFFFC1400u)      /* bit 3 RCF, 2 RCFI, 1 RCFD flush; bit 0 RCE enable */
#define RCCR_FLUSH_ALL 0x0000000Fu       /* flush data + instruction lines, keep the cache on */

/* ---- A/D converters (section 19) --------------------------------------- */
#define ADCSR0   REG8(0xFFFFE800u)
#define ADANS0   REG16(0xFFFFE820u)      /* AN0..AN15  */
#define ADANS1   REG16(0xFFFFE822u)      /* AN16..AN27 */
#define ADR0(n)  REG16(0xFFFFE840u + 2u * (n))
#define ADCSR1   REG8(0xFFFFEC00u)
#define ADANS3   REG16(0xFFFFEC20u)      /* AN40..AN47 */
#define ADR1(n)  REG16(0xFFFFEC40u + 2u * ((n) - 40u))
#define ADCSR_ADST 0x80u
#define ADCSR_ADCS 0x40u                 /* continuous scan */

/* ---- ATU-III (section 13) ----------------------------------------------- */
#define ATUENR   REG16(0xFFFFF000u)
#define ATU_NCMR REG8(0xFFFFF003u)
#define ATU_PSCR(n) REG16(0xFFFFF100u + 2u * (n))
#define ATUENR_PSCE 0x0001u
#define ATUENR_TFE  0x0040u
#define TSTRF    REG32(0xFFFFFA00u)
#define NCCRF    REG32(0xFFFFFA04u)
#define NCRFA(n) REG8(0xFFFFFA11u + 2u * (n))
#define TF_BASE(n) (0xFFFFFA80u + 0x20u * (n))
#define TCRF(n)   REG8(TF_BASE(n) + 0x00u)
#define TIERF(n)  REG8(TF_BASE(n) + 0x01u)
#define TSRF(n)   REG8(TF_BASE(n) + 0x03u)
#define ECNTBF(n) REG16(TF_BASE(n) + 0x08u)
#define ECNTCF(n) REG32(TF_BASE(n) + 0x0Cu)
#define GRDF(n)   REG32(TF_BASE(n) + 0x1Cu)

#endif

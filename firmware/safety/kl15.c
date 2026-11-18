/* kl15.c — the terminal-15 (ignition) object 0xFFF8E9D4 and its state
 * machine E9FF, ported from the stock bytes (kl15.h has the map, the
 * stand-ins and the deviations; ~/tcu_sweep/keyoff_autop_port.md).
 *
 * Disassembly: sh-elf-objdump -b binary -m sh2a -EB of the pristine image,
 * 0x140B80..0x140E50 (the object), 0x140EB0..0x140F16 (the state machine),
 * 0x140F18..0x140F5A (+0x2F), 0x140FCC..0x141124 (the plausibility machine),
 * 0x133CA6..0x133CEA (the gather). Byte offsets below are the object's.
 */
#include "safety/kl15.h"
#include "io/vehicle/vehicle.h"

#include <stdint.h>
#include <string.h>

#if defined(TCU_RAM_APP)
/* the RAM application does not carry the range chain (rng_chain.c stub),
 * so nothing reads the terminal state: the dump's values, constant */
void kl15_init(void) {}
void kl15_tick_10ms(void) {}
uint8_t kl15_e9ff(void) { return 2u; }
uint8_t kl15_e9fe(void) { return 1u; }
uint8_t kl15_ea01(void) { return 1u; }
uint8_t kl15_ea03(void) { return 0u; }
static const kl15_wit_t s_wit0;
const kl15_wit_t *kl15_wit(void) { return &s_wit0; }
#else

#define B(o)    CELL_KL15_U8(o)
#define W(o)    CELL_KL15_U16(o)
#define L(o)    CELL_KL15_U32(o)

/* offsets [O 0x140B80 / 0x140FCC / 0x133CA6] */
#define O_CFG    0x00u   /* u32 the cfg pointer 0x00064928 */
#define O_C1     0x04u   /* u32 voltage off-delay, +0x08 its complement */
#define O_C2     0x0Cu   /* u32 forced-off counter, +0x10 its complement */
#define O_C3     0x14u   /* u32 disagreement timer, +0x18 its complement */
#define O_PREV   0x1Cu   /* voltage flag of the previous pass (raw), +0x1D ~ */
#define O_VOLT   0x1Eu   /* voltage flag (off-delayed) */
#define O_CANON  0x1Fu   /* "CAN on" = table 0x191D0C[code] */
#define O_CANINV 0x20u   /* CAN invalid (status != 0 or code 7) */
#define O_AGREE  0x21u   /* the learned agreement flag (1 agree, 2 disagree) */
#define O_AST    0x22u   /* plausibility machine state 0..2, +0x23 ~ */
#define O_MV     0x24u   /* u16 voltage mV */
#define O_CODE   0x26u   /* 0x112 code, +0x27 the check copy */
#define O_STAT   0x28u   /* 0x112 RX status, +0x29 the check copy */
#define O_E9FE   0x2Au
#define O_E9FF   0x2Bu   /* +0x2C ~ */
#define O_QUAL   0x2Du   /* EA01, +0x2E ~ */
#define O_F2F    0x2Fu   /* EA03, +0x30 ~ */

/* tables 0x191D0C / 0x191D00 ({ u32 n = 8; u8 [8] }, read by 0x151DAC
 * without a bound — the index is & 7) [O image bytes] */
static const uint8_t k_tbl_on[8]  = { 0u, 0u, 0u, 0u, 1u, 1u, 0u, 0u };   /* 0x191D0C */
static const uint8_t k_tbl_2f[8]  = { 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u };   /* 0x191D00 */

#if defined(__sh__)
#define KL_WIT __attribute__((section(".kl15_wit")))
#else
#define KL_WIT
#endif
KL_WIT static kl15_wit_t s_wit;
/* ours: the RAM shadow of the agreement byte 0xFFF82F78 / 79 (the stock's
 * RAM image of EEPROM record 0xFFF82EF4 + 0x84; never flushed here) */
KL_WIT static uint8_t s_agree_store, s_agree_store_n;

/* 0x140B08: a counter pair must be complementary, else a fatal reset */
static uint8_t pair_ok(uint8_t o)
{
    return (uint8_t)(L(o) == (uint32_t)~L(o + 4u));
}

/* one decrement step of a counter pair (0x140C1A.. / 0x140CC6..) */
static void pair_dec(uint8_t o)
{
    if (L(o) != 0u) {
        L(o) = L(o) - 1u;
    }
    if (L(o + 4u) != 0xFFFFFFFFu) {
        L(o + 4u) = L(o + 4u) + 1u;
    }
}

static void pair_set(uint8_t o, uint32_t v)
{
    L(o) = v;
    L(o + 4u) = ~v;
}

/* ours: the stock resets the MCU on a torn complement; here the terminal
 * is forced OFF until power-off (E9FF 4, quality 2 → D5F2 = 0) */
static void fatal(void)
{
    s_wit.fatal_n++;
    s_wit.latched_off = 1u;
    B(O_E9FF) = 4u;
    B(O_E9FF + 1u) = (uint8_t)~4u;
    B(O_E9FE) = 0u;
    B(O_QUAL) = 2u;
    B(O_QUAL + 1u) = (uint8_t)~2u;
    B(O_F2F) = 0u;
    B(O_F2F + 1u) = 0xFFu;
}

/* 0x140EB0 */
static uint8_t fsm(uint8_t st, uint8_t x)
{
    switch (st) {
    case 2u: return x != 0u ? 2u : 3u;     /* 0x140EBC: x == 0 → 3 */
    case 4u: return x == 1u ? 1u : 4u;     /* 0x140ECA */
    case 1u: return 2u;
    case 3u: return 4u;
    case 0u: return x == 1u ? 1u : 4u;
    default: return 0u;
    }
}

/* 0x140FCC: the CAN / voltage plausibility machine; returns 0 on a fatal */
static uint8_t plaus(void)
{
    uint8_t st;

    /* 0x140FCE: +0x21 = the stored byte when its complement holds, else 0 */
    B(O_AGREE) = s_agree_store;
    if (s_agree_store != (uint8_t)~s_agree_store_n) {
        B(O_AGREE) = 0u;
    }
    st = B(O_AST);
    if (st != (uint8_t)~B(O_AST + 1u)) {
        return 0u;
    }
    if (B(O_CANINV) != 0u) {
        B(O_AST) = 0u;                                  /* 0x1410D6 */
    } else if (st == 0u || st == 1u) {
        if (st == 0u) {
            if (B(O_CANON) != 1u) {
                goto out;                               /* 0x141038 */
            }
            B(O_AST) = 1u;                              /* 0x14103C, falls into state 1 */
        }
        if (B(O_CANON) == 0u || B(O_VOLT) == 0u) {      /* 0x141040 */
            B(O_AST) = 2u;
            pair_set(O_C3, KL15_CAL_AGREE_T);
        }
    } else if (st == 2u) {
        if (B(O_CANON) == 0u && B(O_VOLT) == 0u) {      /* 0x141066: both off */
            B(O_AGREE) = 1u;
            B(O_AST) = 0u;
        } else if (B(O_CANON) == 1u && B(O_VOLT) == 1u) {   /* 0x14107A: both on */
            B(O_AST) = 0u;
        } else {
            if (!pair_ok(O_C3)) {
                return 0u;
            }
            pair_dec(O_C3);
            if (!pair_ok(O_C3)) {
                return 0u;
            }
            if (L(O_C3) == 0u) {
                B(O_AGREE) = 2u;                        /* 0x1410B4 */
                B(O_AST) = 0u;
            }
        }
    } else {
        return 0u;                                      /* 0x1410BE: fatal */
    }
out:
    B(O_AST + 1u) = (uint8_t)~B(O_AST);
    /* 0x1410E8: a change is written to the stored byte (and the EEPROM
     * record marked, 0x14115A — not ported) while 0xFFF96068 == 0
     * [stand-in: dump 00] */
    if (B(O_AGREE) != s_agree_store) {
        s_agree_store = B(O_AGREE);
        s_agree_store_n = (uint8_t)~B(O_AGREE);
        s_wit.agree_set_n++;
    }
    return 1u;
}

void kl15_init(void)
{
    /* bss zero + 0x140B30 (0x132980): complements from the zeroed bytes,
     * +0x21 from the stored byte, +0x22 = 0; the counter pairs as the dump
     * holds them at rest (0 / FFFFFFFF) */
    memset((void *)(uintptr_t)KL15_OBJ, 0, KL15_OBJ_LEN);
    L(O_CFG) = 0x00064928u;
    pair_set(O_C1, 0u);
    pair_set(O_C2, 0u);
    pair_set(O_C3, 0u);
    B(O_PREV + 1u) = 0xFFu;
    B(O_CODE + 1u) = 0xFFu;
    B(O_STAT + 1u) = 0xFFu;
    B(O_E9FF + 1u) = 0xFFu;
    B(O_QUAL + 1u) = 0xFFu;
    B(O_F2F + 1u) = 0xFFu;
    B(O_AST + 1u) = 0xFFu;
    s_agree_store = KL15_SI_AGREE;
    s_agree_store_n = (uint8_t)~KL15_SI_AGREE;
    B(O_AGREE) = s_agree_store;
    *(volatile uint16_t *)(uintptr_t)KL15_V_CELL = 0u;
    memset(&s_wit, 0, sizeof s_wit);
}

/* the gather (0x133CA6..0x133CEA) from our receive layer: status 0 when
 * the frame is fresh (CRC / counter / timeout) — our Q_OK, or a fresh frame
 * carrying code 7 (ign_code is set only from a fresh frame); code = byte 0
 * bits 2..0 */
static void gather(void)
{
    const vehicle_in_t *v = vehicle_in();
    const uint8_t code = (uint8_t)(v->ign_code & 7u);
    const uint8_t stat = (uint8_t)((v->q_ign_code == Q_OK || code == 7u) ? 0u : 1u);

    /* 0x7A7E8: 0x938B4 = 0x94F9E (= 0x907EE, 0x8E5E8) — one copy here */
    *(volatile uint16_t *)(uintptr_t)KL15_V_CELL = *(volatile uint16_t *)(uintptr_t)KL15_SRC_CELL;
    W(O_MV) = *(volatile uint16_t *)(uintptr_t)KL15_V_CELL;
    B(O_CODE) = code;
    B(O_CODE + 1u) = (uint8_t)(~code | 0xF8u);
    B(O_STAT) = stat;
    B(O_STAT + 1u) = (uint8_t)~stat;
}

void kl15_tick_10ms(void)
{
    uint8_t x, q, st;

    s_wit.ticks++;
    if (s_wit.latched_off != 0u) {
        return;
    }
    gather();
    /* 0x140B92: integrity of the state and the previous flag */
    if (B(O_E9FF) != (uint8_t)~B(O_E9FF + 1u) || B(O_PREV) != (uint8_t)~B(O_PREV + 1u)) {
        fatal();
        return;
    }
    /* 0x140BBA: the voltage flag (cmp/hs: unsigned) */
    B(O_VOLT) = (uint8_t)(W(O_MV) >= KL15_CAL_MV ? 1u : 0u);
    if (B(O_PREV) == 1u && B(O_VOLT) == 0u) {
        pair_set(O_C1, KL15_CAL_OFFDLY);            /* the falling edge arms the off-delay */
    }
    B(O_PREV) = B(O_VOLT);
    B(O_PREV + 1u) = (uint8_t)~B(O_VOLT);
    if (!pair_ok(O_C1)) {
        fatal();
        return;
    }
    if (L(O_C1) != 0u) {
        B(O_VOLT) = 1u;                             /* held on while it runs */
        if (!pair_ok(O_C1)) {
            fatal();
            return;
        }
        pair_dec(O_C1);
    }
    /* 0x140C32: the CAN path */
    if (B(O_STAT) == 0u && B(O_CODE) != 7u) {
        x = k_tbl_on[B(O_CODE) & 7u];
        B(O_CANINV) = 0u;
    } else {
        x = 0u;
        B(O_CANINV) = 1u;
    }
    B(O_CANON) = x;
    /* 0x140F18: +0x2F */
    {
        const uint8_t f = (B(O_STAT) == 0u && B(O_CODE) != 7u) ? k_tbl_2f[B(O_CODE) & 7u] : 0u;
        B(O_F2F) = f;
        B(O_F2F + 1u) = (uint8_t)~f;
    }
    /* 0x140C68: the source quality */
    if (KL15_CAL_SEL == 0u) {
        q = B(O_CANINV);
    } else {
        if (plaus() == 0u) {
            fatal();
            return;
        }
        q = B(O_CANINV) == 0u ? 0u : (B(O_AGREE) == 1u ? 1u : 2u);
    }
    B(O_QUAL) = q;
    B(O_QUAL + 1u) = (uint8_t)~q;
    /* 0x140CA0: CAN invalid → the voltage path */
    if (B(O_CANINV) != 0u) {
        x = B(O_VOLT);
    }
    /* 0x140CAE: the forced-off counter */
    if (!pair_ok(O_C2)) {
        fatal();
        return;
    }
    if (L(O_C2) != 0u) {
        x = 0u;
    }
    pair_dec(O_C2);
    /* 0x140CDE: the state */
    st = fsm(B(O_E9FF), x);
    if (st == 1u && B(O_E9FF) != 1u) {
        s_wit.on_edges++;
    }
    if (st == 4u && B(O_E9FF) != 4u) {
        s_wit.off_edges++;
    }
    B(O_E9FF) = st;
    /* (state 1: 0x140E52 / 0x140E86, state 3: 0x140E94 — not ported) */
    B(O_E9FE) = (uint8_t)(st == 1u || st == 2u || st == 3u);
    B(O_E9FF + 1u) = (uint8_t)~st;
    B(O_QUAL + 1u) = (uint8_t)~B(O_QUAL);
    /* 0x140DE4: the code and status check copies */
    if (B(O_CODE) != (uint8_t)~B(O_CODE + 1u) || B(O_STAT) != (uint8_t)~B(O_STAT + 1u)) {
        fatal();
    }
}

uint8_t kl15_e9ff(void) { return B(O_E9FF); }
uint8_t kl15_e9fe(void) { return B(O_E9FE); }
uint8_t kl15_ea01(void) { return B(O_QUAL); }
uint8_t kl15_ea03(void) { return B(O_F2F); }
const kl15_wit_t *kl15_wit(void) { return &s_wit; }

#endif /* !TCU_RAM_APP */

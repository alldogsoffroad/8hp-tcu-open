/* kl15.h — the safety layer's terminal-15 (ignition) object 0xFFF8E9D4 and
 * its state machine E9FF (2026-11-17; ~/tcu_sweep/keyoff_autop_port.md,
 * can_inputs_decode.md §5, park_chain_loose_ends_decode.md Q5). Ported 1:1
 * from the stock bytes:
 *
 *   0x133CA6..0x133CEA (mode 0, inside 0x133C4E) / 0x14F34C (modes 1..3,
 *             10 ms list slot 0 = 0x131FD8): the gather — +0x24 voltage
 *             (getter 0x7A7E0 = 0xFFF938B4, itself 0xFFF94F9E = mux 14,
 *             0x7A7E8 / 0x8E5E8), +0x26 / +0x27 the 0x112 ignition code and
 *             its check copy (0x76714 / 0x7671E: 0xFFF90ACC & 7, 0xFFF90ADC
 *             & 7 | 0xF8), +0x28 / +0x29 the 0x112 RX status and its check
 *             copy (0x773E0 / 0x773E8: 0xFFF8403C, 0xFFF84844); then
 *   0x140B80  the object: the voltage flag (+0x24 >= cal 0x195F62 = 4000 mV)
 *             with a 10-pass off-delay (cal 0x195DBD), the CAN path (+0x28
 *             == 0 and code != 7 → table 0x191D0C[code]: 4 and 5 on), the
 *             code-3/4/5 flag +0x2F (0x140F18, table 0x191D00), the
 *             CAN/voltage plausibility machine 0x140FCC (selector cal
 *             0x195DBF = 1; agreement +0x21, timer cal 0x195DC0 = 50) → the
 *             source quality +0x2D, the forced-off counter +0x0C, and
 *   0x140EB0  the state E9FF (+0x2B): 0 → 1 on, else 4; 1 → 2; 2 → 3 when
 *             off; 3 → 4; 4 → 1 when on; other → 0. E9FE (+0x2A) = E9FF in
 *             {1, 2, 3}.
 *
 * Getters (the range chain's gather 0xA7C9C): 0x140AE8 = E9FF → D641,
 * 0x140AF0 = EA01 (+0x2D) → D642, 0x140AF8 = EA03 (+0x2F) → D643;
 * 0x140AE0 = E9FE → the DSM operating cycle 0xFFF8989C.
 *
 * DEVIATIONS (ours, stated): the voltage is mux 14 (0xFFF907EE) copied to
 * 0xFFF938B4 in the gather — the stock's two copy stages 0x8E5E8 (0x94F9E,
 * a cell of the unported 0x8E5E8, left alone) and 0x7A7E8 (5 / 10 ms lists)
 * are skipped (≤ 15 ms earlier than the stock); the agreement flag +0x21 is
 * loaded from a constant (the unit's EEPROM byte 0xFFF82F78 = 01, dump) and
 * NEVER written back (the stock writes a change to 0xFFF82EF4+0x84 and marks
 * the EEPROM record, 0x14115A — not ported: no EEPROM write); the forced-off
 * counter +0x0C is armed only by 0x140E9C ← 0x1420C8 (a system-event hook,
 * tables 0x4D62C / 0x6CD28, undecoded) — not ported, it stays 0 as in the
 * dump; the drive-cycle initialisers of state 1 (0x140E52 / 0x140E86) and
 * state 3 (0x140E94) reach objects our firmware does not have — not ported;
 * the DTC report 0x141126 (+0x21 == 2) not ported; the stock's fatal resets
 * on a torn complement (0x794C8 / 0x7A914) are a counter + the terminal
 * forced OFF (E9FF 4, quality 2) until power-off — the fail-safe direction
 * (the range chain then holds or forces P); the PFM checkpoints 0x148E92 /
 * 0x148EB6 and the RAM byte tests 0x148ACC are dropped.
 */
#ifndef TCU_KL15_H
#define TCU_KL15_H

#include <stdint.h>

#define KL15_OBJ            0xFFF8E9D4u           /* .. 0xFFF8EA04 (0x31 bytes) */
#define KL15_OBJ_LEN        0x31u
#define KL15_V_CELL         0xFFF938B4u           /* u16 mV (0x7A7E8's copy) */
#define KL15_SRC_CELL       0xFFF907EEu           /* u16 mV mux 14 (plat_adcmux.c) */

#define CELL_KL15_U8(o)     (*(volatile uint8_t  *)(uintptr_t)(KL15_OBJ + (o)))
#define CELL_KL15_U16(o)    (*(volatile uint16_t *)(uintptr_t)(KL15_OBJ + (o)))
#define CELL_KL15_U32(o)    (*(volatile uint32_t *)(uintptr_t)(KL15_OBJ + (o)))
#define CELL_KL15_E9FE      CELL_KL15_U8(0x2Au)
#define CELL_KL15_E9FF      CELL_KL15_U8(0x2Bu)
#define CELL_KL15_EA01      CELL_KL15_U8(0x2Du)
#define CELL_KL15_EA03      CELL_KL15_U8(0x2Fu)

/* the stock calibration [O image bytes; cfg 0x64928] */
#define KL15_CAL_MV         4000u   /* 0x195F62 (cfg+8) */
#define KL15_CAL_OFFDLY     10u     /* 0x195DBD (cfg+0xC): voltage off-delay, passes */
#define KL15_CAL_FORCED     30u     /* 0x195DBE (cfg+0x10): 0x140E9C (not reachable here) */
#define KL15_CAL_SEL        1u      /* 0x195DBF (cfg+0x14): plausibility machine on */
#define KL15_CAL_AGREE_T    50u     /* 0x195DC0 (cfg+0x18): disagreement passes */
#define KL15_SI_AGREE       1u      /* 0xFFF82F78 / 79 = 01 / FE (dump): the learned flag */

typedef struct {
    uint32_t ticks;                 /* passes run */
    uint32_t fatal_n;               /* a torn complement (the stock resets): latched off */
    uint32_t on_edges, off_edges;   /* E9FF → 1 / → 4 */
    uint32_t agree_set_n;           /* +0x21 changed by 0x140FCC (RAM only, never stored) */
    uint8_t  latched_off;           /* ours: set by a fatal, until power-off */
    uint8_t  pad[3];
} kl15_wit_t;

void kl15_init(void);
/* one 10 ms pass (the stock runs it every 10 ms in modes 0..3: 0x133C4E /
 * 0x14F34C slot 0), after the CAN receive (vehicle_update_10ms) and before
 * the range chain (slot 3) */
void kl15_tick_10ms(void);

uint8_t kl15_e9ff(void);    /* 0x140AE8 → D641 */
uint8_t kl15_e9fe(void);    /* 0x140AE0 → 0xFFF8989C (the DSM operating cycle) */
uint8_t kl15_ea01(void);    /* 0x140AF0 → D642 */
uint8_t kl15_ea03(void);    /* 0x140AF8 → D643 */
const kl15_wit_t *kl15_wit(void);

#endif /* TCU_KL15_H */

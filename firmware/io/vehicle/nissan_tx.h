/* Nissan transmit set (2026-11-23): the four frames the Nissan ECM expects from the
 * transmission controller, rebuilt from the user's 2015 Frontier logs (TCM connected) and the
 * bench ECM's ROM (docs/reference/nissan-ecm-tcm-interface.md). Every frame starts disabled;
 * nissan_tx_enable(true) is refused until nissan_tx_set() has delivered valid drivetrain data.
 *
 *   0x174  8 bytes, 10 ms  B0..B2 FF E7 1C (constant in every logged frame; two 12-bit values
 *                          the ECM stores but does not use), B3 = target gear << 4 | engaged
 *                          gear, B4[3:0] rolling counter, B5 bit 7 lock-up flag [I], B6 B7 0
 *   0x176  7 bytes, 10 ms  B0:B1 output speed, B2:B3 turbine speed (BE, 1 rpm [I]), B4 B5 0,
 *                          B6[3:0] rolling counter
 *   0x177  6 bytes, 10 ms  B0 B1 0, B2[2:0] engaged gear - 1 and bit 3 lock-up flag [I],
 *                          B3 = 1 in 1st gear, B4 torque cut (fraction x 256; the ECM converts
 *                          it to spark retard), B5[6:3] shift type: 0..3 = 1-2..4-5, 9 = 4-3,
 *                          13 = 5-4, 15 = none
 *   0x560  4 bytes, 100 ms B1 page / B2 page data, 12 pages cycling, B0 0x65 (ATF 51 degC [I]);
 *                          pages 0x10-0x15 = calibration ID "8HP001", 0x16-0x17 = CVN (image CRC),
 *                          0x40-0x43 = TCM fault bitmaps 00; its timeout sets U0101 (lost
 *                          communication with the TCM) in the ECM (ROM 0x9CAFC; bench-proven
 *                          2026-11-23: U0101 cleared only once 0x560 was sent)
 * Losing 0x174 / 0x176 / 0x177 for about 1-2 s sets U1001.
 *
 * NOT sent: 0x421 (the gear indicator; its sender is open, decision D-N1 - two senders on one
 * ID would collide).
 */
#ifndef TCU_NISSAN_TX_H
#define TCU_NISSAN_TX_H

#include "platform/platform.h"

#if !defined(TCU_VEHICLE_NISSAN) || TCU_VEHICLE_NISSAN != 1
#error "nissan_tx.h: the Nissan transmit set - built only with TCU_VEHICLE=NISSAN (defines TCU_VEHICLE_NISSAN=1)"
#endif

#define NISSAN_TX_ID_174 0x174u
#define NISSAN_TX_ID_176 0x176u
#define NISSAN_TX_ID_177 0x177u
#define NISSAN_TX_ID_560 0x560u
#define NISSAN_TX_TODO_ID_GEARBOX 0x421u   /* never sent (D-N1) */

void nissan_tx_init(void);
void nissan_tx_tick_1ms(void);
struct vehicle_out;                          /* io/vehicle/vehicle_tx.h */
void nissan_tx_set(const struct vehicle_out *o);  /* takes the drivetrain fields when drivetrain_valid */
bool nissan_tx_enable(bool on);              /* false (and stays off) until valid data was set */
uint32_t nissan_tx_sent_count(void);         /* frames transmitted since nissan_tx_init */
#if !defined(__sh__) || defined(TCU_RAM_APP)
extern uint32_t nissan_tx_host_crc;          /* host / RAM app: stands for the code-flash image CRC */
#endif
uint8_t nissan_tx_shift_code(uint8_t engaged, uint8_t target);   /* 0x177 B5[6:3] */
void nissan_tx_build(uint8_t idx, can_frame_t *f);   /* 0 = 0x174, 1 = 0x176, 2 = 0x177, 3 = 0x560 */

#endif

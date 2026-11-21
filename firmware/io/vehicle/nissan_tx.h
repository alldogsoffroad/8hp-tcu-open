/* Nissan transmit set: A STUB (2026-11-20). NO FRAME IS ENABLED, NOW OR BY ANY CALL.
 *
 * The Nissan TCM's transmit set is unknown until the full capture identifies every ID the
 * ECM / cluster expect from a TCM (audit §2, D-N3). Two are known from the DBC, both left as
 * documented TODOs and NEVER sent:
 *   0x177 SHIFT_TORQUE_REQ  byte 4 = torque-reduction request, a stepped per-gear-pair map
 *         (needs a ported torque-intervention law first: unported 0xE5FF6);
 *   0x421 GEARBOX           byte 0 = range code (P 0x08, R 0x10, N 0x18, D 0x20).
 * The IDs below are named only so the vehicle seam scan (tests/unit/vehicle_seam_scan.py)
 * knows them; nothing transmits them.
 */
#ifndef TCU_NISSAN_TX_H
#define TCU_NISSAN_TX_H

#include "platform/platform.h"

#if !defined(TCU_VEHICLE_NISSAN) || TCU_VEHICLE_NISSAN != 1
#error "nissan_tx.h: the Nissan transmit stub - built only with TCU_VEHICLE=NISSAN (defines TCU_VEHICLE_NISSAN=1)"
#endif

#define NISSAN_TX_TODO_ID_TORQUE_REQ 0x177u   /* TODO(capture): layout + enable rules, never enabled */
#define NISSAN_TX_TODO_ID_GEARBOX    0x421u   /* TODO(capture): layout + enable rules, never enabled */

void nissan_tx_init(void);
void nissan_tx_tick_1ms(void);                 /* sends nothing */
bool nissan_tx_enable(bool on);                /* always false: no message exists to enable */
uint32_t nissan_tx_sent_count(void);           /* frames ever transmitted: always 0 */

#endif

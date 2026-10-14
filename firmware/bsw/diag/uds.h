/* UDS server (ISO 14229) for the project application.
 *
 * Services: 0x10 DiagnosticSessionControl (0x01 default, 0x03 extended, 0x02 programming =
 * hand-over to the Bosch boot software), 0x11 ECUReset (0x01), 0x22 ReadDataByIdentifier,
 * 0x3E TesterPresent. Requests: 0x7E1 physical (both buses), 0x7DF functional (bus 1);
 * responses 0x7E9.
 */
#ifndef TCU_UDS_H
#define TCU_UDS_H

#include "platform/platform.h"

typedef struct {
    /* Conditions for leaving the application (programming session, reset). */
    bool (*leave_allowed)(void);
    /* Application data identifiers: write up to max bytes, return length or -1 if unknown. */
    int (*read_did)(uint16_t did, uint8_t *out, uint16_t max);
} uds_app_t;

void uds_init(const uds_app_t *app);
/* Feed a received frame; returns true if it was a diagnostic frame. */
bool uds_rx(can_bus_t bus, const can_frame_t *f);
void uds_tick_1ms(void);
uint8_t uds_session(void);

#endif

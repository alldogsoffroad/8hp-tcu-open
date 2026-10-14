/* Boot request record at 0xFFF80080: hands a diagnostic request to the Bosch boot software
 * across a reset (spec 06 section 5a). */
#ifndef TCU_BOOT_RECORD_H
#define TCU_BOOT_RECORD_H

#include <stdint.h>

#define BOOT_REC_ADDR 0xFFF80080u
#define BOOT_REC_LEN  0x48u

/* Build the record the stock application writes for DiagnosticSessionControl into the
 * programming session: target = boot manager (2), request stored (not a full response),
 * so the boot software sends the positive response itself.
 * sid/sub: the request as received (sub may include the suppress-response bit). */
void boot_record_build_prog(uint8_t rec[BOOT_REC_LEN], uint8_t sid, uint8_t sub);

#endif

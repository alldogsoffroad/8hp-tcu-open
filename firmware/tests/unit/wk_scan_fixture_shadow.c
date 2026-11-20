/* negative fixture (wk_shadow_scan.py shadow): the shadow transmitting a request, calling a live module and
 * naming the engagement counter's EEPROM writer */
#include "io/vehicle/fca_tx.h"
void fixture_shadow(void) { fca_tx_set_torque_request(0, 0); drv_rq_request_set(0u, 0u); wk_bf8e8_counter(); }

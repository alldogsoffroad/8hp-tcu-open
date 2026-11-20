/* negative fixture (lp_ti_shadow_scan.py shadow): the shadow transmitting a request and calling a live module */
#include "io/vehicle/fca_tx.h"
void fixture_shadow(void) { fca_tx_set_torque_request(0, 0); drv_rq_request_set(0u, 0u); }

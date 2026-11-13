/* SH-only compile check: enabling the request source on SH is an #error */
#define DRV_RQ_REQUEST_SOURCE 1
#include "bsw/drv/drv_reqlaw.h"
int sh_guard_source;

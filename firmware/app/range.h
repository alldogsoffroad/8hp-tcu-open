/* Park target decision (milestone M4, docs/firmware/01-m4-range-park.md section 3).
 *
 * Stock rule: mode 4 of the park arbiter (spec 14 section 4.2, 0xA57DA), without the stock
 * special cases that only refine "released". Pure decision plus a stored result with an
 * inverted copy.
 */
#ifndef TCU_RANGE_H
#define TCU_RANGE_H

#include "io/vehicle/vehicle.h"

#define RANGE_STATIONARY_RPM 80     /* output speed limit, stock calibration +0x00 */

/* Same codes as the stock park target (spec 14 section 4.1). */
typedef enum { PARK_RELEASE = 6, PARK_ENGAGE = 8 } park_target_t;

typedef enum {
    R_NONE = 0,
    R_LEVER_P,                      /* P selected                                     */
    R_MOVING,                       /* output speed above the limit or not valid      */
    R_PARK_REQ,                     /* park request while stationary (no source yet)  */
    R_LEVER_RND,                    /* R, N or D selected while stationary            */
    R_AUTO_P                        /* stationary without a valid lever request       */
} range_reason_t;

typedef struct {
    range_req_t lever;
    bool        lever_ok;           /* q_lever == Q_OK                                */
    rpm_t       output_speed;
    bool        speed_ok;           /* q_output_speed == Q_OK                         */
    bool        park_request;       /* stock D5F8; fixed false until a source exists  */
} range_in_t;

/* Stationary: valid output speed within +-RANGE_STATIONARY_RPM. */
bool range_stationary(rpm_t output_speed, bool speed_ok);
park_target_t range_decide(const range_in_t *in, range_reason_t *why);

void range_init(void);
void range_update(const range_in_t *in);
park_target_t range_target(void);   /* PARK_ENGAGE when the stored copy is corrupt  */
range_reason_t range_reason(void);
bool range_integrity_ok(void);

#ifdef PARK_TEST_HOOKS
void range_test_store(park_target_t t);     /* consistent but wrong value          */
void range_test_corrupt(void);              /* break the inverted copy             */
#endif

#endif

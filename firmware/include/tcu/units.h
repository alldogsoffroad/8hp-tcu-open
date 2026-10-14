/* Fixed-point units used on every module interface (architecture §5). */
#ifndef TCU_UNITS_H
#define TCU_UNITS_H

#include <stdbool.h>
#include <stdint.h>

typedef int16_t  rpm_t;      /* 1 rpm                          */
typedef int16_t  nm10_t;     /* 0.1 Nm                         */
typedef uint16_t mbar_t;     /* 1 mbar                         */
typedef int16_t  degc10_t;   /* 0.1 degC                       */
typedef uint16_t ma10_t;     /* 0.1 mA (solenoid current)      */
typedef uint16_t permil_t;   /* 0.1 %, 0..1000                 */
typedef uint32_t ms_t;       /* milliseconds since start       */

typedef enum {
    Q_OK = 0,                /* measured, plausible            */
    Q_SUBSTITUTED,           /* replaced by a substitute value */
    Q_DEFAULT,               /* fixed default value            */
    Q_INVALID                /* no usable value                */
} quality_t;

#endif

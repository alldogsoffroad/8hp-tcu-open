/* Nissan: the TCU state -> the transmit set's drivetrain fields (see nissan_out.h). Pure. */
#include "io/vehicle/nissan_out.h"

#include <string.h>

uint8_t nissan_gear_nibble(int8_t code)
{
    if (code >= 1 && code <= 8) {
        return (uint8_t)code;
    }
    return code == -1 ? NISSAN_GEAR_R : NISSAN_GEAR_PN;
}

/* B4 = the share of the ECM's actual torque the 8HP strategy asks to remove, x 256: only for a
 * reduction (type 2) of an armed up- or downshift intervention with a plausible actual torque
 * (>= 10 Nm). The ECM applies it as spark retard and gates it on its own shift timer. */
uint8_t nissan_cut_q8(const nissan_out_src_t *s)
{
    int32_t q;

    if (!s->ti_armed || s->ti_type != 2u || s->ti_cls > 1u || s->ti_delta_nm >= 0 ||
        !s->actual_ok || s->actual_nm10 < 100) {
        return 0u;
    }
    q = ((int32_t)(-s->ti_delta_nm) * 10 * 256 + s->actual_nm10 / 2) / s->actual_nm10;
    return (uint8_t)(q > 255 ? 255 : q);
}

void nissan_out_build(const nissan_out_src_t *s, vehicle_out_t *o)
{
    const uint8_t e = nissan_gear_nibble(s->engaged);
    const uint8_t t = nissan_gear_nibble(s->target);

    memset(o, 0, sizeof *o);
    o->output_rpm = s->output_rpm;
    o->turbine_rpm = s->turbine_rpm;
    o->gear_engaged = e;
    /* a target outside the forward gears while a forward gear is engaged (e.g. into N) is
     * reported as no pending shift */
    o->gear_target = (e <= 8u && t > 8u) ? e : t;
    o->torque_cut_q8 = o->gear_target != o->gear_engaged ? nissan_cut_q8(s) : 0u;
    o->lockup = s->lockup;
    o->drivetrain_valid = true;
}

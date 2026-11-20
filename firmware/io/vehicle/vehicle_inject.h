/* TEST-ONLY vehicle input injector (2026-11-20, ~/tcu_sweep/fork_seams_port.md; the
 * fork-readiness audit ~/tcu_sweep/fork_readiness_audit.md §1.9 / §4.5).
 *
 * vehicle_inject.c is a vehicle "profile" for host unit tests: it implements the profile
 * interface of io/vehicle/vehicle.h (vehicle_init / vehicle_rx / vehicle_update_10ms /
 * vehicle_in) WITHOUT any frame parsing, and lets a test set vehicle_in_t directly. A test
 * links it INSTEAD of a real profile (io/vehicle/profile_fca.c), so the transmission-side
 * goldens (range chain, park, garage engine, ...) can run without building FCA frames —
 * the step a vehicle fork needs to keep them profile-independent.
 *
 * Never part of an SH image: the header and the source refuse to compile for the SH
 * target (#error below; ctest vehicle_inject_sh_guard), and tests/unit/vehicle_seam_scan.py
 * (POST_BUILD of every SH ELF, ctest vehicle_inject_elf_scan_negative as its proof) fails
 * if a vehicle_inject symbol is linked into one.
 */
#ifndef TCU_VEHICLE_INJECT_H
#define TCU_VEHICLE_INJECT_H

#if defined(__sh__) || defined(__SH2A__) || defined(TCU_RAM_APP) || defined(TCU_LOADER)
#error "vehicle_inject: test-only (host unit tests) - never compiled for the SH target"
#endif

#include "io/vehicle/vehicle.h"

/* The inputs a profile reports after vehicle_update_10ms() with no frame received (as
 * profile_fca.c does then): every value 0 with Q_INVALID, brake BRAKE_NA, lever source
 * LEVER_SRC_NONE. vehicle_init() calls it. */
void vehicle_inject_reset(void);

/* Replace every input at once; vehicle_in() returns this copy until the next call. */
void vehicle_inject(const vehicle_in_t *v);

/* Frames handed to vehicle_rx() since vehicle_init() (the injector ignores them; a
 * converted test asserts 0 to prove nothing still goes through a frame path). */
uint32_t vehicle_inject_rx_count(void);

#endif

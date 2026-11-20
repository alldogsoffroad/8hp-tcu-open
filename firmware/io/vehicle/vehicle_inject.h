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
 * LEVER_SRC_NONE. (vehicle_init() does NOT use it: it restores profile_fca.c's power-on state,
 * all zero, before the first update.) */
void vehicle_inject_reset(void);

/* Replace every input at once; vehicle_in() returns this copy until the next call. */
void vehicle_inject(const vehicle_in_t *v);

/* Frames handed to vehicle_rx() since vehicle_init() (the injector ignores them; a
 * converted test asserts 0 to prove nothing still goes through a frame path). */
uint32_t vehicle_inject_rx_count(void);

/* ---- MESSAGE-LEVEL injection (2026-11-20, ~/tcu_sweep/fork_prep2_port.md) ----
 * The transmission tests used to build FCA frames (0x2EA / 0x210 lever, 0x140 brake,
 * 0x108 engine, 0x112 ignition, 0x14B park speed) only to set range / brake / engine /
 * ignition inputs. These calls hand over the same decoded content, vehicle-neutral: one
 * call = one fresh, valid message of that kind (a frame the profile would have accepted;
 * a bad-CRC / missing frame is simply NOT injected). vehicle_update_10ms() then ages the
 * messages and decodes them into vehicle_in_t exactly as profile_fca.c does (lever source
 * order, code 0/6 = no change, 7 = invalid, quality bits), so a golden that depends on
 * the FCA profile's source-loss timing reproduces: a message that is not refreshed is
 * lost after the LOSS DELAY (default 50 ms, the profile's timeout: it stays valid for the
 * 10 ms updates up to the 5th after its last injection, and drops on the 6th).
 * Do not mix with vehicle_inject() (the direct mode) in one run. */
/* a source is lost once its age exceeds this (ms); default 50 */
void vehicle_inject_set_loss_delay_ms(uint16_t ms);
/* a lever message from the primary (0x2EA-like) or the fallback (0x210-like) source;
 * code = the raw 3-bit lever code (1 P, 2 R, 3 N, 4 D, 5 D+flag, 0/6 no change, 7 invalid) */
void vehicle_inject_msg_lever(lever_src_t src, uint8_t code);
/* the brake switch message (BRAKE_OFF / BRAKE_ON / BRAKE_ERROR / BRAKE_NA raw 0..3) */
void vehicle_inject_msg_brake(brake_t raw);
/* the engine message: speed raw (<= 0x7FFF valid), T1 raw 13-bit (0x1FFF invalid) */
void vehicle_inject_msg_engine(uint16_t speed_raw, uint16_t t1_raw13);
/* the ignition status message (raw code 0..7; 7 = not available) */
void vehicle_inject_msg_ign(uint8_t code);
/* the park-interlock speed message (11-bit raw, 0x7FF invalid) */
void vehicle_inject_msg_park_speed(uint16_t raw11);

#endif

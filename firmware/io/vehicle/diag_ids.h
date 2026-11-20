/* THE PROJECT'S DIAGNOSTIC / DEBUG CAN IDS, in one place (2026-11-20,
 * ~/tcu_sweep/fork_seams_port.md; the fork-readiness audit
 * ~/tcu_sweep/fork_readiness_audit.md §1.5 / §4.4 / D-N4).
 *
 * These are not vehicle data: they are the project's own measurement, debug and
 * diagnostic channels. Today every one of them sits on the VEHICLE bus (bus 1, RCAN-A),
 * some on both buses. The values and buses below are EXACTLY what the firmware used
 * before this header existed — collecting them here changes no byte of any image.
 * Moving the debug / measurement IDs to bus 2 (or limiting UDS on bus 1, whose
 * 0x7E1 / 0x7E9 pair is also the Nissan TCM's address [I]) is a LATER DECISION (D-N4),
 * not made here: it changes what the bench tools see.
 *
 * Self-contained (no include): the bus macros expand to the can_bus_t enumerators of
 * platform/platform.h and are only usable where that header is included; the flasher
 * (its own program, no platform.h) uses only the ID values.
 */
#ifndef TCU_DIAG_IDS_H
#define TCU_DIAG_IDS_H

/* Heartbeat, app/main.c task_100ms: every 100 ms, bus 1 (watch.py). */
#define DIAG_ID_HEARTBEAT       0x7F0u
#define DIAG_BUS_HEARTBEAT      CAN_BUS1

/* Exception / vector announce, platform/sh7254r/header.c (application AND loader):
 * [0xDE 0xAD pc sr] on an unwired vector or exception, [0xE6 vecnum] from the RSPI
 * announce net; bus 1. */
#define DIAG_ID_EXCEPTION       0x7F5u
#define DIAG_BUS_EXCEPTION      CAN_BUS1

/* Measurement service, bsw/meas (spec 10): request 0x361 registered on BOTH buses,
 * responses on 0x398 on the bus the request came from; the periodic byte channels
 * 0x368 / 0x378 / 0x388 / 0x398. */
#define DIAG_ID_MEAS_REQ        0x361u
#define DIAG_ID_MEAS_RSP        0x398u
#define DIAG_ID_MEAS_PERIODIC0  0x368u
#define DIAG_ID_MEAS_PERIODIC1  0x378u
#define DIAG_ID_MEAS_PERIODIC2  0x388u
#define DIAG_ID_MEAS_PERIODIC3  0x398u

/* UDS (bsw/diag/uds.c): physical request 0x7E1 on both buses, functional 0x7DF on bus 1
 * only (DIAG_ID_UDS_FUNC_NONE = no functional address on bus 2), responses 0x7E9.
 * The RAM / data-flash flasher (flasher/fl_sh7254r.c, spec 16) uses the same pair on
 * bus 1. */
#define DIAG_ID_UDS_PHYS        0x7E1u
#define DIAG_ID_UDS_FUNC        0x7DFu
#define DIAG_ID_UDS_FUNC_NONE   0x000u
#define DIAG_ID_UDS_RSP         0x7E9u

/* Plant-session RAM dumper (plantdump/pd.c, a lab tool built standalone by
 * plantdump/build.sh with no include path): 0x7EA. Reference only — pd.c keeps its own
 * DUMP_ID literal; tests/unit/vehicle_seam_scan.py checks the two agree. */
#define DIAG_ID_PLANTDUMP       0x7EAu

/* BENCH-ONLY SHADOW SIMULATED-SPEED INPUT (2026-11-20, ~/tcu_sweep/shadow_sim_port.md):
 * 0x7F6 on bus 2 (CAN-C side; the bench tool's second Teensy port), received only in the
 * code-flash application built with TCU_SHADOW_SIM (never the RAM application or the
 * loader), consumed ONLY by the read-only shadows' hooks (io/vehicle/shadow_sim.h).
 * Layout, 8 bytes, big endian:
 *   b0..1 output speed rpm (u16)   b2..3 turbine speed rpm (u16)
 *   b4    pedal raw (the 0x120 byte-2 convention the shift-map hook uses)
 *   b5..6 engine torque Nm (s16)   b7 = 0xA5 ^ b0 ^ b1 ^ b2 ^ b3 ^ b4 ^ b5 ^ b6
 * Valid for 100 ms after the last good frame. */
#define DIAG_ID_SHADOW_SIM      0x7F6u
#define DIAG_BUS_SHADOW_SIM     CAN_BUS2

#endif

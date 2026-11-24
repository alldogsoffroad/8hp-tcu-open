/* fr_cal_nissan.c — OPTION C (2026-11-23): the fault-reaction shadow's comm monitor bound to
 * the NISSAN messages (TCU_VEHICLE=NISSAN only). HAND-WRITTEN, not generated: it replaces the
 * three per-entry tables of the generated app/fr_cal.c (fr_cal_ent, fr_cal_slot_grp,
 * fr_cal_rx — excluded there under TCU_VEHICLE_NISSAN); everything else (the comm cfg, the
 * groups, the reload / threshold / mask tables read in place at 0x191B30 / 0x191BB4 /
 * 0x191C38, the fault manager) stays the stock's.
 *
 * THE METHOD: the stock comm monitor (0x13661C) reads its per-entry lost-time reload, test-start
 * threshold and mask IN PLACE by entry index n. Each Nissan message therefore takes the entry
 * of a stock message of the SAME sending module and the SAME period class, so the stock's own
 * lost time / test start / key bit apply to it unchanged; the entry's group / slot (0xFFF85289)
 * are the stock's. Sources: ~/tcu_sweep/can_inputs_decode.md §2 (the stock table, groups,
 * lost times), docs/reference/nissan-ecm-tcm-interface.md "ECM → TCU inputs" and "Vehicle-log
 * check" (periods, counters [O]), the truck logs ~/Downloads/2026-09-1*_torque_*.txt (DLC [O]:
 * all 8 except 0x160 = 7). ABS/VDC as the sender of 0x280/0x284/0x285/0x354 is [I].
 *
 *  n   Nissan  sender  period  to(x10ms)  stock entry it takes (group, lost / test start)
 *  0   0x551   ECM     100 ms  20         0x101 ECM  100..200 ms class: 139, 800 / 800 ms
 *  1   0x1F9   ECM      10 ms   5         0x108 ECM  10 ms, key:        139, 500 / 650 ms
 *  3   0x180   ECM      10 ms   5         0x118 ECM  key:               139, 500 / 650 ms
 *  5   0x182   ECM      10 ms   5         0x122 ECM  key:               139, 500 / 650 ms
 *  6   0x160   ECM      10 ms   5         0x134 ECM  key:               139, 500 / 650 ms
 * 11   0x280   ABS      20 ms   5         0x140 ABS  key:               140, 160 / 650 ms
 * 12   0x284   ABS      20 ms   5         0x14C ABS  key:               140, 160 / 650 ms
 * 13   0x285   ABS      20 ms   5         0x158 ABS  key:               140, 160 / 650 ms
 * 33   0x354   ABS      40 ms   8         0x2E0 ABS  50 ms class:       140, 400 / 700 ms
 * 16   0x35D   BCM     100 ms  20         0x200 BCM  200 ms class:      146, 800 / 800 ms
 *
 * TIMEOUT RULE: the stock's own, read from its 70 RX objects (period +2, timeout +4, both
 * x 10 ms): timeout = max(5, 2 x period) — 1/2 -> 5, 4 -> 8, 5 -> 10, 6 -> 12, 10 -> 20,
 * 50 -> 100, 100 -> 200, 200 -> 400, no exception. (The receive profile, profile_nissan.c,
 * keeps its own 5 x period for signal validity; this is the fault monitor's.)
 *
 * CHECKS: coding 0x23 (timeout, bus state, DLC >= the logged DLC, tolerance 3) — no CRC /
 * counter. Nissan frames carry no CRC, and their counters do not fit the stock check (0x69F00
 * reads the UPPER nibble of byte DLC-2; Nissan's are LOW nibbles, 0x180 B6[3:0], 0x284 B6[3:0],
 * 0x182 B5[3:0]); the receive profile checks the 0x180 counter itself.
 *
 * INERT ENTRIES (every other n, 43 of 53): id 0 on bus 0 (the stock numbers buses 1 / 2: no
 * frame ever matches), coding 0x00 (the status stays 0: never lost, never invalid), slot 13
 * (0x13661C skips slots > 12: never counted, never reported). The U11E4 key test 0x1362B0 skips
 * them in the Nissan build (app/fr_shadow.c): group 136 fails when every NISSAN key message
 * (n = 1, 3, 5, 6, 11, 12, 13) is lost.
 *
 * FCA-ONLY GROUPS, NOT REPORTED (fr_cal_slot_grp 0): 141 U0102 transfer case (0x160 / 0x308 /
 * 0x161 — Nissan's 0x160 is the ECM's throttle frame, bound to 139 above), 142 U0103 gear shift
 * module (0x2EA / 0x2FA), 143 U11E3 shifter private bus (bus 2, 0x210 / 0x200), 145 U0212
 * steering column (0x102), 147 U0155 cluster (0x310), 144 / 166 / 170 / 171 (coded out or
 * config zero on the stock unit). No Nissan counterpart is monitored for them (the Nissan
 * shifter / lever source is open, D-N1); they stay untested in the fault manager. */
#define TCU_FR_SHADOW_TU 1
#include "app/fr_cal.h"

#if !defined(TCU_VEHICLE_NISSAN)
#error "fr_cal_nissan.c is the Nissan build's comm table: built only with TCU_VEHICLE=NISSAN"
#endif

#define NX { 0u, 13u, 13u }     /* inert: no group, slot 13 (skipped) */
const uint8_t  fr_cal_ent[FR_NE][3] = {
    { 139u, 1u, 1u }, { 139u, 1u, 1u }, NX,               { 139u, 1u, 1u }, NX,               { 139u, 1u, 1u }, { 139u, 1u, 1u }, NX,
    NX,               NX,               NX,               { 140u, 1u, 3u }, { 140u, 1u, 3u }, { 140u, 1u, 3u }, NX,               NX,
    { 146u, 1u, 5u }, NX,               NX,               NX,               NX,               NX,               NX,               NX,
    NX,               NX,               NX,               NX,               NX,               NX,               NX,               NX,
    NX,               { 140u, 1u, 3u }, NX,               NX,               NX,               NX,               NX,               NX,
    NX,               NX,               NX,               NX,               NX,               NX,               NX,               NX,
    NX,               NX,               NX,               NX,               NX,
};
/* the slot -> group map: ECM 139, ABS 140, BCM 146; the FCA-only slots 0 (not reported) */
const uint8_t  fr_cal_slot_grp[13] = { 0u, 139u, 0u, 140u, 0u, 146u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };

/* id, bus, DLC, timeout (x 10 ms), coding, CRC, counter, the three tolerances, then the stock
 * witness fields (handle, status byte, ROM object, coding cal address): 0 — no stock object */
#define RX_NX { 0x000u, 0u, 0u, 0u, 0x00u, 0u, 0u, 0u, 0u, 0u, 0x00u, 0u, 0u, 0u }
#define RX_N(id, dlc, to) { (id), 1u, (dlc), (to), 0x23u, 0u, 0u, 0u, 0u, 3u, 0x00u, 0u, 0u, 0u }
const fr_cal_rx_t fr_cal_rx[FR_NE] = {
    RX_N(0x551u, 8u, 20u),  /* 0  ECM ENGINE_5 (coolant), 100 ms */
    RX_N(0x1F9u, 8u, 5u),   /* 1  ECM ENGINE_1 (engine speed), 10 ms */
    RX_NX,                  /* 2 */
    RX_N(0x180u, 8u, 5u),   /* 3  ECM engine torques / speed mirror, 10 ms */
    RX_NX,                  /* 4 */
    RX_N(0x182u, 8u, 5u),   /* 5  ECM, 10 ms */
    RX_N(0x160u, 7u, 5u),   /* 6  ECM throttle / pedal, 10 ms */
    RX_NX, RX_NX, RX_NX, RX_NX,                                         /* 7..10 */
    RX_N(0x280u, 8u, 5u),   /* 11 ABS vehicle speed, 20 ms */
    RX_N(0x284u, 8u, 5u),   /* 12 ABS wheel speeds 1/2, 20 ms */
    RX_N(0x285u, 8u, 5u),   /* 13 ABS wheel speeds 3/4, 20 ms */
    RX_NX, RX_NX,                                                       /* 14, 15 */
    RX_N(0x35Du, 8u, 20u),  /* 16 BCM brake switch, 100 ms */
    RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX,             /* 17..24 */
    RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX,             /* 25..32 */
    RX_N(0x354u, 8u, 8u),   /* 33 ABS brake light, 40 ms */
    RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX,             /* 34..41 */
    RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX, RX_NX,             /* 42..49 */
    RX_NX, RX_NX, RX_NX,                                                /* 50..52 */
};

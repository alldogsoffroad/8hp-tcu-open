/* os_sel.h — G5b (2026-11-19, ~/tcu_sweep/g5b_port.md; plan
 * ~/tcu_sweep/g5_mode2_plan.md §5 G5b, decode os_mode_selection_decode.md):
 * THE OS-MODE REQUEST PATH, LIVE. Every function ported from the stock bytes
 * (sh-elf-objdump -b binary -m sh2a -EB of the September image) [O]:
 *
 *   0x1418E2  os_sel_tick_10ms   the safety layer's mode selector (10 ms slot
 *             0 0x131FD8 → 0x14F34C, after the engine conditioning 0x13BF66
 *             and BEFORE the terminal-15 machine 0x140B80): writes the
 *             request +7 = 0xFFF95483 of object 0xFFF9547C
 *   0x1507BE  (inside 0x14F34C) object +4 / +6 ← E90E / E910 (the 0x108
 *             engine speed, held while invalid, and its status 0 / 2)
 *   0x68EA8   os_sel_echo_10ms   the 10 ms pre-hook (via 0x6D6E0): on a
 *             change of 0x848CF (last seen in 0xFFF93B56) the applied-mode
 *             echo +8 = 0xFFF95484 = 1 / 2 / 3 (0x1320BC / C4 / CC); a mode
 *             >= 4 counts as 1
 *   0x68EFE   os_sel_handoff_10ms  the 10 ms post-hook (via 0x6D6EC, under
 *             the OS lock 0x9A428 / 0x9A3FC): 0x93B5E = 0x95483 when
 *             0x848CF != 0 and the request is <= 3. bsw/sup/sup_mode.c's
 *             0x6F0B4 (5 ms) takes it as pending, 0x66E06 applies it at the
 *             5 ms task's tail (0x67A34, modes != 0) — UNCHANGED code
 *   0xB983C / 0xB9854 / 0xB9824  os_sel_flags_10ms  slot 26 of the 10 ms
 *             lists of modes 1 / 2 / 3 (0xA2EAC / 0xA2EA4 / 0xA2EB4): the
 *             one-hot flags 0x89C3C..3E and 0x89C53..55 (GBR 0x89C3C)
 *
 * STAND-INS (user decisions 2026-11-19):
 *   D-G5-3  0xFFF9584A (0x14DC70: no fault reaction 0x38, overvoltage
 *           detector 0x9584D not started) held at 1 — mode 3 is never
 *           selected. Kept in the object at +9 (not at its stock address).
 *   D-G5-4  the turbine term as the stock: E7E8 >= 100 with E7EC == 0 — our
 *           turbine sensor rpm (0 when invalid or negative) and its quality.
 *   D-G5-2  the object 0xFFF9547C lives in r04r's retired .bss pad
 *           (app/rq_retired.c, 0xFFF95F7C, 0x20 B; linker ASSERT) so that no
 *           .bss / .noinit symbol moves; the stock addresses 0x9547C..84 lie
 *           inside our .bss. The six flags and 0x93B56 are at their stock
 *           addresses (.os_flg1 / .os_flg2 NOLOAD; 0x93B56 inside .sup_fsm).
 *   D-G5-6  0x89C3D is written here but the engine does NOT read it (its
 *           GS_FORCED c3d = 0 stays): G5b changes only the OS mode.
 *           G5c (r04v, ~/tcu_sweep/g5c_port.md): the engine reads it now
 *           (app/main.c gs_in_build → gs_in_t.c3d, after this task's slot 26).
 *
 * NOT PORTED: 0x14DC70 / 0x14E170 (the overvoltage detector, mode 3), the
 * row-9 init list of modes 2 / 3 (0x7DC30: 0xFFF80D1C, below our RAM), and
 * every mode-2-only function (D-G5-7: 0xE8CF6, 0xD229E, 0xE8BFA, 0xF02EA /
 * 0xF0D5C, the WK lock-up active list, 0xD46A0, 0xCA65C, 0xD8122, 0xDD2C4 —
 * the link-map scan tests/unit/g6a_scan.py refuses their names).
 *
 * Code-flash application only: empty under TCU_RAM_APP (the RAM application
 * stays mode 1, byte-identical). This TU owns NO .bss / .data (its state is
 * at fixed addresses), so adding it to the link moves nothing. */
#ifndef TCU_OS_SEL_H
#define TCU_OS_SEL_H

#include <stdint.h>

#define OS_SEL_OBJ      0xFFF95F7Cu     /* the stock object 0xFFF9547C, relocated (D-G5-2) */
#define OS_SEL_ECHO_LAST 0xFFF93B56u    /* 0x68EA8's last-seen 0x848CF (stock address) */
#define OS_FLG_C3C      0xFFF89C3Cu     /* mode 3 (park_law's c3c source in the stock) */
#define OS_FLG_C3D      0xFFF89C3Du     /* mode 1 */
#define OS_FLG_C3E      0xFFF89C3Eu     /* mode 2 */
#define OS_FLG_C53      0xFFF89C53u     /* mode 2 (+0x17) */
#define OS_FLG_C54      0xFFF89C54u     /* mode 3 (+0x18) */
#define OS_FLG_C55      0xFFF89C55u     /* mode 1 (+0x19) */

/* the selector cal (table 0x649F0 → 0x195F64..6A) [O image bytes] */
#define OS_SEL_ENG_MIN  150u            /* 0x195F64 */
#define OS_SEL_TUR_MIN  100u            /* 0x195F66 */
#define OS_SEL_SUP_MAX  17000u          /* 0x195F68 (mV, strict) */
#define OS_SEL_SUP_MIN  6000u           /* 0x195F6A (mV, strict) */
#define OS_SEL_TBL      0x000649F0u     /* +0: the stock's cal pointer table (a marker here) */

/* the object, stock offsets +0..+8, then OUR witnesses (+9..+0x1F) */
typedef struct {
    uint32_t tbl;           /* +0    the stock's pointer table 0x649F0 (marker; never used) */
    uint16_t eng;           /* +4    E90E: the 0x108 engine speed, held while invalid */
    uint8_t  eng_st;        /* +6    E910: 0 valid, 2 invalid */
    uint8_t  req;           /* +7    0xFFF95483: THE REQUEST */
    uint8_t  app;           /* +8    0xFFF95484: the applied-mode echo (0x68EA8) */
    uint8_t  f584a;         /* +9    0xFFF9584A stand-in 1 (D-G5-3) */
    uint8_t  e9ff;          /* +A    E9FF as the selector read it (the previous pass's) */
    uint8_t  tur_st;        /* +B    E7EC as read: 0 sensor ok */
    uint16_t tur;           /* +C    E7E8 as read (rpm) */
    uint16_t sup;           /* +E    0x96066 as read (mV) */
    uint32_t n;             /* +10   selector passes (modes 1..3) */
    uint32_t n2;            /* +14   passes requesting 2 */
    uint32_t t2;            /* +18   TCNTA (µs) at the echo's first 2 (0 = never) */
    uint16_t chg;           /* +1C   request changes */
    uint8_t  hand;          /* +1E   the last value 0x68EFE handed to 0x93B5E (0xFF = none yet) */
    uint8_t  spd;           /* +1F   this pass's speed term */
} os_sel_t;
_Static_assert(sizeof(os_sel_t) == 0x20u, "os_sel_t fills r04r's 0x20-byte pad exactly");

/* the selector's inputs, by value (app/main.c builds them) */
typedef struct {
    uint16_t eng;           /* vehicle_in()->engine_speed */
    uint8_t  eng_valid;     /* q_engine_speed == Q_OK */
    uint8_t  tur_ok;        /* q_turbine_speed == Q_OK (E7EC == 0) */
    uint16_t tur;           /* the turbine rpm, 0 when invalid or negative (E7E8) */
    uint16_t sup;           /* the supply 0xFFF94F86 (0x93920 → 0x96066 in the stock) */
    uint8_t  e9ff;          /* kl15_e9ff() BEFORE this pass's kl15_tick_10ms */
    uint8_t  pad;
} os_sel_in_t;

void os_sel_init(void);
/* the four 10 ms pieces, in task_10ms order; each runs in OS modes 1..3
 * only (the mode at the task's start: the stock's per-mode task object) */
void os_sel_echo_10ms(uint8_t mode);                             /* 0x68EA8, task head */
void os_sel_tick_10ms(uint8_t mode, const os_sel_in_t *in);      /* 0x1418E2, before kl15 */
void os_sel_flags_10ms(uint8_t mode);                            /* slot 26 */
void os_sel_handoff_10ms(uint8_t mode);                          /* 0x68EFE, task tail */

/* the pure law of 0x1418E2 (cur = +8; for the host tests) */
uint8_t os_sel_law(uint8_t cur, uint8_t e9ff, uint8_t f584a, uint16_t eng, uint8_t eng_st,
                   uint16_t tur, uint8_t tur_st, uint16_t sup, uint8_t *spd);

const volatile os_sel_t *os_sel_state(void);
uint8_t os_sel_req(void);   /* +7 (the gs_bridge m2 cross-check) */

#endif /* TCU_OS_SEL_H */

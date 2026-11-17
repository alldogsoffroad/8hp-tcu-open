/* park_pair.h — the safety layer's park position pair object 0xFFF8EA28
 * and the DTC group 98 monitor, READ-ONLY (2026-11-16;
 * ~/tcu_sweep/park_sensor_decode.md §2.1–2.2, park_position_speed_decode.md
 * §1–2, park_sensor_port.md). Ported 1:1 from the stock bytes:
 *
 *   0x141B1C  init (via 0x132D58) + the object's cal pointer +0 (0x1327B2:
 *             descriptor 0x64A24 index 0 → 0x00064A1C) and timer +4/+8
 *   0x13418A..0x13420E  the gather: twelve getter slots 0x4C724..0x4C750
 *             into +0x0C..+0x17, then +0x0D / +0x11 |= 0xFE, then 0x141B42
 *   0x141B42  → 0x141BA2 (status +0x1C, position +0x18 = 0xFFF8EA40,
 *             qualifier +0x1A = 0xFFF8EA42; 0x141AF4 the timer pair check),
 *             then the change flags +0x1E
 *   0x141E06  group 98: P07B2 (source 12) / P07B8 (13) / P0850 m0 (0) /
 *             P0850 m1 (1); the reporter 0xA0E70 is replaced by witness
 *             cells, 0xA0FE8 ("stored source") is stubbed "nothing stored"
 *
 * Position (+0x18): A = contact A = AN5 (+0x0C), B = AN6 (+0x10);
 *   status 0 and A = 0, B = 1 → 0; A = 1, B = 0 → 1; else (both active,
 *   timing) → 2; any status bit → 3 with qualifier 2.
 * Status (+0x1C): bit 0 A electrical (fault byte 0x847FA), bit 1 B
 *   electrical (0x847FB), bit 2 both inactive, bit 3 sensor-supply fault
 *   (0x84A1F — group 33 not ported: stand-in 0 / FF, the dump's), bit 4 both
 *   active with the timer +4 (reload 100 ticks) run out.
 *
 * DEVIATIONS: scheduled at 10 ms from task_10ms (the stock rate of the
 * gather 0x133C4E region and of 0x14F7EA is not decoded); 0x141B42 runs
 * once per slice (the stock also calls it from a second site, 0x14F6EE,
 * rate undecoded — so the P0850 m1 timer is 100 of OUR ticks = 1.0 s);
 * the fatal 0x794C8 (+0x7A914, hang) is a counter + breadcrumb and the
 * slice returns; the PFM checkpoints 0x148E92 / 0x148EB6 and the RAM byte
 * test 0x148ACC are dropped.
 *
 * READ-ONLY: TCU_OUTPUT_PATH refuses this header (see drv_pksens.h).
 */
#ifndef TCU_PARK_PAIR_H
#define TCU_PARK_PAIR_H

#if defined(TCU_OUTPUT_PATH)
#error "park position sensor is read-only: no output path (range, park law, park demands, bus A, request law, group A) may include park_pair.h"
#endif

#include <stdint.h>

#define PKP_OBJ             0xFFF8EA28u
#define CELL_PKP_U8(o)      (*(volatile uint8_t  *)(uintptr_t)(PKP_OBJ + (o)))
#define CELL_PKP_U32(o)     (*(volatile uint32_t *)(uintptr_t)(PKP_OBJ + (o)))
#define CELL_PKP_POS        CELL_PKP_U8(0x18u)   /* 0xFFF8EA40 → D61A[0x2E] (stock) */
#define CELL_PKP_QUAL       CELL_PKP_U8(0x1Au)   /* 0xFFF8EA42 → D61A[0x38] (stock) */
#define CELL_PKP_STATUS     CELL_PKP_U8(0x1Cu)   /* 0xFFF8EA44 */
#define CELL_PKP_CHG        CELL_PKP_U8(0x1Eu)   /* 0xFFF8EA46 change flags 4 (A) / 8 (B) */

/* group-98 identity bytes [O]: 0x63243 = 98, sources 0x632E6 = 0,
 * 0x632E7 = 1, 0x632F2 = 12, 0x632F3 = 13 */
#define PKP_G98             98u
#define PKP_SRC_P0850_M0    0u
#define PKP_SRC_P0850_M1    1u
#define PKP_SRC_P07B2       12u
#define PKP_SRC_P07B8       13u

void park_pair_init(void);
/* the gather + 0x141B42 + 0x141E06 (task_10ms) */
void park_pair_tick_10ms(void);

/* witnesses (ours, .pk_wit) */
typedef struct {
    uint8_t  source;        /* the last 0xA0E70 call's source */
    uint8_t  failed;
    uint8_t  tested;
    uint32_t calls;         /* 0x141E06 calls */
    uint32_t fail_n;        /* calls with failed = 1 */
    uint32_t pass_n;        /* calls with failed = 0 and tested = 1 */
    uint32_t fatal_n;       /* 0x794C8 hits (torn pairs) */
    uint32_t fatal_site;    /* the stock address of the last check that hit */
    uint32_t fatal_t;       /* plat_frt_us() at it */
} park_pair_wit_t;
void park_pair_witness(park_pair_wit_t *w);
/* 0x14F7EA → 0x141E06 (group 98), the stock's 100 ms slot of modes 1..3 */
void park_pair_g98_100ms(void);
/* 1 once after each park_pair_g98_100ms() call (consumed) */
uint8_t park_pair_take_report(void);

#endif /* TCU_PARK_PAIR_H */

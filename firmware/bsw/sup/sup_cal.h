/* sup_cal.h — bench calibration variables (design §2 A3, §12).
 *
 * All default OFF / stock values. These are the knobs for P8 calibration
 * sweeps: each re-enables a bench-proven behavior that the stock never
 * performs, isolated behind a flag so the pure architecture is measured
 * first and re-adding one variable is a one-line rebuild.
 */
#ifndef TCU_SUP_CAL_H
#define TCU_SUP_CAL_H

/* re-add the E60l init handshake burst (W1-zero + 84|1F ×3) */
/* #define TCU_BENCH_HANDSHAKE 1 */

/* send 84|3F from walk arm until the 0x06 era fires (the bench's
 * load-bearing opening word — the stock never sends it) */
/* #define TCU_BENCH_LEAD_3F 1 */

/* override the walk timer tick for a calibration run (ms) */
/* #define TCU_BENCH_WALK_TICK_MS 2 */

/* resolve to the effective walk tick */
#ifdef TCU_BENCH_WALK_TICK_MS
#define SUP_WALK_TICK_MS TCU_BENCH_WALK_TICK_MS
#else
#define SUP_WALK_TICK_MS 1u
#endif

#endif

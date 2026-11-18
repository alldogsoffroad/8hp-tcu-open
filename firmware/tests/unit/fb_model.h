/* fb_model.h — the host model of the feedback shadow's hardware
 * (platform/plat_fbhw.h on the host): Timer F PWM-measurement channels driven
 * by waveforms (manual §13.26.4 mode 100, as the decode §1.2 reads it), the
 * PHPR / PFPR pin levels, and the Timer-D0 C-compare channel 0 that raises
 * the sampler ISR (fb_shadow_isr) at each compare match.
 *
 * Time: µs (Timer D0 counts 1 per µs); Timer F counts at 4 MHz (4 per µs).
 * Waveform per Timer F number n: active LOW for `low` counts of every
 * `period` counts, starting low at the phase origin. EGSEL rising (sub 0)
 * captures at rising edges with GRAF = the low time since the last capture;
 * falling (sub 1) at falling edges with GRAF = the high time. */
#ifndef TCU_TESTS_FB_MODEL_H
#define TCU_TESTS_FB_MODEL_H

#include <stdint.h>
#include <stdbool.h>

void     fbm_reset(void);
/* n: Timer F number; egsel 1 rising / 2 falling (the F1 configuration) */
void     fbm_config(uint8_t n, uint8_t egsel);
void     fbm_set_pwm(uint8_t n, uint32_t period, uint32_t low, uint32_t phase);
void     fbm_set_static(uint8_t n, bool high);
void     fbm_advance_to(uint32_t t_us);         /* runs the compare-match ISRs on the way */
uint32_t fbm_now(void);
unsigned fbm_isr_n(void);
unsigned fbm_grbf_writes(void);
bool     fbm_armed(void);
uint32_t fbm_ipr10(void);
unsigned fbm_isr_acc_max(void);    /* peripheral accesses of the longest ISR */
unsigned fbm_acc(void);            /* peripheral accesses since fbm_acc_reset */
void     fbm_acc_reset(void);

#endif

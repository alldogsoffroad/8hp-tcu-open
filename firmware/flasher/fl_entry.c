/* Flasher entry on the target (spec 16 section 3). fl_start.S sets the stack, masks interrupts and
 * clears .bss, then calls fl_cmain(). From here on nothing outside the flasher's RAM image runs. */
#include "flasher/fl_core.h"
#include "flasher/fl_hal.h"
#include "flasher/fl_regs.h"

#include <stddef.h>

#define VECTORS 512u

static uint32_t s_vectors[VECTORS] __attribute__((aligned(1024)));

/* Any exception or NMI: stop a running P/E operation with a full FCU reset (FRESET held well
 * beyond tRESW2), then reset through the watchdog. */
static void fl_trap(void)
{
    fl_wr16(FL_FRESETR, 0xCC01u);
    for (volatile uint32_t i = 0u; i < 20000u; i++) {
    }
    fl_wr16(FL_FRESETR, 0xCC00u);
    fl_hal_reset();
}

void fl_cmain(void);

void fl_cmain(void)
{
    for (uint32_t i = 0u; i < VECTORS; i++) {
        s_vectors[i] = (uint32_t)(uintptr_t)fl_trap;
    }
    __asm__ volatile ("ldc %0, vbr" : : "r"(s_vectors));
    fl_hal_init();
    fl_core_init(&fl_target_rom);
    fl_main_loop();
}

/* GCC may emit calls to these for aggregate initialisation; the flasher links without libc. */
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);

void *memset(void *d, int c, size_t n)
{
    uint8_t *p = d;
    while (n-- != 0u) {
        *p++ = (uint8_t)c;
    }
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n-- != 0u) {
        *p++ = *q++;
    }
    return d;
}

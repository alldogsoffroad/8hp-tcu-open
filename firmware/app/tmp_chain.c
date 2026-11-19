/* tmp_chain.c — the temperature byte 0xFFF89C13, live (tmp_chain.h).
 * ~/tcu_sweep/temp_byte_live_port.md.
 */
#include "app/tmp_chain.h"
#include "safety/tmp_safe.h"

extern void tmp_chain_hook_100ms(void) __attribute__((weak));
/* the 30 ms list 0x4D940 runs the shift target (entries 5 / 14 / 15 / 16)
 * before 0xB9646 (entry 19): main.c hands it the read-only shift-map shadow
 * (app/sm_shadow.h) */
extern void tmp_chain_hook_30ms(void) __attribute__((weak));

#if defined(TCU_RAM_APP)
/* not in the RAM application (no temperature acquisition there) */
void tmp_chain_init(void) {}
void tmp_chain_boot_pass(void) {}
void tmp_chain_task_10ms(uint8_t os_mode) { (void)os_mode; }
void tmp_chain_task_slow(uint8_t os_mode) { (void)os_mode; }
void tmp_chain_b9646(void) {}
void tmp_chain_b986c(void) {}
#else

/* the 30 / 100 ms phase: 0 = this 10 ms tick fires it (counted from the
 * first mode-1..3 tick: 0x67A08 copies the 10 ms countdown into the 30 /
 * 100 ms ones at the switch) */
#if defined(__sh__)
#define TWIT __attribute__((section(".tmp_wit")))   /* low RAM: the stack guard */
#else
#define TWIT
#endif
static uint8_t s_ph30 TWIT;
static uint8_t s_ph100 TWIT;
static uint8_t s_slow_on TWIT;

/* 0xB9646 (GBR = 0xFFF89C3C): +0x1B and +0x96 = 0x140050() */
void tmp_chain_b9646(void)
{
    const uint8_t v = tmp_safe_byte();
    TMP_CELL_89C57 = v;                     /* mov.b r0,@(27,gbr) */
    TMP_CELL_89CD2 = v;                     /* mov.b r0,@(150,gbr) */
    /* +0x60/+0xCF (0x140060), +0x1C/+0x97 (0x13BAA8), +0x1D/+0x98
     * (0x140068), +0x3E/+0xAE (8000): not ported (no reader here) */
}

/* 0xB986C @0xB9D66: 0xFFF89C04 + 0xF = +0x96 (the rest of 0xB986C is not
 * ported) */
void tmp_chain_b986c(void)
{
    TMP_CELL_89C13 = TMP_CELL_89CD2;
}

void tmp_chain_init(void)
{
    TMP_CELL_89C13 = 0u;
    TMP_CELL_89C57 = 0u;
    TMP_CELL_89CD2 = 0u;
    s_ph30 = 0u;
    s_ph100 = 0u;
    s_slow_on = 0u;
    tmp_safe_init();
}

void tmp_chain_boot_pass(void)
{
    tmp_safe_boot_pass();                   /* row-1 entry 0: 0x14F32C → 0x151C9C */
    tmp_chain_b9646();                      /* entry 6: 0xA2D88 → 0xB9646 */
    tmp_chain_b986c();                      /*                  → 0xB986C */
}

void tmp_chain_task_10ms(uint8_t os_mode)
{
    if (os_mode == 0u) {
        tmp_safe_mode0_10ms();              /* 0x4D7A0 entry 0: 0x1320B0 → 0x132DCE */
        s_slow_on = 0u;
    } else if (os_mode <= 3u) {
        tmp_chain_b986c();                  /* 0x4D834 entry 29: 0xA2EFC → 0xB986C */
    }
}

void tmp_chain_task_slow(uint8_t os_mode)
{
    if (os_mode < 1u || os_mode > 3u) {
        return;
    }
    if (s_slow_on == 0u) {
        s_slow_on = 1u;                     /* the switch: both phases at 0 */
        s_ph30 = 0u;
        s_ph100 = 0u;
    }
    if (s_ph30 == 0u) {
        if (tmp_chain_hook_30ms) {
            tmp_chain_hook_30ms();          /* entries 5 / 14 / 15 / 16 (main.c: the shift-map shadow) */
        }
        tmp_chain_b9646();                  /* 30 ms: 0x4D940 entry 19 0xA318A */
    }
    if (s_ph100 == 0u) {
        tmp_safe_100ms();                   /* 100 ms: 0x4D9E4 entry 0 0x13204A */
        if (tmp_chain_hook_100ms) {
            tmp_chain_hook_100ms();         /* the rest of 0x14F7EA (main.c: 0x141E06) */
        }
    }
    s_ph30 = (uint8_t)(s_ph30 + 1u >= 3u ? 0u : s_ph30 + 1u);
    s_ph100 = (uint8_t)(s_ph100 + 1u >= 10u ? 0u : s_ph100 + 1u);
}
#endif /* !TCU_RAM_APP */

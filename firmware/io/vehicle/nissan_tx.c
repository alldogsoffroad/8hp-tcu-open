/* Nissan transmit stub: sends nothing (see nissan_tx.h). */
#include "io/vehicle/nissan_tx.h"

/* BSS LAYOUT PAD (SH code-flash application only). The FCA transmit set (fca_tx.c) holds 240 bytes
 * of .bss (s_val 144 + s_st 96) at this position in the link, and the com_tx.c it alone references
 * another 16 (garbage-collected without it) - 256 bytes in all, and several cells after it are pinned
 * by link-time asserts (the retired pad 0xFFF95F7C, safety/os_sel.h OS_SEL_OBJ, ...). The Nissan
 * stub has no state, so it holds the same span: every later .bss symbol keeps its FCA address.
 * nissan_tx_init() touches it so --gc-sections keeps it. Never read for any other purpose. */
#if defined(__sh__) && !defined(TCU_RAM_APP)
static volatile uint8_t s_fca_bss_pad[256] __attribute__((aligned(4)));
#endif

void nissan_tx_init(void)
{
#if defined(__sh__) && !defined(TCU_RAM_APP)
    s_fca_bss_pad[0] = 0u;
#endif
}

void nissan_tx_tick_1ms(void)
{
    /* TODO(capture): 0x177 / 0x421 and whatever else the Nissan ECM expects from a TCM */
}

bool nissan_tx_enable(bool on)
{
    (void)on;
    return false;
}

uint32_t nissan_tx_sent_count(void)
{
    return 0u;
}

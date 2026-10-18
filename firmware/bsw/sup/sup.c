#include "bsw/sup/sup.h"

#include <string.h>

static sup_status_t s_st;
static bool s_toggle;
static bool s_enable_req;
static bool s_flash_mode;
static bool s_pattern;
static uint16_t s_pat_word, s_pat_toggle;

void sup_init(void)
{
    memset(&s_st, 0, sizeof s_st);
    s_toggle = false;
    s_enable_req = false;
    s_flash_mode = false;
    s_pattern = false;
    plat_sup_init();
}

void sup_set_output_enable(bool on)
{
    s_enable_req = on;
}

void sup_set_flash_mode(bool on)
{
    s_flash_mode = on;
}

void sup_set_pattern(bool pattern_on, uint16_t word, uint16_t toggle)
{
    s_pat_word = (uint16_t)(word & ~SUP_BIT_ENABLE);
    s_pat_toggle = (uint16_t)(toggle & ~SUP_BIT_ENABLE);
    s_pattern = pattern_on;
}

void sup_tick_5ms(void)
{
    uint16_t rx = 0u;
    uint16_t tx;
    uint8_t r;

    s_toggle = !s_toggle;
    if (s_pattern) {
        tx = (uint16_t)(s_pat_word ^ (s_toggle ? s_pat_toggle : 0u));
    } else if (s_flash_mode) {
        tx = (uint16_t)(SUP_CMD_CONTROL | SUP_BIT_FL_MODE | (s_toggle ? SUP_BIT_FL_TOGGLE : 0u));
    } else {
        tx = (uint16_t)(SUP_CMD_CONTROL | (s_toggle ? SUP_BIT_TOGGLE : 0u) |
                        (s_enable_req ? SUP_BIT_ENABLE : 0u));
    }
    r = plat_sup_exchange(tx, &rx);
    if ((r & 2u) != 0u) {
        s_st.last_rx = rx;
        if ((rx & SUP_REPLY_MASK) == SUP_REPLY_OK) {
            s_st.replies_ok++;
        } else {
            s_st.replies_bad++;
        }
    }
    if ((r & 1u) != 0u) {
        s_st.last_tx = tx;
        s_st.sent++;
        s_st.enable = (tx & SUP_BIT_ENABLE) != 0u;
    } else {
        s_st.busy++;
        s_toggle = !s_toggle;           /* not sent: keep the toggle phase for the next try */
    }
}

const sup_status_t *sup_status(void)
{
    return &s_st;
}

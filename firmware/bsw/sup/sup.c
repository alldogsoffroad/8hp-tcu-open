#include "bsw/sup/sup.h"

#include <string.h>

static sup_status_t s_st;
static bool s_toggle;
static bool s_enable_req;

void sup_init(void)
{
    memset(&s_st, 0, sizeof s_st);
    s_toggle = false;
    s_enable_req = false;
    plat_sup_init();
}

void sup_set_output_enable(bool on)
{
    s_enable_req = on;
}

void sup_tick_5ms(void)
{
    uint16_t rx = 0u;
    uint16_t tx;
    uint8_t r;

    s_toggle = !s_toggle;
    tx = (uint16_t)(SUP_CMD_CONTROL | (s_toggle ? SUP_BIT_TOGGLE : 0u) |
                    (s_enable_req ? SUP_BIT_ENABLE : 0u));
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

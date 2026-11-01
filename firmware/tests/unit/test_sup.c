/* Unit test: external supervisor service (toggle, output enable, reply check). */
#include "bsw/sup/sup.h"
#include "bsw/sup/sup_engage.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

/* ---- fake supervisor for the engage sequence (spec 03 challenge-response) ---- */
static struct {
    uint8_t sel;         /* rolling challenge */
    uint8_t lvl;         /* achieved level, reported in the 0x0C read */
    uint8_t lvl_cmd;     /* last level write, echoed by the 0x0A read */
    uint8_t reads;       /* 0x0C read counter: rotates the challenge every 3rd */
    bool    accept;      /* 0x22 echo carries bit 0 (the acceptance grant) */
    uint16_t last_8e;
} fs;

static uint16_t fake_sup(uint16_t tx)
{
    static uint16_t prev;
    uint16_t r = 0xAA00u;

    switch (tx >> 8) {
    case 0x88:
        fs.lvl = fs.lvl_cmd;             /* a token round settles the level */
        break;
    case 0x84:
        fs.lvl_cmd = (uint8_t)(tx & 0x3Fu);
        break;
    case 0x12:
        r = 0xAAC2u;                     /* commit ack */
        break;
    case 0x0C:
        /* the cyclic mixed job [0x18, 0x0C, ...] reads the feedback too, but the
         * challenge only rotates on register-job reads (bench v3: fb constant
         * between register jobs while the cyclic stream runs at 200 Hz) */
        if (prev != 0x1800u && ++fs.reads % 3u == 0u) {
            fs.sel = (uint8_t)((fs.sel + 1u) & 0xFu);
        }
        r = (uint16_t)(0xAA00u | ((fs.lvl & 7u) << 4) | fs.sel);
        break;
    case 0x0A:
        r = (uint16_t)(0xAA00u | fs.lvl_cmd);
        break;
    case 0x22:
        r = (uint16_t)(0xA800u | (fs.accept ? 1u : 0u));  /* the grant echo */
        break;
    case 0x8E:
        fs.last_8e = tx;
        r = 0xA800u;
        break;
    default:
        break;
    }
    prev = tx;
    return r;
}

/* i-th frame counting back from the newest (the stub keeps a 64-word ring) */
static uint16_t back(unsigned i)
{
    return stub.sup_tx[(stub.sup_n - 1u - i) % 64u];
}

int main(void)
{
    const sup_status_t *s = sup_status();

    sup_init();
    stub.sup_reply = 0xA804u;
    for (int i = 0; i < 6; i++) {
        sup_tick_5ms();
    }
    /* command 0x8E, bit 2 alternating, output enable off */
    CHECK(stub.sup_n == 6u);
    for (unsigned i = 0u; i < stub.sup_n; i++) {
        CHECK((stub.sup_tx[i] & 0xFF00u) == 0x8E00u);
        CHECK((stub.sup_tx[i] & 0x01u) == 0u);
        if (i > 0u) {
            CHECK(((stub.sup_tx[i] ^ stub.sup_tx[i - 1u]) & 0x04u) != 0u);
        }
    }
    CHECK(s->replies_ok == 5u && s->replies_bad == 0u && !s->enable);

    /* busy transmitter: no frame, toggle phase kept */
    stub.sup_busy = true;
    sup_tick_5ms();
    stub.sup_busy = false;
    sup_tick_5ms();
    CHECK(stub.sup_n == 7u && s->busy == 1u);
    CHECK(((stub.sup_tx[6] ^ stub.sup_tx[5]) & 0x04u) != 0u);

    /* output enable request mask: several sources, each clears only its own bit */
    sup_enable_set(SUP_EN_BENCH);
    sup_tick_5ms();
    CHECK((stub.sup_tx[7] & 0x01u) == 1u && s->enable);
    sup_enable_set(SUP_EN_PARK);
    sup_enable_clear(SUP_EN_BENCH);     /* bench done, park still needs it */
    sup_tick_5ms();
    CHECK((stub.sup_tx[8] & 0x01u) == 1u && s->enable);
    sup_enable_clear(SUP_EN_PARK);
    sup_tick_5ms();
    CHECK((stub.sup_tx[9] & 0x01u) == 0u && !s->enable);

    /* wrong reply header counted */
    stub.sup_reply = 0x0000u;
    sup_tick_5ms();
    CHECK(s->replies_bad == 1u);

    /* flash programming mode: 0x8E80 with bit 4 toggling, no enable, no bit 2 */
    sup_enable_set(SUP_EN_BENCH);
    sup_set_flash_mode(true);
    stub.sup_reply = 0xA804u;
    sup_tick_5ms();
    sup_tick_5ms();
    CHECK((stub.sup_tx[11] & 0xFF85u) == 0x8E80u && (stub.sup_tx[12] & 0xFF85u) == 0x8E80u);
    CHECK(((stub.sup_tx[11] ^ stub.sup_tx[12]) & 0x10u) != 0u && !s->enable);
    sup_set_flash_mode(false);
    sup_enable_clear(SUP_EN_BENCH);
    sup_tick_5ms();
    CHECK((stub.sup_tx[13] & 0xFFF1u) == 0x8E00u);

    /* bench pattern: word ^ toggle on alternate frames, enable bit always cleared
     * (phase-agnostic: the toggle phase depends on the ticks before this point) */
    sup_set_pattern(true, 0x8401u, 0x0001u);
    sup_tick_5ms();
    sup_tick_5ms();
    CHECK((stub.sup_tx[14] | stub.sup_tx[15]) == 0x8401u &&
          (stub.sup_tx[14] & stub.sup_tx[15]) == 0x8400u);
    sup_set_pattern(true, 0x8E81u, 0x0011u);
    sup_tick_5ms();
    sup_tick_5ms();
    CHECK((stub.sup_tx[16] | stub.sup_tx[17]) == 0x8E90u && (stub.sup_tx[16] & stub.sup_tx[17]) == 0x8E80u);
    sup_set_pattern(false, 0u, 0u);
    sup_tick_5ms();
    CHECK((stub.sup_tx[18] & 0xFFF1u) == 0x8E00u);

    /* ---- stock application pattern (FE13, spec 03 section 4.4) ----
     * One 5 ms pass = the three cyclic jobs, 8 frames: [8E|b, C9, 22, 09, 90|b, 24, 28, 2E].
     * Bit 2 self-toggles every pass (the supervisor watchdog's life sign; bench
     * 2026-10-30: a constant word resets the TCU). The scripted echo 0xA800 never
     * matches, so echo_bad counts every pass. */
    {
        uint32_t sent0 = s->sent, busy0 = s->busy;
        stub.sup_xfer_rx = 0xA800u;
        sup_app_pattern(true, 0x40u, 0x21u);
        CHECK(s->app_pattern);
        sup_tick_5ms();
        CHECK(stub.sup_n == 19u + 8u);              /* 8 frames recorded */
        CHECK((stub.sup_tx[19] & 0xFFFBu) == 0x8E40u);   /* state-6 word, toggle phase 1 */
        CHECK(stub.sup_tx[20] == 0xC900u && stub.sup_tx[21] == 0x2200u &&
              stub.sup_tx[22] == 0x0900u);
        CHECK(stub.sup_tx[23] == 0x9021u && stub.sup_tx[24] == 0x2400u);
        CHECK(stub.sup_tx[25] == 0x2800u && stub.sup_tx[26] == 0x2E00u);
        CHECK(s->rx_words[2] == 0xA800u && s->rx_words[7] == 0xA800u);
        CHECK(s->echo_ok == 0u && s->echo_bad == 1u);
        CHECK(s->a8_bad == 0u && s->aa_bad == 4u);  /* SSLB0 frames get the A8 script */
        CHECK(s->sent == sent0 + 1u && s->busy == busy0);
        sup_tick_5ms();
        CHECK((stub.sup_tx[27] ^ stub.sup_tx[19]) == 0x04u);  /* bit 2 alternates */
        CHECK((stub.sup_tx[27] & 0xFFFBu) == 0x8E40u);
        CHECK(s->echo_bad == 2u && s->aa_bad == 8u);
        /* xfer timeout: busy counted, no frames (toggle phase NOT consumed) */
        stub.sup_xfer_fail = true;
        sup_tick_5ms();
        CHECK(stub.sup_n == 19u + 16u && s->busy == busy0 + 1u);
        stub.sup_xfer_fail = false;
        /* suspension honored in pattern mode */
        sup_suspend(true);
        sup_tick_5ms();
        CHECK(stub.sup_n == 19u + 16u);
        sup_suspend(false);
        /* enable bit passes through verbatim (danger word, deliberate) */
        sup_app_pattern(true, 0x41u, 0x00u);
        sup_tick_5ms();
        CHECK((stub.sup_tx[35] & 0x01u) == 1u && s->enable);
        CHECK((stub.sup_tx[35] & 0xFFFEu) == 0x8E40u || (stub.sup_tx[35] & 0xFFFEu) == 0x8E44u);
        /* pattern off: back to the single word, no enable */
        sup_app_pattern(false, 0u, 0u);
        sup_tick_5ms();
        CHECK(!s->app_pattern);
        CHECK((stub.sup_tx[43] & 0xFF00u) == 0x8E00u && (stub.sup_tx[43] & 0x01u) == 0u);
    }

    /* ---- engage sequence (spec 03, challenge-response decoded 2026-10-31) ----
     * The fake supervisor starts at challenge sel 6 and achieved level 0; the
     * 0x0C read rotates the challenge every 3rd read; a level write is echoed
     * by the 0x0A read and settles into the achieved level on the next token. */
    {
        const sup_eng_status_t *e = sup_engage_status();

        memset(&fs, 0, sizeof fs);
        fs.sel = 6u;
        stub.sup_reply_fn = fake_sup;
        sup_app_pattern2(true, 0x00u, 0x20u, true);
        sup_engage_arm(true);
        CHECK(e->state == 1u && e->lvl_cmd == 1u);

        /* first register job: the read-only probe (challenge harvest before the
         * first token — 0A00 0C00 0C01), then the duty token for sel 6 */
        while (e->jobs == 0u) {
            sup_tick_5ms();
        }
        CHECK(e->jobs == 1u && e->sel == 6u && e->lvl_cmd == 1u);
        CHECK(back(2u) == 0x0A00u && back(1u) == 0x0C00u && back(0u) == 0x0C01u);
        while (e->jobs == 1u) {
            sup_tick_5ms();
        }
        CHECK(e->jobs == 2u);
        CHECK(back(7u) == 0x8863u && back(6u) == 0x8893u &&
              back(5u) == 0x886Cu && back(4u) == 0x889Cu);
        CHECK(back(3u) == 0x1200u && back(2u) == 0x0C00u &&
              back(1u) == 0x0A00u && back(0u) == 0x0C01u);

        /* sync = two duty rounds, then the ramp: level jobs climb 1..7 then the
         * engaged 31, each confirmed by the 0x0A echo before advancing (the
         * stock's 0x6C742 confirm loop; mismatch = resend) */
        {
            unsigned guard = 0u;
            while (e->state == 1u && ++guard < 200000u) {
                sup_tick_5ms();
            }
            CHECK(e->state == 2u);           /* probe goes straight to the ramp */
            guard = 0u;
            while (e->state == 2u && ++guard < 200000u) {
                sup_tick_5ms();
            }
            CHECK(guard < 200000u);
        }
        CHECK(e->state == 3u);                     /* hold: 0x1F confirmed */
        CHECK(e->lvl_cmd == 31u && e->lvl_echo == 31u);
        CHECK(e->echo_ok >= 8u && e->echo_bad == 0u);   /* 1..7 + the 31 */
        CHECK(e->rot > 0u);                        /* challenge rotations observed */

        /* enable: NEVER before the supervisor's echo grants bit 0 (stock law —
         * the request is a shadow; every unilateral enable resets the TCU) */
        sup_engage_request_enable(true);
        sup_tick_5ms();
        CHECK(!e->enable_sent && !s->enable);         /* no grant yet: no bit 0 */
        CHECK((fs.last_8e & 0x01u) == 0u);
        fs.accept = true;                             /* the supervisor grants */
        sup_tick_5ms();
        CHECK(!e->enable_sent);                       /* grant seen this pass */
        sup_tick_5ms();                               /* word carries it next pass */
        CHECK(e->enable_sent && s->enable);
        CHECK((fs.last_8e & 0x01u) == 1u);
        fs.accept = false;                            /* grant withdrawn */
        sup_tick_5ms();
        CHECK(e->enable_sent);                        /* one pass of latency */
        sup_tick_5ms();
        CHECK(!e->enable_sent && !s->enable);
        sup_engage_request_enable(false);
        sup_tick_5ms();
        CHECK(!e->enable_sent);

        /* hold state keeps answering the rolling challenge */
        {
            uint16_t tok = e->tokens;
            unsigned guard = 0u;
            while (e->tokens == tok && ++guard < 200000u) {
                sup_tick_5ms();
            }
            CHECK(e->tokens == tok + 1u && e->state == 3u);
        }

        /* disarm: enable forced off, state idle */
        sup_engage_arm(false);
        sup_drv_sync(false);
        CHECK(!e->enable_sent && e->state == 0u && !sup_engage_active());
        sup_tick_5ms();
        CHECK(!s->enable);
        stub.sup_reply_fn = 0;
    }

    /* ---- five-select driver read sync (the stock commissioning witness) ---- */
    {
        const sup_drv_status_t *dv = sup_drv_status();

        sup_app_pattern2(true, 0x00u, 0x20u, true);
        sup_drv_sync(true);
        for (unsigned i = 0u; i < 402u && dv->drv_jobs == 0u; i++) {
            sup_tick_5ms();
        }
        /* arm queues the WRITE first: 8 regs x 4 driver chips (no supervisor
         * in the write rotation - 0x8E55 to it reset the TCU, bench 2026-11-01) */
        CHECK(dv->drv_jobs == 1u && dv->drv_writes == 1u);
        CHECK(stub.drv5_jobs == 1u);
        CHECK(dv->drv_a8 == 0u && dv->drv_aa == 32u && dv->drv_bad == 0u);
        for (unsigned i = 0u; i < 402u && dv->drv_jobs == 1u; i++) {
            sup_tick_5ms();
        }
        /* then the keep-warm setpoint pass (E8/F0 = 0x1141, 4 chips) */
        for (unsigned i = 0u; i < 402u && dv->drv_jobs == 1u; i++) {
            sup_tick_5ms();
        }
        CHECK(dv->drv_jobs == 2u && dv->drv_writes == 1u && dv->drv_sp == 1u);
        CHECK(dv->drv_a8 == 0u && dv->drv_aa == 68u && dv->drv_bad == 0u);
        /* then the periodic jobs alternate read / dithered setpoint */
        for (unsigned i = 0u; i < 402u && dv->drv_jobs == 2u; i++) {
            sup_tick_5ms();
        }
        CHECK(dv->drv_jobs == 3u && dv->drv_writes == 1u && dv->drv_sp == 1u);
        CHECK(dv->drv_a8 == 12u && dv->drv_aa == 116u && dv->drv_bad == 0u);
        for (unsigned i = 0u; i < 402u && dv->drv_jobs == 3u; i++) {
            sup_tick_5ms();
        }
        CHECK(dv->drv_jobs == 4u && dv->drv_sp == 2u);   /* dither pass */
        CHECK(dv->drv_a8 == 12u && dv->drv_aa == 152u && dv->drv_bad == 0u);
        sup_drv_sync(false);
        sup_tick_5ms();
        CHECK(dv->drv_jobs == 4u);                    /* off: no more jobs */
    }

    if (fails == 0) {
        printf("test_sup: ok\n");
    }
    return fails != 0;
}

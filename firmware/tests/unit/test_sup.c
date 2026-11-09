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
        fs.lvl = (uint8_t)(fs.lvl_cmd & 7u);   /* E60p: settles on the write */
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
    case 0x14:
        r = 0xAAD5u;                     /* the bench's 0x14: bits 6/7 faulted
                                          * (unpopulated outputs 1/2) — drives
                                          * the fault-path twin F every pass */
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

    /* ---- E60p: the commissioning walk (spec 22, the unified protocol) ----
     * The engine runs from a 1 ms tick (the stock's timer-ISR 0x9106C cadence):
     * boot init 0x3F -> S0 stages the 0x06 era (dwell 3600 ms) -> S8 (target
     * reached on the reply) -> S12 -> S23 duty era (token, context cycling
     * while level==0, 0x19 sends ~token) -> reported 0 -> the 0x1F era
     * (dwell 8600 ms) -> S45 proceed gate -> S34 -> STEADY (duty resumes).
     * The fake supervisor settles its achieved level on the 0x84 write and
     * echoes it on 0x0A; token rounds keep it settled. */
    {
        const sup_eng_status_t *e = sup_engage_status();
        static uint32_t tms;
        uint16_t wj[SUP_ENG_JOB_MAX], wrx[SUP_ENG_JOB_MAX];
        uint8_t wn;

        memset(&fs, 0, sizeof fs);
        fs.sel = 6u;
        stub.sup_reply_fn = fake_sup;
        sup_app_pattern2(true, 0x00u, 0x20u, true);

        /* boot: S0, value 0x3F, expected/reported 7; the cell arms at pass 21
         * and the walk holds until 500 ms (E60r: the proven-safe timings) */
        sup_engage_arm(true);
        CHECK(e->state == SUP_ENG_S0 && e->value == 0x06u && e->lvl_exp == 7u);
        CHECK(!e->enable_req);                     /* not yet: pass 0 */

        tms = 0u;
        wn = 0u;
#define WJ (e->last_words)      /* the engine's composed job this pass */
#define WN (e->last_n)
#define WALK_TICK() do { tms += 5u; sup_tick_5ms(); } while (0)  /* one 5 ms pass */

        /* E60ab: the walk runs from pass 1 (the hold deleted — the chip
         * can't tolerate 84-class silence after the handshake arms it);
         * the first W2 is 84|06 (never 3F); the ask arms at pass 21 */
        CHECK(!e->enable_req);                  /* ask not yet at pass 0 */
        WALK_TICK();
        CHECK(WN == 4u && WJ[0] == 0x8406u);   /* the FIRST-EVER 84 word */
        CHECK(WJ[1] == 0x0A00u && WJ[2] == 0x1200u && WJ[3] == 0x0C00u);
        /* pre-ask mirror word: no bit 0 */
        {
            uint16_t first8e = 0u;
            for (unsigned i = 0u; i < stub.sup_n && i < 64u; i++) {
                const uint16_t w = stub.sup_tx[(stub.sup_n - 1u - i) % 64u];
                if ((w & 0xFF00u) == 0x8E00u) { first8e = w; break; }
            }
            CHECK((first8e & 0xFF00u) == 0x8E00u && (first8e & 0x01u) == 0u);
        }
        /* run to the ask's pass and beyond */
        {
            unsigned guard = 0u;
            while (!e->enable_req && ++guard < 40u) { WALK_TICK(); }
            CHECK(e->enable_req);               /* armed at pass 21 */
            CHECK(e->jobs > 15u);               /* the walk has been flowing */
        }
        CHECK(e->value == 0x06u);
        CHECK(e->state == SUP_ENG_S8 || e->state == SUP_ENG_S12 ||
             e->state == SUP_ENG_S23);                      /* warm chip advances */
        CHECK(e->echo_ok >= 1u && e->lvl_echo == 0x06u);    /* 0x0A echo confirmed */
        CHECK(e->lvl == 6u);                                /* fake settled on the write */

        /* S8: target 5 reached -> S12 -> S23 (duty era) on the next ticks */
        WALK_TICK();
        CHECK(e->state == SUP_ENG_S12 || e->state == SUP_ENG_S23);
        WALK_TICK();
        CHECK(e->state == SUP_ENG_S23 && e->mode == 0u);    /* the duty job takes the wire */

        /* duty jobs: token frames + commit/fb/echo, one per pass */
        {
            const uint16_t jobs0 = e->jobs;
            WALK_TICK();
            CHECK(e->jobs == jobs0 + 1u && WN == 7u);
        }
        CHECK((WJ[0] & 0xFF00u) == 0x8800u && (WJ[3] & 0xFF00u) == 0x8800u);
        CHECK(WJ[4] == 0x1200u && WJ[5] == 0x0C00u && WJ[6] == 0x0A00u);
        CHECK(e->tokens >= 1u);

        /* context cycling while the level is 0: the fake is dragged down
         * (lvl_cmd = 0: every duty round reports 0 — the zero-rounds counter
         * runs; the ctx cycle 0 -> 7 -> 0x19 completes inside the staging
         * window of five rounds) */
        fs.lvl_cmd = 0u;
        fs.lvl = 0u;
        {
            unsigned guard = 0u;
            while (e->ctx != 0x19u && ++guard < 30u) {
                WALK_TICK();
            }
            CHECK(e->ctx == 0x19u);              /* 0 -> 7 -> 0x19 while level 0 */
            /* context 0x19 sends the COMPLEMENT token: capture it */
            uint32_t comp = 0u;
            for (uint8_t j = 0u; j < 4u; j++) {
                comp = (comp << 8) | (uint32_t)(WJ[j] & 0xFFu);
            }
            /* the level rises: the settle takes one round (the next job is
             * still 0x19-complement), then the plain token at context 0x1E */
            fs.lvl_cmd = 6u;
            fs.lvl = 6u;
            guard = 0u;
            while (e->ctx != 0x1Eu && ++guard < 20u) {
                WALK_TICK();
            }
            CHECK(e->ctx == 0x1Eu);
            uint32_t plain = 0u;
            for (uint8_t j = 0u; j < 4u; j++) {
                plain = (plain << 8) | (uint32_t)(WJ[j] & 0xFFu);
            }
            CHECK(plain != comp);                /* complement dialect observed */
            CHECK(e->mode == 0u);                /* no premature 0x1F staging */
        }

        /* level settles at 0 for the staging window: the 0x1F era returns to
         * the W2 job after the five duty rounds */
        fs.lvl_cmd = 0u;
        fs.lvl = 0u;
        {
            unsigned guard = 0u;
            while (e->mode != 7u && ++guard < 40u) {
                WALK_TICK();
            }
            CHECK(e->mode == 7u && e->pending == 0x1Fu);   /* staged, un-promoted */
            WALK_TICK();                                    /* the ping-pong promote */
            CHECK(e->pending == 0u && WJ[0] == 0x841Fu && e->value == 0x1Fu);
            CHECK(e->state == SUP_ENG_S45 || e->state == SUP_ENG_S34);
        }

        /* the fake settles 7 on the write: proceed gate passes -> STEADY,
         * and the duty token resumes */
        fs.lvl_cmd = 0x1Fu;
        fs.lvl = 7u;
        {
            unsigned guard = 0u;
            while (e->state != SUP_ENG_STEADY && ++guard < 40u) {
                WALK_TICK();
            }
            CHECK(e->state == SUP_ENG_STEADY && e->mode == 0u);
        }

        /* steady state runs ON (the stock: hundreds of token rounds) — the
         * ask's pass 21 arrives here on the fast-settling fake */
        WALK_TICK();
        WALK_TICK();
        WALK_TICK();
        CHECK(e->tokens >= 3u);              /* the steady duty rounds flow */

        /* the enable CELL law: on from boot; the kill switch suppresses it */
        CHECK(e->enable_sent);
        sup_engage_kill(true);
        CHECK(!sup_engage_enable_bit());
        sup_engage_kill(false);
        CHECK(sup_engage_enable_bit());

        /* the fault twin: the fake's 0x14 (0xD5) queues F every 5 ms pass —
         * the enables word fault-masked (0x20 keeps bit 5: output 0 clean) */
        CHECK(s->reg14 == 0xD5u && s->fault_jobs >= 1u);

        /* disarm: idle */
        sup_engage_arm(false);
        CHECK(e->state == SUP_ENG_IDLE && !sup_engage_active());
        stub.sup_reply_fn = 0;
    }

    /* ---- driver sync emitters: RETIRED (spec 03 section 6.2e) --------------------
     * The ID/write/setpoint jobs were bus-B emulations of bus-0 stock jobs (live RX
     * staging proof, driving dump 2026-11-04). The real implementations live in
     * sol_drv (bus A); these checks now pin the retirement: no bus-B driver jobs,
     * no counter movement, whatever the sync request says. */
    {
        const sup_drv_status_t *dv = sup_drv_status();
        uint16_t words[48];
        uint8_t wn = 0u;

        sup_app_pattern2(true, 0x00u, 0x20u, true);
        sup_drv_sync(true);
        for (unsigned i = 0u; i < 100u; i++) {
            sup_tick_5ms();
        }
        CHECK(!sup_drv_poll(words, &wn));
        CHECK(dv->drv_jobs == 0u && dv->drv_a8 == 0u && dv->drv_aa == 0u &&
              dv->drv_bad == 0u);
        sup_drv_sync(false);
        sup_drv_setpoint_once();
        sup_tick_5ms();
        CHECK(dv->drv_jobs == 0u);                    /* retired: no jobs ever */
    }

    /* ---- E15: the missing seventh job 0x40BC0 (spec 03 section 6.2m) ----
     * The stock's rotation includes a one-word 0x0000 poll of SSLB0 alone (select
     * cfg 0x40B76 = SPCMD 0xAF01, the status-nibble poll, handler 0x92CC8); our
     * pass never sent it. In the full-fidelity pass it rides after the mixed
     * jobs; the reply is counted, never faulted (the live stock's own reply
     * reads 0x0000 - not AA-class - and the stock tolerates that). */
    {
        const uint32_t aa0 = s->nib_aa, oth0 = s->nib_other;

        stub.sup_n = 0u;                          /* re-arm the stub's 1024-frame ring */
        stub.sup_reply_fn = 0;
        stub.sup_xfer_rx = 0xA800u;               /* not AA-class */
        sup_app_pattern2(true, 0x00u, 0x20u, true);
        sup_tick_5ms();
        CHECK(s->nib_aa == aa0 && s->nib_other == oth0 + 1u && s->nib_rx == 0xA800u);
        CHECK(back(0) == 0x0000u && back(1) == 0x1400u);   /* after job6's 0x14 */
        stub.sup_xfer_rx = 0xAA05u;               /* AA-class reply */
        sup_tick_5ms();
        CHECK(s->nib_aa == aa0 + 1u && s->nib_other == oth0 + 1u && s->nib_rx == 0xAA05u);
        stub.sup_xfer_fail = true;                /* transport failure: pass lost, no fault */
        sup_tick_5ms();
        CHECK(s->nib_aa == aa0 + 1u && s->nib_other == oth0 + 1u);
        stub.sup_xfer_fail = false;
        sup_app_pattern(false, 0u, 0u);
    }

    if (fails == 0) {
        printf("test_sup: ok\n");
    }
    return fails != 0;
}

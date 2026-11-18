/* fb_model.c — see fb_model.h. Implements platform/plat_fbhw.h's host
 * declarations and plat_fbsamp_start. */
#include "tests/unit/fb_model.h"
#include "platform/plat_fbhw.h"
#include "app/fb_shadow.h"

#include <string.h>

#define NF 20u

typedef struct {
    bool     pwm;
    bool     level;         /* static level (true high) */
    uint64_t period, low, t0;
    uint8_t  egsel;         /* 1 rising, 2 falling */
    uint32_t grc, gra;
    bool     icf;
    uint64_t last_cap;      /* counts */
    uint64_t acc_off;       /* off-duty counts since the last capture */
    uint64_t t_sync;        /* counts */
} fch_t;

static fch_t    m_f[NF];
static uint64_t m_now_us;
static uint32_t m_ocr24;
static bool     m_en, m_flag;
static unsigned m_isr_n, m_grbf_w;
static uint32_t m_ipr10;
static unsigned m_acc, m_acc_isr_max;

void fbm_reset(void)
{
    memset(m_f, 0, sizeof m_f);
    for (unsigned n = 0u; n < NF; n++) {
        m_f[n].level = true;
        m_f[n].egsel = 1u;
    }
    m_now_us = 0u;
    m_ocr24 = 0u;
    m_en = false;
    m_flag = false;
    m_isr_n = 0u;
    m_grbf_w = 0u;
    m_ipr10 = 0u;
    m_acc = 0u;
    m_acc_isr_max = 0u;
}

static uint64_t now_cnt(void) { return m_now_us * 4u; }

static bool level_at(const fch_t *c, uint64_t x)   /* level just after x */
{
    if (!c->pwm) {
        return c->level;
    }
    if (x < c->t0) {
        return true;
    }
    return ((x - c->t0) % c->period) >= c->low;
}

static uint64_t next_edge(const fch_t *c, uint64_t x)   /* first edge > x */
{
    uint64_t k, base, e1, e2;
    if (!c->pwm || c->low == 0u || c->low >= c->period) {
        return UINT64_MAX;
    }
    if (x < c->t0) {
        return c->t0;
    }
    k = (x - c->t0) / c->period;
    base = c->t0 + k * c->period;
    e1 = base + c->low;                 /* rising */
    e2 = base + c->period;              /* falling */
    return (e1 > x) ? e1 : e2;
}

static void sync_ch(uint8_t n)
{
    fch_t *c = &m_f[n];
    const uint64_t t = now_cnt();
    uint64_t x = c->t_sync;
    const bool off_low = (c->egsel == 1u);  /* rising: off-duty = low */
    while (x < t) {
        const uint64_t e = next_edge(c, x);
        const uint64_t end = (e < t) ? e : t;
        const bool lv = level_at(c, x);
        if ((off_low && !lv) || (!off_low && lv)) {
            c->acc_off += end - x;
        }
        if (e <= t) {
            const bool newlv = level_at(c, e);
            const bool rising = newlv;
            if ((c->egsel == 1u && rising) || (c->egsel == 2u && !rising)) {
                c->grc = (uint32_t)(((e - c->last_cap) << 8) & 0xFFFFFFFFu);
                c->gra = (uint32_t)((c->acc_off << 8) & 0xFFFFFFFFu);
                c->icf = true;
                c->last_cap = e;
                c->acc_off = 0u;
            }
        }
        x = end;
    }
    c->t_sync = t;
}

void fbm_config(uint8_t n, uint8_t egsel)
{
    sync_ch(n);
    m_f[n].egsel = egsel;
    m_f[n].last_cap = now_cnt();
    m_f[n].acc_off = 0u;
    m_f[n].icf = false;
}

void fbm_set_pwm(uint8_t n, uint32_t period, uint32_t low, uint32_t phase)
{
    sync_ch(n);
    m_f[n].pwm = true;
    m_f[n].period = period;
    m_f[n].low = low;
    m_f[n].t0 = now_cnt() + phase;
}

void fbm_set_static(uint8_t n, bool high)
{
    sync_ch(n);
    m_f[n].pwm = false;
    m_f[n].level = high;
}

void fbm_advance_to(uint32_t t_us)
{
    while (m_en) {
        uint64_t dt = (uint64_t)((m_ocr24 - (uint32_t)(m_now_us & 0xFFFFFFu)) & 0xFFFFFFu);
        if (dt == 0u) {
            dt = 0x1000000u;
        }
        if (m_now_us + dt > (uint64_t)t_us) {
            break;
        }
        m_now_us += dt;
        m_flag = true;
        m_isr_n++;
        m_acc = 0u;
        fb_shadow_isr();
        if (m_acc > m_acc_isr_max) {
            m_acc_isr_max = m_acc;
        }
    }
    if ((uint64_t)t_us > m_now_us) {
        m_now_us = t_us;
    }
}

uint32_t fbm_now(void) { return (uint32_t)m_now_us; }
unsigned fbm_isr_n(void) { return m_isr_n; }
unsigned fbm_grbf_writes(void) { return m_grbf_w; }
bool fbm_armed(void) { return m_en; }
uint32_t fbm_ipr10(void) { return m_ipr10; }
unsigned fbm_isr_acc_max(void) { return m_acc_isr_max; }
unsigned fbm_acc(void) { return m_acc; }
void fbm_acc_reset(void) { m_acc = 0u; }

/* ── platform/plat_fbhw.h, host ── */
uint8_t fbhw_tsrf(uint8_t n)
{
    m_acc++;
    sync_ch(n);
    return m_f[n].icf ? 1u : 0u;
}
void fbhw_icff_clear(uint8_t n) { m_acc += 2u; m_f[n].icf = false; }   /* bclr.b: read + write */
uint32_t fbhw_grcf(uint8_t n) { m_acc++; sync_ch(n); return m_f[n].grc; }
uint32_t fbhw_graf(uint8_t n) { m_acc++; sync_ch(n); return m_f[n].gra; }
uint32_t fbhw_ecntcf(uint8_t n)
{
    m_acc++;
    sync_ch(n);
    return (uint32_t)(((now_cnt() - m_f[n].last_cap) << 8) & 0xFFFFFFFFu);
}
void fbhw_grbf_set(uint8_t n, uint16_t v) { (void)n; (void)v; m_acc++; m_grbf_w++; }
void fbhw_ecntbf_set(uint8_t n, uint16_t v) { (void)n; (void)v; m_acc++; }
uint16_t fbhw_phpr(void)
{
    m_acc++;
    uint16_t v = 0u;
    for (uint8_t n = 0u; n < 6u; n++) {
        sync_ch(n);
        if (level_at(&m_f[n], now_cnt())) {
            v = (uint16_t)(v | (1u << n));
        }
    }
    return v;
}
uint16_t fbhw_pfpr(void)
{
    m_acc++;
    uint16_t v = 0u;
    for (uint8_t n = 6u; n < NF; n++) {
        sync_ch(n);
        if (level_at(&m_f[n], now_cnt())) {
            v = (uint16_t)(v | (1u << (n - 6u)));
        }
    }
    return v;
}
uint32_t fbhw_tcnt1d0(void) { m_acc++; return (uint32_t)((m_now_us & 0xFFFFFFu) << 8); }
void fbhw_ocrd00_set(uint32_t v) { m_acc++; m_ocr24 = (v >> 8) & 0xFFFFFFu; }
void fbhw_cmfad00_clear(void) { m_acc += 2u; m_flag = false; }
uint32_t fbhw_frt_us(void) { m_acc++; return (uint32_t)m_now_us; }

void plat_fbsamp_start(uint32_t first)
{
    m_ocr24 = first & 0xFFFFFFu;
    m_flag = false;
    m_ipr10 = (m_ipr10 & 0x0FFFu) | 0x6000u;
    m_en = true;
}

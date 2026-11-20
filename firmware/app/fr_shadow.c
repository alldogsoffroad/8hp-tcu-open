/* fr_shadow.c — THE READ-ONLY FAULT-REACTION / GEAR-CODE SHADOW (F1,
 * 2026-11-19; app/fr_shadow.h; plan ~/tcu_sweep/fault_reaction_gearcode_plan.md
 * "F1"; decode ~/tcu_sweep/f0_fault_chain_decode.md §2..§4, gear_code_decode.md,
 * can_inputs_decode.md; the port ~/tcu_sweep/f1_fr_shadow_port.md).
 *
 * Its own RAM (fr_t, section .fr_shadow) and the fault manager's (app/fr_fm.c,
 * .fr_fm). Inputs arrive by value (fr_in_t) and as raw frames; nothing live
 * is read or written here. The stock order inside one 10 ms pass (list
 * 0x4D834): the RX scan 0x6E83C (row 6, modes 0..3), slot 0 (the gatherer
 * 0x14FC5C, 0x13A616), slot 1 (the comm monitor 0x131FEE), slot 3 (the
 * range chain — the group-109 report arrives by value), slot 27 (0xB912C →
 * 0xB90B2: the code), slot 29 (0xB986C: 0x89CF5), slot 30 (0xCFFDE: the
 * hinge — here its second, fault-fed evaluation).
 */
#define TCU_FR_SHADOW_TU 1
#include "app/fr_shadow.h"
#include "app/fr_cal.h"
#include "app/sm_cal.h"

#include <stdbool.h>

#if !defined(TCU_RAM_APP)

#if defined(__sh__)
#define FRSEC __attribute__((section(".fr_shadow")))
#else
#define FRSEC
#endif

static fr_t s_fr FRSEC;
#define X  s_fr
#define CM s_fr.cm
#define CD s_fr.cd
#define W  s_fr.w

/* ── the stand-ins (the driving dump; f0 §3.3 / §8, gear_code §1.2) ────── */
#define SI_BUSERR1   0u     /* 0x761CE(1) → +0xAE (bus-1 controller error) */
#define SI_BUSERR2   0u     /* 0x761CE(2) → +0xBD */
#define SI_EA21      0u     /* 0xFFF8EA21 (the bus-timer reset edge) */
#define SI_9582E     0u     /* 0xFFF9582E (after-run active) */
#define SI_9535X     0u     /* 0xFFF9535B / 5C / 64 (0x136966's coding bytes) */
#define SI_RXDRV     0u     /* 0x7828C(0) (the RX status bit 1) */
#define SI_P1        0      /* 0xFFF8B274 */
#define SI_P2        0u     /* 0xFFF8B275 */
#define SI_C310      0      /* 0xFFF8C310 */
#define SI_EB216     0u     /* 0xBAB7A → 0xEB216(BD14): the shift object's query */
#define SI_B10C      0u     /* 0xFFF8B10C (0x89CF5 term 2) */
#define SI_85195     0u     /* 0xFFF85195 (term 3) */
#define SI_CEE       0u     /* 0x89CEE (term 4) */
#define SI_D11       0u     /* 0x89D11 (term 4) */
#define SI_AE2B      0u
#define SI_AE2C      0u
#define SI_EF12      0u     /* 0xFFF8EF12 / EF08 (0 / 0, dump) */
#define SI_EF08      0u
#define SI_95592     0u     /* 0xFFF95592 >> 4 (obj+0x36) / 0xFFF95590 (obj+0x38): 0 / 1 */
#define SI_95590     1u
#define SI_E298      0u
#define SI_AE70      0u
#define SI_9B36      0u     /* 0xFFF89B36 (0xAEA3A: == 2) */
#define SI_9CA0      0      /* 0xFFF89CA0 (0xD1854) */
_Static_assert(SI_BUSERR1 == 0u && SI_BUSERR2 == 0u && SI_EA21 == 0u && SI_9582E == 0u &&
               SI_9535X == 0u && SI_RXDRV == 0u && SI_P1 == 0 && SI_P2 == 0u && SI_C310 == 0 &&
               SI_EB216 == 0u && SI_B10C == 0u && SI_85195 == 0u && SI_CEE == 0u && SI_D11 == 0u &&
               SI_AE2B == 0u && SI_AE2C == 0u && SI_EF12 == 0u && SI_EF08 == 0u &&
               SI_95592 == 0u && SI_95590 == 1u && SI_E298 == 0u && SI_AE70 == 0u &&
               SI_9B36 == 0u && SI_9CA0 == 0,
               "fr_shadow: every stand-in equals the stock driving dump (stockram_driving_20261104)");

/* the reaction ids (0x632A6 + id = id: fr_cal_bytes checks them) */
#define RID_SUB3    0x0Du   /* substitute gear 3 (stop-latched) */
#define RID_HOLD    0x16u   /* hold → code 3 */
#define RID_SUB4    0x19u
#define RID_G1      0x1Au   /* 0x1A..0x21 substitute gear 1..8 */
#define RID_43      0x2Bu
#define RID_50      0x32u   /* line pressure max (0x89CF5) */
#define RID_56      0x38u   /* 0x14E0FC */
#define RID_57      0x39u

/* the content-plausibility monitors (rules undecoded): reported untested */
static const uint8_t s_plaus[] = { 148u, 149u, 150u, 173u, 159u, 120u, 127u, 117u, 154u, 155u,
                                   105u, 107u, 108u };

/* ══ 0x6EC80: the per-message RX status ═════════════════════════════════ */
static uint8_t crc8(const uint8_t *d, uint8_t n)   /* 0x6DAA8: J1850 CRC-8 */
{
    uint8_t c = 0xFFu;
    for (uint8_t i = 0u; i < n; i++) {
        c ^= d[i];
        for (uint8_t b = 0u; b < 8u; b++) {
            c = (uint8_t)((c & 0x80u) != 0u ? (uint8_t)((uint8_t)(c << 1) ^ 0x1Du) : (uint8_t)(c << 1));
        }
    }
    return (uint8_t)(c ^ 0xFFu);
}

/* one check of 0x6EEC0: k = 0 CRC, 1 counter, 2 DLC; returns its flag */
static uint8_t chk(fr_rx_t *m, unsigned k, bool bad, uint8_t tol, uint8_t flag)
{
    if (bad) {
        if (m->cnt[k] < tol) {
            m->cnt[k]++;
            return 0u;
        }
        return flag;
    }
    m->cnt[k] = 0u;
    return 0u;
}

static void rx_pass(void)
{
    for (unsigned n = 0u; n < FR_NENT; n++) {
        const fr_cal_rx_t *c = &fr_cal_rx[n];
        fr_rx_t *m = &X.rx[n];
        const uint8_t en = c->en;
        uint8_t st = m->st;
        st = (SI_RXDRV != 0u && (en & 2u) != 0u) ? (uint8_t)(st | 2u) : (uint8_t)(st & ~2u);
        if (m->newf != 0u) {
            m->newf = 0u;
            m->timer = 1u;
            uint8_t r = 0u;
            const uint8_t dl = m->rdlc;
            if ((en & 0x08u) != 0u && c->crc != 0u) {
                const bool bad = (dl < 2u) || (crc8(m->data, (uint8_t)(dl - 1u)) != m->data[dl - 1u]);
                r |= chk(m, 0u, bad, c->tol_crc, 0x08u);
            }
            if ((en & 0x10u) != 0u && c->ctr != 0u) {
                /* 0x69F00: the nibble of byte DLC-2; the first frame (no 0x80) passes */
                const uint8_t nb = (dl >= 2u) ? (uint8_t)(m->data[dl - 2u] >> 4) : 0u;
                bool bad = false;
                if ((m->nib & 0x80u) != 0u) {
                    const uint8_t step = (uint8_t)((nb - (m->nib & 0x0Fu)) & 0x0Fu);
                    bad = !(step >= 1u && step <= fr_cal_ctr_step);
                }
                m->nib = (uint8_t)(nb | 0x80u);
                r |= chk(m, 1u, bad, c->tol_ctr, 0x10u);
            }
            if ((en & 0x20u) != 0u) {
                r |= chk(m, 2u, dl < c->dlc, c->tol_dlc, 0x20u);
            }
            m->st = (uint8_t)(((st & 2u) | r) & en);
        } else {
            if (m->timer > c->to) {
                st = (uint8_t)(st | (en & 1u));     /* the timeout (timer saturates at to + 1) */
            } else {
                m->timer++;
            }
            m->st = (uint8_t)(st & en);
        }
    }
}

void fr_shadow_rx(uint8_t bus, uint16_t id, uint8_t dlc, const uint8_t *data)
{
    const uint8_t sbus = (uint8_t)(bus + 1u);       /* the stock numbers bus 1 / 2 */
    for (unsigned n = 0u; n < FR_NENT; n++) {
        if (fr_cal_rx[n].id == id && fr_cal_rx[n].bus == sbus) {
            fr_rx_t *m = &X.rx[n];
            const uint8_t k = (dlc > 8u) ? 8u : dlc;
            for (uint8_t i = 0u; i < 8u; i++) {
                m->data[i] = (i < k) ? data[i] : 0u;
            }
            m->rdlc = k;
            m->newf = 1u;                           /* the latest frame of the window counts */
            W.rx_frames++;
            return;
        }
    }
    W.rx_other++;
}

/* ══ the comm monitor 0x131FEE: 0x1513F8, then 0x13626A ═════════════════ */
static void comm_timers_zero(void)                  /* 0x136952 */
{
    CM.t3a = 0u; CM.t3c = 0u; CM.ta8 = 0u; CM.taa = 0u;
}

static void comm_1513f8(const fr_in_t *in)
{
    for (unsigned n = 0u; n < FR_NENT; n++) {
        CM.st[n] = X.rx[n].st;
    }
    CM.sup = in->sup_mv;
    CM.ae = (uint8_t)SI_BUSERR1;
    CM.bd = (uint8_t)SI_BUSERR2;
    CM.b8 = fr_fm_failing(111u);
    CM.b9 = fr_fm_failing(136u);
    CM.c0 = fr_fm_failing(112u);
    CM.c1 = fr_fm_failing(137u);
}

static void comm_1364c6(void)                       /* b2 (after-run) */
{
    if (CM.arprev == 0u && CM.b1 == 1u) {
        CM.arseen = 1u;
    }
    if (SI_9582E == 1u) {
        if (CM.b1 != 0u) {
            CM.b2 = 0u;
        } else if (CM.arseen == 1u) {
            CM.b2 = 1u;
        }
    } else {
        CM.arseen = 0u;
        CM.b2 = 0u;
    }
    CM.arprev = CM.b1;
}

static void comm_136506(void)                       /* the bus timers */
{
    if ((CM.prev[4] == 0u && SI_EA21 != 0u) ||
        CM.sup < fr_cal_comm.sup_min || CM.sup > fr_cal_comm.sup_max) {
        comm_timers_zero();
    }
    if (CM.b8 == 0u && CM.prev[0] == 1u) { CM.t3a = 0u; }
    if (CM.b9 == 0u && CM.prev[1] == 1u) { CM.t3c = 0u; }
    if (CM.c0 == 0u && CM.prev[2] == 1u) { CM.ta8 = 0u; }
    if (CM.c1 == 0u && CM.prev[3] == 1u) { CM.taa = 0u; }
    if (CM.t3a != 0xFFFFu) { CM.t3a++; }
    if (CM.t3c != 0xFFFFu) { CM.t3c++; }
    if (CM.ta8 != 0xFFFFu) { CM.ta8++; }
    if (CM.taa != 0xFFFFu) { CM.taa++; }
    CM.prev[0] = CM.b8; CM.prev[1] = CM.b9; CM.prev[2] = CM.c0; CM.prev[3] = CM.c1;
    CM.prev[4] = (uint8_t)SI_EA21;
}

static uint8_t comm_1362b0(unsigned lo, unsigned hi) /* every key message timed out */
{
    uint8_t r = 0u;
    for (unsigned n = lo; n <= hi; n++) {
        const uint8_t m = fr_cal_mask[n];
        if ((m & 1u) != 0u && (m & 0x80u) != 0u) {
            if ((CM.st[n] & 1u) == 0u) {
                return 0u;
            }
            r = 1u;
        }
    }
    return r;
}

static void comm_13631a(void)                       /* the bus-level groups */
{
    const fr_cal_comm_t *k = &fr_cal_comm;
    CM.ba = (uint8_t)(CM.t3a > k->thr_u0001 && (CM.b1 != 0u || (CM.b2 != 0u && (k->b2en & 1u) != 0u)));
    CM.b6 = (uint8_t)(k->mask_u0001 & CM.ae);
    fr_fm_report(111u, CM.b6, CM.ba);
    CM.c2 = (uint8_t)(CM.ta8 > k->thr_u0001 && (CM.b1 != 0u || (CM.b2 != 0u && (k->b2en & 2u) != 0u)));
    CM.be = (uint8_t)(k->mask_u0074 & CM.bd);
    fr_fm_report(112u, CM.be, CM.c2);
    CM.bb = (uint8_t)(CM.t3c > k->thr_u11e4 && CM.b1 != 0u && CM.b8 == 0u);
    fr_fm_report(136u, CM.b7, CM.bb);
    CM.c3 = (uint8_t)(CM.taa > k->thr_137 && CM.b1 != 0u && CM.c0 == 0u);
    fr_fm_report(137u, CM.bf, CM.c3);
}

static uint8_t comm_coded(unsigned n)               /* 0x136966 */
{
    if (n == 38u) {
        return (uint8_t)(SI_9535X != 0u);
    }
    if (n == 46u || n == 47u) {
        return (uint8_t)(SI_9535X != 0u);
    }
    return 1u;
}

static void comm_13661c(void)                       /* the node groups */
{
    uint8_t lvl[13], mon[13];
    const uint16_t t1 = (CM.t3a < CM.t3c) ? CM.t3a : CM.t3c;
    const uint16_t t2 = (CM.ta8 < CM.taa) ? CM.ta8 : CM.taa;
    const uint8_t en1 = (uint8_t)((CM.b1 != 0u || CM.b2 != 0u) && CM.b8 == 0u && !fr_fm_failing(136u));
    const uint8_t en2 = (uint8_t)((CM.b1 != 0u || CM.b2 != 0u) && CM.c0 == 0u && !fr_fm_failing(137u));
    CM.bc = en1;
    CM.c4 = en2;
    for (unsigned s = 0u; s < 13u; s++) {
        lvl[s] = 0xFFu;
        mon[s] = 0u;
    }
    for (unsigned n = 0u; n < FR_NENT; n++) {
        const uint8_t bus = fr_cal_ent[n][1], s = fr_cal_ent[n][2];
        const uint8_t m = fr_cal_mask[n], st = CM.st[n];
        uint8_t testable = (uint8_t)(((bus == 1u) ? en1 : en2) != 0u &&
                                     fr_cal_thr[n] < ((bus == 1u) ? t1 : t2));
        uint8_t fail = 0u, id = 0u;
        if (CM.b2 != 0u && (m & 0x40u) == 0u) {
            testable = 0u;
        }
        if (comm_coded(n)) {
            if ((m & 1u) != 0u && (st & 1u) != 0u)    { fail = 1u; id = 0u; }   /* LOST */
            if ((m & 2u) != 0u && (st & 8u) != 0u)    { fail = 1u; id = 1u; }   /* INVALID */
            if ((m & 4u) != 0u && (st & 0x10u) != 0u) { fail = 1u; id = 1u; }
            if ((m & 8u) != 0u && (st & 0x20u) != 0u) { fail = 1u; id = 1u; }
        }
        if (s > 12u) {
            continue;
        }
        if (fail != 0u && testable != 0u) {
            if (CM.cnt[n] != 0u) {
                CM.cnt[n]--;
            }
            if (CM.cnt[n] == 0u || id == 1u) {
                lvl[s] = 0u;                         /* the direct confirm */
                mon[s] = id;
            }
        } else {
            CM.cnt[n] = fr_cal_reload[n];
        }
        const uint8_t v = testable ? (fail ? 1u : 5u) : 4u;
        if (v < lvl[s]) {
            lvl[s] = v;
            mon[s] = id;
        }
        if (testable != 0u && bus == 1u && m != 0u) {
            CM.b0 = 1u;
        }
    }
    /* the slot → group map (0xFFF85289's distinct slots) */
    for (unsigned s = 1u; s <= 12u; s++) {
        const uint8_t g = fr_cal_slot_grp[s];
        if (g == 0u) {
            continue;
        }
        (void)mon[s];                                /* the monitor id: DTC / record only */
        switch (lvl[s]) {
        case 5u:  fr_fm_report(g, 0u, 1u); break;
        case 4u:  fr_fm_report(g, 0u, 0u); break;
        case 1u:  fr_fm_report(g, 1u, 1u); break;
        case 0u:  fr_fm_direct(g); fr_fm_report(g, 1u, 1u); break;
        default:  fr_fm_report(g, 0u, 0u); break;
        }
        if (CM.af == 0u) {
            CM.af = fr_fm_confirmed(g);
        }
    }
}

static void comm_pass(const fr_in_t *in)
{
    comm_1513f8(in);
    /* 0x13626A */
    CM.b1 = in->e9fe;
    comm_1364c6();
    comm_136506();
    CM.b7 = 0u;
    CM.bf = 0u;
    if (CM.b1 == 1u) {
        CM.b7 = comm_1362b0(0u, 0x32u);
        CM.bf = comm_1362b0(0x33u, 0x34u);
    }
    comm_13631a();
    comm_13661c();
}

/* ══ 0x13A616 / 0x13A9D4: the substitute-gear selector ══════════════════ */
static uint8_t fwd_range(void)                      /* 0x13BA5C: obj+0x19 in 2..5 */
{
    return (uint8_t)(CD.o19 >= 2u && CD.o19 <= 5u);
}

static void sel_reset_latches(void)
{
    for (unsigned k = 0u; k < 6u; k++) {
        CD.latch[k] = 0u;
    }
}

static void r13a616(const fr_in_t *in)
{
    const fr_cal_13a_t *k = &fr_cal_13a;
    CD.x48 = k->x80;
    CD.fwd = fwd_range();
    if (CD.reinit == 1u || CD.icnt < 0x65u) {
        CD.reinit = 0u;
        CD.icnt = (uint8_t)(CD.icnt + 1u);
        for (unsigned g = 0u; g < 6u; g++) {
            /* the rescale (flag cal +0x68.. == 1) needs the +0x3C.. inputs: the
             * flags read 0 on this unit — the cal limits */
            CD.lim[g] = k->w[2u + g];
            if (k->b[g] == 1u) {
                W.unp_n++;
            }
        }
        CD.t14 = k->w[1];
        CD.t12 = 5u;
        CD.t16 = 5u;
        CD.l1d = 0u; CD.l1e = 0u; CD.l1f = 0u;
        CD.a2b = 0u;
        CD.t10 = k->w[0];
    }
    uint8_t c;
    bool tail_only = false;
    if (fr_fm_react(RID_SUB3)) {
        if (in->d646 == 0u) {
            CD.l1e = 1u;
        }
        if (CD.fwd == 1u && CD.l1e != 0u) {
            CD.l1d = 1u;
        }
        c = CD.l39;
    } else if (fr_fm_react(RID_SUB4)) {
        if (in->d646 == 0u) {
            CD.l1e = 1u;
        }
        if (CD.fwd == 1u && CD.l1e != 0u) {
            CD.l1d = 1u;
        }
        c = CD.l3a;
    } else {
        CD.l39 = 0u; CD.l3a = 0u;
        CD.t14 = k->w[1];
        CD.l1d = 0u; CD.l1e = 0u;
        CD.t16 = 5u;
        CD.l1f = 0u;
        c = 0u;
        tail_only = true;
    }
    if (!tail_only && c == 1u) {
        if (CD.t16 != 0u) {
            CD.t16--;
        }
        if (CD.t16 == 0u) {
            CD.l1f = 1u;
        }
    }
    if (CD.l1d == 1u && CD.t14 != 0u) {
        CD.t14--;
    }
    const uint32_t m1 = fr_cal_13a.sub0 & fr_fm_bank(0u), m2 = fr_cal_13a.sub1 & fr_fm_bank(1u);
    if (m1 == 0u && m2 == 0u) {
        sel_reset_latches();
        CD.t10 = k->w[0];
        CD.t12 = 5u;
        CD.a2b = 0u;
    } else {
        if (CD.t10 != 0u) {
            CD.t10--;
        }
        bool any = false;
        for (unsigned g = 0u; g < 6u; g++) {
            any = any || (CD.latch[g] == 1u);
        }
        if (any) {
            if (CD.t12 != 0u) {
                CD.t12--;
            }
            if (CD.t12 == 0u) {
                CD.a2b = 1u;
            }
        }
    }
}

/* the gear-g / 0x0D / 0x19 latch condition (the speed terms skipped at status 2) */
static bool lim_ok(const fr_in_t *in, unsigned gi, bool with_out_veh)
{
    const fr_cal_13a_t *k = &fr_cal_13a;
    if (with_out_veh) {
        if (CD.t10 != 0u) {
            return false;
        }
        if (!(CD.o32 < CD.lim[gi] || CD.o34 == 2u)) {
            return false;
        }
        if (!((uint16_t)(CD.o36 >> 4) < k->w[8u + gi] || CD.o38 == 2u)) {
            return false;
        }
    } else if (CD.t14 != 0u) {
        return false;
    }
    if (!(in->eng < k->w[14u + gi] || in->eng_st == 2u)) {
        return false;
    }
    if (!((uint16_t)SI_EF12 < k->w[20u + gi] || SI_EF08 == 2u)) {
        return false;
    }
    return true;
}

static uint8_t r13a9d4(const fr_in_t *in, uint8_t p2)
{
    static const uint8_t code_a[8] = { 0x23u, 0x1Bu, 0x1Cu, 0x1Du, 0x17u, 0x1Eu, 0x25u, 0x27u };
    static const uint8_t code_b[8] = { 0x24u, 0x1Fu, 0x20u, 0x21u, 0x09u, 0x22u, 0x26u, 0x28u };
    const fr_cal_13a_t *k = &fr_cal_13a;
    uint8_t res;
    CD.fwd = fwd_range();
    const uint32_t m1 = k->sub0 & fr_fm_bank(0u), m2 = k->sub1 & fr_fm_bank(1u);
    if (m1 == 0u && m2 == 0u) {
        if (fr_fm_react(RID_HOLD)) {
            res = 3u;
        } else {
            res = (p2 == 1u || p2 == 2u) ? p2 : 0u;
        }
        goto store;
    }
    if (m1 != CD.m24 || m2 != CD.m28) {
        sel_reset_latches();
        CD.t10 = k->w[0];
        CD.t12 = 5u;
        CD.l39 = 0u; CD.l3a = 0u;
        CD.t14 = k->w[1];
        CD.t16 = 5u;
        CD.l1d = 0u; CD.l1e = 0u; CD.l1f = 0u;
        CD.a2b = 0u;
    }
    /* obj+0x32 / 0x34 = 0xFFF8EE74 / 76 [I output], obj+0x36 / 0x38 = 0xFFF95592 / 90 */
    CD.o32 = in->d646;
    CD.o34 = in->out_st;
    CD.o36 = SI_95592;
    CD.o38 = SI_95590;
    for (unsigned g = 0u; g < 8u; g++) {
        if (!fr_fm_react((uint8_t)(RID_G1 + g))) {
            continue;
        }
        const unsigned li = (g < 6u) ? g : 5u;      /* gears 6..8: the 6th-gear limits, latch +0x31 */
        uint8_t *const lt = &CD.latch[li];
        if (CD.fwd == 1u) {
            if (*lt == 0u) {
                *lt = lim_ok(in, li, true) ? 1u : 0u;
            }
            if (*lt == 0u) {
                res = 4u;
                goto store;
            }
            if (CD.a2b == 0u) {
                res = code_a[g];
                goto store;
            }
        }
        res = code_b[g];
        goto store;
    }
    if (fr_fm_react(RID_SUB3) || fr_fm_react(RID_SUB4)) {
        const bool s3 = fr_fm_react(RID_SUB3) != 0u;
        uint8_t *const lt = s3 ? &CD.l39 : &CD.l3a;
        const unsigned gi = s3 ? 2u : 3u;           /* cal +0x40 / +0x58 (3rd), +0x44 / +0x5C (4th) */
        if (CD.l1e == 0u) {
            res = 3u;
        } else if (CD.fwd != 1u) {
            res = s3 ? 0x20u : 0x21u;
        } else {
            if (*lt == 0u) {
                *lt = lim_ok(in, gi, false) ? 1u : 0u;
            }
            if (*lt == 0u) {
                res = 4u;
            } else if (CD.l1f == 0u) {
                res = s3 ? 0x1Cu : 0x1Du;
            } else {
                res = s3 ? 0x20u : 0x21u;
            }
        }
        goto store;
    }
    res = (p2 == 1u || p2 == 2u) ? p2 : 0u;
store:
    CD.res = res;
    CD.m24 = m1;
    CD.m28 = m2;
    return res;
}

/* ══ 0x14E0FC (via 0x14E0B6): the mode-3 reaction decision → 0x89C44 ═════ */
static uint8_t r14e0b6(void)
{
    if (fr_fm_react(RID_56) == 0u && CD.e14_l != 1u) {
        if (fr_fm_react(RID_57) == 0u &&
            (CD.e14_t == 0u || fr_cal_14e_reload <= CD.e14_t)) {
            CD.e14_r = 0u;
            CD.e14_t = fr_cal_14e_reload;
            return CD.e14_r;
        }
        CD.e14_r = 2u;
    } else {
        CD.e14_r = 1u;
    }
    return CD.e14_r;
}

/* ══ 0xDB63C: code → the target gear 0x89C43 ═══════════════════════════ */
static int8_t rdb63c(uint8_t code, uint8_t ced, const fr_in_t *in)
{
    const int8_t c43 = CD.c43;
    const bool pf0 = (in->e6ee == 0u && in->e6ee == in->e69b);   /* 0x89C2F == 0 == 0x89C30 */
    int8_t g = 0;
    if (code == 3u && c43 == in->gf_c25 && c43 == in->gf_c2b) {
        CD.x519d = 0u;
    }
    if (ced != 0u && code != 6u && code != 8u && code != 0x1Au) {
        if (ced == 1u || ced == 2u || ced == 7u) {
            W.unp_n++;                              /* 0xB8396(E5C4): not ported */
        } else {
            W.ev49a_n++;
        }
        g = 0;
        goto tail;
    }
    switch (code) {
    case 0x00u: g = 0; break;
    case 0x01u: g = (int8_t)SI_P1; break;
    case 0x02u: case 0x07u: case 0x13u: case 0x14u: case 0x15u: case 0x2Cu: case 0x2Eu: case 0x2Fu:
    case 0x31u: case 0x32u: case 0x33u:
        g = 0;
        if (code == 0x02u) {
            W.unp_n++;                              /* the object 0xFFF8B27C (not ported) */
        }
        break;
    case 0x03u: case 0x04u:
        if (CD.x519e == 3u || CD.x519e == 4u) {
            g = c43;
        } else {
            if (SI_C310 == 0) {
                g = (SI_EB216 != 0u) ? in->gf_c28 : in->gf_c2b;
            } else {
                CD.x519d = 1u;
                g = (int8_t)SI_C310;
            }
            if (in->gf_c25 == -1 || in->gf_c2b == -1) {
                g = 1;
            }
        }
        break;
    case 0x06u: case 0x08u: case 0x1Au:
        W.unp_n++;                                  /* 0xB8396(E5C4) */
        g = 0;
        break;
    case 0x09u: case 0x10u: g = 5; break;
    case 0x0Au: case 0x21u: g = 4; break;
    case 0x11u: case 0x20u: g = 3; break;
    case 0x12u: case 0x16u: W.ev49a_n++; g = 0; break;
    case 0x17u: case 0x19u: g = pf0 ? 5 : in->gf_c2b; break;
    case 0x18u: case 0x1Cu: g = pf0 ? 3 : in->gf_c2b; break;
    case 0x1Bu: g = pf0 ? 2 : in->gf_c2b; break;
    case 0x1Fu: g = 2; break;
    case 0x1Du: g = pf0 ? 4 : in->gf_c2b; break;
    case 0x1Eu: g = pf0 ? 6 : in->gf_c2b; break;
    case 0x22u: case 0x35u: g = 6; break;
    case 0x23u: g = pf0 ? 1 : in->gf_c2b; break;
    case 0x24u: g = 1; break;
    case 0x25u: g = pf0 ? 7 : in->gf_c2b; break;
    case 0x26u: g = 7; break;
    case 0x27u: g = pf0 ? 8 : in->gf_c2b; break;
    case 0x28u: g = 8; break;
    default:    W.ev49a_n++; g = 0; break;
    }
tail:
    CD.x519e = code;
    return g;
}

/* ══ 0xB90B2 (slot 27): the code writer ══════════════════════════════════ */
static void rb90b2(const fr_in_t *in)
{
    CD.c41 = r13a9d4(in, SI_P2);
    CD.c42 = CD.c41;
    if (CD.c42 > 0x35u) {                           /* 0xDD16C */
        CD.c42 = 0u;
        W.ev49c_n++;
    }
    CD.c44 = r14e0b6();
    CD.ced = CD.c44;                                /* 0xDD118 */
    if (CD.ced != 0u && CD.ced != 1u && CD.ced != 2u && CD.ced != 7u) {
        if (CD.ced == 6u) {
            CD.ced = 0u;
        } else {
            W.ev49c_n++;
            CD.ced = 1u;
        }
    }
    CD.c43 = rdb63c(CD.c42, CD.ced, in);
    CD.ae2d = (uint8_t)(CD.c43 != 0);               /* 0xD0262 → 0xDBC9E */
}

/* ══ 0xB986C's 0x89CF5 ═══════════════════════════════════════════════════ */
static uint8_t rcf5(void)
{
    /* term 4 = 0xDCFDA(E6C6, 6): 0xDD1C2(6) = 0, 0x89CEE, 0x89D11, reaction 43, 0x89CED */
    const uint8_t t4 = (uint8_t)(SI_CEE != 0u || SI_D11 != 0u || fr_fm_react(RID_43) != 0u ||
                                 CD.ced != 0u);
    return (uint8_t)(fr_fm_react(RID_50) != 0u || SI_B10C != 0u || SI_85195 != 0u || t4 != 0u);
}

/* ══ THE FAULT-FED HINGE: 0x10485E' / 0xD15A6' on the healthy chain's state ═
 * (app/sm_gear.c's functions with the shadow's AE2D / 0x89C43 / 0x89C42 /
 * 0x89CED instead of its healthy 0; the gear-pair tables of app/sm_cal.c) */
static int8_t gtab(const int8_t *t, int8_t from, int8_t to)
{
    const unsigned c = (unsigned)(uint8_t)(to + 1), r = (unsigned)(uint8_t)(from + 1);
    if (c >= 10u || r >= 10u) {
        W.ff_err_n++;
        return 0;
    }
    return t[c + r * 10u];
}

static int16_t tspd(unsigned k, uint8_t x, uint8_t y)
{
    return (x < SM_CAL_GN && y < SM_CAL_GN) ? sm_cal_gspd[k][y * SM_CAL_GN + x] : 0;
}

static uint8_t ttemp(uint8_t x, uint8_t y)
{
    return (x < SM_CAL_GN && y < SM_CAL_GN) ? sm_cal_gtemp[y * SM_CAL_GN + x] : 0u;
}

static bool dbca8(void)     /* 0xBB0C2 → 0xDBCA8 */
{
    if (CD.c43 == 0) {
        return true;
    }
    if (CD.c42 < 54u && fr_cal_6344e[CD.c42] == 1u && CD.ced == 0u && SI_C310 == 0) {
        return true;
    }
    return false;
}

static bool dbcf8(void)     /* 0xBB0D0 → 0xDBCF8 */
{
    return CD.c43 == 0 || CD.c42 == 7u || CD.c42 == 0x15u || CD.c42 == 0x13u;
}

static int8_t ff_d1922(int8_t cur, int8_t t, const fr_in_t *in)
{
    bool more;
    do {
        more = false;
        if (t < cur) {
            const uint16_t lim = (uint16_t)tspd(4u, (uint8_t)(t + 2), (uint8_t)(t + 1));
            if (lim <= in->cb8) {
                more = true;
                t = (int8_t)(t + 1);
                W.ff_lim_n++;
            }
        }
    } while (more && t < cur);
    return t;
}

static int8_t ff_d16ba(int8_t cur, int8_t t, const fr_in_t *in)
{
    const uint8_t x = (uint8_t)(cur + 1);
    bool again;
    int8_t c7;
    do {
        again = false;
        c7 = gtab(sm_cal_gtype, cur, t);
        if (c7 == 0) {
            t = cur;
        } else if (c7 == 2) {
            const int8_t c8 = gtab(sm_cal_gkind, cur, t);
            const uint8_t y = (uint8_t)(t + 1);
            const bool hot = (c8 == 4 && in->cd2 < ttemp(x, y));
            if ((uint16_t)tspd(2u, x, y) <= in->cba || (uint16_t)tspd(3u, x, y) <= in->cd0 ||
                (uint16_t)tspd(4u, x, y) <= in->cb8 || hot) {
                t = sm_cal_gskip[(unsigned)y * 10u + x];
                again = true;
                W.ff_lim_n++;
                if (t < -1 || t > 8 || t == 0) {
                    W.ff_err_n++;
                    t = (int8_t)(t - 100);
                }
            }
        }
    } while (again && c7 == 2);
    return t;
}

static int8_t ff_d1854(int8_t cur, int8_t t, const fr_in_t *in)
{
    const uint8_t x = (uint8_t)(cur + 1);
    bool again;
    int8_t c;
    do {
        again = false;
        c = gtab(sm_cal_gtype, cur, t);
        if (c == 0) {
            t = cur;
        } else {
            const uint8_t y = (uint8_t)(t + 1);
            if (in->cb8 < (uint16_t)tspd(0u, x, y) && tspd(1u, x, y) <= (int16_t)SI_9CA0) {
                t = sm_cal_gskip[(unsigned)y * 10u + x];
                again = true;
                W.ff_lim_n++;
                if (t < -1 || t > 8 || t == 0) {
                    W.ff_err_n++;
                    t = (int8_t)(t - 100);
                }
            }
        }
    } while (again && c != 0);
    return t;
}

static int8_t ff_d1990(int8_t t, const fr_in_t *in)
{
    const int8_t cur = in->gf_e69f;
    int8_t l = gtab(sm_cal_gnext, cur, t);
    if (in->gf_e69c1 == 2u && gtab(sm_cal_gkind, cur, l) == 4 && in->gf_e343 == (int8_t)(cur - 1)) {
        l = in->gf_e343;
    }
    if (in->gf_e69c1 == 1u && gtab(sm_cal_gkind, cur, l) == 2 && in->gf_e343 == (int8_t)(cur + 1)) {
        l = in->gf_e343;
    }
    if (cur != l) {
        if (dbca8()) {
            l = ff_d16ba(cur, l, in);
        }
        if (dbcf8()) {
            l = ff_d1854(cur, l, in);
        }
        if (CD.ae2d == 0u && SI_9B36 == 2u && gtab(sm_cal_gtype, cur, l) == 1) {
            l = cur;
        }
    }
    return l;
}

static void ff_hinge(const fr_in_t *in)
{
    /* 0x10485E' */
    int8_t v = (int8_t)in->ce8;
    if (SI_E298 != 0u || SI_AE2C == 1u || SI_AE70 == 1u || SI_AE70 == 2u) {
        W.unp_n++;
    } else if (CD.ae2d != 1u) {
        if (in->gf_e4a5 == 1u) {
            v = in->gf_cce4;
        }
    } else {
        v = CD.c43;
    }
    CD.ff_e4a4 = v;
    /* 0xD15A6' */
    int8_t l = v;
    if (in->c3d != 0u && in->e69b == 1u && SI_AE2C == 0u) {
        CD.ff_e6a1 = in->gf_e69f;                   /* the mode-1 return */
        return;
    }
    if (l < 1 || l > 8) {
        l = in->gf_e69f;
        if (l == -1 || l == 0) {
            l = 1;
        }
    }
    if (in->e6ef == 2u || in->e69b == 2u) {
        l = -1;
    } else if (SI_AE2C == 0u) {
        if (in->e69b == 0u) {
            if (dbca8() && SI_AE2B == 0u) {         /* 0xD1B10 */
                l = ff_d1922(in->gf_e69f, l, in);
            }
        } else if (SI_AE2B == 0u) {
            l = ff_d1990(l, in);
        }
    }
    CD.ff_e6a1 = l;
}

/* ══ the pass ═══════════════════════════════════════════════════════════ */
static void compare(const fr_in_t *in)
{
    const uint8_t u1267 = fr_fm_in_masks(109u), p0850 = fr_fm_in_masks(98u);
    if (u1267 != (uint8_t)(in->rng_g109 != 0u)) {
        W.c109_dis_n++;
        if (W.c109_first_ms == 0u) {
            W.c109_first_ms = fr_fm_state()->now;
        }
    }
    if (p0850 != (uint8_t)(in->rng_g98 != 0u)) {
        W.c98_dis_n++;
        if (W.c98_first_ms == 0u) {
            W.c98_first_ms = fr_fm_state()->now;
        }
    }
    if (fr_fm_react(16u) != (uint8_t)(in->rng_g109 != 0u)) {
        W.d661_dis_n++;
    }
    if (CD.ff_e6a1 != in->gf_e6a1) {
        W.tgt_dis_n++;
        if (W.tgt_first_ms == 0u) {
            W.tgt_first_ms = fr_fm_state()->now;
        }
    }
    if (CD.cf5 != 0u) {
        W.cf5_n++;
    } else {
        W.cf5_off_n++;
    }
}

void fr_shadow_tick_10ms(const fr_in_t *in)
{
    X.in = *in;
    W.passes++;
    fr_fm_tick(in->e9fe);
    rx_pass();                                      /* 0x6E83C, modes 0..3 */
    /* the kl15 machine's state 1 (0x140E52, every mode, before this) restarts
     * the drive cycle: 0x136952 and obj+0x1C (0x140E86) */
    if (in->e9ff == 1u) {
        comm_timers_zero();
        CD.reinit = 1u;
    }
    /* the reports rng_chain took this pass (its counters moved; the same
     * arguments it took) — in whatever mode rng_chain took them (slot 3 /
     * the 100 ms slice run in modes 1..3 on the target) */
    if (in->rep109_n != W.rep109_seen) {
        W.rep109_seen = in->rep109_n;
        W.rep109_n++;
        fr_fm_report(109u, in->u_failed, in->u_tested);
    }
    if (in->rep98_n != W.rep98_seen) {
        W.rep98_seen = in->rep98_n;
        W.rep98_n++;
        fr_fm_report(98u, in->g98_failed, in->g98_tested);
    }
    if (in->os_mode < 1u || in->os_mode > 3u) {
        return;
    }
    W.mon_passes++;
    /* slot 0: the gatherer 0x14FC5C, 0x13A616 */
    CD.o18 = (uint8_t)in->gf_c25;
    CD.o19 = in->d6d6;
    CD.o1a = in->d6da;
    r13a616(in);
    /* slot 1: the comm monitor */
    comm_pass(in);
    /* the content-plausibility monitors: untested (their rules are undecoded) */
    for (unsigned i = 0u; i < sizeof s_plaus; i++) {
        fr_fm_report(s_plaus[i], 0u, 0u);
        W.plaus_untested_n++;
    }
    /* slot 27, slot 29, slot 30 */
    const uint8_t c41 = CD.c41, cf5 = CD.cf5;
    const int8_t c43 = CD.c43, ff = CD.ff_e6a1;
    rb90b2(in);
    CD.cf5 = rcf5();
    ff_hinge(in);
    if (CD.c41 != c41 || CD.c43 != c43) {
        fr_fm_note(FR_EV_CODE, CD.c41, (uint16_t)(uint8_t)CD.c43);
    }
    if (CD.cf5 != cf5) {
        fr_fm_note(FR_EV_CF5, CD.cf5, 0u);
    }
    if (CD.ff_e6a1 != ff) {
        fr_fm_note(FR_EV_TARGET, (uint8_t)CD.ff_e6a1, (uint16_t)(uint8_t)CD.ff_e4a4);
    }
    compare(in);
}

void fr_shadow_init(void)
{
    uint8_t *p = (uint8_t *)&s_fr;
    for (size_t i = 0u; i < sizeof s_fr; i++) {
        p[i] = 0u;
    }
    fr_fm_init();
}

void fr_shadow_time(uint32_t us)
{
    W.t_us_last = us;
    if (us > W.t_us_max) {
        W.t_us_max = us;
    }
}

const volatile fr_t *fr_shadow_state(void)
{
    return &s_fr;
}

#endif /* !TCU_RAM_APP */

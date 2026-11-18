/* rq_fill.c — M5-1: the stock ELEMENT FILL machinery of the request objects
 * A..E and the actuator mode-0 current law, ported 1:1 from the bytes of
 * the pristine image (sh-elf-objdump -b binary -m sh2a -EB; flash address =
 * file offset), checked against the driving dump stockram_driving_20261104.
 * app/rq_fill.h for the cells, ~/tcu_sweep/m5_1_fill_port.md for the
 * write-for-write table.
 *
 * PORTED (every write, comparison and constant from the bytes):
 *   0xE3710  constructor                      0xE3836  the fill FSM
 *   0xE38F2  next state   0xE39DC entry       0xE3CE8  transition?
 *   0xE3E28  state action 0xE3CA0 / 0xE3C1E   0xE3F84  modelled pressure
 *   0xE4086  fill-time chain → 0xE47CA, 0xE476E, 0xE44B6, 0xE5164, 0xE447A
 *   0xE40DA  (cases 0/2/5/7/9/B/C/F)          0xE4214  fill pressure
 *   0xE4374, 0xE4424, 0xE4490 + 0xE4522, 0xE497A, 0xE49B8 (tick),
 *   0xE4A4E, 0xE4C8E, 0xE4CCC, 0xE4CF4 (0xE4D92), 0xE4DB0, 0xE4DBA,
 *   0xE4DC8 (boost table 0x1A877C), 0xE4E30, 0xE4E5C (whole, incl. the
 *   0xE4EF6 dispatch, 0xE4F1C, 0xE4F4C, 0xE4F66, 0xE4FA4, 0xE5030),
 *   0xE5106, 0xE517A, 0xE51BC, 0xE5236, 0xE552C, 0xE5602 / 0xE5624 /
 *   0xE566C (per-pass loops), 0xE3816 (update),
 *   0x104DC4 / 0x104DEC / 0x104DFE (filter), 0xD1BE0 (ramp)
 *   0x107C3E (element curve law) → 0xD9D18, 0xD9F96 → 0xD9EA4, 0xD9EF6,
 *   0xD9F10 (0xB7472), 0xD9F62, 0x107D6A → 0xB7786
 *   interpolators 0xB6EAE, 0xB6E10, 0xB6D6C, 0xB7164, 0xB72D8, 0xB7472,
 *   0xB6CBC, 0xB6C9E; saturators 0xB785C, 0xB781E, 0xB7888, 0xB77BC,
 *   0xB7872, 0xB7910, 0xB78C8, 0xB7786
 *
 * STAND-INS (inputs our firmware does not produce; rqf_in_t, compile-time
 * on SH — rq_prod.h): see the header of rq_prod.h and the report.
 *
 * NOT PORTED (no write to any cell the current chain reads, or unreachable
 * for the source-0 holder; each reachable one counts in rq_prod_unp_n):
 *   0xBA544 / 0xAEAF2  fill-start notifications to other modules
 *   0xE4548            +0x4F (no reader in this chain)
 *   0xE530C            +0x31 (only when 0xE4D92 fails in state 3)
 *   0xE4636, 0xE469C, 0x10CCD0, 0xE433C / 0xE4340, 0xBABC8: shift sources
 *   0xB79E4 fault reports: counted in rq_prod_f19f_n
 */
/* READ-ONLY park position sensor: this translation unit is an output path */
#define TCU_OUTPUT_PATH 1
#include "app/rq_fill.h"

#if !defined(TCU_RAM_APP)

static const rqf_in_t *s_in;
static volatile uint32_t *s_unp;
static volatile uint32_t *s_f19f;

void rqf_bind(const rqf_in_t *in, volatile uint32_t *unp, volatile uint32_t *f19f)
{
    s_in = in;
    s_unp = unp;
    s_f19f = f19f;
}

static void unp(void)  { (*s_unp)++; }
static void f19f(void) { (*s_f19f)++; }

/* ── flash copies ──────────────────────────────────────────────────────── */
static const uint8_t *fl(uint32_t a, uint32_t n)
{
    for (uint32_t i = 0u; i < RQF_NBLOBS; i++) {
        const rqf_blob_t *b = &RQF_BLOBS[i];
        if (a >= b->addr && a - b->addr + n <= b->len) {
            return &b->data[a - b->addr];
        }
    }
#if defined(RQF_FL_EXTRA_LOOKUP)
    {   /* the garage shadow's instance (app/gs_fill.c): its own flash copies */
        const uint8_t *q = RQF_FL_EXTRA_LOOKUP(a, n);
        if (q != 0) {
            return q;
        }
    }
#endif
    unp();
    return 0;
}

uint8_t rqf_fl8(uint32_t a)
{
    const uint8_t *p = fl(a, 1u);
    return p != 0 ? p[0] : (uint8_t)0u;
}

uint16_t rqf_fl16(uint32_t a)
{
    const uint8_t *p = fl(a, 2u);
    return p != 0 ? (uint16_t)(((uint16_t)p[0] << 8) | p[1]) : (uint16_t)0u;
}

uint32_t rqf_fl32(uint32_t a)
{
    const uint8_t *p = fl(a, 4u);
    return p != 0 ? (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8) | p[3]) : 0u;
}

static float flf(uint32_t a)
{
    union { uint32_t u; float f; } v;
    v.u = rqf_fl32(a);
    return v.f;
}

/* the stock's RAM cal pointer cells (dump 0xFFF84CF0 row; the ROM data-init
 * values). Ours are not materialised: our .pk_wit sits at 0xFFF84D00. */
uint32_t rqf_cell_ptr(uint32_t cell)
{
    switch (cell) {
    case 0xFFF84D14u: return 0x00054C4Cu;   /* 0xD9EA4 correction maps */
    case 0xFFF84D18u: return 0x00054C54u;   /* 0xD9D18 / D9EF6 / D9F10 */
    case 0xFFF84D1Cu: return 0x00054C80u;   /* G: the global element block */
    case 0xFFF84D20u: return 0x00054CC0u;   /* A */
    case 0xFFF84D24u: return 0x00054D60u;   /* B */
    case 0xFFF84D28u: return 0x00054E00u;   /* C */
    case 0xFFF84D2Cu: return 0x00054EA0u;   /* D */
    case 0xFFF84D30u: return 0x00054F40u;   /* E */
    default: unp(); return 0u;
    }
}

#define G_BLK       rqf_cell_ptr(0xFFF84D1Cu)
#define G_MAP(off)  rqf_fl32(G_BLK + (off))
/* **(obj+0x10) + off: the element's cal map pointer */
static uint32_t cmap(uint32_t o, uint32_t off)
{
    return rqf_fl32(rqf_cell_ptr(RQF_U32(o, RO_CAL)) + off);
}

/* ── saturators ─────────────────────────────────────────────────────────── */
static uint16_t b785c(int32_t v)            /* int → [0, 0xFFFF] */
{
    return (uint16_t)(v >= 0x10000 ? 0xFFFF : (v < 0 ? 0 : v));
}
static uint16_t b781e(uint32_t v)           /* unsigned → ≤ 0xFFFF */
{
    return (uint16_t)(v < 0x10000u ? v : 0xFFFFu);
}
static int16_t b7888(int32_t v)             /* int → s16 */
{
    return (int16_t)(v < 0x8000 ? (v > -0x8001 ? v : -0x8000) : 0x7FFF);
}
static int16_t b77bc(int16_t v)
{
    return v < 0 ? (int16_t)0 : v;
}
static uint8_t b7872(int32_t v)
{
    return (uint8_t)(v < 0x100 ? (v < 0 ? 0 : v) : 0xFF);
}
static int16_t b7910(float f)               /* float → s16 (0xB79C8 / 0xB79CC) */
{
    if (!(f > 32767.0f) && !(-32768.0f > f)) {
        return (int16_t)(int32_t)f;
    }
    return (f > 32767.0f) ? (int16_t)0x7FFF : (int16_t)-0x8000;
}
static uint16_t b78c8(float f)              /* float → u16 (0xB78EC = 65535.0) */
{
    if (!(f > 65535.0f) && !(0.0f > f)) {
        return (uint16_t)(int32_t)f;
    }
    return (f > 65535.0f) ? (uint16_t)0xFFFFu : (uint16_t)0u;
}
static uint16_t b7786(uint16_t v, uint16_t lo, uint16_t hi)
{
    if (v < lo) {
        return lo;
    }
    return (hi < v) ? hi : v;
}

/* ── interpolators (formats from their code) ───────────────────────────── */
/* 0xB6EAE: {u32 n; u16 x[n]; u16 y[n]} */
uint16_t rqf_b6eae(uint32_t m, uint16_t x)
{
    const int32_t n = (int32_t)rqf_fl32(m);
#define X(i) rqf_fl16(m + 4u + 2u * (uint32_t)(i))
#define Y(i) rqf_fl16(m + 4u + 2u * (uint32_t)(n + (i)))
    int32_t k = 0;
    float num = 0.0f, den = 0.0f;
    if (X(0) < x) {
        if (x < X(n - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            k = p - 1;
            den = (float)(int32_t)((uint32_t)X(p) - X(k));
            num = (float)(int32_t)((uint32_t)x - X(k));
        } else {
            num = 1.0f;
            den = 1.0f;
            k = n - 2;
        }
    }
    {
        const uint32_t y0 = Y(k);
        if (num == 0.0f) {
            return (uint16_t)y0;
        }
        {
            const uint32_t y1 = Y(k + 1);
            if (y1 < y0) {
                return (uint16_t)(y0 - ((uint32_t)(int32_t)(((float)(int32_t)(y0 - y1) * num) / den) & 0xFFFFu));
            }
            return (uint16_t)((uint32_t)(int32_t)(((float)(int32_t)(y1 - y0) * num) / den) + y0);
        }
    }
#undef X
#undef Y
}

/* 0xB6E10: {u32 n; s16 x[n]; s16 y[n]} */
int32_t rqf_b6e10(uint32_t m, int16_t x)
{
    const int32_t n = (int32_t)rqf_fl32(m);
#define X(i) ((int32_t)(int16_t)rqf_fl16(m + 4u + 2u * (uint32_t)(i)))
#define Y(i) ((int32_t)(int16_t)rqf_fl16(m + 4u + 2u * (uint32_t)(n + (i))))
    int32_t k = 0;
    float num = 0.0f, den = 0.0f;
    if (X(0) < x) {
        if (x < X(n - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            k = p - 1;
            den = (float)(X(p) - X(k));
            num = (float)(x - X(k));
        } else {
            den = 1.0f;
            num = 1.0f;
            k = n - 2;
        }
    }
    {
        int32_t y0 = Y(k);
        if (num != 0.0f) {
            const int32_t y1 = Y(k + 1);
            const int32_t d = y0 - y1;
            if (y0 <= y1) {
                return (int32_t)(int16_t)(int32_t)(((float)-d * num) / den) + y0;
            }
            y0 = y0 - (int32_t)(int16_t)(int32_t)(((float)d * num) / den);
        }
        return y0;
    }
#undef X
#undef Y
}

/* 0xB6D6C: {u32 n; u8 x[n]; u8 y[n]} */
uint8_t rqf_b6d6c(uint32_t m, uint8_t x)
{
    const int32_t n = (int32_t)rqf_fl32(m);
#define X(i) ((uint32_t)rqf_fl8(m + 4u + (uint32_t)(i)))
#define Y(i) ((uint32_t)rqf_fl8(m + 4u + (uint32_t)(n + (i))))
    int32_t k = 0, i1 = 0, i4 = 0;
    if (X(0) < x) {
        if (x < X(n - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            k = p - 1;
            i4 = (int32_t)(X(p) - X(k));
            i1 = (int32_t)(x - X(k));
        } else {
            i1 = 1;
            i4 = 1;
            k = n - 2;
        }
    }
    {
        uint32_t y0 = Y(k);
        if (i1 != 0) {
            const uint32_t y1 = Y(k + 1);
            if (y1 < y0) {
                y0 = (y0 - ((uint32_t)(int32_t)((float)(int32_t)(i1 * (int32_t)(y0 - y1)) / (float)i4) & 0xFFu)) & 0xFFu;
            } else {
                y0 = ((uint32_t)(int32_t)((float)(int32_t)(i1 * (int32_t)(y1 - y0)) / (float)i4) + y0) & 0xFFu;
            }
        }
        return (uint8_t)y0;
    }
#undef X
#undef Y
}

/* 0xB7164: {u32 nx; u32 ny; s16 x[nx]; s16 y[ny]; s16 z[ny][nx]} */
int32_t rqf_b7164(uint32_t m, int16_t x, int16_t y)
{
    const int32_t nx = (int32_t)rqf_fl32(m);
    const int32_t ny = (int32_t)rqf_fl32(m + 4u);
    const uint32_t xs = m + 8u;
#define X(i) ((int32_t)(int16_t)rqf_fl16(xs + 2u * (uint32_t)(i)))
#define Y(i) ((int32_t)(int16_t)rqf_fl16(xs + 2u * (uint32_t)(nx + (i))))
#define Z(i) ((int32_t)(int16_t)rqf_fl16(xs + 2u * (uint32_t)(i)))
    int32_t kx = 0, ky = 0;
    float fx = 0.0f, dx = 0.0f, fy = 0.0f, dy;
    if (X(0) < x) {
        if (x < X(nx - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            kx = p - 1;
            dx = (float)(X(p) - X(kx));
            fx = (float)(x - X(kx));
        } else {
            dx = 1.0f;
            fx = 1.0f;
            kx = nx - 2;
        }
    }
    if (Y(0) < y) {
        if (y < Y(ny - 1)) {
            int32_t p = 0;
            while (Y(p) < y) {
                p++;
            }
            ky = p - 1;
            dy = (float)(Y(p) - Y(ky));
            fy = (float)(y - Y(ky));
        } else {
            dy = 1.0f;
            fy = 1.0f;
            ky = ny - 2;
        }
    } else {
        dy = (float)(Y(1) - Y(0));
    }
    {
        const int32_t b0 = nx * ky + nx + ny + kx;     /* index from xs */
        int32_t a = Z(b0);
        int32_t c;
        if (fx == 0.0f) {
            c = (int32_t)(int16_t)Z(b0 + nx);
        } else {
            const int32_t z01 = Z(b0 + 1);
            int32_t d = a - z01;
            if (z01 < a) {
                d = -(int32_t)(((float)d * fx) / dx);
            } else {
                d = (int32_t)(((float)-d * fx) / dx);
            }
            a = d + a;
            {
                const int32_t z10 = (int16_t)Z(b0 + nx);
                const int32_t z11 = Z(b0 + nx + 1);
                const int32_t e = z10 - z11;
                if (z11 < z10) {
                    c = (int16_t)(z10 - (int16_t)(int32_t)(((float)e * fx) / dx));
                } else {
                    c = (int16_t)((int16_t)(int32_t)(((float)-e * fx) / dx) + z10);
                }
            }
        }
        if (fy != 0.0f) {
            const int32_t a16 = (int16_t)a;
            const int32_t d = a16 - c;
            if (a16 <= c) {
                return (int32_t)(int16_t)(int32_t)(((float)-d * fy) / dy) + a16;
            }
            a = a16 - (int32_t)(int16_t)(int32_t)(((float)d * fy) / dy);
        }
        return a;
    }
#undef X
#undef Y
#undef Z
}

/* 0xB72D8: as 0xB7164 with u16 axes and z (state-5 release ramp only) */
static uint32_t b72d8(uint32_t m, uint16_t x, uint16_t y)
{
    const int32_t nx = (int32_t)rqf_fl32(m);
    const int32_t ny = (int32_t)rqf_fl32(m + 4u);
    const uint32_t xs = m + 8u;
#define X(i) ((uint32_t)rqf_fl16(xs + 2u * (uint32_t)(i)))
#define Y(i) ((uint32_t)rqf_fl16(xs + 2u * (uint32_t)(nx + (i))))
#define Z(i) ((uint32_t)rqf_fl16(xs + 2u * (uint32_t)(i)))
    int32_t kx = 0, ky = 0;
    float fx = 0.0f, dx = 0.0f, fy = 0.0f, dy;
    if (X(0) < x) {
        if (x < X(nx - 1)) {
            int32_t p = 0;
            while (X(p) < x) {
                p++;
            }
            kx = p - 1;
            dx = (float)(int32_t)(X(p) - X(kx));
            fx = (float)(int32_t)(x - X(kx));
        } else {
            dx = 1.0f;
            fx = 1.0f;
            kx = nx - 2;
        }
    }
    if (Y(0) < y) {
        if (y < Y(ny - 1)) {
            int32_t p = 0;
            while (Y(p) < y) {
                p++;
            }
            ky = p - 1;
            dy = (float)(int32_t)(Y(p) - Y(ky));
            fy = (float)(int32_t)(y - Y(ky));
        } else {
            dy = 1.0f;
            fy = 1.0f;
            ky = ny - 2;
        }
    } else {
        dy = (float)(int32_t)(Y(1) - Y(0));
    }
    {
        const int32_t b0 = nx * ky + nx + ny + kx;
        const uint32_t z00 = Z(b0);
        uint32_t a = (uint32_t)(int32_t)(int16_t)z00;
        uint32_t c;
        if (fx == 0.0f) {
            c = (uint32_t)(int32_t)(int16_t)Z(b0 + nx);
        } else {
            const uint32_t z01 = Z(b0 + 1);
            if (z01 < z00) {
                a = a - (uint32_t)(int32_t)(((float)(int32_t)(z00 - z01) * fx) / dx);
            } else {
                a = (uint32_t)(int32_t)(((float)(int32_t)(z01 - z00) * fx) / dx) + z00;
            }
            {
                const uint32_t z10 = Z(b0 + nx);
                const uint32_t z11 = Z(b0 + nx + 1);
                if (z11 < z10) {
                    c = (uint32_t)(int32_t)(int16_t)z10 - (uint32_t)(int32_t)(((float)(int32_t)(z10 - z11) * fx) / dx);
                } else {
                    c = (uint32_t)(int32_t)(((float)(int32_t)(z11 - z10) * fx) / dx) + z10;
                }
            }
        }
        if (fy != 0.0f) {
            const uint32_t a5 = a & 0xFFFFu;
            if ((c & 0xFFFFu) < a5) {
                a = (a5 - ((uint32_t)(int32_t)(((float)(int32_t)(a5 - (c & 0xFFFFu)) * fy) / dy) & 0xFFFFu)) & 0xFFFFu;
            } else {
                a = ((uint32_t)(int32_t)(((float)(int32_t)((c & 0xFFFFu) - a5) * fy) / dy) + a5) & 0xFFFFu;
            }
        }
        return a;
    }
#undef X
#undef Y
#undef Z
}

/* 0xB7472: float 2-D map {u32 nx; u32 ny; f32 x[nx]; f32 y[ny]; f32 z[ny][nx]} */
static float b7472(float px, float py, uint32_t m)
{
    const int32_t nx = (int32_t)rqf_fl32(m);
    const int32_t ny = (int32_t)rqf_fl32(m + 4u);
    const uint32_t xs = m + 8u;
#define F(i) flf(xs + 4u * (uint32_t)(i))
    int32_t kx = 0, ky = 0;
    float dx, dy;
    if (F(0) < px) {
        if (px < F(nx - 1)) {
            int32_t p = 0;
            while (F(p) < px) {
                p++;
            }
            px = px - F(p - 1);
            dx = F(p) - F(p - 1);
            kx = p - 1;
        } else {
            dx = 1.0f;
            px = 1.0f;
            kx = nx - 2;
        }
    } else {
        dx = 0.0f;
        kx = 0;
        px = 0.0f;
    }
    if (F(nx) < py) {
        if (py < F(nx + ny - 1)) {
            int32_t p = nx;
            while (F(p) < py) {
                p++;
            }
            py = py - F(p - 1);
            dy = F(p) - F(p - 1);
            ky = p - 1 - nx;
        } else {
            dy = 1.0f;
            py = 1.0f;
            ky = ny - 2;
        }
    } else {
        ky = 0;
        py = 0.0f;
        dy = F(nx + 1) - F(nx);
    }
    {
        const int32_t b0 = nx * ky + nx + ny + kx;
        float a = F(b0);
        float c;
        if (px == 0.0f) {
            c = F(b0 + nx);
        } else {
            float d = a - F(b0 + 1);
            if (F(b0 + 1) < a) {
                a = a - (d * px) / dx;
            } else {
                a = (-d * px) / dx + a;
            }
            c = F(b0 + nx);
            {
                const float c1 = F(b0 + nx + 1);
                const float e = c - c1;
                if (c1 < c) {
                    c = c - (e * px) / dx;
                } else {
                    c = (-e * px) / dx + c;
                }
            }
        }
        if (py != 0.0f) {
            if (a <= c) {
                return (-(a - c) * py) / dy + a;
            }
            a = a - ((a - c) * py) / dy;
        }
        return a;
    }
#undef F
}

/* 0xB6CBC: {u32 nx; u32 ny; u8 z[ny][nx]} */
static uint8_t b6cbc(uint32_t m, uint8_t x, uint8_t y)
{
    const uint32_t nx = rqf_fl32(m);
    if ((uint32_t)x < nx && (uint32_t)y < rqf_fl32(m + 4u)) {
        return rqf_fl8(m + (uint32_t)y * nx + x + 8u);
    }
    return 0u;
}

/* 0xB6C9E: s16 table +4 */
static int16_t b6c9e(uint32_t m, uint16_t i)
{
    return (int16_t)rqf_fl16(m + 4u + 2u * (uint32_t)i);
}

/* 0xD1BE0: t ≥ T → b, else a + t·(b − a)/T */
static uint16_t d1be0(uint16_t t, uint16_t T, uint16_t a, uint16_t b)
{
    int32_t v = (int32_t)b;
    if ((int32_t)t < (int32_t)T) {
        v = ((int32_t)t * ((int32_t)b - (int32_t)a)) / (int32_t)T + (int32_t)a;
    }
    return b785c(v);
}

/* ── the stock's own input reads (stand-ins: rqf_in_t) ─────────────────── */
static uint8_t temp(void)           { return s_in->temp; }          /* 0xFFF89C13 */
static uint16_t lp(void)            { return s_in->lp; }            /* 0xBAA58: (s16)*0xFFF8BCB0 */
static uint8_t elem_i(uint32_t o)   { return (uint8_t)(RQF_U8(o, RO_ELEM) - 1u); }

/* the record 0x1FD shadow (0xBCBD4 reads / 0xBCC00 writes back B's +0x46;
 * record 0xFFF80E28 + 0xC0 lies outside our RAM). The write-back's
 * checksum update 0xF71EC and dirty flag 0xFFF85175 (the EEPROM write)
 * are NOT ported: the shadow lives in RAM only. */
static uint16_t s_rec_frac[5];

uint16_t rqf_rec_frac(uint8_t idx)
{
    return idx < 5u ? s_rec_frac[idx] : (uint16_t)0u;
}

/* 0xE552C */
static bool e552c(void)
{
    return s_in->c3d == 1u && RQF_CELL_851A5 != s_in->c3d;
}

/* 0xE4DB0 / 0xE4DBA (vtable +0x24 / +0x30) */
static bool e4db0(uint32_t o) { return RQF_S16(o, RO_FRAC) == 0; }
static bool e4dba(uint32_t o) { return RQF_U16(o, RO_FRAC) == 10000u; }

/* 0xE5164 */
static void e5164(uint32_t o, uint32_t v)
{
    uint16_t r = (uint16_t)v;
    if ((int32_t)(uint16_t)v > (int32_t)RQF_U16(o, RO_TQF)) {
        r = RQF_U16(o, RO_TQF);
    }
    RQF_U16(o, RO_QF) = r;
}

/* 0xE5236: (float)(s16) 0xAEAE2(0xFFF8E5BB, elem) — the adaptation */
static float e5236(uint32_t o)
{
    return (float)(int32_t)s_in->adapt38[elem_i(o)];
}

/* 0xE447A: +0x5C = 0x10CADC(elem) (stand-in) */
static uint16_t e447a(uint32_t o)
{
    const uint16_t v = s_in->v10cadc[elem_i(o)];
    RQF_U16(o, RO_5C) = v;
    return v;
}

/* 0xE4C8E: B6E10(cal +0x74, temp); source 6 → 0x10CD20 (not ported) */
static int16_t e4c8e(uint32_t o)
{
    int16_t v = (int16_t)rqf_b6e10(cmap(o, 0x74u), (int16_t)temp());
    if (RQF_S8(o, RO_SRC) == 6) {
        unp();                                  /* 0x10CD20 scaling */
    }
    return v;
}

/* 0xE4CCC: B6C9E(*(G+0x10), +0x4B) */
static uint16_t e4ccc(uint32_t o)
{
    return (uint16_t)b6c9e(G_MAP(0x10u), RQF_U8(o, RO_4B));
}

/* 0xE4A4E: +0x42 = 0x10CBFC(0xFFF8E6F3, cal, elem, src) (stand-in) */
static void e4a4e(uint32_t o)
{
    RQF_U16(o, RO_PKISS) = s_in->adapt42[elem_i(o)];
}

/* 0xE44B6 */
static uint16_t e44b6(uint32_t o, uint8_t flag)
{
    uint32_t q = rqf_b6d6c(cmap(o, 0x58u), temp());
    if (flag != 0u) {
        const uint32_t q2 = rqf_b6d6c(cmap(o, 0x5Cu), temp());
        q = (uint16_t)b77bc((int16_t)((int32_t)q2 - 0x80 + (int32_t)(int16_t)q));
    }
    {
        const uint32_t q10 = (q & 0xFFFFu) * 10u;
        const int32_t r4 = (int32_t)(q10 & 0xFFFFu);
        if (r4 < (int32_t)RQF_U16(o, RO_QF)) {
            e5164(o, q10);
        }
        return b785c(r4 - (int32_t)RQF_U16(o, RO_QF));
    }
}

/* 0xE476E */
static int16_t e476e(uint32_t o, uint8_t p52)
{
    uint32_t v = 100u;
    if (RQF_S8(o, RO_ELEM) == 5 && s_in->ba79c != 0u) {
        unp();                                  /* 0xBA79C → 0xEAC5E: not decoded */
        v = rqf_fl8(G_MAP(0x28u));
    } else if (p52 != 0u) {
        v = rqf_fl8(cmap(o, 0x48u));
    }
    {
        const float f = e5236(o);
        float r = (float)(int32_t)v;
        r = r * f;
        r = r / 100.0f;                         /* 0xE4854 = 100.0 */
        return b7910(r);
    }
}

/* 0xE47CA: the fill time */
static uint16_t e47ca(uint32_t o, uint8_t p4c, uint8_t p4d, uint8_t p4, uint8_t p52, uint8_t p5b)
{
    const int16_t x5c = (int16_t)e447a(o);
    uint32_t base;
    uint32_t map2;
    int32_t r14;

#if !defined(RQF_E469C_HOOK)
    (void)p4d;
#endif
    if (RQF_S8(o, RO_ELEM) == 5 && s_in->ba79c != 0u) {
        unp();                                  /* 0xBA79C branch: not decoded */
        base = rqf_b6eae(G_MAP(0x30u), temp());
        map2 = G_MAP(0x34u);
    } else if (p52 != 0u) {
        base = rqf_b6eae(cmap(o, 0x14u), temp());
        map2 = cmap(o, 0x1Cu);
    } else {
        base = rqf_b6eae(cmap(o, 0x10u), temp());
        map2 = cmap(o, 0x18u);
    }
    r14 = (int32_t)(int16_t)rqf_b6e10(map2, x5c) + (int32_t)(base & 0xFFFFu);
    if (p4c != 0u) {
#if defined(RQF_E469C_HOOK)
        /* the garage shadow's instance (app/gs_fill.c): 0xE48A2..0xE48DE */
        r14 += RQF_E469C_HOOK(o, p4c, p4d, p52);
#else
        unp();                                  /* 0x89C2F / 0x89C30 / 0xE469C: shift sources */
#endif
    }
    r14 += (int32_t)e476e(o, p52);
    (void)p4;
    if (RQF_U8(o, RO_55) != 0u) {
        r14 += (int32_t)(int16_t)rqf_b7164(cmap(o, 0x4Cu), (int16_t)temp(), (int16_t)s_in->e6a3);
    }
    if (RQF_U8(o, RO_57) != 0u) {
        r14 += (int32_t)(int16_t)rqf_fl16(cmap(o, 0x50u));
    }
    r14 += (int32_t)s_in->ba7dc[elem_i(o)];     /* 0xBA7DC */
    if (p5b != 0u) {
        r14 += (int32_t)(int16_t)rqf_b6e10(cmap(o, 0x54u), (int16_t)temp());
    }
    {
        const uint16_t qf = e44b6(o, p5b);
        r14 -= (int32_t)RQF_U16(o, RO_QF);
        if (r14 < (int32_t)qf) {
            r14 = (int32_t)qf;
        }
    }
    return b785c(r14);
}

/* 0xE4086 */
static void e4086(uint32_t o)
{
    RQF_U16(o, RO_TFILL) = e47ca(o, RQF_U8(o, RO_4C), RQF_U8(o, RO_4D), 0u,
                                 RQF_U8(o, RO_52), RQF_U8(o, RO_5B));
    RQF_U16(o, RO_ADAPT) = (uint16_t)e476e(o, RQF_U8(o, RO_52));
    RQF_U16(o, RO_TQF) = e44b6(o, RQF_U8(o, RO_5B));
    RQF_U16(o, RO_TRAMP) = b785c((int32_t)RQF_U16(o, RO_TFILL) - (int32_t)RQF_U16(o, RO_TQF));
}

/* 0x104DC4 / 0x104DEC / 0x104DFE: the first-order filter on +0x24 (time
 * base 0xFFF89CF8: OUR 10 ms producer clock ×10 — stand-in [I]) */
static uint16_t s_tb_89cf8;
static void f104dec(uint32_t o, uint16_t v)
{
    RQF_U16(o, RO_FLT) = s_tb_89cf8;
    RQF_F32(o, RO_FLTV) = (float)(int32_t)v;
}
static uint16_t f104dfe(uint32_t o, uint16_t x, uint16_t tau)
{
    const uint16_t now = s_tb_89cf8;
    float a = (float)(int32_t)(uint16_t)(now - RQF_U16(o, RO_FLT));
    const float t = (float)(int32_t)tau;
    if (!(t > 0.0f)) {
        a = 1.0f;
    } else {
        a = a / t;
        if (a > 1.0f) {
            a = 1.0f;
        } else if (0.0099999998f > a) {
            a = 0.0099999998f;
        }
    }
    {
        const float y = RQF_F32(o, RO_FLTV);
        RQF_F32(o, RO_FLTV) = y + a * ((float)(int32_t)x - y);
    }
    RQF_U16(o, RO_FLT) = now;
    return b78c8(RQF_F32(o, RO_FLTV) + 0.5f);
}

/* ── the FSM ───────────────────────────────────────────────────────────── */
static void e39dc(uint32_t o);

/* 0xE3E28: the state's action */
static void e3e28(uint32_t o)
{
    switch (RQF_U8(o, RO_ST)) {
    case RQF_ST_OFF:
        RQF_U16(o, RO_FRAC) = 0u;
        break;
    case RQF_ST_QFILL:
        e4086(o);
        break;
    case RQF_ST_RAMP: {
        uint32_t t3c, r2, r4 = 0u;
        e4086(o);
        t3c = RQF_U16(o, RO_TRAMP);
        r2 = ((uint32_t)RQF_U16(o, RO_FRAC0) * t3c) / 10000u;
        if (t3c >= r2) {
            r4 = t3c - r2;
        }
        RQF_U16(o, RO_FRAC) = d1be0(RQF_U16(o, RO_T), b781e(r4), RQF_U16(o, RO_FRAC0), 10000u);
        break;
    }
    case RQF_ST_APPLIED:
        RQF_U16(o, RO_FRAC) = 10000u;
        break;
    case RQF_ST_RELWAIT:
        break;
    case RQF_ST_RELRAMP: {
        uint32_t r14;
        int32_t r4;
        if (RQF_S8(o, RO_ELEM) == 2 && s_in->eng_stop != 0u) {
            break;
        }
        RQF_U16(o, RO_ADAPT) = (uint16_t)b7910(e5236(o));
        r14 = (uint16_t)b72d8(cmap(o, 0x60u), temp(), RQF_U16(o, RO_44));
        r4 = (int16_t)rqf_b7164(cmap(o, 0x64u), (int16_t)temp(), RQF_S16(o, RO_ADAPT));
        r4 = (int16_t)(r4 * 10);
        r4 += (int32_t)(uint16_t)(r14 * 10u);
        RQF_U16(o, RO_TREL) = b785c(r4);
        {
            const uint32_t T = ((uint32_t)RQF_U16(o, RO_TREL) * RQF_U16(o, RO_FRAC0)) / 10000u;
            RQF_U16(o, RO_FRAC) = d1be0(RQF_U16(o, RO_T), b781e(T), RQF_U16(o, RO_FRAC0), 0u);
        }
        break;
    }
    case RQF_ST_CTOR:
        break;
    default:
        f19f();                                 /* 0x19F (1, 9, 0x53) */
        break;
    }
}

/* 0xE3CE8: take a transition? */
static bool e3ce8(uint32_t o)
{
    const uint8_t st = RQF_U8(o, RO_ST);
    bool r = st == RQF_ST_CTOR;
    const bool act = RQF_U8(o, RO_ACTIVE) != 0u;

    if (RQF_S8(o, RO_ELEM) == 2 && s_in->eng_stop != 0u) {
        return r;                               /* B frozen while the engine is stopped */
    }
    switch (st) {
    case RQF_ST_OFF:
        r = act;
        break;
    case RQF_ST_QFILL:
        r = !act || ((uint32_t)(int32_t)RQF_S16(o, RO_T) >= (uint32_t)(int32_t)RQF_S16(o, RO_TQF));
        break;
    case RQF_ST_RAMP:
        if (!act) {
            r = true;
        } else {
            r = true;
            if (!e4dba(o) && RQF_S16(o, RO_TRAMP) != 0) {
                r = false;
            }
        }
        break;
    case RQF_ST_APPLIED:
        r = !act;
        break;
    case RQF_ST_RELWAIT:
        if (act) {
            r = true;
        } else {
            r = !((uint32_t)(int32_t)RQF_S16(o, RO_PMOD) > (uint32_t)(int32_t)RQF_S16(o, RO_PKISS));
            if (RQF_U8(o, RO_5A) == 1u) {
                const uint16_t lim = b781e((uint32_t)rqf_fl8(cmap(o, 0x9Cu)) * 10u);
                if ((int32_t)RQF_U16(o, RO_T) < (int32_t)lim) {
                    r = false;
                }
            }
        }
        break;
    case RQF_ST_RELRAMP:
        r = act ? true : e4db0(o);
        break;
    case RQF_ST_CTOR:
        r = true;
        break;
    default:
        f19f();                                 /* 0x19F (1, 7, 0x1A) */
        break;
    }
    return r;
}

/* 0xE3CA0 */
static void e3ca0(uint32_t o)
{
    uint8_t st = RQF_U8(o, RO_ST);
    if (st == RQF_ST_QFILL) {
        e5164(o, RQF_U8(o, RO_ACTIVE) != 0u ? 0u : (uint32_t)(int32_t)RQF_S16(o, RO_T));
        st = RQF_U8(o, RO_ST);
    }
    if (st == RQF_ST_RELRAMP) {
        RQF_U8(o, RO_C30) = 0u;
        if (s_in->c3d == 1u) {
            RQF_U8(o, RO_57) = 1u;
        }
    }
}

/* 0xE38F2: the next state */
static void e38f2(uint32_t o)
{
    const bool act = RQF_U8(o, RO_ACTIVE) != 0u;
    uint8_t n;
    switch (RQF_U8(o, RO_ST)) {
    case RQF_ST_OFF:     n = 1u; break;
    case RQF_ST_QFILL:   n = act ? 2u : 5u; break;
    case RQF_ST_RAMP:    n = act ? 3u : 5u; break;
    case RQF_ST_APPLIED: n = 4u; break;
    case RQF_ST_RELWAIT: n = act ? 3u : 5u; break;
    case RQF_ST_RELRAMP: n = act ? 1u : 0u; break;
    case RQF_ST_CTOR:    n = 0u; break;
    default:
        f19f();                                 /* 0x19F (1, 7, 0x74) */
        return;
    }
    RQF_U8(o, RO_ST) = n;
}

/* 0xE39DC: the entry action of the (new) state */
static void e39dc(uint32_t o)
{
    const uint8_t bit = (uint8_t)(1u << RQF_U8(o, RO_IDX));

    RQF_U16(o, RO_T) = 0u;
    switch (RQF_U8(o, RO_ST)) {
    case RQF_ST_OFF:
        RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 & (uint8_t)~bit);
        RQF_U8(o, RO_BOOST) = 1u;
        RQF_U16(o, RO_FRAC) = 0u;
        if (s_in->e5c2 != 0u) {                 /* 0xB803C */
            RQF_U8(o, RO_56) = 1u;
        }
        RQF_U8(o, RO_5B) = 0u;
        if (RQF_U32(o, RO_T18) == 0u &&
            (RQF_U32(o, RO_T14) != 0u || RQF_U8(o, RO_51) == 0u)) {
            RQF_U32(o, RO_T18) = (uint32_t)rqf_fl16(G_MAP(0x1Cu)) * 100u;
        }
        break;
    case RQF_ST_QFILL:
        /* 0xBA544(0xFFF8E613, elem): notification, not ported */
        RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 | bit);
        RQF_CELL_851A3 = (uint8_t)(RQF_CELL_851A3 | bit);
        RQF_U8(o, RO_4E) = 1u;
        e4086(o);
        /* 0xAEAF2(0xFFF8E5BB, elem): adaptation notification, not ported */
        break;
    case RQF_ST_RAMP:
        RQF_CELL_851A3 = (uint8_t)(RQF_CELL_851A3 | bit);
        RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 | bit);
        RQF_U8(o, RO_4E) = 1u;
        RQF_U16(o, RO_FRAC0) = RQF_U16(o, RO_FRAC);
        break;
    case RQF_ST_APPLIED:
        RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 | bit);
        RQF_CELL_851A3 = (uint8_t)(RQF_CELL_851A3 & (uint8_t)~bit);
        RQF_U8(o, RO_55) = 0u;
        RQF_U8(o, RO_57) = 0u;
        RQF_U8(o, RO_56) = 0u;
        RQF_U8(o, RO_5B) = 0u;
        if (s_in->b0fe == 0u) {                 /* 0xB8054 */
            RQF_U16(o, RO_FRAC0) = 10000u;
        }
        f104dec(o, (uint16_t)RQF_S16(o, RO_PMOD));
        if (RQF_U8(o, RO_51) != 0u || RQF_U32(o, RO_T18) != 0u) {
            RQF_U32(o, RO_T14) = (uint32_t)rqf_fl16(G_MAP(0x18u)) * 100u;
        }
        RQF_U8(o, RO_C30) = 0u;
        RQF_U16(o, RO_FRAC) = 10000u;
        break;
    case RQF_ST_RELWAIT:
        RQF_U8(o, RO_5B) = 0u;
        RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 & (uint8_t)~bit);
        RQF_CELL_851A3 = (uint8_t)(RQF_CELL_851A3 & (uint8_t)~bit);
        RQF_U32(o, RO_T14) = 0u;
        RQF_U32(o, RO_T1C) = 0u;
        break;
    case RQF_ST_RELRAMP: {
        uint16_t f46;
        RQF_U16(o, RO_ADAPT) = (uint16_t)b7910(e5236(o));
        RQF_U16(o, RO_44) = e447a(o);
        if (rqf_fl8(G_MAP(0x24u)) != 0u) {
            RQF_U8(o, RO_BOOST) = 1u;
        }
        RQF_CELL_851A3 = (uint8_t)(RQF_CELL_851A3 & (uint8_t)~bit);
        RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 & (uint8_t)~bit);
        RQF_U8(o, RO_5B) = 0u;
        f46 = RQF_U16(o, RO_FRAC);
        {
            /* 0xE517A(obj, +0x30, +0x38) */
            const int32_t s = (int16_t)rqf_b7164(cmap(o, 0x70u), (int16_t)RQF_U8(o, RO_C30),
                                                 RQF_S16(o, RO_ADAPT));
            const uint32_t u = rqf_b6eae(cmap(o, 0x6Cu), temp());
            int32_t v = (s * (int32_t)(u & 0xFFFFu)) / 100;
            if (v > 100) {
                v = 100;
            }
            RQF_U16(o, RO_FRAC0) = b781e(((uint32_t)b7872(v) * f46) / 100u);
        }
        e3e28(o);
        break;
    }
    default:
        f19f();                                 /* 0x19F (1, 8, 0x39) */
        break;
    }
}

/* 0xE3C1E */
static void e3c1e(uint32_t o)
{
    if (RQF_U32(o, RO_T1C) != 0u && RQF_U32(o, RO_T14) == 0u) {
        RQF_U8(o, RO_51) = 0u;
        RQF_U32(o, RO_T20) = 0u;
        RQF_U32(o, RO_T18) = 0u;
    }
    RQF_U32(o, RO_T1C) = RQF_U32(o, RO_T14);
    if (RQF_U32(o, RO_T20) != 0u && RQF_U32(o, RO_T18) == 0u) {
        RQF_U8(o, RO_51) = 1u;
    }
    RQF_U32(o, RO_T20) = RQF_U32(o, RO_T18);
    if (RQF_U8(o, RO_ST) == RQF_ST_APPLIED) {
        RQF_U8(o, RO_57) = 0u;
    }
}

/* 0xE497A: set +0x46 from outside (the record restore) */
static void e497a(uint32_t o, uint16_t v)
{
    RQF_U16(o, RO_FRAC) = v;
    RQF_CELL_851A3 = (uint8_t)(RQF_CELL_851A3 & (uint8_t)~(1u << RQF_U8(o, RO_IDX)));
    RQF_U8(o, RO_4E) = 0u;
    if (v == 0u) {
        RQF_U8(o, RO_ST) = 0u;
    } else {
        RQF_U8(o, RO_ST) = (v == 10000u) ? 3u : 1u;
    }
    e39dc(o);
}

/* 0xE3836: the fill FSM */
static void e3836(uint32_t o)
{
    if (e552c()) {
        RQF_U8(o, RO_57) = 1u;
    }
    if (RQF_U8(o, RO_54) == 1u) {
        RQF_U8(o, RO_52) = 0u;
    }
    if (s_in->e5c2 != 0u && RQF_U8(o, RO_ST) == 0u) {    /* 0xB803C(0xFFF8E5C2) */
        RQF_U8(o, RO_56) = 1u;
    }
    if (RQF_S8(o, RO_ELEM) == 2 && s_in->rec1fd != 0u) { /* 0xBC82C: record 0x1FD ready */
        if (RQF_U8(o, RO_4A) == 0u) {
            uint16_t v;
            RQF_U8(o, RO_4A) = 1u;
            v = s_rec_frac[RQF_U8(o, RO_IDX) % 5u];      /* 0xBCBD4 */
            if (v != 0u && (RQF_U8(o, RO_ST) == 0u || RQF_U8(o, RO_ST) == 6u)) {
                e497a(o, v);
            }
        }
        if (s_in->eng_stop == 0u) {                      /* 0xBCC00 write-back */
            s_rec_frac[RQF_U8(o, RO_IDX) % 5u] = (uint16_t)RQF_S16(o, RO_FRAC);
        }
    }
    e3e28(o);
    {
        uint8_t guard = 0u;
        while (e3ce8(o)) {
            e3ca0(o);
            e38f2(o);
            e39dc(o);
            if (++guard >= 16u) {
                f19f();                         /* ours: a cycle the stock cannot take */
                break;
            }
        }
    }
    e3c1e(o);
}

/* 0xE3F84: the modelled pressure +0x40 */
static void e3f84(uint32_t o)
{
    switch (RQF_U8(o, RO_ST)) {
    case RQF_ST_OFF:
        RQF_U16(o, RO_PMOD) = 0u;
        break;
    case RQF_ST_QFILL:
        break;
    case RQF_ST_RAMP: {
        const int32_t r0 = (int32_t)e4c8e(o) + (int32_t)RQF_U16(o, RO_PKISS);
        RQF_U16(o, RO_PMOD) = b785c((int32_t)((int32_t)RQF_U16(o, RO_FRAC) * r0) / 10000);
        break;
    }
    case RQF_ST_APPLIED:
    case RQF_ST_RELWAIT: {
        int32_t r13 = (int32_t)RQF_U16(o, RO_RES);
        if (!(r13 >= (int32_t)lp())) {
            if ((int32_t)RQF_U16(o, RO_RES) >= (int32_t)e4ccc(o)) {
                r13 = (int32_t)lp();
            }
        } else {
            r13 = (int32_t)lp();
        }
        /* r13 == 0: + 0xAEAFA(0xFFF8E5BB, elem) → 0xA1664 (adaptation offset,
         * not decoded): stand-in 0 — it moves only +0x40, the release-wait
         * exit (state 4), never a current */
        {
            const uint16_t x = b785c(r13);
            const uint8_t tau = rqf_b6d6c(rqf_fl32(rqf_cell_ptr(RQF_U32(o, RO_CAL))), temp());
            RQF_U16(o, RO_PMOD) = f104dfe(o, x, tau);
        }
        if (!((uint32_t)(int32_t)RQF_S16(o, RO_PMOD) >= (uint32_t)(int32_t)RQF_S16(o, RO_PKISS))) {
            RQF_U16(o, RO_PMOD) = RQF_U16(o, RO_PKISS);
        }
        break;
    }
    case RQF_ST_RELRAMP:
        RQF_U16(o, RO_PMOD) = b785c((int32_t)((int32_t)RQF_U16(o, RO_PKISS) *
                                              (int32_t)RQF_U16(o, RO_FRAC)) / 10000);
        break;
    default:
        f19f();                                 /* 0x19F (1, 0xA, 0x34) */
        break;
    }
}

/* 0xE4DC8: the first-pass boost (table *(G+0x20) = 0x1A877C) */
static bool e4dc8(uint32_t o, uint8_t p4d)
{
    if (p4d > 4u) {
        f19f();                                 /* 0x19F (5, 0x10, 0x98) */
        return false;
    }
    return b6cbc(G_MAP(0x20u), RQF_U8(o, RO_IDX), p4d) == 1u;
}

/* 0xE4214: the fill pressure into +2 / +4 */
static void e4214(uint32_t o)
{
    int32_t base;
    int32_t rem = -1;

    if (RQF_S8(o, RO_ELEM) == 5 && s_in->ba79c != 0u) {
        unp();                                  /* 0xBA79C branch (G +0x2C): not decoded */
        base = (int16_t)rqf_b7164(G_MAP(0x2Cu), (int16_t)temp(), (int16_t)s_in->c1c);
    } else if (RQF_S8(o, RO_52) != 0) {
        base = (int16_t)rqf_b7164(cmap(o, 0x08u), (int16_t)temp(), (int16_t)s_in->c1c);
    } else {
        base = (int16_t)rqf_b7164(cmap(o, 0x04u), (int16_t)temp(), (int16_t)s_in->c1c);
    }
    base += (int16_t)rqf_b7164(cmap(o, 0x0Cu), (int16_t)temp(), (int16_t)s_in->c0a);
    if (RQF_S8(o, RO_SRC) == 15) {
        unp();                                  /* 0xAE9FA / 0xAEA08: source 15 */
    } else if (RQF_S8(o, RO_ST) == 2) {
        rem = (int32_t)RQF_S16(o, RO_TRAMP) - (int32_t)RQF_S16(o, RO_T);
    }
    {
        const int32_t r11 = (int32_t)(uint16_t)rem;
        if (r11 < 10) {
            const int32_t r = (int32_t)RQF_U16(o, RO_PKISS) + (int32_t)e4c8e(o);
            base = ((base - r) * r11) / 10 + r;
        }
    }
    if (RQF_S8(o, RO_BOOST) != 0) {
        if (e4dc8(o, RQF_U8(o, RO_4D))) {
            base = 20000;                       /* 0xE4306 movi20 #20000 */
        }
        RQF_U8(o, RO_BOOST) = 0u;
    }
    {
        const uint16_t v = b785c(base);
        RQF_U16(o, RO_RES) = v;
        RQF_U16(o, RO_LIM) = v;
        if ((int32_t)v > (int32_t)SYS_RES) {
            RQF_U16(o, RO_LIM) = SYS_RES;
        }
    }
    RQF_CELL_E384 = RQF_U8(o, RO_ELEM);
}

/* 0xE4374 */
static void e4374(uint32_t o)
{
    const uint8_t mode = RQF_U8(o, RO_MODE);
    const uint8_t pr = RQF_U8(o, RO_PRIO);
    const bool zero = mode != 0u && RQF_S16(o, RO_P) == 0;

    if ((((pr == 3u && mode == 0u) || pr == 0u || pr == 1u) ||
         (pr == 4u && mode == 0u)) && !zero) {
        int32_t v = (int32_t)RQF_U16(o, RO_P);
        if (pr != 4u && pr != 3u) {
            v += (int32_t)RQF_U16(o, RO_PKISS);
        }
        if (pr == 0u || pr == 3u) {
            v += (int32_t)e4c8e(o);
        }
        if (v < 0) {
            f19f();                             /* 0x19F (6, 0xC, 0xA4) */
            v = 1;
        }
        {
            const uint16_t r = b785c(v);
            RQF_U16(o, RO_RES) = r;
            RQF_U16(o, RO_LIM) = r;
            if ((int32_t)r > (int32_t)SYS_RES) {
                RQF_U16(o, RO_LIM) = SYS_RES;
            }
        }
    }
}

/* 0xE40DA: the not-yet-applied path writes mode 0 */
static void e40da(uint32_t o)
{
    const uint8_t src = RQF_U8(o, RO_SRC);
    RQF_U8(o, RO_MODE) = 0u;                    /* 0xE40EC */
    switch (src) {
    case 0u: case 2u: case 5u: case 7u: case 9u: case 0xBu: case 0xCu: case 0xFu:
        RQF_U8(o, RO_4C) = 0u;
        RQF_U8(o, RO_4D) = 0u;
        RQF_U8(o, RO_52) = 0u;
        if (src == 0xBu || src == 0xCu) {
            RQF_U8(o, RO_5B) = (uint8_t)(RQF_U8(o, RO_5B) | (s_in->b0fe != 0u ? 1u : 0u));
        } else {
            RQF_U8(o, RO_5B) = 0u;
        }
        break;
    case 1u: case 3u: case 6u: case 8u: case 10u:
#if defined(RQF_SHIFT_SRC_HOOK)
        RQF_SHIFT_SRC_HOOK(o);                  /* the garage shadow's instance (gs_fill.c) */
#else
        unp();                                  /* 0xE433C / 0xE4340: shift sources */
#endif
        break;
    default:
        f19f();                                 /* 0x19F (8 / 0xB, 0xB, 0x66) */
        break;
    }
}

/* 0xE4CF4 (0xE4D92; 0xE530C not ported) */
static void e4cf4(uint32_t o, uint8_t param)
{
    const bool c = (uint16_t)((int32_t)(int16_t)rqf_fl16(G_MAP(0x14u)) + (int32_t)e4ccc(o)) <=
                   RQF_U16(o, RO_RES);
    if (param == 1u && RQF_U8(o, RO_ST) == RQF_ST_APPLIED && !c) {
        unp();                                  /* 0xE530C(…, 0xBA8EC, 0xBAB88) → +0x31 */
    }
    RQF_U8(o, RO_54) = (RQF_U8(o, RO_ST) == RQF_ST_APPLIED && RQF_U8(o, RO_C31) == 0u && c) ? 1u : 0u;
}

/* 0xE4424 */
static void e4424(uint32_t o)
{
    if (e4dba(o) && RQF_S8(o, RO_SRC) == 2) {
        unp();                                  /* 0xBAB1E(0xFFF8E61A): source 2 */
        RQF_U8(o, RO_5A) = 0u;
    } else if (RQF_U8(o, RO_ACTIVE) != 0u) {
        RQF_U8(o, RO_5A) = 0u;
    }
    if (!((uint32_t)(int32_t)(int8_t)s_in->cd2 > (uint32_t)(int32_t)(int8_t)rqf_fl8(G_MAP(0x3Cu)))) {
        RQF_U8(o, RO_5A) = 0u;
    }
}

/* 0xE4490 + 0xE4522 */
static void e4490(uint32_t o)
{
    const uint8_t was = RQF_U8(o, RO_ACTIVE);
    const uint8_t now = (RQF_S16(o, RO_P) == 0 && RQF_S8(o, RO_MODE) != 0) ? (uint8_t)0u : (uint8_t)1u;
    RQF_U8(o, RO_ACTIVE) = now;
    if (now != was && now != 0u) {              /* 0xE4522 */
        RQF_U8(o, RO_52) = 0u;
        if (s_in->e3ed == 1u) {                 /* 0xBA78C: *0xFFF8E3ED */
            RQF_U8(o, RO_52) = 1u;
        }
    }
}

/* 0xE3816: the per-pass update */
static void e3816(uint32_t o)
{
    e4a4e(o);
    e3836(o);
    e3f84(o);
    e4cf4(o, 0u);
    /* 0xE4548: +0x4F, not ported (no reader in this chain) */
}

/* 0xE4E5C: the element request method (whole) */
void rqf_e4e5c(uint32_t o)
{
    const uint8_t pr = RQF_U8(o, RO_PRIO);
    const uint8_t src = RQF_U8(o, RO_SRC);

    RQF_U8(o, RO_59) = 0u;
    if (RQF_U8(o, RO_MODE) != 0u) {
        const uint16_t p = RQF_U16(o, RO_P);
        uint16_t r;
        if ((int32_t)p >= 20000 || p == 0u) {   /* 0xE4E82 / 0xE4E88 */
            r = p;
        } else {
            f19f();                             /* 0x19F (9, 0x11, 0x7A) */
            r = lp();
        }
        RQF_U16(o, RO_RES) = r;
        RQF_U16(o, RO_LIM) = r;
        if ((int32_t)r > (int32_t)SYS_RES) {
            RQF_U16(o, RO_LIM) = SYS_RES;
        }
    }
    e4490(o);
    e4424(o);
    if (RQF_U8(o, RO_ACTIVE) != 0u && src == 6u) {
        unp();                                  /* 0x10CCD0(0xFFF8D0D8, …): source 6 */
    }
    if (pr == 2u || (pr != 3u && !e4dba(o) && RQF_U8(o, RO_ACTIVE) != 0u)) {
        /* 0xE4F4C */
        RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 | (uint8_t)(1u << RQF_U8(o, RO_IDX)));
        e40da(o);
    } else {
        /* 0xE4F1C */
        if (RQF_U8(o, RO_4E) != 0u) {
            if (RQF_U8(o, RO_50) != 0u && RQF_U8(o, RO_MODE) == 0u) {
                unp();                          /* 0xE4636 → +0x59: not ported */
            }
            RQF_U8(o, RO_4E) = 0u;
            RQF_U8(o, RO_50) = 0u;
        }
        e4a4e(o);
    }
    e3836(o);
    {
        const bool s15 = src == 15u;
        uint8_t st = RQF_U8(o, RO_ST);
        if (((st == 1u || st == 2u) && !s15 && RQF_U8(o, RO_ACTIVE) != 0u) ||
            (pr == 2u && s15)) {
            RQF_U8(o, RO_50) = 1u;
            e4214(o);
            st = RQF_U8(o, RO_ST);
        }
        if ((st == 3u && !s15) || (pr == 3u && s15)) {
            if (RQF_U8(o, RO_59) == 1u) {
                const uint16_t v = rqf_b6eae(cmap(o, 0x90u), temp());
                RQF_U16(o, RO_RES) = v;
                RQF_U16(o, RO_LIM) = v;
                if ((int32_t)v > (int32_t)SYS_RES) {
                    RQF_U16(o, RO_LIM) = SYS_RES;
                }
            } else {
                e4374(o);
            }
        }
    }
    /* 0xE5030 */
    if (RQF_U16(o, RO_FRAC) == 10000u) {
        const uint8_t bit = (uint8_t)(1u << RQF_U8(o, RO_IDX));
        if ((RQF_CELL_851A2 & bit) != 0u) {
            bool clr;
            if (src > 0x1Cu) {
                f19f();
                clr = true;
            } else {
                /* table 0xE5068 (braf base 0xE5062): 0x7E clear, 0x40 0xBABC8,
                 * 0x54 fault + clear, 0x80 keep */
                static const uint8_t T[29] = {
                    0x7E, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
                    0x7E, 0x7E, 0x54, 0x80, 0x7E, 0x7E, 0x7E, 0x54, 0x7E, 0x54, 0x54, 0x54,
                    0x54, 0x54, 0x54, 0x7E, 0x80 };
                switch (T[src]) {
                case 0x7Eu: clr = true; break;
                case 0x54u: f19f(); clr = true; break;
                case 0x40u: unp(); clr = false; break;      /* 0xBABC8 */
                default:    clr = false; break;
                }
            }
            if (clr) {
                RQF_CELL_851A2 = (uint8_t)(RQF_CELL_851A2 & (uint8_t)~bit);
            }
        }
    }
    e4cf4(o, 1u);
    /* 0xE4548: not ported */
}

/* 0xE49B8: the tick */
static void e49b8(uint32_t o)
{
    const bool frozen = RQF_S8(o, RO_ELEM) == 2 && RQF_S8(o, RO_ST) == 5 && s_in->eng_stop != 0u;
    const uint16_t step = 10u;                  /* *0xFFF8E222 [O dump 0x000A] */
    const uint16_t bstep = 1u;                  /* *0xFFF8E220 [O dump 0x0001] */

    if (!frozen) {
        RQF_U16(o, RO_T) = b781e((uint32_t)RQF_U16(o, RO_T) + step);       /* 0xBAE02 */
    }
    if (frozen || RQF_S8(o, RO_ST) != 1) {
        const uint16_t v = RQF_U16(o, RO_QF);                              /* 0xBAE1E */
        const uint32_t d = (uint32_t)v - step;
        RQF_U16(o, RO_QF) = (d >= v) ? 0u : b781e(d);
    }
    {
        const uint8_t v = RQF_U8(o, RO_C31);                               /* 0xBAD64 */
        const uint32_t d = (uint32_t)v - bstep;
        RQF_U8(o, RO_C31) = (d >= v) ? 0u : (uint8_t)(d < 0x100u ? d : 0xFFu);
    }
    if (RQF_U32(o, RO_T18) > 0u) {
        RQF_U32(o, RO_T18) = RQF_U32(o, RO_T18) - 1u;
    }
    if (RQF_U32(o, RO_T14) > 0u) {
        RQF_U32(o, RO_T14) = RQF_U32(o, RO_T14) - 1u;
    }
    if (RQF_S8(o, RO_ST) == 3) {
        if (RQF_S8(o, RO_PRIO) == 1) {          /* 0xE4E30 */
            const int32_t lim = (int32_t)RQF_U16(o, RO_PKISS) +
                                (int32_t)(uint16_t)(int16_t)rqf_fl16(cmap(o, 0x68u));   /* 0xE51BC */
            if ((int32_t)RQF_U16(o, RO_PMOD) > lim) {
                const uint8_t v = RQF_U8(o, RO_C30);                            /* 0xBAD64 */
                const uint32_t d = (uint32_t)v - bstep;
                RQF_U8(o, RO_C30) = (d >= v) ? 0u : (uint8_t)d;
            } else {
                const uint32_t v = (uint32_t)RQF_U8(o, RO_C30) + bstep;         /* 0xBAD48 */
                RQF_U8(o, RO_C30) = (uint8_t)(v < 0x100u ? v : 0xFFu);
            }
        } else {
            RQF_U8(o, RO_C30) = 0u;
        }
    }
}

static const uint32_t ELEM_OBJ5[5] = { RQ_OBJ_A, RQ_OBJ_B, RQ_OBJ_C, RQ_OBJ_D, RQ_OBJ_E };

void rqf_e5602(void)                    /* boot state 1: 0xE5106 → 0xE4A4E */
{
    for (uint8_t e = 0u; e < 5u; e++) {
        e4a4e(ELEM_OBJ5[e]);
    }
}

void rqf_e5624(void)                    /* 10 ms: the tick */
{
    s_tb_89cf8 = (uint16_t)(s_tb_89cf8 + 10u);
    for (uint8_t e = 0u; e < 5u; e++) {
        e49b8(ELEM_OBJ5[e]);
    }
}

void rqf_e566c(void)                    /* 10 ms: the update, then 0x851A5 */
{
    for (uint8_t e = 0u; e < 5u; e++) {
        e3816(ELEM_OBJ5[e]);
    }
    RQF_CELL_851A5 = s_in->c3d;
}

/* 0xE3710: the constructor (+ our clear of the object and shadows) */
void rqf_construct(uint32_t o, uint32_t cal_cell, uint8_t elem)
{
    RQF_U32(o, RO_VPTR) = 0x0004A008u;
    RQF_U32(o, RO_CAL) = cal_cell;
    RQF_U8(o, RO_ELEM) = elem;
    RQF_U8(o, RO_IDX) = (uint8_t)(elem - 1u);
    if (elem == 0u || elem > 5u) {
        f19f();                                 /* 0x19F (0xD, 5, 0xFC) */
    }
    RQF_U8(o, RO_ST) = RQF_ST_CTOR;
    RQF_U8(o, RO_55) = 1u;
    RQF_U8(o, RO_BOOST) = 1u;
    for (uint32_t off = RO_TFILL; off <= RO_FRAC0; off += 2u) {
        RQF_U16(o, off) = 0u;
    }
    RQF_U8(o, RO_4B) = 0u;
    RQF_U8(o, RO_4C) = 0u;
    RQF_U8(o, RO_4D) = 0u;
    RQF_U8(o, RO_4E) = 0u;
    RQF_U8(o, RO_50) = 0u;
    RQF_U8(o, RO_51) = 0u;
    RQF_U8(o, RO_ACTIVE) = 0u;
    RQF_U8(o, RO_54) = 0u;
    RQF_U8(o, RO_57) = 0u;
    RQF_U8(o, RO_59) = 0u;
    RQF_U16(o, RO_5C) = 0u;
    RQF_U32(o, RO_T1C) = 0u;
    RQF_U32(o, RO_T20) = 0u;
    RQF_U8(o, RO_4A) = 0u;
    RQF_U8(o, RO_4F) = 0u;
    RQF_U8(o, RO_52) = 0u;
    RQF_U8(o, RO_56) = 0u;
    RQF_U8(o, RO_5B) = 0u;
    RQF_U8(o, RO_5A) = 0u;
    /* +0x4B: the vtable +0x90 setter 0xE4CEE (caller not ported): the
     * dump's value, stand-in */
    if (elem >= 1u && elem <= 5u) {
        RQF_U8(o, RO_4B) = s_in->k4b[elem - 1u];
        s_rec_frac[elem - 1u] = s_in->rec_frac[elem - 1u];
    }
}

/* ── the actuator mode-0 law 0x107C3E ──────────────────────────────────── */
/* the per-unit curve table 0xFFF8CDFC (RAM, EOL data; dump: all flags 1)
 * and the correction table 0xFFF8CDEC — stand-ins at the dump's values */
static const uint32_t UNIT_8CDFC[14] = {
    0x0000E004u, 1u, 0x0000E038u, 1u, 0x0000E06Cu, 1u, 0x0000E0A0u, 1u,
    0x0000E0D4u, 1u, 0x0000E108u, 1u, 0x0000E13Cu, 1u };
static const uint32_t CORR_8CDEC[4] = { 0x0000E170u, 1u, 0x0000E214u, 1u };

/* 0xD9D18: the pressure → current curve map by actuator id */
static uint32_t d9d18_map(uint8_t id, bool *ok)
{
    /* (per-unit pointer index, fallback offset in *0xFFF84D18) per id */
    static const uint8_t UI[7] = { 0u, 6u, 2u, 8u, 4u, 12u, 10u };
    static const uint8_t FB[7] = { 0x08u, 0x14u, 0x0Cu, 0x18u, 0x10u, 0x20u, 0x1Cu };
    *ok = id < 7u;
    if (!*ok) {
        return 0u;
    }
    if (UNIT_8CDFC[UI[id] + 1u] == 1u) {
        return UNIT_8CDFC[UI[id]];
    }
    return rqf_fl32(rqf_cell_ptr(0xFFF84D18u) + FB[id]);
}

/* 0xD9EA4(type, I) via 0xD9F96 */
static int16_t d9ea4(uint8_t type, int16_t i)
{
    uint32_t m;
    const uint32_t t = rqf_cell_ptr(0xFFF84D14u);
    if (type == 0u) {
        m = (CORR_8CDEC[3] == 1u) ? CORR_8CDEC[2] : rqf_fl32(t + 4u);
    } else {
        m = (CORR_8CDEC[1] == 1u) ? CORR_8CDEC[0] : rqf_fl32(t);
    }
    return (int16_t)rqf_b7164(m, i, (int16_t)temp());
}

/* 0xD9EF6 (B only) */
static int16_t d9ef6(int16_t i)
{
    return (int16_t)rqf_b7164(rqf_fl32(rqf_cell_ptr(0xFFF84D18u) + 4u), i, (int16_t)temp());
}

/* 0xD9F10 (0xD9F62 = 0xBAA5E: *(s16 *)0xFFF8BCC0) */
static int16_t d9f10(uint8_t type, int16_t i)
{
    const float x = (float)(int32_t)i;
    const float y = (float)(int32_t)s_in->lp2;
    const uint32_t m = rqf_fl32(rqf_cell_ptr(0xFFF84D18u) + (type == 0u ? 0x24u : 0x28u));
    return b7910(b7472(x, y, m));
}

uint16_t rqf_107c3e(uint8_t id, uint8_t type, uint16_t p, uint16_t imin, uint16_t imax)
{
    bool ok;
    const uint32_t m = d9d18_map(id, &ok);
    uint32_t i0;
    int32_t r14 = 0;

    if (!ok) {
        f19f();                                 /* 0x1B9 */
        i0 = 0u;
    } else {
        i0 = b785c((int32_t)rqf_b6eae(m, p));   /* 0xD9D18 → 0xB785C */
    }
    if (id == 0u || id == 1u || id == 3u || id == 4u) {
        /* nothing */
    } else if (id == 2u) {
        r14 = (int32_t)d9ef6(b7888((int32_t)i0));
    } else {
        f19f();                                 /* 0x1C4 (id) */
        i0 = 0u;
    }
    {
        const int16_t s = b7888((int32_t)i0);
        int32_t v = (int32_t)d9ea4(type, s) + ((int32_t)i0 + r14);
        v += (int32_t)d9f10(type, s);
        return b7786(b785c(v), imin, imax);     /* 0x107D6A */
    }
}

#endif /* !TCU_RAM_APP */

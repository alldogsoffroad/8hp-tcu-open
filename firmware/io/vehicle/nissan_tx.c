/* Nissan transmit set (see nissan_tx.h). */
#include "io/vehicle/vehicle_tx.h"
#include "io/vehicle/nissan_tx.h"
#include "bsw/com/com_tx.h"

#include <string.h>

enum { T_174, T_176, T_177, T_COUNT };

static const com_tx_cfg_t s_msg[T_COUNT] = {
    [T_174] = { NISSAN_TX_ID_174, CAN_BUS1, 8u, 10u, false, false },
    [T_176] = { NISSAN_TX_ID_176, CAN_BUS1, 7u, 10u, false, false },
    [T_177] = { NISSAN_TX_ID_177, CAN_BUS1, 6u, 10u, false, false },
};

/* the drivetrain snapshot the frames are packed from */
typedef struct {
    uint16_t out_rpm;
    uint16_t tur_rpm;
    uint8_t  engaged;
    uint8_t  target;
    uint8_t  cut;
    bool     lockup;
} tx_in_t;

typedef struct {
    com_tx_state_t st[T_COUNT];
    tx_in_t        in;
    bool           valid;
} nissan_tx_t;

/* BSS LAYOUT PAD (SH code-flash application only). The FCA transmit set (fca_tx.c) holds 240 bytes
 * of .bss (s_val 144 + s_st 96) at this position in the link, and the com_tx.c it references
 * another 16 - 256 bytes in all; several cells after it are pinned by link-time asserts (the
 * retired pad 0xFFF95F7C, safety/os_sel.h OS_SEL_OBJ, ...). The Nissan set's state and the
 * com_tx.c it now references live inside the same 256-byte span: the pad is what is left, so
 * every later .bss symbol keeps its FCA address. */
#define NISSAN_TX_SPAN    256u
#define COM_TX_BSS_BYTES   16u
static nissan_tx_t s_tx;
#if defined(__sh__) && !defined(TCU_RAM_APP)
static volatile uint8_t s_fca_bss_pad[NISSAN_TX_SPAN - COM_TX_BSS_BYTES - ((sizeof(nissan_tx_t) + 3u) & ~3u)]
    __attribute__((aligned(4)));
#endif

static uint8_t clamp_gear(uint8_t g)
{
    return g > 15u ? 15u : g;
}

uint8_t nissan_tx_shift_code(uint8_t engaged, uint8_t target)
{
    if (engaged >= 1u && engaged <= 4u && target == (uint8_t)(engaged + 1u)) {
        return (uint8_t)(engaged - 1u);            /* 1-2 = 0 ... 4-5 = 3 (logged) */
    }
    if (engaged == 4u && target == 3u) {
        return 9u;                                 /* logged */
    }
    if (engaged == 5u && target == 4u) {
        return 13u;                                /* logged */
    }
    return 15u;                                    /* none, or a shift with no logged code */
}

static void pack(uint8_t idx, uint8_t d[8])
{
    const tx_in_t *in = &s_tx.in;
    const uint8_t ctr = (uint8_t)(com_tx_state(idx)->counter & 0x0Fu);
    const uint8_t e = clamp_gear(in->engaged);
    const uint8_t t = clamp_gear(in->target);
    const uint8_t code = nissan_tx_shift_code(e, t);

    switch (idx) {
    case T_174:
        d[0] = 0xFFu;
        d[1] = 0xE7u;
        d[2] = 0x1Cu;
        d[3] = (uint8_t)(t << 4 | e);
        d[4] = ctr;
        d[5] = in->lockup ? 0x80u : 0x00u;
        break;
    case T_176:
        d[0] = (uint8_t)(in->out_rpm >> 8);
        d[1] = (uint8_t)in->out_rpm;
        d[2] = (uint8_t)(in->tur_rpm >> 8);
        d[3] = (uint8_t)in->tur_rpm;
        d[6] = ctr;
        break;
    case T_177:
        d[2] = (uint8_t)((e > 0u ? (uint8_t)((e - 1u) & 7u) : 0u) | (in->lockup ? 0x08u : 0u));
        d[3] = e == 1u ? 0x01u : 0x00u;
        d[4] = code <= 3u ? in->cut : 0u;          /* a cut only during a known upshift */
        d[5] = (uint8_t)(code << 3);
        break;
    default:
        break;
    }
}

void nissan_tx_init(void)
{
    memset(&s_tx, 0, sizeof s_tx);
    com_tx_init(s_msg, s_tx.st, T_COUNT, pack);
#if defined(__sh__) && !defined(TCU_RAM_APP)
    s_fca_bss_pad[0] = 0u;
#endif
}

void nissan_tx_tick_1ms(void)
{
    com_tx_tick(1u);
}

void nissan_tx_set(const vehicle_out_t *o)
{
    if (o == NULL || !o->drivetrain_valid) {
        return;
    }
    s_tx.in.out_rpm = o->output_rpm;
    s_tx.in.tur_rpm = o->turbine_rpm;
    s_tx.in.engaged = o->gear_engaged;
    s_tx.in.target = o->gear_target;
    s_tx.in.cut = o->torque_cut_q8;
    s_tx.in.lockup = o->lockup;
    s_tx.valid = true;
}

bool nissan_tx_enable(bool on)
{
    if (on && !s_tx.valid) {
        return false;
    }
    for (uint8_t i = 0u; i < (uint8_t)T_COUNT; i++) {
        com_tx_enable(i, on);
    }
    return on;
}

uint32_t nissan_tx_sent_count(void)
{
    uint32_t n = 0u;

    for (uint8_t i = 0u; i < (uint8_t)T_COUNT; i++) {
        n += s_tx.st[i].sent;
    }
    return n;
}

void nissan_tx_build(uint8_t idx, can_frame_t *f)
{
    com_tx_build(idx, f);
}

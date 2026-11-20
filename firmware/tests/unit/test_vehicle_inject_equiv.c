/* The message-level injector (io/vehicle/vehicle_inject.h) against the FCA profile: the same
 * message schedule - lever on both sources, brake, engine, ignition, park speed, with gaps
 * long enough to time messages out - is fed to profile_fca.c as real frames (counter + CRC) and
 * to the injector as messages, and vehicle_in() must be identical field by field after every
 * 10 ms update. This is what lets the transmission tests run on the injector with their goldens
 * (the FCA profile's 50 ms source-loss timing included) unchanged.
 * Both vehicle interfaces are compiled in under renamed symbols (CMake). */
#include "io/vehicle/vehicle.h"
#include "io/vehicle/vehicle_inject.h"
#include "bsw/com/crc8.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

void fca_vehicle_init(void);
void fca_vehicle_rx(can_bus_t bus, const can_frame_t *f);
void fca_vehicle_update_10ms(void);
const vehicle_in_t *fca_vehicle_in(void);

void inj_vehicle_init(void);
void inj_vehicle_update_10ms(void);
const vehicle_in_t *inj_vehicle_in(void);

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint8_t s_ctr[0x800];

static void send(can_bus_t bus, uint16_t id, uint8_t dlc, bool prot, uint8_t *d)
{
    can_frame_t f = { .id = id, .dlc = dlc };

    memcpy(f.data, d, dlc);
    if (prot) {
        s_ctr[id] = (uint8_t)((s_ctr[id] + 1u) & 0x0Fu);
        f.data[dlc - 2u] = (uint8_t)((f.data[dlc - 2u] & 0x0Fu) | (uint8_t)(s_ctr[id] << 4));
        f.data[dlc - 1u] = crc8_j1850(f.data, (uint8_t)(dlc - 1u));
    }
    fca_vehicle_rx(bus, &f);
}

static uint32_t s_seed = 12345u;
static uint32_t rnd(void)
{
    s_seed = s_seed * 1664525u + 1013904223u;
    return s_seed >> 8;
}

static unsigned same(const vehicle_in_t *a, const vehicle_in_t *b)
{
    /* the fields this injector models (the rest never leave their reset value in either) */
    return a->engine_speed == b->engine_speed && a->q_engine_speed == b->q_engine_speed &&
           a->torque_actual == b->torque_actual && a->q_torque_actual == b->q_torque_actual &&
           a->torque_t2 == b->torque_t2 && a->q_torque_t2 == b->q_torque_t2 &&
           a->torque_t3 == b->torque_t3 && a->q_torque_t3 == b->q_torque_t3 &&
           a->pedal == b->pedal && a->q_pedal == b->q_pedal &&
           a->engine_temp == b->engine_temp && a->q_engine_temp == b->q_engine_temp &&
           a->brake == b->brake && a->q_brake == b->q_brake &&
           a->engine_mode == b->engine_mode && a->q_engine_mode == b->q_engine_mode &&
           a->park_speed_raw == b->park_speed_raw && a->q_park_speed == b->q_park_speed &&
           a->soak_s == b->soak_s && a->q_soak == b->q_soak &&
           a->ign_code == b->ign_code && a->q_ign_code == b->q_ign_code &&
           a->lever == b->lever && a->q_lever == b->q_lever &&
           a->lever_flag == b->lever_flag && a->lever_src == b->lever_src;
}

int main(void)
{
    unsigned diffs = 0u;
    /* per-source activity windows (ticks): a source sends for a while, then falls silent */
    unsigned on[6] = { 0 };
    uint8_t lev[2] = { 1u, 1u };
    uint8_t brk = 1u;
    uint16_t eng = 700u, t1 = 0x1FFFu, psp = 100u;
    uint8_t ign = 4u;

    inj_vehicle_init();
    fca_vehicle_init();
    CHECK(same(fca_vehicle_in(), inj_vehicle_in()));       /* the power-on state */
    for (unsigned t = 0u; t < 20000u; t++) {
        uint8_t d[8];

        if (t % 17u == 0u) {            /* re-roll a source's window and content now and then */
            const unsigned s = rnd() % 6u;
            const unsigned r = rnd() % 4u;

            on[s] = r == 0u ? 0u : (r == 1u ? 3u : (r == 2u ? 6u : 40u));   /* 6 ticks > the 5-tick timeout */
            lev[0] = (uint8_t)(rnd() % 8u);
            lev[1] = (uint8_t)(rnd() % 8u);
            brk = (uint8_t)(rnd() % 4u);
            eng = (uint16_t)(rnd() % 0x9000u);
            t1 = (uint16_t)(rnd() % 3u == 0u ? 0x1FFFu : rnd() % 0x1FFFu);
            psp = (uint16_t)(rnd() % 0x800u);
            ign = (uint8_t)(rnd() % 8u);
        }
        if (on[0] != 0u) {              /* lever, primary (0x2EA-like) */
            on[0]--;
            memset(d, 0, sizeof d);
            d[0] = lev[0];
            send(CAN_BUS1, 0x2EAu, 5u, true, d);
            vehicle_inject_msg_lever(LEVER_SRC_PRIMARY, lev[0]);
        }
        if (on[1] != 0u) {              /* lever, fallback (0x210-like) */
            on[1]--;
            memset(d, 0, sizeof d);
            d[0] = lev[1];
            send(CAN_BUS2, 0x210u, 5u, true, d);
            vehicle_inject_msg_lever(LEVER_SRC_FALLBACK, lev[1]);
        }
        if (on[2] != 0u) {              /* brake */
            on[2]--;
            memset(d, 0, sizeof d);
            d[0] = (uint8_t)(brk << 2);
            send(CAN_BUS1, 0x140u, 8u, true, d);
            vehicle_inject_msg_brake((brake_t)brk);
        }
        if (on[3] != 0u) {              /* engine */
            on[3]--;
            memset(d, 0, sizeof d);
            d[0] = (uint8_t)(eng >> 8);
            d[1] = (uint8_t)eng;
            d[2] = (uint8_t)(t1 >> 8);
            d[3] = (uint8_t)t1;
            send(CAN_BUS1, 0x108u, 8u, true, d);
            vehicle_inject_msg_engine(eng, t1);
        }
        if (on[4] != 0u) {              /* ignition status */
            on[4]--;
            memset(d, 0, sizeof d);
            d[0] = ign;
            send(CAN_BUS1, 0x112u, 2u, false, d);
            vehicle_inject_msg_ign(ign);
        }
        if (on[5] != 0u) {              /* park speed */
            on[5]--;
            memset(d, 0, sizeof d);
            d[0] = (uint8_t)(psp >> 8);
            d[1] = (uint8_t)psp;
            send(CAN_BUS1, 0x14Bu, 8u, true, d);
            vehicle_inject_msg_park_speed(psp);
        }
        fca_vehicle_update_10ms();
        inj_vehicle_update_10ms();
        if (!same(fca_vehicle_in(), inj_vehicle_in())) {
            if (diffs < 3u) {
                const vehicle_in_t *a = fca_vehicle_in(), *b = inj_vehicle_in();
                printf("t%u eng %d/%d q%d/%d tq %d/%d q%d/%d t2 q%d/%d pedal q%d/%d temp q%d/%d brake %d/%d q%d/%d mode q%d/%d psp %u/%u q%d/%d soak q%d/%d ign %u/%u q%d/%d lev %d/%d q%d/%d fl %d/%d src %d/%d\n", t,
                       a->engine_speed, b->engine_speed, a->q_engine_speed, b->q_engine_speed, a->torque_actual, b->torque_actual, a->q_torque_actual, b->q_torque_actual, a->q_torque_t2, b->q_torque_t2, a->q_pedal, b->q_pedal, a->q_engine_temp, b->q_engine_temp, a->brake, b->brake, a->q_brake, b->q_brake, a->q_engine_mode, b->q_engine_mode, a->park_speed_raw, b->park_speed_raw, a->q_park_speed, b->q_park_speed, a->q_soak, b->q_soak, a->ign_code, b->ign_code, a->q_ign_code, b->q_ign_code, a->lever, b->lever, a->q_lever, b->q_lever, a->lever_flag, b->lever_flag, a->lever_src, b->lever_src);
            }
            diffs++;
        }
    }
    CHECK(diffs == 0u);
    printf("vehicle_inject_equiv: %u differing updates of 20000%s\n", diffs, diffs == 0u ? " - OK" : "");
    return fails == 0 ? 0 : 1;
}

/* Platform stubs for unit tests: capture transmitted frames, resets and hand-overs. */
#include "tests/unit/stub_plat.h"

#include <string.h>

stub_plat_t stub;

bool plat_can_rx_register(can_bus_t bus, uint16_t id)
{
    (void)bus;
    (void)id;
    return true;
}

bool plat_can_send(can_bus_t bus, const can_frame_t *f)
{
    if (stub.n_tx >= STUB_TX_MAX) {
        return false;
    }
    stub.tx_bus[stub.n_tx] = bus;
    stub.tx[stub.n_tx++] = *f;
    return true;
}

void plat_reset(void)
{
    stub.resets++;
}

void plat_boot_handover(const uint8_t *rec, uint8_t len)
{
    memcpy(stub.rec, rec, len);
    stub.handovers++;
}

bool plat_mem_read(uint32_t addr, uint8_t *out, uint16_t n)
{
    if (addr < 0xFFF80000u || addr - 0xFFF80000u + n > sizeof stub.ram) {
        return false;
    }
    memcpy(out, &stub.ram[addr - 0xFFF80000u], n);
    return true;
}

bool plat_adc_raw(uint8_t an, uint16_t *raw)
{
    if (an >= 47u || !stub.adc_on[an]) {
        return false;
    }
    *raw = stub.adc[an];
    return true;
}

bool plat_speed_read(uint8_t ch, speed_raw_t *r)
{
    if (ch >= SPEED_CHANNELS || !stub.speed_on[ch]) {
        return false;
    }
    *r = stub.speed[ch];
    return true;
}

void plat_sup_init(void) { stub.sup_n = 0u; }

uint8_t plat_sup_exchange(uint16_t tx, uint16_t *rx)
{
    uint8_t r = 0u;
    if (stub.sup_n != 0u) {
        *rx = stub.sup_reply;
        r |= 2u;
    }
    if (!stub.sup_busy) {
        stub.sup_tx[stub.sup_n % 64u] = tx;     /* ring: long tests keep running */
        stub.sup_n++;
        r |= 1u;
    }
    return r;
}

/* Simulated driver chips: writes (bit 7) store the data byte(s) per command; reads return the
 * register written by the paired write command, with the header the stock handlers check. */
static uint16_t drv_read(uint8_t chip, uint8_t cmd)
{
    switch (cmd) {
    case 0x28: return stub.drv_reg[chip][0x88];
    case 0x30: return stub.drv_reg[chip][0x90];
    case 0x2E: return stub.drv_reg[chip][0x8E];
    case 0x36: return stub.drv_reg[chip][0x96];
    case 0x60: return stub.drv_reg[chip][0xA0];
    case 0xC0: return (uint16_t)(stub.drv_reg[chip][0xE8] | stub.drv_reg[chip][0xE9] << 8);
    case 0x66: return stub.drv_reg[chip][0xA6];
    case 0xD8: return (uint16_t)(stub.drv_reg[chip][0xF0] | stub.drv_reg[chip][0xF1] << 8);
    case 0x06: return 0x12;
    default:   return 0;
    }
}

void plat_drv_init(void) { }

bool plat_drv_xfer(const uint16_t *tx, uint16_t *rx, uint8_t n)
{
    if (stub.drv_fail) {
        return false;
    }
    stub.drv_jobs++;
    for (uint8_t i = 0u; i < n; i++) {
        const uint8_t chip = (uint8_t)(i % 5u), cmd = (uint8_t)(tx[i] >> 8);
        if (n == 45u && (i / 5u == 2u || i / 5u == 4u)) {      /* 11-bit setpoints */
            const uint8_t reg = i / 5u == 2u ? 0xE8u : 0xF0u;
            stub.drv_reg[chip][reg] = (uint8_t)tx[i];
            stub.drv_reg[chip][reg + 1u] = (uint8_t)((tx[i] >> 8) & 0x07u);
        } else if ((cmd & 0x80u) != 0u) {
            stub.drv_reg[chip][cmd] = (uint8_t)tx[i];
        }
        if (i < 45u) {
            stub.drv_last_tx[i] = tx[i];
        }
    }
    for (uint8_t i = 0u; i < n; i++) {
        const uint8_t chip = (uint8_t)(i % 5u), cmd = (uint8_t)(tx[i] >> 8);
        uint16_t v = drv_read(chip, cmd);
        if (stub.drv_bad_readback) {
            v ^= 0x01u;
        }
        rx[i] = n == 45u ? (uint16_t)(0xA000u | (v & 0x7FFu)) : (uint16_t)(0xA200u | (v & 0xFFu));
        if (stub.drv_bad_header) {
            rx[i] = 0x0000u;
        }
    }
    return true;
}

bool plat_userboot_copy(uint8_t *dst, uint32_t len)
{
    if (!stub.userboot_ok) {
        return false;
    }
    for (uint32_t i = 0u; i < len; i++) {
        dst[i] = (uint8_t)(i * 7u + 3u);        /* recognisable pattern */
    }
    return true;
}

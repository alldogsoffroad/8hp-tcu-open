/* FCU driver: firmware transfer, P/E mode entry/exit, program, block erase, error recovery.
 * Sequences follow the manual's figures 25.14-25.19 (section 25.6.3); see spec 16 section 3.
 * Rules kept here:
 *   - the ROM is read only in ROM read mode (FENTRYR = 0); every operation leaves P/E mode before
 *     returning, and the ROM cache is flushed after every program/erase;
 *   - FENTRYR is changed only from 0x0000 (a write while it is non-zero clears it);
 *   - every wait loop services the watchdog and supervisor (no watchdog reset during P/E);
 *   - lock bits are never touched and FPROTR is left at its reset value. */
#include "flasher/fl_fcu.h"
#include "flasher/fl_hal.h"
#include "flasher/fl_regs.h"

void (*fl_fcu_busy_hook)(void);

static bool s_fw_loaded;
static uint8_t s_last_err;

void fl_fcu_init(void)
{
    s_fw_loaded = false;
    s_last_err = FL_OK;
}

bool fl_fcu_fwe(void)
{
    return (fl_rd8(FL_FPMON) & FL_FPMON_FWE) != 0u;
}

static void idle(void)
{
    fl_hal_poll();
    if (fl_fcu_busy_hook != 0) {
        fl_fcu_busy_hook();
    }
}

/* FRESET held for at least tRESW2 (20 us); 1 ms here. Clears FENTRYR. */
static void fcu_reset(void)
{
    const uint32_t t0 = fl_hal_ms();

    fl_wr16(FL_FRESETR, 0xCC01u);
    while (fl_hal_ms() - t0 < 2u) {
        fl_hal_poll();
    }
    fl_wr16(FL_FRESETR, 0xCC00u);
}

static void read_mode(void)
{
    fl_wr16(FL_FENTRYR, 0xAA00u);
    (void)fl_rd16(FL_FENTRYR);          /* dummy read, then at least 5 NOPs (25.6.2 (1)) */
    FL_NOP5();
    fl_hal_rom_cache_flush();
}

/* Figure 25.14: FCU firmware to FCU RAM. CPU copy instead of the manual's DMAC [I]. */
static void fw_transfer(void)
{
    if (fl_rd16(FL_FENTRYR) != 0u) {
        read_mode();
    }
    fl_wr16(FL_FCURAME, 0xC401u);
    for (uint32_t i = 0u; i < FL_FCU_FW_LEN; i += 4u) {
        fl_wr32(FL_FCU_RAM + i, fl_rd32(FL_FCU_FW + i));
    }
    s_fw_loaded = true;
}

/* Error handling of figure 25.16: reset on a timeout or FCU-internal error (and reload the
 * firmware next time), else clear the command-locked state with FASTAT = 0x10 + status clear. */
static fl_err_t recover(uint32_t pe, fl_err_t e)
{
    const uint8_t s1 = fl_rd8(FL_FSTATR1);

    if (e == FL_E_TIMEOUT || (s1 & FL_FSTATR1_ERRS) != 0u) {
        fcu_reset();
        if ((s1 & FL_FSTATR1_ERRS) != 0u) {
            s_fw_loaded = false;
            e = FL_E_FCU;
        }
    } else {
        if (fl_rd8(FL_FASTAT) != FL_FASTAT_CMDLK) {
            fl_wr8(FL_FASTAT, FL_FASTAT_CMDLK);
        }
        fl_wr8(pe, 0x50u);              /* status register clear */
    }
    read_mode();
    s_last_err = (uint8_t)e;
    return e;
}

static fl_err_t check(void)
{
    const uint8_t s0 = fl_rd8(FL_FSTATR0);
    const uint8_t s1 = fl_rd8(FL_FSTATR1);

    if ((s1 & FL_FSTATR1_ERRS) != 0u) {
        return FL_E_FCU;
    }
    if ((s0 & FL_FSTATR0_ILGL) != 0u) {
        return FL_E_ILGL;
    }
    if ((s0 & FL_FSTATR0_ERS) != 0u) {
        return FL_E_ERS;
    }
    if ((s0 & FL_FSTATR0_PRG) != 0u) {
        return FL_E_PRG;
    }
    return FL_OK;
}

/* Figure 25.15: P/E mode for one area (bit = FENTRYR value), command address pe. */
static fl_err_t enter_mode(uint16_t bit, uint32_t pe)
{
    fl_err_t e;

    if (!s_fw_loaded) {
        fw_transfer();
    }
    if (fl_rd16(FL_FENTRYR) != 0u) {
        read_mode();
    }
    fl_wr16(FL_FENTRYR, (uint16_t)(0xAA00u | bit));
    if (fl_rd16(FL_FENTRYR) != bit) {
        read_mode();
        s_last_err = FL_E_FWE;
        return FL_E_FWE;
    }
    e = check();
    if (e != FL_OK) {                   /* left over from an earlier failure */
        (void)recover(pe, e);
        fl_wr16(FL_FENTRYR, (uint16_t)(0xAA00u | bit));
        if (fl_rd16(FL_FENTRYR) != bit || check() != FL_OK) {
            read_mode();
            return (fl_err_t)s_last_err;
        }
    }
    return FL_OK;
}

/* Code flash: FWE must be high (hardware protection, manual 25.8.1). */
static fl_err_t enter(uint32_t rom)
{
    if (!fl_fcu_fwe()) {
        s_last_err = FL_E_FWE;
        return FL_E_FWE;
    }
    return enter_mode(rom < 0x00100000u ? 0x0001u : 0x0002u, FL_PE_BASE + rom);
}

static fl_err_t wait_ready(uint32_t pe, uint32_t timeout_ms)
{
    const uint32_t t0 = fl_hal_ms();
    fl_err_t e;

    while ((fl_rd8(FL_FSTATR0) & FL_FSTATR0_FRDY) == 0u) {
        idle();
        if (fl_hal_ms() - t0 > timeout_ms) {
            return recover(pe, FL_E_TIMEOUT);
        }
    }
    e = check();
    if (e != FL_OK) {
        return recover(pe, e);
    }
    read_mode();
    s_last_err = FL_OK;
    return FL_OK;
}

fl_err_t fl_fcu_erase(uint32_t rom)
{
    const uint32_t pe = FL_PE_BASE + rom;
    fl_err_t e = enter(rom);

    if (e != FL_OK) {
        return e;
    }
    fl_wr8(pe, 0x20u);
    fl_wr8(pe, 0xD0u);
    return wait_ready(pe, FL_ERASE_TIMEOUT_MS);
}

fl_err_t fl_fcu_program(uint32_t rom, const uint8_t *p)
{
    const uint32_t pe = FL_PE_BASE + rom;
    fl_err_t e = enter(rom);

    if (e != FL_OK) {
        return e;
    }
    fl_wr8(pe, 0xE8u);
    fl_wr8(pe, 0x80u);
    for (uint32_t i = 0u; i < 256u; i += 2u) {
        fl_wr16(pe + i, (uint16_t)((uint16_t)p[i] << 8 | p[i + 1u]));
    }
    fl_wr8(pe, 0xD0u);
    return wait_ready(pe, FL_PROG_TIMEOUT_MS);
}

/* ---- Data flash (manual section 26.6.3): FENTRYD, no FWE dependency ---- */
void fl_fcu_df_enable(uint16_t read_blocks, uint16_t write_blocks)
{
    fl_wr16(FL_EEPRE0, (uint16_t)(0x2D00u | (read_blocks & 0xFFu)));
    fl_wr16(FL_EEPRE1, (uint16_t)(0xD200u | (read_blocks >> 8)));
    fl_wr16(FL_EEPWE0, (uint16_t)(0x1E00u | (write_blocks & 0xFFu)));
    fl_wr16(FL_EEPWE1, (uint16_t)(0xE100u | (write_blocks >> 8)));
}

fl_err_t fl_fcu_df_erase(uint32_t addr)
{
    const uint32_t ba = addr & ~(FL_DF_BLOCK - 1u);
    fl_err_t e = enter_mode(0x0080u, ba);

    if (e != FL_OK) {
        return e;
    }
    fl_wr8(ba, 0x20u);
    fl_wr8(ba, 0xD0u);
    return wait_ready(ba, FL_DF_ERASE_TIMEOUT_MS);
}

fl_err_t fl_fcu_df_program(uint32_t addr, const uint8_t *p)
{
    fl_err_t e = enter_mode(0x0080u, addr);

    if (e != FL_OK) {
        return e;
    }
    fl_wr8(addr, 0xE8u);
    fl_wr8(addr, 0x40u);                /* 64 words = 128 bytes */
    for (uint32_t i = 0u; i < 128u; i += 2u) {
        fl_wr16(addr + i, (uint16_t)((uint16_t)p[i] << 8 | p[i + 1u]));
    }
    fl_wr8(addr, 0xD0u);
    return wait_ready(addr, FL_DF_PROG_TIMEOUT_MS);
}

void fl_fcu_read(uint32_t rom, uint8_t *dst, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        dst[i] = fl_rd8(rom + i);
    }
}

void fl_fcu_diag(uint8_t out[6])
{
    out[0] = fl_rd8(FL_FPMON);
    out[1] = fl_rd8(FL_FSTATR0);
    out[2] = fl_rd8(FL_FSTATR1);
    out[3] = fl_rd8(FL_FASTAT);
    out[4] = (uint8_t)fl_rd16(FL_FPESTAT);
    out[5] = s_last_err;
}

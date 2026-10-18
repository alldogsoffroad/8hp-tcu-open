#include "tests/unit/fl_sim.h"
#include "flasher/fl_core.h"
#include "flasher/fl_hal.h"
#include "flasher/fl_regs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

fl_sim_t sim;

#define PROG_MS   2u
#define ERASE_MS  800u

void fl_sim_init(void)
{
    memset(&sim, 0, sizeof sim);
    memset(sim.rom, 0xFF, sizeof sim.rom);
    for (uint32_t i = 0u; i < SIM_DF_SIZE; i++) {      /* unknown prior content, not erased */
        sim.df[i] = (uint8_t)(i * 37u + 11u);
    }
    sim.fwe = true;
    sim.fstatr0 = FL_FSTATR0_FRDY;
    sim.cut_after = -1;
}

uint32_t fl_sim_fw_word(uint32_t i)
{
    return 0xF0C00000u ^ (i * 0x9E3779B9u);
}

void fl_sim_push(const uint8_t *d, uint8_t dlc)
{
    memcpy(sim.rxq[sim.rx_tail % SIM_QLEN], d, 8);
    sim.rxq[sim.rx_tail % SIM_QLEN][8] = dlc;
    sim.rx_tail++;
}

static void violation(const char *what, uint32_t a)
{
    sim.violations++;
    fprintf(stderr, "fl_sim: RULE VIOLATION: %s at 0x%08X\n", what, a);
}

static void lock(uint8_t bit)
{
    sim.fstatr0 |= bit;
    sim.fastat |= FL_FASTAT_CMDLK;
    sim.cmd = 0;
}

static uint32_t block_start(uint32_t a)
{
    const uint32_t sz = fl_block_size(a);
    return a & ~(sz - 1u);
}

/* power cut: the area being worked on becomes undefined, then the "process" ends */
static bool power_cut_mem(uint8_t *mem, uint32_t a, uint32_t len)
{
    const long done = (long)(sim.erases + sim.programs + sim.df_erases + sim.df_programs);

    if (sim.cut_after < 0 || done < sim.cut_after) {
        return false;
    }
    for (uint32_t i = 0u; i < len; i += 2u) {
        mem[a + i] = (uint8_t)(0x5Au ^ i);
    }
    sim.cut = true;
    sim.stop = true;
    return true;
}

static bool power_cut(uint32_t a, uint32_t len)
{
    return power_cut_mem(sim.rom, a, len);
}

/* ---- data MAT ---- */
static bool df_addr(uint32_t a)
{
    return a >= FL_DF_BASE && a < FL_DF_BASE + SIM_DF_SIZE;
}

static unsigned df_block(uint32_t a)
{
    return (unsigned)((a - FL_DF_BASE) / FL_DF_BLOCK);
}

static uint32_t undefined_byte(uint32_t off, unsigned salt)
{
    return (uint8_t)((off * 2654435761u) >> 13) ^ (uint8_t)(salt * 0x3Du);
}

static void df_write(uint32_t a, uint32_t v, unsigned size)
{
    const uint32_t off = a - FL_DF_BASE;

    if (sim.cut) {
        return;
    }
    if (sim.fentryr != 0x0080u) {
        violation("data-flash write outside EEPROM P/E mode", a);
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (!sim.fw_loaded) {
        sim.fstatr1 |= 0x80u;
        sim.fastat |= FL_FASTAT_CMDLK;
        return;
    }
    if ((sim.fstatr0 & FL_FSTATR0_FRDY) == 0u) {
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (sim.cmd == 12) {                    /* program data phase */
        if (size == 2u && sim.prog_n < sim.prog_count) {
            if (sim.prog_n == 0u) {
                if ((off & (sim.prog_count * 2u - 1u)) != 0u) {
                    violation("data-flash program start not aligned", a);
                    lock(FL_FSTATR0_ILGL);
                    return;
                }
                sim.prog_base = off;
            }
            sim.prog_buf[sim.prog_n++] = (uint16_t)v;
            return;
        }
        if (size == 1u && v == 0xD0u && sim.prog_n == sim.prog_count) {
            const uint32_t len = sim.prog_count * 2u;
            sim.df_programs++;
            sim.cmd = 0;
            if (power_cut_mem(sim.df, sim.prog_base, len)) {
                return;
            }
            if ((sim.eepwe & (1u << df_block(FL_DF_BASE + sim.prog_base))) == 0u) {
                sim.fpestat = 0x01u;
                lock(FL_FSTATR0_PRG);
                return;
            }
            for (uint32_t u = sim.prog_base / 8u; u < (sim.prog_base + len) / 8u; u++) {
                if (!sim.df_erased[u]) {
                    violation("programming a data-flash unit that is not erased",
                              FL_DF_BASE + u * 8u);
                    sim.fpestat = 0x02u;
                    lock(FL_FSTATR0_PRG);
                    return;
                }
            }
            for (uint32_t i = 0u; i < sim.prog_count; i++) {
                sim.df[sim.prog_base + 2u * i] = (uint8_t)(sim.prog_buf[i] >> 8);
                sim.df[sim.prog_base + 2u * i + 1u] = (uint8_t)sim.prog_buf[i];
            }
            for (uint32_t u = sim.prog_base / 8u; u < (sim.prog_base + len) / 8u; u++) {
                sim.df_erased[u] = 0u;
            }
            sim.fstatr0 &= (uint8_t)~FL_FSTATR0_FRDY;
            sim.busy_ms = 1u;
            return;
        }
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (size != 1u) {
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (sim.cmd == 11) {
        sim.prog_n = 0u;
        if (v == 0x40u || v == 0x04u) {
            sim.prog_count = v;
            sim.cmd = 12;
        } else {
            sim.cmd = 0;
            lock(FL_FSTATR0_ILGL);
        }
        return;
    }
    if (sim.cmd == 13) {
        sim.cmd = 0;
        if (v != 0xD0u) {
            lock(FL_FSTATR0_ILGL);
            return;
        }
        {
            const uint32_t b = off & ~(FL_DF_BLOCK - 1u);
            sim.df_erases++;
            if (power_cut_mem(sim.df, b, FL_DF_BLOCK)) {
                return;
            }
            if ((sim.eepwe & (1u << df_block(a))) == 0u) {
                sim.fpestat = 0x11u;
                lock(FL_FSTATR0_ERS);
                return;
            }
            for (uint32_t i = 0u; i < FL_DF_BLOCK; i++) {
                sim.df[b + i] = (uint8_t)undefined_byte(b + i, sim.df_erases);
            }
            memset(&sim.df_erased[b / 8u], 1, FL_DF_BLOCK / 8u);
            sim.fstatr0 &= (uint8_t)~FL_FSTATR0_FRDY;
            sim.busy_ms = 80u;
        }
        return;
    }
    switch (v) {
    case 0xE8u: sim.cmd = 11; break;
    case 0x20u: sim.cmd = 13; break;
    case 0xFFu: break;
    case 0x50u:
        if ((sim.fastat & 0xEFu) != 0u) {
            break;
        }
        sim.fstatr0 &= (uint8_t)~(FL_FSTATR0_ILGL | FL_FSTATR0_ERS | FL_FSTATR0_PRG);
        sim.fastat = 0u;
        break;
    default:
        lock(FL_FSTATR0_ILGL);
        break;
    }
}

static uint8_t df_read(uint32_t a)
{
    if (sim.fentryr != 0u) {
        violation("data-flash read while in P/E mode", a);
        return 0xEEu;
    }
    if ((sim.eepre & (1u << df_block(a))) == 0u) {
        violation("data-flash read from a block without read enable", a);
        return 0xEEu;
    }
    return sim.df[a - FL_DF_BASE];
}

static bool pe_area_ok(uint32_t a)
{
    if (a >= 0x80800000u && a < 0x80900000u) {
        return sim.fentryr == 0x0001u;
    }
    if (a >= 0x80900000u && a < 0x80A00000u) {
        return sim.fentryr == 0x0002u;
    }
    return false;
}

static void pe_write(uint32_t a, uint32_t v, unsigned size)
{
    const uint32_t rom = a - FL_PE_BASE;

    if (sim.cut) {
        return;                         /* no power any more */
    }
    if (!pe_area_ok(a)) {
        violation("P/E address outside the selected area", a);
        sim.fastat |= 0x80u;
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (!sim.fw_loaded) {
        sim.fstatr1 |= 0x80u;           /* FCUERR: no firmware */
        sim.fastat |= FL_FASTAT_CMDLK;
        return;
    }
    if ((sim.fstatr0 & FL_FSTATR0_FRDY) == 0u) {
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (sim.cmd == 2) {                 /* program data phase */
        if (size == 2u && sim.prog_n < 128u) {
            if (sim.prog_n == 0u) {
                if ((rom & 0xFFu) != 0u) {
                    violation("program start not 256-aligned", a);
                    lock(FL_FSTATR0_ILGL);
                    return;
                }
                sim.prog_base = rom;
            }
            sim.prog_buf[sim.prog_n++] = (uint16_t)v;
            return;
        }
        if (size == 1u && v == 0xD0u && sim.prog_n == 128u) {
            sim.programs++;
            sim.cmd = 0;
            if (power_cut(sim.prog_base, 256u)) {
                return;
            }
            for (unsigned i = 0u; i < 256u; i++) {
                if (sim.rom[sim.prog_base + i] != 0xFFu) {
                    violation("programming a page that is not erased", FL_PE_BASE + sim.prog_base);
                    sim.fpestat = 0x02u;
                    lock(FL_FSTATR0_PRG);
                    return;
                }
            }
            if (sim.prog_base == sim.fail_prog_addr && sim.fail_prog_addr != 0u) {
                sim.fpestat = 0x02u;
                lock(FL_FSTATR0_PRG);
                return;
            }
            for (unsigned i = 0u; i < 128u; i++) {
                sim.rom[sim.prog_base + 2u * i] = (uint8_t)(sim.prog_buf[i] >> 8);
                sim.rom[sim.prog_base + 2u * i + 1u] = (uint8_t)sim.prog_buf[i];
            }
            sim.fstatr0 &= (uint8_t)~FL_FSTATR0_FRDY;
            sim.busy_ms = PROG_MS;
            return;
        }
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (size != 1u) {
        lock(FL_FSTATR0_ILGL);
        return;
    }
    if (sim.cmd == 1) {
        sim.cmd = v == 0x80u ? 2 : 0;
        sim.prog_n = 0u;
        if (v != 0x80u) {
            lock(FL_FSTATR0_ILGL);
        }
        return;
    }
    if (sim.cmd == 3) {
        sim.cmd = 0;
        if (v != 0xD0u) {
            lock(FL_FSTATR0_ILGL);
            return;
        }
        {
            const uint32_t b = block_start(rom);
            const uint32_t sz = fl_block_size(rom);
            sim.erases++;
            if (power_cut(b, sz)) {
                return;
            }
            if (b == sim.fail_erase_addr && sim.fail_erase_addr != 0u) {
                sim.fpestat = 0x12u;
                lock(FL_FSTATR0_ERS);
                return;
            }
            memset(&sim.rom[b], 0xFF, sz);
            sim.fstatr0 &= (uint8_t)~FL_FSTATR0_FRDY;
            sim.busy_ms = ERASE_MS;
        }
        return;
    }
    switch (v) {
    case 0xE8u: sim.cmd = 1; break;
    case 0x20u: sim.cmd = 3; break;
    case 0xFFu: break;
    case 0x50u:                         /* status clear */
        if ((sim.fastat & 0xEFu) != 0u) {
            break;                      /* still an access error flag: stays locked */
        }
        sim.fstatr0 &= (uint8_t)~(FL_FSTATR0_ILGL | FL_FSTATR0_ERS | FL_FSTATR0_PRG);
        sim.fastat = 0u;
        break;
    default:
        lock(FL_FSTATR0_ILGL);
        break;
    }
}

static uint8_t rom_read(uint32_t a)
{
    if (sim.fentryr != 0u) {
        violation("ROM read while in P/E mode", a);
        sim.fastat |= 0x80u;
        lock(FL_FSTATR0_ILGL);
        return 0xEEu;
    }
    if (sim.freset) {
        violation("ROM read during FCU reset", a);
    }
    return a < SIM_ROM_SIZE ? sim.rom[a] : 0xFFu;
}

uint8_t fl_rd8(uint32_t a)
{
    switch (a) {
    case FL_FPMON:   return sim.fwe ? 0x80u : 0u;
    case FL_FASTAT:  return sim.fastat;
    case FL_FSTATR0:
        if (sim.busy_ms != 0u) {
            sim.polls_busy++;
        }
        return sim.fstatr0;
    case FL_FSTATR1: return sim.fstatr1;
    default: break;
    }
    if (a < SIM_ROM_SIZE) {
        return rom_read(a);
    }
    if (df_addr(a)) {
        return df_read(a);
    }
    violation("unexpected 8-bit read", a);
    return 0u;
}

uint16_t fl_rd16(uint32_t a)
{
    if (a == FL_FENTRYR) {
        return sim.fentryr;
    }
    if (a == FL_FPESTAT) {
        return sim.fpestat;
    }
    violation("unexpected 16-bit read", a);
    return 0u;
}

uint32_t fl_rd32(uint32_t a)
{
    if (a >= FL_FCU_FW && a < FL_FCU_FW + FL_FCU_FW_LEN) {
        if (sim.fentryr != 0u) {
            violation("FCU firmware read in P/E mode", a);
        }
        return fl_sim_fw_word((a - FL_FCU_FW) / 4u);
    }
    if (df_addr(a) && df_addr(a + 3u)) {
        return (uint32_t)df_read(a) << 24 | (uint32_t)df_read(a + 1u) << 16 |
               (uint32_t)df_read(a + 2u) << 8 | df_read(a + 3u);
    }
    violation("unexpected 32-bit read", a);
    return 0u;
}

void fl_wr8(uint32_t a, uint8_t v)
{
    if (a == FL_FASTAT) {
        if (v == FL_FASTAT_CMDLK) {
            sim.fastat &= FL_FASTAT_CMDLK;  /* error flags cleared by writing 0 */
        }
        return;
    }
    if (a >= 0x80800000u && a < 0x80A00000u) {
        pe_write(a, v, 1u);
        return;
    }
    if (df_addr(a)) {
        df_write(a, v, 1u);
        return;
    }
    violation("unexpected 8-bit write", a);
}

void fl_wr16(uint32_t a, uint16_t v)
{
    if (a == FL_FENTRYR) {
        const uint16_t bits = (uint16_t)(v & 0x00FFu);
        if ((v >> 8) != 0xAAu) {
            sim.fentryr = 0u;
            return;
        }
        if (sim.fentryr != 0u) {
            sim.fentryr = 0u;           /* a write while non-zero clears it */
            if (bits != 0u) {
                violation("FENTRYR changed without passing through 0", a);
            }
            return;
        }
        if (bits != 0u && (sim.fstatr0 & FL_FSTATR0_FRDY) == 0u) {
            return;                     /* entry refused */
        }
        if ((bits == 1u || bits == 2u) && !sim.fwe) {
            return;                     /* code flash: FWE pin protection (25.8.1) */
        }
        if (bits != 0u && bits != 1u && bits != 2u && bits != 0x80u) {
            lock(FL_FSTATR0_ILGL);
            return;
        }
        sim.fentryr = bits;
        return;
    }
    if (a == FL_FCURAME) {
        if ((v >> 8) == 0xC4u) {
            if (sim.fentryr != 0u) {
                violation("FCURAME written in P/E mode", a);
            }
            sim.fcurame = (v & 1u) != 0u;
        }
        return;
    }
    if (a == FL_FRESETR) {
        if ((v >> 8) == 0xCCu) {
            if ((v & 1u) != 0u) {
                sim.freset = true;
                sim.resets_fcu++;
                sim.fentryr = 0u;
                sim.fstatr0 = FL_FSTATR0_FRDY;
                sim.fstatr1 = 0u;
                sim.fastat = 0u;
                sim.busy_ms = 0u;
                sim.hang = false;       /* a reset clears an injected hang */
                sim.cmd = 0;
            } else {
                sim.freset = false;
            }
        }
        return;
    }
    if (a >= 0x80800000u && a < 0x80A00000u) {
        pe_write(a, v, 2u);
        return;
    }
    if (df_addr(a)) {
        df_write(a, v, 2u);
        return;
    }
    {
        static const struct { uint32_t reg; uint8_t key; uint8_t shift; bool write; } en[4] = {
            { FL_EEPRE0, 0x2Du, 0u, false }, { FL_EEPRE1, 0xD2u, 8u, false },
            { FL_EEPWE0, 0x1Eu, 0u, true }, { FL_EEPWE1, 0xE1u, 8u, true },
        };
        for (unsigned i = 0u; i < 4u; i++) {
            if (a == en[i].reg) {
                if ((v >> 8) == en[i].key) {
                    uint16_t *r = en[i].write ? &sim.eepwe : &sim.eepre;
                    *r = (uint16_t)((*r & ~(0xFFu << en[i].shift)) | ((v & 0xFFu) << en[i].shift));
                }
                return;
            }
        }
    }
    violation("unexpected 16-bit write", a);
}

void fl_wr32(uint32_t a, uint32_t v)
{
    if (a >= FL_FCU_RAM && a < FL_FCU_RAM + FL_FCU_FW_LEN) {
        const uint32_t i = (a - FL_FCU_RAM) / 4u;
        if (!sim.fcurame || sim.fentryr != 0u) {
            violation("FCU RAM written while not enabled or in P/E mode", a);
            return;
        }
        if (v == fl_sim_fw_word(i) && i == sim.fw_words_ok) {
            sim.fw_words_ok++;
        }
        if (sim.fw_words_ok == FL_FCU_FW_LEN / 4u) {
            sim.fw_loaded = true;
        }
        return;
    }
    if (a == FL_RCCR) {
        return;
    }
    violation("unexpected 32-bit write", a);
}

/* ---- HAL ---- */
void fl_hal_init(void)
{
}

void fl_hal_set_supervisor(uint16_t base, uint16_t toggle)
{
    (void)base;
    (void)toggle;
}

void fl_hal_poll(void)
{
    sim.ms++;
    if (sim.ms - sim.last_poll_ms > sim.max_gap_ms) {
        sim.max_gap_ms = sim.ms - sim.last_poll_ms;
    }
    sim.last_poll_ms = sim.ms;
    if (sim.busy_ms != 0u && !sim.hang && --sim.busy_ms == 0u) {
        sim.fstatr0 |= FL_FSTATR0_FRDY;
    }
}

uint32_t fl_hal_ms(void)
{
    return sim.ms;
}

/* one frame per line on stdin; own line buffer (stdio buffering would hide lines from select) */
static char s_in[8192];
static size_t s_in_len;

static bool bridge_line(char *line, size_t max)
{
    char *nl = memchr(s_in, '\n', s_in_len);
    size_t n;

    if (nl == NULL) {
        return false;
    }
    n = (size_t)(nl - s_in) + 1u;
    if (n < max) {
        memcpy(line, s_in, n);
        line[n] = '\0';
    } else {
        line[0] = '\0';
    }
    memmove(s_in, s_in + n, s_in_len - n);
    s_in_len -= n;
    return true;
}

static bool bridge_recv(uint8_t d[8], uint8_t *dlc)
{
    char line[128];
    unsigned b[8];
    int n;

    if (!bridge_line(line, sizeof line)) {
        fd_set fds;
        struct timeval tv = { 0, 1000 };
        ssize_t got;

        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (select(1, &fds, NULL, NULL, &tv) <= 0) {
            return false;
        }
        got = read(0, s_in + s_in_len, sizeof s_in - s_in_len);
        if (got <= 0) {
            sim.stop = true;
            return false;
        }
        s_in_len += (size_t)got;
        if (!bridge_line(line, sizeof line)) {
            return false;
        }
    }
    n = sscanf(line, "%x %x %x %x %x %x %x %x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5],
               &b[6], &b[7]);
    if (n <= 0) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        d[i] = (uint8_t)(i < n ? b[i] : 0xAAu);
    }
    *dlc = (uint8_t)n;
    return true;
}

bool fl_hal_can_recv(uint8_t d[8], uint8_t *dlc)
{
    if (sim.bridge) {
        return bridge_recv(d, dlc);
    }
    if (sim.rx_head == sim.rx_tail) {
        return false;
    }
    memcpy(d, sim.rxq[sim.rx_head % SIM_QLEN], 8);
    *dlc = sim.rxq[sim.rx_head % SIM_QLEN][8];
    sim.rx_head++;
    return true;
}

bool fl_hal_can_send(const uint8_t d[8])
{
    if (sim.cut) {
        return true;
    }
    if (sim.bridge) {
        printf("%02X %02X %02X %02X %02X %02X %02X %02X\n", d[0], d[1], d[2], d[3], d[4], d[5],
               d[6], d[7]);
        fflush(stdout);
        return true;
    }
    if (sim.tx_n < SIM_QLEN) {
        memcpy(sim.txq[sim.tx_n++], d, 8);
    }
    return true;
}

bool fl_hal_can_idle(void)
{
    return true;
}

void fl_hal_rom_cache_flush(void)
{
}

void fl_hal_reset(void)
{
    if (sim.fentryr != 0u || (sim.fstatr0 & FL_FSTATR0_FRDY) == 0u) {
        violation("reset while the FCU is busy or in P/E mode", 0u);
    }
    sim.hal_resets++;
    sim.stop = true;
}

bool fl_sim_stop(void)
{
    return sim.stop;
}

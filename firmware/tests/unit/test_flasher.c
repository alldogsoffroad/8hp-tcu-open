/* Unit test: project flasher (spec 16) on the simulated flash controller, and the application's
 * entry routine FE30. */
#include "flasher/fl_core.h"
#include "flasher/fl_fcu.h"
#include "flasher/fl_regs.h"
#include "tests/unit/fl_sim.h"
#include "app/flash_entry.h"
#include "tests/unit/stub_plat.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint8_t rsp[300];
static unsigned rsp_len;
static unsigned pending_seen;

/* Send one request as ISO-TP frames, then decode the flasher's response from the TX queue. */
static unsigned request(const uint8_t *q, unsigned n)
{
    uint8_t f[8];
    unsigned i = 0u;

    sim.tx_n = 0u;
    pending_seen = 0u;
    if (n <= 7u) {
        memset(f, 0xAA, 8);
        f[0] = (uint8_t)n;
        memcpy(&f[1], q, n);
        fl_core_rx(f, 8u);
    } else {
        uint8_t sn = 1u;
        f[0] = (uint8_t)(0x10u | (n >> 8));
        f[1] = (uint8_t)n;
        memcpy(&f[2], q, 6);
        fl_core_rx(f, 8u);
        for (i = 6u; i < n; i += 7u) {
            const unsigned k = n - i > 7u ? 7u : n - i;
            memset(f, 0xAA, 8);
            f[0] = (uint8_t)(0x20u | (sn++ & 0x0Fu));
            memcpy(&f[1], &q[i], k);
            fl_core_rx(f, 8u);
        }
    }
    rsp_len = 0u;
    for (unsigned t = 0u; t < sim.tx_n; t++) {
        const uint8_t *d = sim.txq[t];
        const uint8_t pci = (uint8_t)(d[0] >> 4);
        if (pci == 3u) {
            continue;                   /* flow control to our request */
        }
        if (pci == 0u) {
            if (d[0] == 3u && d[1] == 0x7Fu && d[3] == 0x78u) {
                pending_seen++;
                continue;
            }
            rsp_len = d[0] & 0x0Fu;
            memcpy(rsp, &d[1], rsp_len);
        } else if (pci == 1u) {
            rsp_len = (unsigned)((d[0] & 0x0Fu) << 8 | d[1]);
            memcpy(rsp, &d[2], 6);
            i = 6u;
        } else if (pci == 2u) {
            memcpy(&rsp[i], &d[1], 7);
            i += 7u;
        }
    }
    return rsp_len;
}

static void push_fc(void)
{
    static const uint8_t fc[8] = { 0x30, 0, 0, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
    fl_sim_push(fc, 8u);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static unsigned routine(uint16_t rid, uint32_t a, uint32_t len)
{
    uint8_t q[12] = { 0x31, 0x01, (uint8_t)(rid >> 8), (uint8_t)rid };
    put32(&q[4], a);
    put32(&q[8], len);
    if (rid == 0xFF01u) {
        push_fc();
    }
    return request(q, 12u);
}

static unsigned download(uint32_t a, uint32_t len)
{
    uint8_t q[11] = { 0x34, 0x00, 0x44 };
    put32(&q[3], a);
    put32(&q[7], len);
    return request(q, 11u);
}

static unsigned transfer(uint8_t ctr, const uint8_t *page)
{
    uint8_t q[258] = { 0x36, ctr };
    memcpy(&q[2], page, 256);
    return request(q, 258u);
}

static bool is_neg(uint8_t sid, uint8_t nrc)
{
    return rsp_len == 3u && rsp[0] == 0x7Fu && rsp[1] == sid && rsp[2] == nrc;
}

static uint32_t crc32(const uint8_t *d, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

static void test_flasher(void)
{
    uint8_t page[256], page2[256], ff[256], boot_before[0x40000];

    fl_sim_init();
    fl_core_init(&fl_target_rom);
    for (uint32_t i = 0u; i < SIM_ROM_SIZE; i++) {
        sim.rom[i] = (uint8_t)(i * 13u + (i >> 9));
    }
    memcpy(boot_before, sim.rom, sizeof boot_before);
    for (unsigned i = 0u; i < 256u; i++) {
        page[i] = (uint8_t)(i ^ 0x3Cu);
        page2[i] = (uint8_t)(255u - i);
    }
    memset(ff, 0xFF, sizeof ff);

    /* identification, multi-frame response */
    push_fc();
    CHECK(request((const uint8_t *)"\x22\xF1\x95", 3u) == 12u);
    CHECK(rsp[0] == 0x62u && rsp[3] == 'F' && rsp[4] == 'L' && rsp[5] == FL_VERSION);
    CHECK(rsp[6] == 0x80u);             /* FPMON: FWE high */
    CHECK(request((const uint8_t *)"\x3E\x00", 2u) == 2u && rsp[0] == 0x7Eu);
    CHECK(request((const uint8_t *)"\x3E\x80", 2u) == 0u);
    CHECK(request((const uint8_t *)"\x10\x03", 2u) == 3u && is_neg(0x10u, 0x11u));

    /* CRC over boot and application areas (read only) */
    CHECK(routine(0xFF01u, 0x0u, 0x1000u) == 8u && rsp[0] == 0x71u);
    CHECK(((uint32_t)rsp[4] << 24 | (uint32_t)rsp[5] << 16 | (uint32_t)rsp[6] << 8 | rsp[7]) ==
          crc32(sim.rom, 0x1000u));
    routine(0xFF01u, 0x1FF000u, 0x2000u);
    CHECK(is_neg(0x31u, 0x31u));

    /* erase: everything outside the window or not whole blocks is refused, nothing erased */
    routine(0xFF00u, 0x30000u, 0x10000u);   CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, 0x0u, 0x2000u);        CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, 0x30000u, 0x20000u);   CHECK(is_neg(0x31u, 0x31u));   /* straddles 0x40000 */
    routine(0xFF00u, 0x41000u, 0x10000u);   CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, 0x40000u, 0x8000u);    CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, 0xA0000u, 0x10000u);   CHECK(is_neg(0x31u, 0x31u));   /* half a 128 KB block */
    routine(0xFF00u, 0x1E0000u, 0x40000u);  CHECK(is_neg(0x31u, 0x31u));   /* beyond 0x1FFFFF */
    routine(0xFF00u, 0x200000u, 0x20000u);  CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, 0x40000u, 0u);         CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, 0xFFFF0000u, 0x20000u); CHECK(is_neg(0x31u, 0x31u));  /* wrap-around */
    CHECK(sim.erases == 0u);

    /* erase two 64 KB blocks: response pending first, then positive, blank afterwards */
    CHECK(routine(0xFF00u, 0x40000u, 0x20000u) == 4u && rsp[0] == 0x71u);
    CHECK(pending_seen >= 1u && sim.erases == 2u);
    CHECK(sim.rom[0x40000] == 0xFFu && sim.rom[0x5FFFF] == 0xFFu && sim.rom[0x60000] != 0xFFu);

    /* download window checks */
    download(0x3FF00u, 0x200u);   CHECK(is_neg(0x34u, 0x31u));
    download(0x40080u, 0x100u);   CHECK(is_neg(0x34u, 0x31u));
    download(0x40000u, 0x180u);   CHECK(is_neg(0x34u, 0x31u));
    download(0x1FFF00u, 0x200u);  CHECK(is_neg(0x34u, 0x31u));
    transfer(1u, page);           CHECK(is_neg(0x36u, 0x31u) || is_neg(0x36u, 0x24u));

    /* two pages, a repeated counter, a wrong counter, one page too many */
    CHECK(download(0x40000u, 0x200u) == 4u && rsp[0] == 0x74u && rsp[1] == 0x20u);
    CHECK(transfer(1u, page) == 2u && rsp[0] == 0x76u && rsp[1] == 1u);
    CHECK(memcmp(&sim.rom[0x40000], page, 256) == 0 && sim.programs == 1u);
    CHECK(transfer(1u, page) == 2u && rsp[0] == 0x76u && sim.programs == 1u);
    transfer(3u, page2);          CHECK(is_neg(0x36u, 0x73u));
    CHECK(transfer(2u, page2) == 2u && rsp[1] == 2u);
    CHECK(memcmp(&sim.rom[0x40100], page2, 256) == 0);
    transfer(3u, page2);          CHECK(is_neg(0x36u, 0x13u));
    CHECK(request((const uint8_t *)"\x37", 1u) == 1u && rsp[0] == 0x77u);
    request((const uint8_t *)"\x37", 1u); CHECK(is_neg(0x37u, 0x24u));

    /* programming onto data is refused before the FCU sees it */
    CHECK(download(0x40000u, 0x100u) == 4u);
    transfer(1u, page2);          CHECK(is_neg(0x36u, 0x70u));
    CHECK(sim.programs == 2u);

    /* an all-0xFF page is only blank-checked, never programmed */
    CHECK(download(0x40200u, 0x100u) == 4u);
    CHECK(transfer(1u, ff) == 2u && rsp[0] == 0x76u && sim.programs == 2u);

    /* PRGERR: negative response, FCU recovered, next page programs normally */
    sim.fail_prog_addr = 0x40300u;
    CHECK(download(0x40300u, 0x200u) == 4u);
    transfer(1u, page);           CHECK(is_neg(0x36u, 0x72u));
    push_fc();
    request((const uint8_t *)"\x22\xF1\x95", 3u);
    CHECK(rsp[11] == FL_E_PRG && rsp[10] == 0x02u);   /* last error, FPESTAT */
    sim.fail_prog_addr = 0u;
    CHECK(download(0x40400u, 0x100u) == 4u);
    CHECK(transfer(1u, page) == 2u && rsp[0] == 0x76u);

    /* erase time-out: FCU reset, negative response, next erase works */
    sim.hang = true;
    routine(0xFF00u, 0x60000u, 0x10000u);
    CHECK(is_neg(0x31u, 0x72u) && sim.resets_fcu == 1u);
    CHECK(routine(0xFF00u, 0x60000u, 0x10000u) == 4u && rsp[0] == 0x71u);

    /* ERSERR */
    sim.fail_erase_addr = 0x70000u;
    routine(0xFF00u, 0x70000u, 0x10000u);
    CHECK(is_neg(0x31u, 0x72u));
    sim.fail_erase_addr = 0u;

    /* the second 1 MB area (FENTRY1) */
    CHECK(routine(0xFF00u, 0x1E0000u, 0x20000u) == 4u && rsp[0] == 0x71u);
    CHECK(download(0x1FFF00u, 0x100u) == 4u && transfer(1u, page) == 2u && rsp[0] == 0x76u);
    CHECK(memcmp(&sim.rom[0x1FFF00], page, 256) == 0);

    /* FWE low: P/E mode cannot be entered */
    sim.fwe = false;
    routine(0xFF00u, 0x80000u, 0x10000u);
    CHECK(is_neg(0x31u, 0x72u));
    sim.fwe = true;

    /* boot area never touched; no rule broken; watchdog serviced throughout */
    CHECK(memcmp(sim.rom, boot_before, sizeof boot_before) == 0);
    CHECK(sim.violations == 0u);
    CHECK(sim.max_gap_ms <= 2u);
    CHECK(sim.polls_busy > 0u);

    /* reset */
    CHECK(request((const uint8_t *)"\x11\x01", 2u) == 2u && rsp[0] == 0x51u && sim.hal_resets == 1u);
}


static unsigned transfer_df(uint8_t ctr, const uint8_t *page)
{
    uint8_t q[130] = { 0x36, ctr };
    memcpy(&q[2], page, 128);
    return request(q, 130u);
}

/* Data-flash target (spec 18): window DB00-DB07, 128-byte pages, undefined erased content,
 * independent of FWE. */
static void test_dataflash(void)
{
    uint8_t page[128], ff[128], stock[0x10000];
    const uint32_t db00 = FL_DF_BASE, db02 = FL_DF_BASE + 0x4000u, db08 = FL_DF_BASE + 0x10000u;

    fl_sim_init();
    sim.fwe = false;                        /* the reason for spec 18: FWE low under applications */
    fl_core_init(&fl_target_df);
    fl_fcu_df_enable(0xFFFFu, 0x00FFu);     /* read all blocks, write DB00-DB07 only */
    memcpy(stock, &sim.df[0x10000], sizeof stock);
    for (unsigned i = 0u; i < 128u; i++) {
        page[i] = (uint8_t)(i * 3u + 1u);
    }
    memset(ff, 0xFF, sizeof ff);

    push_fc();
    CHECK(request((const uint8_t *)"\x22\xF1\x95", 3u) == 12u);
    CHECK(rsp[3] == 'L' && rsp[4] == 'D' && rsp[6] == 0x00u);   /* FPMON: FWE low */

    /* window: DB00-DB07 only, whole 8 KB blocks */
    routine(0xFF00u, db08, 0x2000u);               CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, db00 - 0x2000u, 0x2000u);     CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, db00 + 0x1000u, 0x2000u);     CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, db00, 0x1000u);               CHECK(is_neg(0x31u, 0x31u));
    routine(0xFF00u, FL_DF_BASE + 0xE000u, 0x4000u); CHECK(is_neg(0x31u, 0x31u)); /* into DB08 */
    routine(0xFF00u, 0x00040000u, 0x10000u);       CHECK(is_neg(0x31u, 0x31u)); /* code flash */
    CHECK(sim.df_erases == 0u && sim.erases == 0u);

    /* erase DB00-DB01 with FWE low: allowed, content afterwards undefined (not 0xFF) */
    CHECK(routine(0xFF00u, db00, 0x4000u) == 4u && rsp[0] == 0x71u);
    CHECK(sim.df_erases == 2u);

    /* download: max block length 2 + 128 */
    download(db08, 0x80u);                          CHECK(is_neg(0x34u, 0x31u));
    download(db00 + 0x40u, 0x80u);                  CHECK(is_neg(0x34u, 0x31u));
    CHECK(download(db00, 0x180u) == 4u && rsp[1] == 0x20u && rsp[2] == 0x00u && rsp[3] == 0x82u);
    CHECK(transfer_df(1u, page) == 2u && rsp[0] == 0x76u);
    CHECK(memcmp(&sim.df[0], page, 128) == 0);
    CHECK(transfer_df(2u, ff) == 2u && rsp[0] == 0x76u);   /* all-0xFF page IS programmed */
    CHECK(memcmp(&sim.df[128], ff, 128) == 0 && sim.df_programs == 2u);
    CHECK(transfer_df(3u, page) == 2u);
    CHECK(request((const uint8_t *)"\x37", 1u) == 1u && rsp[0] == 0x77u);

    /* CRC over what was written; reading the stock blocks is allowed */
    CHECK(routine(0xFF01u, db00, 0x80u) == 8u &&
          ((uint32_t)rsp[4] << 24 | (uint32_t)rsp[5] << 16 | (uint32_t)rsp[6] << 8 | rsp[7]) ==
              crc32(page, 128));
    CHECK(routine(0xFF01u, db08, 0x100u) == 8u && rsp[0] == 0x71u);

    /* programming a block that was not erased: the FCU refuses, negative response */
    CHECK(download(db02, 0x80u) == 4u);
    transfer_df(1u, page);                          CHECK(is_neg(0x36u, 0x72u));
    sim.violations--;                               /* deliberate: counted by the model */

    /* P/E disabled for a block: erase refused by the FCU */
    fl_fcu_df_enable(0xFFFFu, 0x0000u);
    routine(0xFF00u, db02, 0x2000u);                CHECK(is_neg(0x31u, 0x72u));
    fl_fcu_df_enable(0xFFFFu, 0x00FFu);
    CHECK(routine(0xFF00u, db02, 0x2000u) == 4u && rsp[0] == 0x71u);

    /* stock EEPROM blocks DB08-DB15 never touched; code flash never touched */
    CHECK(memcmp(&sim.df[0x10000], stock, sizeof stock) == 0);
    CHECK(sim.erases == 0u && sim.programs == 0u);
    CHECK(sim.violations == 0u);
}

static void test_entry(void)
{
    uint8_t out[8];

    stub.fwe = true;
    CHECK(flash_entry_routine(1u, FLASH_RID, NULL, 0u, out, 8u, false) == -0x22);
    stub.fwe = false;
    CHECK(flash_entry_did(out, 8u) == 1 && out[0] == 0u);
    stub.fwe = true;
    CHECK(flash_entry_did(out, 8u) == 1 && out[0] == 1u);
    CHECK(flash_entry_routine(2u, FLASH_RID, NULL, 0u, out, 8u, true) == -0x12);
    CHECK(flash_entry_routine(1u, FLASH_RID, out, 1u, out, 8u, true) == -0x13);
    CHECK(flash_entry_routine(1u, 0xFE3Fu, NULL, 0u, out, 8u, true) == -0x31);
    for (unsigned i = 0u; i < 50u; i++) {
        flash_entry_tick_1ms();
    }
    CHECK(stub.flasher_starts == 0u);
    CHECK(flash_entry_routine(1u, FLASH_RID, NULL, 0u, out, 8u, true) == 0);
    for (unsigned i = 0u; i < FLASH_START_DELAY_MS - 1u; i++) {
        flash_entry_tick_1ms();
    }
    CHECK(stub.flasher_starts == 0u);
    flash_entry_tick_1ms();
    CHECK(stub.flasher_starts == 1u);
    for (unsigned i = 0u; i < 50u; i++) {
        flash_entry_tick_1ms();
    }
    CHECK(stub.flasher_starts == 1u);

    /* FWE follows the enable lines: FE30 raises them first; FE31 reports before / after */
    stub.fwe = false;
    stub.flash_enable = false;
    stub.fwe_follows_enable = true;
    CHECK(flash_entry_routine(1u, FLASH_EN_RID, NULL, 0u, out, 8u, false) == -0x22);
    CHECK(!stub.flash_enable);
    CHECK(flash_entry_routine(1u, FLASH_EN_RID, NULL, 0u, out, 8u, true) == 3);
    CHECK(out[0] == 0x00u && out[1] == 0x80u && stub.flash_enable);
    CHECK(flash_entry_routine(2u, FLASH_EN_RID, NULL, 0u, out, 8u, true) == 2 && !stub.flash_enable);
    stub.fwe = false;
    CHECK(flash_entry_routine(1u, FLASH_RID, NULL, 0u, out, 8u, true) == 0 && stub.flash_enable);
    for (unsigned i = 0u; i < FLASH_START_DELAY_MS; i++) {
        flash_entry_tick_1ms();
    }
    CHECK(stub.flasher_starts == 2u);
    /* FWE does not follow: no flasher start, lines dropped */
    stub.fwe = false;
    stub.fwe_follows_enable = false;
    CHECK(flash_entry_routine(1u, FLASH_RID, NULL, 0u, out, 8u, true) == 0 && stub.flash_enable);
    for (unsigned i = 0u; i < FLASH_START_DELAY_MS; i++) {
        flash_entry_tick_1ms();
    }
    CHECK(stub.flasher_starts == 2u && !stub.flash_enable);

    /* FE32 supervisor pattern */
    {
        const uint8_t w[3] = { 0x84u, 0x01u, 0x00u };
        CHECK(flash_entry_routine(1u, SUP_PAT_RID, w, 3u, out, 8u, false) == -0x22);
        CHECK(flash_entry_routine(1u, SUP_PAT_RID, w, 2u, out, 8u, true) == -0x13);
        CHECK(flash_entry_routine(1u, SUP_PAT_RID, w, 3u, out, 8u, true) == 0);
        CHECK(flash_entry_routine(3u, SUP_PAT_RID, NULL, 0u, out, 8u, true) == 5);
        CHECK(flash_entry_routine(2u, SUP_PAT_RID, NULL, 0u, out, 8u, true) == 0);
    }
}

int main(void)
{
    test_flasher();
    test_dataflash();
    test_entry();
    printf("%s (%d failures)\n", fails ? "FAILED" : "passed", fails);
    return fails ? 1 : 0;
}

/* Data-flash application loader (spec 18 section 5). Lives in code flash at 0x40000 with the
 * stock identification fields; written with the Teensy flasher (spec 17 path).
 *
 * Start-up:
 *   1. stay request from a running application (RAM word, see LD_STAY_WORD) -> loader mode;
 *   2. otherwise listen LD_LISTEN_MS on 0x7E1 for "31 01 FE 40" (reply "71 01 FE 40") -> loader
 *      mode;
 *   3. otherwise run the image in data flash if its header and CRC are valid: copy the payload to
 *      RAM and jump to its entry (interrupts stay masked);
 *   4. no valid image -> loader mode.
 * Loader mode is the spec 16 protocol on the data-flash target (DB00-DB07 only). The supervisor is
 * serviced with the normal word (bit 2 toggling, outputs off); the watchdog every millisecond. */
#include "platform/platform.h"
#include "flasher/fl_core.h"
#include "flasher/fl_fcu.h"
#include "flasher/fl_hal.h"
#include "flasher/fl_regs.h"
#include "loader/ld_image.h"

#define LD_STAY_WORD   (*(volatile uint32_t *)0xFFF800D0u)  /* boot-preserved RAM, unused by Bosch */
#define LD_STAY_MAGIC  0x4C4F4144u                          /* "LOAD" */
#define LD_LISTEN_MS   100u

void plat_jump_ram(const uint32_t *src, uint32_t dst, uint32_t len, uint32_t entry);

static bool listen_for_stay(void)
{
    static const uint8_t ok[8] = { 0x04u, 0x71u, 0x01u, 0xFEu, 0x40u, 0xAAu, 0xAAu, 0xAAu };
    const uint32_t t0 = fl_hal_ms();
    uint8_t d[8], dlc;

    while (fl_hal_ms() - t0 < LD_LISTEN_MS) {
        fl_hal_poll();
        if (fl_hal_can_recv(d, &dlc) && dlc >= 5u && d[0] == 0x04u && d[1] == 0x31u &&
            d[2] == 0x01u && d[3] == 0xFEu && d[4] == 0x40u) {
            (void)fl_hal_can_send(ok);
            return true;
        }
    }
    return false;
}

static bool image_valid(ld_image_t *img)
{
    uint8_t buf[LD_HDR_LEN];
    uint32_t c = 0xFFFFFFFFu;

    fl_fcu_read(FL_DF_BASE, buf, LD_HDR_LEN);
    if (!ld_image_parse(buf, img)) {
        return false;
    }
    for (uint32_t off = 0u; off < img->len; off += LD_HDR_LEN) {
        fl_fcu_read(FL_DF_BASE + LD_HDR_LEN + off, buf, LD_HDR_LEN);
        c = ld_crc32(c, buf, LD_HDR_LEN);
        fl_hal_poll();
    }
    return ~c == img->crc;
}

int main(void)
{
    const bool requested = LD_STAY_WORD == LD_STAY_MAGIC;
    ld_image_t img;

    LD_STAY_WORD = 0u;
    plat_init();                               /* entry_snapshot() runs first, before port setup */
    {
        uint8_t snap0 = 0u;                    /* FPMON latched at boot hand-over */
        (void)plat_entry_snapshot(&snap0, 1u);
        fl_ident_aux = snap0;                  /* report it in the flasher ident, byte 12 */
        fl_ident_aux_set = true;
    }
    fl_hal_set_supervisor(0x8E00u, 0x0004u);   /* normal word, bit 2 toggles, outputs off */
    fl_hal_init();
    fl_fcu_df_enable(0xFFFFu, 0x00FFu);        /* read all blocks, write DB00-DB07 only */
    if (!requested && !listen_for_stay() && image_valid(&img)) {
        plat_wdt_service();
        plat_jump_ram((const uint32_t *)(uintptr_t)(FL_DF_BASE + LD_HDR_LEN), img.load, img.len,
                      img.entry);
    }
    fl_core_init(&fl_target_df);
    fl_main_loop();
    return 0;
}

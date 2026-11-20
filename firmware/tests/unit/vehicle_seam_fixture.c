/* NEGATIVE FIXTURE of tests/unit/vehicle_seam_scan.py (never compiled): vehicle CAN IDs
 * named in code outside io/vehicle/ — every line below must be a hit (ctest
 * vehicle_seam_scan_negative, WILL_FAIL). */
static void fixture(can_frame_t *f)
{
    can_frame_t g = { .id = 0x2EAu, .dlc = 5u };    /* the lever frame */
    if (f->id == 0x108u) {                          /* engine speed */
        (void)plat_can_rx_register(CAN_BUS2, 0x210u);
    }
}
static const fixture_row_t s_rows[] = {
    { 0x14Bu, CAN_BUS1, 8u },
    { 0x2F9u, 1u, 8u },
};

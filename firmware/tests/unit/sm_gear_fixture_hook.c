/* negative fixture (2026-11-19): the gear gather reading a cell the shadow's
 * gear feedback chain models — the live engine's E6A1 / CCE4 (must FAIL) */
unsigned char fixture(void) { return *(const volatile unsigned char *)0xFFF8E6A1u
                                   + *(const volatile unsigned char *)0xFFF8CCE4u; }

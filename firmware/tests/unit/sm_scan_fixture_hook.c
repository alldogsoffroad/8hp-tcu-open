/* negative fixture: the hook writing the target cell (must FAIL) */
void fixture(void) { *(volatile unsigned char *)0xFFF8A697u = 1u; }

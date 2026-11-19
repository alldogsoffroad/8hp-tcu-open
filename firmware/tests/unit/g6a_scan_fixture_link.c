/* g6a_scan_fixture_link.c — NEGATIVE fixture for g6a_scan.py link: an object
 * that calls the stock NVM manager and writes the EEPROM dirty flag (must be
 * refused). Compiled only, never linked. */
#include <stdint.h>
extern void nvm_e8bfa(uint8_t g);
void fixture_link(void)
{
    nvm_e8bfa(1u);
    *(volatile uint8_t *)0xFFF85175u = 1u;
}

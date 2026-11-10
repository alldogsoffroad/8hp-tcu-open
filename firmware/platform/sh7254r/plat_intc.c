/* plat_intc.c — INTC setup for the top-down rebuild (design §5.2).
 *
 * Programs the interrupt controller per the validated spec 24 ISR inventory:
 * every IPR zeroed (hygiene), then the sources we enable, then IBNR=0
 * (register banks OFF — mandatory with GCC stack ISRs, the E60d lesson).
 * SR helpers for the permanent-open model (design A6).
 */
#include "platform.h"
#include "regs.h"

void plat_intc_setup(const plat_ipr_map_t *m)
{
    /* hygiene: zero IPR03..IPR29 (stock 0x963A4-class) */
    for (uint32_t n = 3u; n <= 29u; n++) {
        INTC_IPR(n) = 0u;
    }
    /* program the enabled sources */
    for (uint8_t i = 0u; i < m->n; i++) {
        const plat_ipr_ent_t *e = &m->ent[i];
        uint16_t v = INTC_IPR(e->ipr);
        v = (uint16_t)((v & ~((uint16_t)0xFu << e->shift)) |
                       ((uint16_t)e->prio << e->shift));
        INTC_IPR(e->ipr) = v;
    }
    /* register banks OFF — GCC rte never RESBANKs; the 16th interrupt
     * would overflow and exception (E60d bench law, regs.h note) */
    INTC_IBNR = 0x0000u;
}

void plat_sr_open(void)
{
    /* SR.I = 0 — TRULY open. The r01/r01b bench proved 0xE is NOT "open":
     * SH-2A delivers only priorities ABOVE SR.I, so 0xE masked the tick (6)
     * and RSPI (4) entirely while the polled CMT fallback hid it (uptime
     * cell = 0, both RSPI channels stuck FEEDING). The E60-era burst code
     * used exactly this value for its proven interrupt windows
     * (plat_sh7254r.c:711 'ldc sr & ~0xF0'). */
    uint32_t sr;
    __asm__ volatile ("stc sr, %0" : "=r"(sr));
    sr &= ~0xF0u;
    __asm__ volatile ("ldc %0, sr" : : "r"(sr));
}

void plat_sr_close(void)
{
    /* SR.I = 0xF — all maskable ISRs masked (critical sections / bring-up). */
    uint32_t sr;
    __asm__ volatile ("stc sr, %0" : "=r"(sr));
    sr |= 0xF0u;
    __asm__ volatile ("ldc %0, sr" : : "r"(sr));
}

uint32_t plat_sr_i(void)
{
    uint32_t sr;
    __asm__ volatile ("stc sr, %0" : "=r"(sr));
    return (sr >> 4) & 0xFu;
}

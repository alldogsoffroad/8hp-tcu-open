/* Flasher register access (spec 16). On the target these are plain volatile accesses; the host
 * build (FL_HOST) routes them to the simulated flash controller in tests/unit/fl_sim.c, so the
 * driver sequence in fl_fcu.c is exercised unchanged.
 * Addresses: SH7254R hardware manual R01UH0480EJ0400 section 25.3 and table 25.2. */
#ifndef TCU_FL_REGS_H
#define TCU_FL_REGS_H

#include <stdint.h>

#define FL_FPMON     0xFFFFA800u    /* 8:  bit 7 FWE pin level                        */
#define FL_FASTAT    0xFFFFA810u    /* 8:  bit 7 ROMAE, bit 4 CMDLK                    */
#define FL_FCURAME   0xFFFFA854u    /* 16: key 0xC4, bit 0 FCU RAM enable              */
#define FL_FSTATR0   0xFFFFA900u    /* 8:  FRDY ILGLERR ERSERR PRGERR ...              */
#define FL_FSTATR1   0xFFFFA901u    /* 8:  FCUERR, FRDTCT, FRCRCT                      */
#define FL_FENTRYR   0xFFFFA902u    /* 16: key 0xAA, bit 0 FENTRY0, bit 1 FENTRY1      */
#define FL_FRESETR   0xFFFFA906u    /* 16: key 0xCC, bit 0 FRESET                      */
#define FL_FPESTAT   0xFFFFA91Cu    /* 16: P/E error source                            */
#define FL_RCCR      0xFFFC1400u    /* 32: ROM cache control (section 27)              */
#define FL_EEPRE0    0xFFFFA840u    /* 16: key 0x2D, data-flash read enable DB00-DB07  */
#define FL_EEPRE1    0xFFFFA842u    /* 16: key 0xD2, read enable DB08-DB15             */
#define FL_EEPWE0    0xFFFFA850u    /* 16: key 0x1E, data-flash P/E enable DB00-DB07   */
#define FL_EEPWE1    0xFFFFA852u    /* 16: key 0xE1, P/E enable DB08-DB15              */

#define FL_FCU_FW      0x00402000u  /* FCU firmware area (ROM read address)           */
#define FL_FCU_RAM     0x80FF8000u  /* FCU RAM                                        */
#define FL_FCU_FW_LEN  0x2000u
#define FL_PE_BASE     0x80800000u  /* ROM program/erase address = FL_PE_BASE + ROM address */
#define FL_DF_BASE     0x80100000u  /* data flash (data MAT): read and P/E address, 128 KB    */
#define FL_DF_SIZE     0x20000u
#define FL_DF_BLOCK    0x2000u      /* 16 blocks DB00-DB15 of 8 KB (manual 26.1)           */

#define FL_FPMON_FWE      0x80u
#define FL_FSTATR0_FRDY   0x80u
#define FL_FSTATR0_ILGL   0x40u
#define FL_FSTATR0_ERS    0x20u
#define FL_FSTATR0_PRG    0x10u
#define FL_FSTATR1_ERRS   0x83u     /* FCUERR | FRDTCT | FRCRCT: reset and reload firmware */
#define FL_FASTAT_CMDLK   0x10u

#ifdef FL_HOST
uint8_t  fl_rd8(uint32_t a);
uint16_t fl_rd16(uint32_t a);
uint32_t fl_rd32(uint32_t a);
void     fl_wr8(uint32_t a, uint8_t v);
void     fl_wr16(uint32_t a, uint16_t v);
void     fl_wr32(uint32_t a, uint32_t v);
#define FL_NOP5() ((void)0)
#else
static inline uint8_t  fl_rd8(uint32_t a)  { return *(volatile uint8_t *)(uintptr_t)a; }
static inline uint16_t fl_rd16(uint32_t a) { return *(volatile uint16_t *)(uintptr_t)a; }
static inline uint32_t fl_rd32(uint32_t a) { return *(volatile uint32_t *)(uintptr_t)a; }
static inline void fl_wr8(uint32_t a, uint8_t v)   { *(volatile uint8_t *)(uintptr_t)a = v; }
static inline void fl_wr16(uint32_t a, uint16_t v) { *(volatile uint16_t *)(uintptr_t)a = v; }
static inline void fl_wr32(uint32_t a, uint32_t v) { *(volatile uint32_t *)(uintptr_t)a = v; }
#define FL_NOP5() __asm__ volatile ("nop\n nop\n nop\n nop\n nop")
#endif

#endif

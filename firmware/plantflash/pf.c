/* Plant-flash programming engine (spec 16 flasher ported to the plant session's F0F0 pipe).
 *
 * RAM image for the boot-MAT download window 0xFFF88400-0xFFF8FFFF, downloaded with
 * 34/36/37 and driven by RoutineControl F0F0 (routine_table_findings.md steps 5-6, 9):
 *
 *   0xFFF88400 +0x00  16 bytes zero pad
 *             +0x10  u32 -> pf_tbl (the engine table T)
 *   T +0x00 EXEC (cmd_ctx 0xFFF84A6C, res_ctx 0xFFF84960)   [F0F0, boot 0x109E]
 *     +0x04 READY (res_ctx) -> 1 = idle/operational         [FF01 needs 1, boot 0x1090]
 *     +0x08 PUMP (cmd_ctx, res_ctx), every main-loop cycle   [run hook, boot 0x105A]
 *     +0x0C STOP (res_ctx) -> 1 = stopped                   [F0F1, boot 0x104C]
 *     +0x10 -> u32 0x494E4143 ("CANI"), the hook-enable identity word (boot 0x10B0)
 *
 * EXEC/PUMP return: 1 = command complete (reply 71 01 F0F0 <cmd> <param> <result[n]>),
 * 0 = in progress (7F 31 78, re-send to poll), 2 = error (byte at res+0x16, mapped
 * 19 -> NRC 0x13, 36 -> 0x24, 49 -> 0x31, 51 -> 0x33, 114 -> 0x72).
 *
 * The FCU sequences are the spec 16 flasher's (firmware/flasher/fl_fcu.c): FCURAME 0xC401,
 * FCU firmware 0x402000 -> 0x80FF8000, FENTRYR 0xAA01/0xAA02/0xAA00, 256-byte program unit,
 * P/E address 0x80800000 + ROM address, FRDY polling with error recovery per manual 25.6.3.
 * Re-structured as a non-blocking step machine: EXEC and PUMP both advance it, so the boot
 * MAT's main loop keeps running (supervisor service, ISO-TP) during 1.75 s erases. The
 * internal WDT is serviced by this image in every path that can take longer than a few
 * hundred microseconds. The boot MAT services the external supervisor before each PUMP, so
 * it is never ours to service: the longest blocking section here is the 2 ms FCU reset hold.
 *
 * MAT discipline (dual-MAT SH7254R, ROMMAT 0xFFFFA820: 0x3B00 app MAT at 0, 0x3B01 boot MAT):
 * the plant session runs with the boot MAT mapped at 0. FENTRY0/1 and the 0x80800000 P/E
 * window address the user MAT (the programmed array) regardless of the mapping; this engine
 * never writes ROMMAT while FENTRYR != 0. READ/VERIFY/the program read-back need the app
 * MAT's bytes, so they map the app MAT at 0 for a bounded copy (FENTRYR == 0 only, the same
 * transient remap the session's own F0F2/FF01 handlers do) and restore the boot MAT before
 * returning; the ROM cache is flushed after every remap.
 *
 * ---- BENCH OPS (opcode table) -----------------------------------------------
 * Request on the wire: 31 01 F0 F0 <cmd> <param BE16> <data 0..0xF8> (data starts at
 * request byte 6). data[0..3] is a big-endian address where used.
 *
 *  cmd  name      param            data                    result
 *  0x00 PING      0x0000           none                    'P','F','E','1', ver, FPMON,
 *                                                             FSTATR0, FSTATR1, FASTAT,
 *                                                             FPESTAT lo, FENTRYR lo,
 *                                                             last-err (12 bytes;
 *                                                             FPMON bit 7 = FWE level)
 *  0x01 READ      length 1..0xF8   addr BE32               <length> bytes. Sources: user
 *                                                             MAT 0x000000-0x1FFFFF (app MAT
 *                                                             via transient remap) and data
 *                                                             flash 0x00600000-0x0067FFFF /
 *                                                             0x80100000-0x8011FFFF (direct).
 *                                                             Long reads = repeated calls
 *                                                             with a running address (no
 *                                                             offset register: host advances).
 *  0x02 ERASE     0x0000           block addr BE32         1 byte 0x00. Address must be an
 *                                                             exact spec-16 block start inside
 *                                                             0x040000-0x1FFFFF (64 KB grid
 *                                                             below 0x0A0000, 128 KB above).
 *                                                             One block per command; async
 *                                                             (7F 31 78 while 0.4-1.75 s).
 *  0x03 PROGRAM   page offset      page addr BE32 + chunk  staging chunk: result 1 byte =
 *                  (0x0000 / 0x00F4)  (4 + n bytes,          bytes staged so far. A 256-byte
 *                                    n <= 0xF4)             page is programmed when fully
 *                                                             staged; async (2-12 ms), then
 *                                                             read back and compared:
 *                                                             result 1 byte 0x00, mismatch ->
 *                                                             err 114.
 *  0x04 VERIFY    length 1..0xF4   addr BE32 + expected    3 bytes: status (0 = match,
 *                                        <length> bytes       1 = mismatch), BE16 first
 *                                                             differing offset (0 when match).
 *  0x05 RESET     0x0000           none                    4 bytes FPMON, FSTATR0,
 *                                                             FSTATR1, FASTAT. Leaves P/E mode
 *                                                             (FENTRYR 0xAA00), drops any
 *                                                             half-staged page.
 *  0x06 SUPX      flags            state byte (0xFF keep)  14 bytes FPMON, FSTATR0/1,
 *                      bit0 zero counter,                   FENTRYR, state, port2,
 *                      bit1 port bit5 set,                  counter BE32, S.ms BE32.
 *                      bit2 port bit4 clear                 bit3 no-settle; bit4/5 set/
 *                      bit3 no-settle                      clear fwe_skip; bit6 send
 *                      bit4/5 fwe_skip set/clear            BE16 word at data[1..2] to
 *                      bit6 send raw word (data+1)          SSLB1 after CPU-mode init;
 *                      bit7 boot handshake first           bit7 = boot handshake first.
 *
 * PROGRAM chunking: a page is 256 bytes, one F0F0 carries at most 0xF4 payload bytes, so a
 * full page takes two calls: offset 0x0000 with the first 0xF4 bytes, then offset 0x00F4
 * with the remaining 0x0C. Chunks must be contiguous (offset == bytes staged so far) and
 * address the same 256-aligned page; a shorter tail is padded with 0xFF to 256 bytes. The
 * call that completes the page is the one that goes async.
 *
 * ---- SMOKE / EXAMPLE REQUESTS (after 10 60, 27 61/62, 34+36 download, 37) ----
 *   ping            31 01 F0 F0 00 00 00
 *                   -> 71 01 F0 F0 00 00 00 50 46 45 31 01 <FPMON> ...
 *   read 248 B      31 01 F0 F0 01 00 F8 00 04 00 00
 *                   -> 71 01 F0 F0 01 00 F8 <248 bytes>
 *   erase EB20      31 01 F0 F0 02 00 00 00 10 00 00     (block 0x100000)
 *                   -> 7F 31 78 (re-send until) 71 01 F0 F0 02 00 00 00
 *   program page    31 01 F0 F0 03 00 00 00 10 00 00 <244 bytes>
 *                   31 01 F0 F0 03 00 F4 00 10 00 00 <12 bytes>
 *                   -> (poll) 71 01 F0 F0 03 00 F4 00
 *   verify 16 B     31 01 F0 F0 04 00 10 00 10 00 00 <16 bytes>
 *                   -> 71 01 F0 F0 04 00 10 00 00 00 00
 *   FCU reset       31 01 F0 F0 05 00 00 -> 71 01 F0 F0 05 00 00 <FPMON> <FSTATR0> ...
 *   engine stop     31 01 F0 F1                        (clears the engine-active flag)
 *
 * DO NOT send 31 01 FF 01 while this image is loaded: the FF01 verifier hashes and RSA-parses
 * the image, and a failed verify erases both download windows (0xFFF88400+0x7C00,
 * 0xFFF80100+0x1F00) - it would wipe this engine. The run hook is already live without it
 * (engine-active defaults to 1; proven by the round-42 canary).
 *
 * Safety: erase/program are refused (err 51) outside 0x040000-0x1FFFFF; erase additionally
 * only on exact block boundaries (err 49 otherwise); every completed command leaves the FCU
 * in ROM read mode. Power-loss sequencing stays with the host (spec 16 section 5).
 */

#include <stdint.h>

#define REG8(a)   (*(volatile uint8_t  *)(a))
#define REG16(a)  (*(volatile uint16_t *)(a))
#define REG32(a)  (*(volatile uint32_t *)(a))

/* ---- hardware (same registers as plantdump / fl_sh7254r.c) ---- */
#ifdef PF_HOST
/* host build (tests): the register accesses route to the simulator's bus, and the three
 * MMIO symbols below are provided by the harness */
extern volatile uint16_t pfh_wdt_wtcnt;
extern volatile uint8_t  pfh_cmt_cmsr0;
extern volatile uint16_t pfh_rommat;
#define WDT_WTCNT    pfh_wdt_wtcnt
#define CMT_CMSR0    pfh_cmt_cmsr0
#define ROMMAT       pfh_rommat
#else
#define WDT_WTCNT    REG16(0xFFFE0002u)   /* write 0x5Axx */
#define FRCTR        0xFFFFF220u          /* TCNTA: 32-bit LONGWORD-ACCESS-ONLY free-running
                                           * counter (manual 13.10.7); the boot MAT's own time
                                           * source (supervisor 5 ms = 5000 ticks, low 16 bits). */
#define ROMMAT       REG16(0xFFFFA820u)   /* 0x3B00 app MAT at 0, 0x3B01 boot MAT at 0 */
#endif
#define ROMMAT_APP   0x3B00u

#define FL_FPMON     0xFFFFA800u    /* 8:  bit 7 FWE pin level */
#define FL_FASTAT    0xFFFFA810u    /* 8:  bit 4 CMDLK */
#define FL_FCURAME   0xFFFFA854u    /* 16: key 0xC4, bit 0 FCU RAM enable */
#define FL_FSTATR0   0xFFFFA900u    /* 8:  bit 7 FRDY, 6 ILGLERR, 5 ERSERR, 4 PRGERR */
#define FL_FSTATR1   0xFFFFA901u    /* 8:  bit 7 FCUERR, 1 FRDTCT, 0 FRCRCT */
#define FL_FENTRYR   0xFFFFA902u    /* 16: key 0xAA, bit 0 FENTRY0, bit 1 FENTRY1 */
#define FL_FRESETR   0xFFFFA906u    /* 16: key 0xCC, bit 0 FRESET */
#define FL_FPESTAT   0xFFFFA91Cu    /* 16: P/E error source */
#define FL_RCCR      0xFFFC1400u    /* 32: ROM cache control */

#define FL_FCU_FW      0x00402000u  /* FCU firmware area (MAT-independent) */
#define FL_FCU_RAM     0x80FF8000u
#define FL_FCU_FW_LEN  0x2000u
#define FL_PE_BASE     0x80800000u  /* P/E address = FL_PE_BASE + user-MAT address */

#define FL_FPMON_FWE    0x80u
#define FL_FSTATR0_FRDY 0x80u
/* FSTATR1 bits 0/1 (FRDTCT/FRCRCT, "FCU firmware transfer" state) read 0x03 from boot
 * in the plant session, before any P/E attempt (bench 2026-10-26) - informational, not
 * errors; the manual's remedy ("reset and reload firmware") is applied in fcu_enter_pe.
 * Only FCUERR (bit 7) is a hard FSTATR1 failure. */
#define FL_FSTATR1_ERRS 0x80u
#define FL_FASTAT_CMDLK 0x10u

/* spec 16 timing budgets (max > 100 cycles, x 1.1) */
#define PF_PROG_TIMEOUT_MS  25u
#define PF_ERASE_TIMEOUT_MS 2400u

/* ---- engine ABI context addresses (routine_table_findings.md step 5) ---- */
#define PF_CMD_CTX   0xFFF84A6Cu
#define PF_RES_CTX   0xFFF84960u

/* ---- opcodes ---- */
#define PF_OP_NOP     0x00u
#define PF_OP_READ    0x01u
#define PF_OP_ERASE   0x02u
#define PF_OP_PROGRAM 0x03u
#define PF_OP_VERIFY  0x04u
#define PF_OP_RESET   0x05u
#define PF_OP_SUPX    0x06u
#define PF_OP_DFERASE 0x07u
#define PF_OP_DFPROG  0x08u
#define PF_OP_RAMW    0x09u
#define PF_OP_LOG     0x0Au
#define PF_OP_BOOTMAT 0x0Cu

/* ---- data MAT (EEPROM) programming — FWE-FREE per manual section 26 ----
 * FENTRYD (FENTRYR=0xAA80) needs only ROM-enabled mode + FRDY + AA key.
 * Block enables: word access, KEY byte then DBWE flags (manual 26.3.6/26.3.7).
 * Target discipline: DB00-DB07 only (the loader staging area, spec 18). */
#define FL_EEPRE0    0xFFFFA840u    /* 16: DB00-07 read enable,  KEY 0x2D */
#define FL_EEPRE1    0xFFFFA842u    /* 16: DB08-15 read enable,  KEY 0xD2 */
#define FL_EEPWE0    0xFFFFA850u    /* 16: DB00-07 P/E enable,   KEY 0x1E */
#define FL_EEPWE1    0xFFFFA852u    /* 16: DB08-15 P/E enable,   KEY 0xE1 */
#define PF_DFW_LO    0x80100000u    /* DB00 (direct view for P/E) */
#define PF_DFW_HI    0x80110000u    /* end of DB07 (8 x 8KB) */
#define PF_DF_PAGE   128u

/* engine error codes -> F0F0 NRC: 19->0x13, 36->0x24, 49->0x31, 51->0x33, 114->0x72 */
#define PF_E_LEN      19u    /* data length / param inconsistent */
#define PF_E_SEQ      36u    /* protocol sequencing */
#define PF_E_RANGE    49u    /* address out of range / misaligned / unknown opcode */
#define PF_E_DENIED   51u    /* outside the writable window (protection refusal) */
#define PF_E_FCU      114u   /* FCU failure (FWE, timeout, ILGL/ERS/PRG/FCU), read-back bad */

/* fl_fcu.c error vocabulary, reported as the PING last-err byte */
#define PF_LAST_OK       0u
#define PF_LAST_FWE      1u
#define PF_LAST_TIMEOUT  2u
#define PF_LAST_ILGL     3u
#define PF_LAST_ERS      4u
#define PF_LAST_PRG      5u
#define PF_LAST_FCU      6u
#define PF_LAST_READBACK 7u
#define PF_LAST_FENTRYR  8u    /* FENTRYR write did not stick (P/E entry refused) */
#define PF_LAST_STATUS   9u    /* status unclean even after FCU reset + FW reload */

/* writable window (spec 16 section 1) and readable ranges */
#define PF_WIN_LO    0x00040000u
#define PF_WIN_HI    0x00200000u
#define PF_DF_LO     0x00600000u     /* data flash, mirrored view */
#define PF_DF_HI     0x00680000u
#define PF_DF2_LO    0x80100000u     /* data flash, direct view (boot MAT's EEPROM path) */
#define PF_DF2_HI    0x80120000u

#define PF_PAGE      256u
#define PF_DATA_MAX  0xF8u           /* F0F0 data field */
#define PF_CHUNK_MAX 0xF4u           /* PF_DATA_MAX - 4 address bytes */

/* ---- FWE/supervisor experiment (boot MAT supervisor service 0x244C) ----
 * Plant session runs supervisor state 0x19 (word 0x8E80, bit 7). The service
 * fires every 5 ms (FRC delta >= 5000); after 120000 fires (10 min) it clears
 * the state byte, clamps the counter, and flips port 0xFFFE3902 (bit4 set,
 * bit5 clear) - the session's FWE window closing. SUPX re-arms it. */
#define SUP_STATE    REG8 (0xFFF8526Fu)
#define SUP_GATE     REG8 (0xFFF8526Eu)
#define SUP_COUNT    REG32(0xFFF85264u)
#define SUP_PORT2    REG8 (0xFFFE3902u)

/* step-machine states */
#define PF_ST_IDLE    0u
#define PF_ST_ERASE   1u             /* erase issued, FRDY pending */
#define PF_ST_PROG    2u             /* program issued, FRDY pending */
#define PF_ST_RESULT  3u             /* finished, result in res ctx, awaiting delivery */
#define PF_ST_SUPWAIT 4u             /* supervisor experiment settle delay */

#define PF_ASYNC     0xFFu           /* op_* return: async operation started */
#define PF_VERSION   1u

/* All engine state lives in this downloaded block: the section is initialised data, so a
 * 34/36 download writes a known-good state (ST_IDLE) into RAM. No .bss anywhere. */
struct pf_state {
    uint32_t magic;         /* PF_STATE_MAGIC: endianness/state sanity for the host */
    uint32_t ms;            /* free-running millisecond counter */
    uint32_t t0;            /* wait start */
    uint32_t page;          /* staging page address (user MAT) */
    uint32_t op_addr;       /* user-MAT address of the in-flight FCU command */
    uint16_t staged;        /* bytes staged in the current page */
    uint16_t last_frc;      /* FRC snapshot at the previous service call */
    uint32_t us_acc;        /* sub-millisecond remainder of the FRC deltas */
    uint8_t  st;
    uint8_t  last_err;      /* PF_LAST_* detail for PING */
    uint8_t  fw_loaded;
    uint8_t  err;           /* pending engine error (delivery returns 2) */
    uint8_t  fwe_skip;      /* bench experiment: enter P/E without the FPMON.FWE check */
    uint16_t last_reply;   /* last RSPI frame's receive word (SPRF-correct read) */
    uint8_t  buf[PF_PAGE];  /* staging page */
};

#define PF_STATE_MAGIC 0x50464531u   /* "PFE1" */

__attribute__((used))
__attribute__((section(".img_state")))
static struct pf_state S = {
    PF_STATE_MAGIC, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    PF_ST_IDLE, PF_LAST_OK, 0u, 0u, 0u, 0u, { 0u }
};

int pf_exec(const uint8_t *cmd, uint8_t *res);
int pf_ready(uint8_t *res);
int pf_pump(const uint8_t *cmd, uint8_t *res);
int pf_stop(uint8_t *res);
extern const uint32_t pf_tbl[5];

__attribute__((used))
__attribute__((section(".img_pad")))
static const uint32_t pf_pad[4] = { 0u, 0u, 0u, 0u };

__attribute__((used))
__attribute__((section(".img_slot")))
static const uint32_t pf_slot[1] = { (uint32_t)&pf_tbl[0] };

/* T + 0x10: identity word "CANI" (hook-enable bypass, boot thunk 0x10B0) */
__attribute__((used))
__attribute__((section(".img_tbl")))
static const uint32_t pf_ident[1] = { 0x494E4143u };

__attribute__((used))
__attribute__((section(".img_tbl")))
const uint32_t pf_tbl[5] = {
    (uint32_t)&pf_exec, (uint32_t)&pf_ready, (uint32_t)&pf_pump,
    (uint32_t)&pf_stop, (uint32_t)&pf_ident[0]
};

/* ---- tick / watchdog ---- */

static inline uint32_t rd32f(uint32_t a) { return *(volatile uint32_t *)(uintptr_t)a; }

#ifdef PF_HOST
/* the simulator has no CMT timer: it decides on every service call whether the 1 ms
 * compare flag is set (so time also passes inside this image's two blocking loops) */
void pf_h_ms_advance(void);
#endif

static void pf_service(void)
{
#ifdef PF_HOST
    pf_h_ms_advance();
#else
    {
        /* TCNTA is LONGWORD-ACCESS-ONLY (manual 13.10.7); the boot MAT reads it
         * with mov.l. 16-bit reads returned garbage -> S.ms appeared frozen. */
        const uint16_t now = (uint16_t)rd32f(FRCTR);

        S.us_acc += (uint32_t)(uint16_t)(now - S.last_frc);
        S.last_frc = now;
        while (S.us_acc >= 1000u) {
            S.us_acc -= 1000u;
            S.ms++;
        }
    }
#endif
    WDT_WTCNT = 0x5ADBu;
}

static inline uint32_t sr_read(void)
{
#ifndef PF_HOST
    uint32_t v;

    __asm__ volatile ("stc sr, %0" : "=r"(v));
    return v;
#else
    return 0u;
#endif
}

static inline void sr_write(uint32_t v)
{
#ifndef PF_HOST
    __asm__ volatile ("ldc %0, sr\n nop\n nop" : : "r"(v));
#else
    (void)v;
#endif
}

/* ---- register helpers (fl_regs.h, target build) ---- */

#ifdef PF_HOST
/* host build: every register access routes into the simulator (fl_sim.c pattern) */
uint8_t  pf_h_rd8(uint32_t a);
uint16_t pf_h_rd16(uint32_t a);
uint32_t pf_h_rd32(uint32_t a);
void pf_h_wr8(uint32_t a, uint8_t v);
void pf_h_wr16(uint32_t a, uint16_t v);
void pf_h_wr32(uint32_t a, uint32_t v);
static inline uint8_t  rd8(uint32_t a)  { return pf_h_rd8(a); }
static inline uint16_t rd16(uint32_t a) { return pf_h_rd16(a); }
static inline uint32_t rd32(uint32_t a) { return pf_h_rd32(a); }
static inline void wr8(uint32_t a, uint8_t v)    { pf_h_wr8(a, v); }
static inline void wr16(uint32_t a, uint16_t v)  { pf_h_wr16(a, v); }
static inline void wr32(uint32_t a, uint32_t v)  { pf_h_wr32(a, v); }
#define PF_NOP5() ((void)0)
#else
static inline uint8_t  rd8(uint32_t a)  { return *(volatile uint8_t *)(uintptr_t)a; }
static inline uint16_t rd16(uint32_t a) { return *(volatile uint16_t *)(uintptr_t)a; }
static inline uint32_t rd32(uint32_t a) { return *(volatile uint32_t *)(uintptr_t)a; }
static inline void wr8(uint32_t a, uint8_t v)   { *(volatile uint8_t *)(uintptr_t)a = v; }
static inline void wr16(uint32_t a, uint16_t v) { *(volatile uint16_t *)(uintptr_t)a = v; }
static inline void wr32(uint32_t a, uint32_t v) { *(volatile uint32_t *)(uintptr_t)a = v; }
#define PF_NOP5() __asm__ volatile ("nop\n nop\n nop\n nop\n nop")
#endif

static void cache_flush(void)
{
    /* UNTD map helper (ROM 0x37DC) sequence: RCCR bit 3 only, read-modify-write.
     * A full write corrupts the other RCCR bits and crashes the session (bench
     * 2026-10-26). */
    wr32(FL_RCCR, rd32(FL_RCCR) | 8u);
}

/* ---- MAT remap (only ever with FENTRYR == 0) ---- */

/* The UNTD's map helper, RAM-resident at 0xFFF84104 for the whole session life:
 * r4=1 -> app MAT at 0, r4=0 -> boot MAT back (ROM 0x37DC sequence incl. the
 * register read-back). Hand-rolling the register writes hung the session twice
 * (bench 2026-10-26) — call the vendor's own helper like F0F2/FF01 do. */
#define PF_MAP_HELPER ((void (*)(uint32_t))0xFFF84104u)

static uint16_t pf_map_app(void)
{
    const uint16_t saved = ROMMAT;

    if (saved != ROMMAT_APP) {
        PF_MAP_HELPER(1u);
    }
    return saved;
}

static void pf_unmap_app(uint16_t saved)
{
    if (saved != ROMMAT_APP) {
        PF_MAP_HELPER(0u);
    }
}

/* ---- FCU driver (fl_fcu.c sequences as a step machine) ---- */

static void fcu_read_mode(void)
{
    wr16(FL_FENTRYR, 0xAA00u);
    (void)rd16(FL_FENTRYR);
    PF_NOP5();
    cache_flush();
}

static uint8_t fcu_status(void)
{
    const uint8_t s0 = rd8(FL_FSTATR0);
    const uint8_t s1 = rd8(FL_FSTATR1);

    if ((s1 & FL_FSTATR1_ERRS) != 0u) {
        return PF_LAST_FCU;
    }
    if ((s0 & 0x40u) != 0u) {
        return PF_LAST_ILGL;
    }
    if ((s0 & 0x20u) != 0u) {
        return PF_LAST_ERS;
    }
    if ((s0 & 0x10u) != 0u) {
        return PF_LAST_PRG;
    }
    return PF_LAST_OK;
}

/* figure 25.16 soft recovery: clear the command-locked state, then the status registers.
 * Called with the FCU in P/E mode; leaves it in read mode. */
static void fcu_recover_soft(uint32_t pe)
{
    if (rd8(FL_FASTAT) != FL_FASTAT_CMDLK) {
        wr8(FL_FASTAT, FL_FASTAT_CMDLK);
    }
    wr8(pe, 0x50u);
    fcu_read_mode();
}

/* FRESETR hold: cycle-counted, NOT timer-based - no known timer runs in the plant
 * session (CMT0 and FRC both dead, bench 2026-10-26), so an S.ms-based wait would
 * hang forever. tRESW2 is 20 us; ~10 ms of loop is generous. WDT stays fed. */
static void fcu_reset(void)
{
    uint32_t n = 1000000u;

    wr16(FL_FRESETR, 0xCC01u);
    while (n-- != 0u) {
        if ((n & 0xFFFFu) == 0u) {
            pf_service();
        }
    }
    wr16(FL_FRESETR, 0xCC00u);
    S.fw_loaded = 0u;            /* firmware must be re-transferred after a reset */
    cache_flush();
}

static void fcu_fw_transfer(void)
{
    if (rd16(FL_FENTRYR) != 0u) {
        fcu_read_mode();
    }
    wr16(FL_FCURAME, 0xC401u);
    for (uint32_t i = 0u; i < FL_FCU_FW_LEN; i += 4u) {
        wr32(FL_FCU_RAM + i, rd32(FL_FCU_FW + i));
        if ((i & 0x1FFu) == 0u) {
            pf_service();
        }
    }
    S.fw_loaded = 1u;
}

/* figure 25.15 for one user-MAT area. Returns 0 or an engine error code. */
static uint8_t fcu_enter_pe(uint32_t rom)
{
    const uint16_t bit = rom < 0x00100000u ? 0x0001u : 0x0002u;

    if (!S.fw_loaded) {
        fcu_fw_transfer();
    }
    if (rd16(FL_FENTRYR) != 0u) {
        fcu_read_mode();
    }
    if (!S.fwe_skip) {
        if ((rd8(FL_FPMON) & FL_FPMON_FWE) == 0u) {
            S.last_err = PF_LAST_FWE;
            return PF_E_FCU;
        }
    }
    wr16(FL_FENTRYR, (uint16_t)(0xAA00u | bit));
    (void)rd16(FL_FENTRYR);
    PF_NOP5();
    if (rd16(FL_FENTRYR) != bit) {
        fcu_read_mode();
        S.last_err = PF_LAST_FENTRYR;
        return PF_E_FCU;
    }
    if (fcu_status() != PF_LAST_OK) {
        /* manual's remedy for stale/latched status (FRDTCT/FRCRCT read 0x03 from boot
         * in the plant session): FCU reset, firmware reload, one re-entry */
        fcu_reset();
        fcu_fw_transfer();
        wr16(FL_FENTRYR, (uint16_t)(0xAA00u | bit));
        (void)rd16(FL_FENTRYR);
        PF_NOP5();
        if (rd16(FL_FENTRYR) != bit) {
            fcu_read_mode();
            S.last_err = PF_LAST_FENTRYR;
            return PF_E_FCU;
        }
        if (fcu_status() != PF_LAST_OK) {
            fcu_read_mode();
            S.last_err = PF_LAST_STATUS;
            return PF_E_FCU;
        }
    }
    return 0u;
}

static void op_erase_start(uint32_t addr)
{
    const uint32_t pe = FL_PE_BASE + addr;

    wr8(pe, 0x20u);
    wr8(pe, 0xD0u);
    S.op_addr = addr;
    S.t0 = S.ms;
    S.st = PF_ST_ERASE;
}

static void op_prog_start(void)
{
    const uint32_t pe = FL_PE_BASE + S.page;

    wr8(pe, 0xE8u);
    wr8(pe, 0x80u);
    for (uint32_t i = 0u; i < PF_PAGE; i += 2u) {
        wr16(pe + i, (uint16_t)((uint16_t)S.buf[i] << 8 | S.buf[i + 1u]));
    }
    wr8(pe, 0xD0u);
    S.op_addr = S.page;
    S.t0 = S.ms;
    S.st = PF_ST_PROG;
}

/* ---- data MAT (EEPROM) P/E — manual section 26, FWE-free ---- */

/* FENTRYD entry: same enabling conditions as FENTRY0 but NO FWE requirement.
 * Proactive cleanup: clear CMDLK + error flags + FCU reset + fw reload BEFORE
 * the FENTRYR write (accumulated error state from code-flash attempts blocks
 * the FCU from accepting new FENTRYR writes — bench 2026-10-27). */
static uint8_t fcu_enter_df(void)
{
    /* 1. release command lock */
    if ((rd8(FL_FASTAT) & FL_FASTAT_CMDLK) != 0u) {
        wr8(FL_FASTAT, FL_FASTAT_CMDLK);       /* documented release: write H'10 */
    }
    /* 2. ensure read mode */
    if (rd16(FL_FENTRYR) != 0u) {
        fcu_read_mode();
    }
    /* 3. clear error flags: status-clear command at any data MAT address */
    if ((rd8(FL_FSTATR0) & 0x7Fu) != 0u) {     /* ILGL/ERS/PRG or not FRDY */
        wr8(PF_DFW_LO, 0x50u);                 /* status-clear */
    }
    /* 4. FCU reset (clears everything) + firmware reload */
    fcu_reset();
    fcu_fw_transfer();
    /* 5. set block enables BEFORE entry (matches fl_fcu.c ordering) */
    wr16(FL_EEPWE0, (uint16_t)(0x1E00u | 0xFFu));
    /* 6. NOW enter P/E mode */
    wr16(FL_FENTRYR, 0xAA80u);
    (void)rd16(FL_FENTRYR);
    PF_NOP5();
    if (rd16(FL_FENTRYR) != 0x0080u) {
        fcu_read_mode();
        S.last_err = PF_LAST_FENTRYR;
        return PF_E_FCU;
    }
    /* stale ILGL from earlier code-flash attempts survives the FCU reset;
     * clear it with the status-clear command at the data MAT address */
    if ((rd8(FL_FSTATR0) & 0x7Fu) != 0u) {
        wr8(PF_DFW_LO, 0x50u);             /* status-clear */
        PF_NOP5();
        if ((rd8(FL_FSTATR0) & 0x7Fu) != 0u) {
            /* one more try after a dummy FCU read-mode cycle */
            fcu_read_mode();
            wr16(FL_FENTRYR, 0xAA80u);
            (void)rd16(FL_FENTRYR);
            PF_NOP5();
            if (rd16(FL_FENTRYR) != 0x0080u) {
                fcu_read_mode();
                S.last_err = PF_LAST_FENTRYR;
                return PF_E_FCU;
            }
        }
    }
    return 0u;
}

static void op_df_erase_start(uint32_t addr)
{
    /* data MAT is directly addressable at 0x80100000: no P/E window needed */
    wr8(addr, 0x20u);
    wr8(addr, 0xD0u);
    S.op_addr = addr;
    S.t0 = S.ms;
    S.st = PF_ST_ERASE;
}

static void op_df_prog_start(void)
{
    const uint32_t addr = S.page;      /* direct 0x80100000+ address */

    wr8(addr, 0xE8u);
    wr8(addr, 0x40u);                  /* N = 0x40 words = 128 bytes */
    for (uint32_t i = 0u; i < PF_DF_PAGE; i += 2u) {
        wr16(addr + i, (uint16_t)((uint16_t)S.buf[i] << 8 | S.buf[i + 1u]));
    }
    wr8(addr, 0xD0u);
    S.op_addr = S.page;
    S.t0 = S.ms;
    S.st = PF_ST_PROG;
}

/* Advance an in-flight FCU wait: 0 = still busy, 1 = finished. On failure S.err is set and
 * the state moved to PF_ST_RESULT; on success the caller completes the result. */
static uint8_t step_fcu(uint32_t timeout_ms)
{
    pf_service();
    if ((rd8(FL_FSTATR0) & FL_FSTATR0_FRDY) == 0u) {
        if (S.ms - S.t0 > timeout_ms) {
            S.last_err = PF_LAST_TIMEOUT;
            fcu_reset();
            S.err = PF_E_FCU;
            S.st = PF_ST_RESULT;
            return 1u;
        }
        return 0u;
    }
    {
        const uint8_t e = fcu_status();

        if (e != PF_LAST_OK) {
            S.last_err = e;
            if (e == PF_LAST_FCU) {
                fcu_reset();                   /* FCUERR: reset; firmware reloaded later */
            } else {
                fcu_recover_soft(FL_PE_BASE + S.op_addr);
            }
            S.err = PF_E_FCU;
            S.st = PF_ST_RESULT;
            return 1u;
        }
    }
    fcu_read_mode();
    return 1u;                                 /* command finished OK */
}

/* ---- result helpers (res ctx: count u8 at +4, buffer at +0x14, error at +0x16) ---- */

static void res_reset(uint8_t *res)
{
    res[4] = 0u;
    res[0x16] = 0u;
}

static void res_byte(uint8_t *res, uint8_t v)
{
    res[0x14] = v;
    res[4] = 1u;
}

static void finish(uint8_t err)
{
    S.err = err;                    /* 0 = success (result already in the res ctx) */
    S.st = PF_ST_RESULT;
}

/* ---- flash read paths (ROM read mode only) ---- */

/* Map a readable flash source into a pointer. mat != 0 on success means the app MAT was
 * remapped and flash_unview() must restore. 0 = ok, else engine error. */
static uint8_t flash_view(uint32_t addr, uint32_t n, const uint8_t **p, uint16_t *mat)
{
    *mat = 0u;
    if (rd16(FL_FENTRYR) != 0u) {
        return PF_E_SEQ;                /* P/E mode active: ROM read impossible */
    }
    if (addr >= 0xFFF80000u && addr < 0xFFFA0000u) {
        /* on-chip RAM1: raw read, no remap (boot-manager image 0xFFF88000-0xFFF8BFFF,
         * service image 0xFFF82000-0xFFF83A5F, supervisor shared cells 0xFFF852xx) */
        if (n >= 0xFFFA0000u - addr) {
            return PF_E_RANGE;
        }
        *p = (const uint8_t *)(uintptr_t)addr;
    } else if (addr < PF_WIN_HI) {      /* user MAT 0x000000-0x1FFFFF via app-MAT view */
        if (n > PF_WIN_HI - addr) {
            return PF_E_RANGE;
        }
        *mat = pf_map_app();
        *p = (const uint8_t *)(uintptr_t)addr;
    } else if (addr >= PF_DF_LO && addr < PF_DF_HI) {
        if (n > PF_DF_HI - addr) {
            return PF_E_RANGE;
        }
        *p = (const uint8_t *)(uintptr_t)(0x80100000u + (addr - PF_DF_LO));
    } else if (addr >= PF_DF2_LO && addr < PF_DF2_HI) {
        if (n > PF_DF2_HI - addr) {
            return PF_E_RANGE;
        }
        *p = (const uint8_t *)(uintptr_t)addr;
    } else {
        return PF_E_RANGE;
    }
    return 0u;
}

static void flash_unview(uint16_t mat)
{
    if (mat != 0u && mat != ROMMAT_APP) {
        pf_unmap_app(mat);
    }
}

/* copy n bytes from a readable source into dst. 0 = ok, else engine error. */
static uint8_t read_flash(uint32_t addr, uint8_t *dst, uint32_t n)
{
    const uint8_t *src;
    uint16_t mat;
    uint8_t e = flash_view(addr, n, &src, &mat);

    if (e != 0u) {
        return e;
    }
    for (uint32_t i = 0u; i < n; i++) {
        dst[i] = src[i];
    }
    flash_unview(mat);
    return 0u;
}

/* compare n bytes against exp. *diff = first differing offset (n when equal).
 * 0 = equal, 1 = differs, else engine error. */
static uint8_t cmp_flash(uint32_t addr, const uint8_t *exp, uint32_t n, uint32_t *diff)
{
    const uint8_t *src;
    uint16_t mat;
    uint8_t e = flash_view(addr, n, &src, &mat);
    uint32_t i = 0u;

    if (e != 0u) {
        return e;
    }
    while (i < n && src[i] == exp[i]) {
        i++;
    }
    flash_unview(mat);
    *diff = i;
    return i == n ? 0u : 1u;
}

/* ---- opcodes: synchronous ones fill the result and return 0, or an error code;
 * ERASE / final PROGRAM return PF_ASYNC after issuing the FCU command. ---- */

static uint8_t op_nop(uint8_t *res)
{
    uint8_t *r = &res[0x14];

    r[0] = 'P';
    r[1] = 'F';
    r[2] = 'E';                      /* error cell: only meaningful on error replies */
    r[3] = '1';
    r[4] = PF_VERSION;
    r[5] = rd8(FL_FPMON);
    r[6] = rd8(FL_FSTATR0);
    r[7] = rd8(FL_FSTATR1);
    r[8] = rd8(FL_FASTAT);
    r[9] = (uint8_t)rd16(FL_FPESTAT);
    r[10] = (uint8_t)rd16(FL_FENTRYR);
    r[11] = S.last_err;
    res[4] = 12u;
    return 0u;
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint8_t op_read(const uint8_t *data, uint16_t dlen, uint16_t param, uint8_t *res)
{
    const uint32_t addr = be32(data);
    uint8_t e;

    if (dlen != 4u || param == 0u || param > PF_DATA_MAX) {
        return PF_E_LEN;
    }
    e = read_flash(addr, &res[0x14], param);
    if (e != 0u) {
        return e;
    }
    res[4] = (uint8_t)param;
    return 0u;
}

/* USER BOOT MAT read (2026-10-29): the 32KB boot MAT shares area 0 with the
 * user MAT; the vendor map helper (RAM 0xFFF84104) selects it with r4=0 and
 * restores with r4=1. Never hand-roll ROMMAT (bench 2026-10-26: hung twice).
 * data = BE32 offset in 0x0000-0x7FFF, param = length <= 0xF8. */
static uint8_t op_bootmat(const uint8_t *data, uint16_t dlen, uint16_t param, uint8_t *res)
{
    const uint32_t addr = be32(data);
    const uint16_t saved = ROMMAT;      /* bit0 = ROMSEL as read back */
    uint8_t *r = &res[0x14];
    uint16_t i;

    if (dlen != 4u || param == 0u || param > PF_DATA_MAX || addr > 0x8000u - (uint32_t)param) {
        return PF_E_LEN;
    }
    if (rd16(FL_FENTRYR) != 0u) {
        return PF_E_SEQ;                /* P/E mode active: ROM read impossible */
    }
    if ((saved & 1u) == 0u) {
        PF_MAP_HELPER(0u);              /* user boot MAT at area 0 */
    }
    asm volatile ("" ::: "memory");
    {
        const volatile uint8_t *src = (const volatile uint8_t *)(uintptr_t)addr;
        for (i = 0u; i < param; i++) {
            r[i] = src[i];
        }
    }
    asm volatile ("" ::: "memory");
    if ((saved & 1u) == 0u) {
        PF_MAP_HELPER(1u);              /* user MAT back at area 0 */
    }
    res[4] = (uint8_t)param;
    return 0u;
}

/* RAM poke (bench): data = BE32 addr + bytes (1..0xF4) -> raw write into RAM1.
 * Echoes the written bytes back for verification. Guard: 0xFFF80000-0xFFFA0000
 * EXCLUDING this engine's own footprint is allowed; anything else -> RANGE. */
static uint8_t op_ramwrite(const uint8_t *data, uint16_t dlen, uint8_t *res)
{
    const uint32_t addr = be32(data);
    const uint16_t n = (uint16_t)(dlen - 4u);

    if (dlen < 5u || n > PF_CHUNK_MAX) {
        return PF_E_LEN;
    }
    if (addr < 0xFFF80000u || (addr + n) > 0xFFFA0000u) {
        return PF_E_RANGE;
    }
    for (uint32_t i = 0u; i < n; i++) {
        *(volatile uint8_t *)(uintptr_t)(addr + i) = data[4u + i];
    }
    for (uint32_t i = 0u; i < n; i++) {
        res[0x14 + i] = *(volatile const uint8_t *)(uintptr_t)(addr + i);
    }
    res[4] = (uint8_t)n;
    return 0u;
}

static uint8_t erase_block_ok(uint32_t a)
{
    if (a < PF_WIN_LO || a >= PF_WIN_HI) {
        return 0u;
    }
    if (a < 0x000A0000u) {
        return (a & 0xFFFFu) == 0u ? 1u : 0u;     /* EB11-EB16: 64 KB grid */
    }
    return (a & 0x1FFFFu) == 0u ? 1u : 0u;        /* EB17-EB27: 128 KB grid */
}

static uint8_t op_erase(const uint8_t *data, uint16_t dlen)
{
    const uint32_t addr = be32(data);
    uint8_t e;

    if (dlen != 4u) {
        return PF_E_LEN;
    }
    if (addr < PF_WIN_LO || addr >= PF_WIN_HI) {
        return PF_E_DENIED;             /* boot blocks / outside: hard refusal */
    }
    if (erase_block_ok(addr) == 0u) {
        return PF_E_RANGE;              /* inside the window but not a block start */
    }
    e = fcu_enter_pe(addr);
    if (e != 0u) {
        return e;
    }
    op_erase_start(addr);
    return PF_ASYNC;
}

static uint8_t op_program(const uint8_t *data, uint16_t dlen, uint16_t off, uint8_t *res)
{
    const uint32_t addr = be32(data);
    const uint32_t n = (uint32_t)dlen - 4u;
    uint8_t e;

    if (dlen < 5u || n > PF_CHUNK_MAX) {
        return PF_E_LEN;
    }
    if (off == 0u) {
        if (addr < PF_WIN_LO || addr >= PF_WIN_HI) {
            return PF_E_DENIED;
        }
        if ((addr & (PF_PAGE - 1u)) != 0u) {
            return PF_E_RANGE;
        }
        S.page = addr;
        S.staged = 0u;
        for (uint32_t i = 0u; i < PF_PAGE; i++) {
            S.buf[i] = 0xFFu;
        }
    } else {
        if (addr != S.page || off != S.staged) {
            return PF_E_SEQ;
        }
        if ((uint32_t)off + n > PF_PAGE) {
            return PF_E_LEN;
        }
    }
    for (uint32_t i = 0u; i < n; i++) {
        S.buf[off + i] = data[4u + i];
    }
    S.staged = (uint16_t)(off + n);
    if (S.staged != PF_PAGE) {
        res_byte(res, (uint8_t)S.staged);   /* staged; waiting for the rest of the page */
        return 0u;
    }
    e = fcu_enter_pe(S.page);
    if (e != 0u) {
        return e;
    }
    op_prog_start();
    return PF_ASYNC;
}

/* data MAT erase: 8 KB block, DB00-DB07 only. Data = block address BE32. */
static uint8_t op_df_erase(const uint8_t *data, uint16_t dlen)
{
    const uint32_t addr = be32(data);
    uint8_t e;

    if (dlen != 4u) {
        return PF_E_LEN;
    }
    if (addr < PF_DFW_LO || addr >= PF_DFW_HI) {
        return PF_E_DENIED;             /* DB08-15 / outside: refusal */
    }
    if ((addr & 0x1FFFu) != 0u) {
        return PF_E_RANGE;              /* not an 8 KB block boundary */
    }
    e = fcu_enter_df();
    if (e != 0u) {
        return e;
    }
    op_df_erase_start(addr);
    return PF_ASYNC;
}

/* data MAT program: 128-byte pages staged like ROM PROGRAM. param = page offset
 * (0x0000 or 0x0080); data = page address BE32 + chunk (n <= 0x78 after addr). */
static uint8_t op_df_program(const uint8_t *data, uint16_t dlen, uint16_t off, uint8_t *res)
{
    const uint32_t addr = be32(data);
    const uint32_t n = (uint32_t)dlen - 4u;
    uint8_t e;

    if (dlen < 5u || n > 0x78u) {
        return PF_E_LEN;
    }
    if (off == 0u) {
        if (addr < PF_DFW_LO || addr >= PF_DFW_HI) {
            return PF_E_DENIED;
        }
        if ((addr & (PF_DF_PAGE - 1u)) != 0u) {
            return PF_E_RANGE;
        }
        S.page = addr;
        S.staged = 0u;
        for (uint32_t i = 0u; i < PF_DF_PAGE; i++) {
            S.buf[i] = 0xFFu;           /* erased data-flash content is undefined:
                                            FF padding programs benignly */
        }
    } else {
        if (addr != S.page || off != S.staged) {
            return PF_E_SEQ;
        }
        if ((uint32_t)off + n > PF_DF_PAGE) {
            return PF_E_LEN;
        }
    }
    for (uint32_t i = 0u; i < n; i++) {
        S.buf[off + i] = data[4u + i];
    }
    S.staged = (uint16_t)(off + n);
    if (S.staged != PF_DF_PAGE) {
        res_byte(res, (uint8_t)S.staged);
        return 0u;
    }
    e = fcu_enter_df();
    if (e != 0u) {
        return e;
    }
    op_df_prog_start();
    return PF_ASYNC;
}

static uint8_t op_verify(const uint8_t *data, uint16_t dlen, uint16_t param, uint8_t *res)
{    const uint32_t addr = be32(data);
    uint32_t i;
    uint8_t e;

    if (param == 0u || param > PF_CHUNK_MAX || (uint32_t)dlen != 4u + (uint32_t)param) {
        return PF_E_LEN;
    }
    e = cmp_flash(addr, &data[4], param, &i);
    if (e > 1u) {
        return e;                      /* mapping/range error */
    }
    res[0x14] = e;                     /* 0 = match, 1 = mismatch */
    res[0x15] = (uint8_t)(i >> 8);     /* error cell region: success path only */
    res[0x16] = (uint8_t)i;
    res[4] = 3u;
    return 0u;
}

static uint8_t op_reset(uint8_t *res)
{
    if (rd16(FL_FENTRYR) != 0u) {
        fcu_read_mode();
    }
    S.staged = 0u;
    res[0x14] = rd8(FL_FPMON);
    res[0x15] = rd8(FL_FSTATR0);
    res[0x16] = rd8(FL_FSTATR1);       /* error cell: success path only */
    res[0x17] = rd8(FL_FASTAT);
    res[4] = 4u;
    return 0u;
}

static void supx_result(uint8_t *res);

/* ---- raw RSPI-B access to the supervisor (SSLB1), spec 03 section 4.4 ----
 * CPU-mode recipe = boot FUN_00002660; handshake = boot FUN_000026A8 (0x8E01
 * then eleven 0x8401). All polling is cycle-capped: no timer runs here. */
#define RSP_SPCR    REG8 (0xFFFFB800u)
#define RSP_SPPCR   REG8 (0xFFFFB802u)
#define RSP_SPSR    REG8 (0xFFFFB803u)
#define RSP_SPDR    REG16(0xFFFFB804u)
#define RSP_SPBR    REG8 (0xFFFFB80Au)
#define RSP_SPDCR   REG8 (0xFFFFB808u)
#define RSP_SPCKD   REG8 (0xFFFFB80Cu)
#define RSP_SSLND   REG8 (0xFFFFB80Du)
#define RSP_SPND    REG8 (0xFFFFB80Eu)
#define RSP_SPCMD0  REG16(0xFFFFB810u)
#define DMA_RX76    REG16(0xFFFE6074u)

/* module-stop (clock-gate) registers the boot MAT init FUN_00002ddc SETS:
 * stopped modules read/write as bus errors from engine context (bench: RSPI-B
 * write crashed the session 2026-10-26; FRC frozen all day = also gated). */
#define STB_A       REG16(0xFFFE0400u)   /* boot sets |= 0x3C10 */
#define STB_B       REG16(0xFFFE0420u)   /* boot sets |= 0x0040 */
#define STB_C       REG16(0xFFFE0430u)   /* boot sets  = 0x00E0 */

static void rspi_init(void)
{
    DMA_RX76 = 0u;                    /* both halves via one 32-bit-friendly cell */
    REG16(0xFFFE6076u) = 0u;
    RSP_SPSR = 0u;
    RSP_SPPCR = 0u;
    RSP_SPBR = 5u;
    RSP_SPDCR = 0u;
    RSP_SPCKD = 0u;
    RSP_SSLND = 0u;
    RSP_SPND = 1u;
    RSP_SPCMD0 = 0xAF11u;             /* SSLB1, 16 bit, mode 1 */
    RSP_SPCR = 0x48u;
}

/* PFC: route the RSPI-B pins to their SPI functions (manual section 22):
 * PG7/SSLB1 + PG6/SSLB0 in PGCR1 (bits 15-12 = 10,10), PB13/RSPCKB in PBCR4
 * (bits 15-14 = 11), PB3/MISOB + PB2/MOSIB in PBCR1 (bits 15-12 = 11).
 * Without this the supervisor's CS pin carries whatever the pin is muxed to -
 * an SPI transfer then resets the TCU (bench 2026-10-26 step-10 bisect). */
static void rspi_pfc(void)
{
    REG16(0xFFFFC840u) = (uint16_t)((REG16(0xFFFFC840u) & 0x0FFFu) | 0xA000u);
    REG16(0xFFFE3890u) = (uint16_t)((REG16(0xFFFE3890u) & 0x3FFFu) | 0xC000u);
    REG16(0xFFFE3896u) = (uint16_t)((REG16(0xFFFE3896u) & 0x3FFFu) | 0xC000u);
}

/* send one 16-bit frame with the MANUAL'S flag discipline (17.3.4/17.3.5):
 * poll SPTEF (bit5) before writing SPDR; poll SPRF (bit7) for the reply, read
 * SPDR, then clear SPSR (read-then-0). Without the SPRF clear, SPDR FREEZES
 * after the first transfer - the constant-AA00 bench artifact (2026-10-26).
 * 0 = ok (reply in S.last_reply), 1 = poll budget exhausted. */
static uint8_t rspi_send(uint16_t word)
{
    uint32_t n = 100000u;

    /* stateless frame: full setup before EVERY word (the stock 5 ms keep-alive
     * discipline, boot ROM 0x1D430-per-tick). A persistent-enable exchange wedges
     * after the first frame — SPRF never sets again — which froze S.last_reply at
     * the first reply (the constant-AA00 artifact) and silently dropped burst
     * frames 2..n (bench 2026-10-27, lab-app counters sent=36049/ok=1). */
    rspi_init();
    if ((RSP_SPSR & 0x20u) == 0u) {
        return 1u;
    }
    RSP_SPDR = word;
    while (n-- != 0u) {
        if ((RSP_SPSR & 0x80u) != 0u) {
            S.last_reply = RSP_SPDR;
            RSP_SPSR = 0u;
            return 0u;
        }
    }
    return 1u;
}

static uint8_t rspi_handshake(void)
{
    uint8_t e = rspi_send(0x8E01u);
    uint8_t i;

    for (i = 0u; e == 0u && i < 11u; i++) {
        e = rspi_send(0x8401u);
    }
    return e;
}

/* Supervisor / FWE experiment: data[0] = state byte for 0xFFF8526F (0xFF = leave),
 * data[1..2] = BE16 raw word for SSLB1 (flags bit6), param bit0 = zero the
 * 120000-fire counter, bit1 = set port 0xFFFE3902 bit5, bit2 = clear port bit4,
 * bit3 = NO-SETTLE (return immediately, never async), bit4 = set fwe_skip
 * (P/E entry without the FPMON.FWE check), bit5 = clear it, bit6 = send the raw
 * word (after CPU-mode init), bit7 = run the boot handshake first.
 * Reports: FPMON, FSTATR0, FSTATR1, FENTRYR, state, port2, counter BE32,
 * S.ms BE32 (14 bytes) - after a 60 ms settle unless bit3. */
static uint8_t op_supx(const uint8_t *data, uint16_t dlen, uint16_t param, uint8_t *res)
{
    if (dlen < 1u) {
        return PF_E_LEN;
    }
    if ((param & 1u) != 0u) {
        SUP_COUNT = 0u;
    }
    if (data[0] != 0xFFu) {
        SUP_STATE = data[0];
    }
    if ((param & 2u) != 0u) {
        SUP_PORT2 = (uint8_t)(SUP_PORT2 | 0x20u);
    }
    if ((param & 4u) != 0u) {
        SUP_PORT2 = (uint8_t)(SUP_PORT2 & (uint8_t)~0x10u);
    }
    if ((param & 0x10u) != 0u) {
        S.fwe_skip = 1u;
    }
    if ((param & 0x20u) != 0u) {
        S.fwe_skip = 0u;
    }
    if ((param & 0x0100u) != 0u) {     /* ungate: clear the boot's stop-bits */
        STB_A = (uint16_t)(STB_A & (uint16_t)~0x3C10u);
        (void)STB_A;
    }
    if ((param & 0x0200u) != 0u) {
        STB_B = (uint16_t)(STB_B & (uint16_t)~0x0040u);
        (void)STB_B;
    }
    if ((param & 0x0400u) != 0u) {
        STB_C = (uint16_t)(STB_C & (uint16_t)~0x00E0u);
        (void)STB_C;
    }
    if ((param & 0x40u) != 0u) {       /* raw supervisor word (full sequence) */
        rspi_pfc();
        rspi_init();
        if ((param & 0x80u) != 0u) {
            (void)rspi_handshake();
        }
        if (dlen >= 3u) {
            (void)rspi_send((uint16_t)((uint16_t)data[1] << 8 | data[2]));
        }
    }
    switch ((param >> 12) & 0x0Fu) {   /* step-bisect: ONE action per command */
    case 1u:
        REG32(0xFFFE6074u) = 0u;       /* DMAC request disable (32-bit as the boot does) */
        break;
    case 2u:
        RSP_SPSR = 0u;
        break;
    case 3u:
        RSP_SPPCR = 0u;
        break;
    case 4u:
        RSP_SPBR = 5u;
        break;
    case 5u:
        REG8(0xFFFFB808u) = 0u;        /* SPSCRB (spec 03 called it SPDCR) */
        break;
    case 6u:
        REG8(0xFFFFB80Cu) = 0u;        /* SPCKD */
        REG8(0xFFFFB80Du) = 0u;        /* SSLND */
        break;
    case 7u:
        RSP_SPND = 1u;
        break;
    case 8u:
        RSP_SPCMD0 = 0xAF11u;
        break;
    case 9u:
        RSP_SPCR = 0x48u;
        break;
    case 10u:
        (void)rspi_send(0x8E01u);      /* first handshake frame: the real transfer */
        break;
    case 11u:
        (void)RSP_SPSR;                /* bounded by nothing: a single read */
        break;
    case 12u:
        RSP_SPSR = 0u;                 /* clear */
        break;
    case 13u:
        rspi_pfc();                    /* mux the RSPI-B pins (PGCR1/PBCR4/PBCR1) */
        break;
    case 14u:
        (void)rspi_send(0x8E01u);      /* transfer retry AFTER the PFC mux */
        break;
    case 15u:
        rspi_pfc();
        rspi_init();
        (void)rspi_send(0x8E00u);      /* MID-RUN LEGAL frame: keep-alive, bit2=0 */
        break;
    case 16u:
        (void)rspi_send(0x8E04u);      /* keep-alive toggle, bit2=1 */
        break;
    case 17u:
        /* THE CANDIDATE: the vendor's flash-context arming burst, back-to-back with
         * poll/clear pacing, minus the power-up-only [8E,01] prefix (mid-run = reset). */
        rspi_pfc();
        rspi_init();
        for (uint8_t i = 0u; i < 11u; i++) {
            (void)rspi_send(0x8401u);
        }
        break;
    default:
        break;
    }
    if ((param & 8u) != 0u) {
        supx_result(res);
        return 0u;
    }
    S.t0 = S.ms;
    S.st = PF_ST_SUPWAIT;
    return PF_ASYNC;
}

static void supx_result(uint8_t *res)
{
    uint8_t *r = &res[0x14];

    r[0] = rd8(FL_FPMON);
    r[1] = rd8(FL_FSTATR0);
    r[2] = rd8(FL_FSTATR1);
    r[3] = (uint8_t)rd16(FL_FENTRYR);
    r[4] = SUP_STATE;
    r[5] = SUP_PORT2;
    r[6] = (uint8_t)(SUP_COUNT >> 24);
    r[7] = (uint8_t)(SUP_COUNT >> 16);
    r[8] = (uint8_t)(SUP_COUNT >> 8);
    r[9] = (uint8_t)SUP_COUNT;
    r[10] = (uint8_t)(S.ms >> 24);
    r[11] = (uint8_t)(S.ms >> 16);
    r[12] = (uint8_t)(S.ms >> 8);
    r[13] = (uint8_t)S.ms;
    {
        /* MISR (HW manual sec.24): double-read per the stale-read trap */
        const uint32_t misr = rd32f(0xFFFC1C04u);

        r[14] = (uint8_t)(misr >> 24);
        r[15] = (uint8_t)(misr >> 16);
        r[16] = (uint8_t)(misr >> 8);
        r[17] = (uint8_t)misr;
    }
    r[18] = REG8(0xFFFC1C00u);                      /* MISRCR (bit0 = MISREN) */
    r[19] = 0u;
    {
        const uint32_t tcn = rd32(FRCTR);   /* longword read of TCNTA */

        r[20] = (uint8_t)(tcn >> 24);
        r[21] = (uint8_t)(tcn >> 16);
        r[22] = (uint8_t)(tcn >> 8);
        r[23] = (uint8_t)tcn;
    }
    r[24] = (uint8_t)(REG16(0xFFFFC840u) >> 8);      /* PGCR1 */
    r[25] = (uint8_t)REG16(0xFFFFC840u);
    r[26] = (uint8_t)(REG16(0xFFFE3890u) >> 8);      /* PBCR4 */
    r[27] = (uint8_t)REG16(0xFFFE3890u);
    r[28] = (uint8_t)(REG16(0xFFFE3896u) >> 8);      /* PBCR1 */
    r[29] = (uint8_t)REG16(0xFFFE3896u);
    r[30] = (uint8_t)(S.last_reply >> 8);            /* last supervisor reply (SPRF-read) */
    r[31] = (uint8_t)S.last_reply;
    r[32] = (uint8_t)(rd32f(0xFFFFA820u) >> 16);     /* ROMMAT high (ROMSEL bit 15) */
    r[33] = (uint8_t)rd32f(0xFFFFA820u);
    res[4] = 34u;
}

/* ---- command start (state IDLE): fills the result, records an error, or leaves the
 * state machine in ERASE/PROG. Always ends in ST_RESULT (sync) or an async state. ---- */

static void start_cmd(const uint8_t *cmd, uint8_t *res)
{
    const uint8_t opcode = cmd[0x0A];
    const uint16_t param = (uint16_t)((uint16_t)cmd[0x0C] << 8 | cmd[0x0D]);
    /* F0F0 stores the data length as ONE byte at +4; +5 is the connection id. */
    const uint16_t dlen = (uint16_t)cmd[0x04];
    const uint8_t *data = &cmd[0x14];
    uint8_t e;

    res_reset(res);
    if (dlen > PF_DATA_MAX) {
        finish(PF_E_LEN);              /* cannot happen: F0F0 caps data at 0xF8 */
        return;
    }
    switch (opcode) {
    case PF_OP_NOP:
        e = op_nop(res);
        break;
    case PF_OP_READ:
        e = dlen >= 4u ? op_read(data, dlen, param, res) : PF_E_LEN;
        break;
    case PF_OP_ERASE:
        e = dlen >= 4u ? op_erase(data, dlen) : PF_E_LEN;
        break;
    case PF_OP_PROGRAM:
        e = op_program(data, dlen, param, res);
        break;
    case PF_OP_VERIFY:
        e = op_verify(data, dlen, param, res);
        break;
    case PF_OP_DFERASE:
        e = dlen >= 4u ? op_df_erase(data, dlen) : PF_E_LEN;
        break;
    case PF_OP_DFPROG:
        e = op_df_program(data, dlen, param, res);
        break;
    case PF_OP_RESET:
        e = op_reset(res);
        break;
    case PF_OP_RAMW:
        e = op_ramwrite(data, dlen, res);
        break;
    case PF_OP_LOG:
        /* param = sample period in ms (0 = off). Arms the gate telemetry ring at
         * 0xFFF8E000; read back with F0F0 READ. */
        {
            volatile uint32_t *ctl = (volatile uint32_t *)(uintptr_t)0xFFF8E000u;
            if (param == 0u) {
                ctl[0] = 0u;
            } else {
                ctl[0] = 0x6C6F6731u; ctl[1] = param; ctl[2] = 0u;
                ctl[3] = 0u; ctl[4] = 0u; ctl[5] = 0u; ctl[6] = 0u;
            }
            res[0x14] = (uint8_t)ctl[3];
            res[0x15] = (uint8_t)(ctl[3] >> 8);
            res[4] = 2u;
            e = 0u;
        }
        break;
    case PF_OP_BOOTMAT:
        e = dlen >= 4u ? op_bootmat(data, dlen, param, res) : PF_E_LEN;
        break;
    case PF_OP_SUPX:
        e = dlen >= 1u ? op_supx(data, dlen, param, res) : PF_E_LEN;
        break;
    default:
        finish(PF_E_RANGE);            /* unknown opcode */
        return;
    }
    if (e == PF_ASYNC) {
        return;                        /* state now ERASE/PROG; delivery on completion */
    }
    finish(e);                         /* e == 0: success with result in the res ctx */
}

/* Advance the machine one step; used by EXEC and PUMP. */
static void pf_advance(const uint8_t *cmd, uint8_t *res)
{
    pf_service();

    switch (S.st) {
    case PF_ST_IDLE:
        start_cmd(cmd, res);
        return;
    case PF_ST_ERASE:
        if (step_fcu(PF_ERASE_TIMEOUT_MS) == 0u) {
            return;                    /* still erasing */
        }
        if (S.st != PF_ST_RESULT) {    /* finished OK: erase command result */
            res_reset(res);
            res_byte(res, 0u);
            finish(0u);
        }
        return;
    case PF_ST_PROG:
        if (step_fcu(PF_PROG_TIMEOUT_MS) == 0u) {
            return;                    /* still programming */
        }
        if (S.st != PF_ST_RESULT) {    /* finished OK: read the page back and compare */
            uint32_t i;
            const uint8_t e = cmp_flash(S.page, S.buf, PF_PAGE, &i);

            if (e > 1u) {
                S.last_err = PF_LAST_FCU;          /* mapping failed after programming */
                finish(PF_E_FCU);
                return;
            }
            if (e != 0u) {
                S.last_err = PF_LAST_READBACK;     /* programmed bytes differ */
                finish(PF_E_FCU);
                return;
            }
            S.staged = 0u;
            res_reset(res);
            res_byte(res, 0u);         /* programmed and verified */
            finish(0u);
        }
        return;
    case PF_ST_SUPWAIT:
        if (S.ms - S.t0 < 60u) {
            return;                    /* let the supervisor settle */
        }
        res_reset(res);
        supx_result(res);
        finish(0u);
        return;
    default:
        return;                        /* ST_RESULT: nothing to do */
    }
}

/* ---- engine ABI entries ---- */

int pf_exec(const uint8_t *cmd, uint8_t *res)
{
    pf_advance(cmd, res);
    pf_service();
    if (S.st == PF_ST_RESULT) {
        const uint8_t e = S.err;

        S.st = PF_ST_IDLE;
        S.err = 0u;
        if (e != 0u) {
            res_reset(res);
            res[0x16] = e;             /* F0F0 error cell (also result byte 2) */
            return 2;
        }
        return 1;                      /* result already in the res ctx */
    }
    return 0;                          /* async op in flight */
}

int pf_ready(uint8_t *res)
{
    (void)res;
    pf_service();
    return (S.st == PF_ST_IDLE || S.st == PF_ST_RESULT) ? 1 : 0;
}

int pf_pump(const uint8_t *cmd, uint8_t *res)
{
    /* gate telemetry logger: the session/driver loop pumps us continuously;
     * sample the shared-cell block on change while armed (F0F0 op 0x0A). */
#define LOG_CTRL  0xFFF8E000u
#define LOG_RING  0xFFF8E100u
#define LOG_END   0xFFF8FC00u
#define LOG_SRC   0xFFF85240u
#define LOG_BYTES 0x80u
#define LOG_MAGIC 0x6C6F6731u   /* "log1" */
    {
        volatile uint32_t *ctl = (volatile uint32_t *)(uintptr_t)LOG_CTRL;
        if (ctl[0] == LOG_MAGIC && ctl[1] != 0u &&
            (uint32_t)(S.ms - ctl[2]) >= ctl[1]) {
            const volatile uint8_t *src = (const volatile uint8_t *)(uintptr_t)LOG_SRC;
            uint8_t buf[LOG_BYTES];
            for (uint32_t i = 0u; i < LOG_BYTES; i++) { buf[i] = src[i]; }
            ctl[2] = S.ms;
            uint32_t head = ctl[4];
            const volatile uint8_t *prev =
                (const volatile uint8_t *)(uintptr_t)(LOG_RING + head * 0x88u + 8u);
            uint32_t differ = (ctl[3] == 0u) ? 1u : 0u;
            if (differ == 0u) {
                for (uint32_t i = 0u; i < LOG_BYTES; i++) {
                    if (prev[i] != buf[i]) { differ = 1u; break; }
                }
            }
            if (differ != 0u) {
                volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)(LOG_RING + head * 0x88u);
                *(volatile uint32_t *)(uintptr_t)dst = S.ms;
                *(volatile uint32_t *)(uintptr_t)(dst + 4u) = ctl[5];
                for (uint32_t i = 0u; i < LOG_BYTES; i++) { dst[8u + i] = buf[i]; }
                head++;
                if (head >= (LOG_END - LOG_RING) / 0x88u) { head = 0u; ctl[6] = 1u; }
                ctl[4] = head;
                ctl[3]++;
                ctl[5] = 0u;
            } else {
                ctl[5]++;
            }
        }
    }
    if (S.st == PF_ST_ERASE || S.st == PF_ST_PROG || S.st == PF_ST_SUPWAIT) {
        pf_advance(cmd, res);
    } else {
        pf_service();
    }
    return 0;
}

int pf_stop(uint8_t *res)
{
    pf_service();
    if (S.st == PF_ST_ERASE || S.st == PF_ST_PROG) {
        pf_advance((const uint8_t *)(uintptr_t)PF_CMD_CTX, res);
        if (S.st == PF_ST_ERASE || S.st == PF_ST_PROG) {
            return 0;                  /* still stopping; F0F1 re-polls */
        }
    }
    if (S.st == PF_ST_RESULT) {        /* undelivered result is dropped on stop */
        S.st = PF_ST_IDLE;
        S.err = 0u;
    }
    if (rd16(FL_FENTRYR) != 0u) {      /* never leave P/E mode behind */
        fcu_read_mode();
    }
    S.staged = 0u;
    res_reset(res);
    return 1;
}

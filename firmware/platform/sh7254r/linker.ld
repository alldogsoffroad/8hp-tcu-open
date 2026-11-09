/* SH7254R application block (spec 06): 0x040000-0x17FFFF, header at 0x40000.
 * RAM: the boot handover area 0xFFF84000-0xFFF8403F is not used (spec 06 section 5);
 * the application uses 0xFFF84100-0xFFF9FFFF. */
OUTPUT_FORMAT("elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)

MEMORY
{
    APPHDR (rx)  : ORIGIN = 0x00040000, LENGTH = 0x80
    APPVEC (rx)  : ORIGIN = 0x00040080, LENGTH = 0x7F0      /* vectors 4..511 */
    APPROM (rx)  : ORIGIN = 0x00040870, LENGTH = 0x0017FB70 - 0x00040870
    APPTAIL(r)   : ORIGIN = 0x0017FB70, LENGTH = 0x8C
    APPEND (r)   : ORIGIN = 0x0017FBFC, LENGTH = 4
    RAM    (rwx) : ORIGIN = 0xFFF84100, LENGTH = 0xFFFA0000 - 0xFFF84100
}

SECTIONS
{
    .app_header : { KEEP(*(.app_header)) } > APPHDR
    .app_vectors : { KEEP(*(.app_vectors)) } > APPVEC

    .text : {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > APPROM

    /* ── pinned supervisor cells (rebuild design §9/§10) ────────────────────
     * The stock's RAM addresses. Every range below is RESERVED to its full
     * extent (cursor forced past the last cell) so .data/.bss can NEVER be
     * auto-placed over a cell — empty reservations are what let .bss run
     * through 0xFFF90540-0xFFF90980 in the first P6-P7 image.
     * All sections NOLOAD: sup_cells_init() clears them; start.S does not.
     * The four bench-read staging words are pinned so cell == buffer. */

    .sup_gate 0xFFF84A40 (NOLOAD) : {      /* output-gate cell 0xFFF84A46    */
        __sup_gate_start = .; *(.sup_gate) . += 0x10; __sup_gate_end = .;
    } > RAM

    .sup_stage 0xFFF88400 (NOLOAD) : {     /* staging window (J5/J6/J4)      */
        __sup_stage_start = .; *(.sup_stage) . += 0x10; __sup_stage_end = .;
    } > RAM

    .sup_j1 0xFFF88424 (NOLOAD) : {        /* JOB_CTRL: 0x8E + C9 pair       */
        __sup_j1_start = .; *(.sup_j1) __sup_j1_end = .;
    } > RAM

    .sup_j2 0xFFF88434 (NOLOAD) : {        /* JOB_EN: 90|en                  */
        __sup_j2_start = .; *(.sup_j2) __sup_j2_end = .;
    } > RAM

    .sup_j3 0xFFF88444 (NOLOAD) : {        /* JOB_POS: 28/2E                 */
        __sup_j3_start = .; *(.sup_j3) __sup_j3_end = .;
    } > RAM

    .sup_stage2 0xFFF88450 (NOLOAD) : {    /* W1/W2/fault-twin staging       */
        __sup_stage2_start = .; *(.sup_stage2) . += 0x16; __sup_stage2_end = .;
    } > RAM

    .sup_j7 0xFFF885E0 (NOLOAD) : {        /* JOB_NIB: 0000                  */
        __sup_j7_start = .; *(.sup_j7) . += 0x02; __sup_j7_end = .;
    } > RAM

    .sup_walk 0xFFF90540 (NOLOAD) : {      /* walk reply cells .. 0x90551    */
        __sup_walk_start = .; *(.sup_walk) . += 0x14; __sup_walk_end = .;
    } > RAM

    .sup_cells 0xFFF90740 (NOLOAD) : {     /* protocol cells 0x907C8-0x9097B */
        __sup_cells_start = .; *(.sup_cells) . += 0x240; __sup_cells_end = .;
    } > RAM

    .sup_era 0xFFF9240C (NOLOAD) : {       /* era FRT stamps + staged token  */
        __sup_era_start = .; *(.sup_era) . += 0x0C; __sup_era_end = .;
    } > RAM

    .sup_period 0xFFF93844 (NOLOAD) : {    /* era period cells 0x93846/48    */
        __sup_period_start = .; *(.sup_period) . += 0x08; __sup_period_end = .;
    } > RAM

    .sup_fsm 0xFFF93B40 (NOLOAD) : {       /* walk FSM cells .. 0x93BA7      */
        __sup_fsm_start = .; *(.sup_fsm) . += 0x68; __sup_fsm_end = .;
    } > RAM

    .sup_use 0xFFF94218 (NOLOAD) : {       /* job use counters (14 × u16)    */
        __sup_use_start = .; *(.sup_use) . += 0x1C; __sup_use_end = .;
    } > RAM

    .sup_latch 0xFFF94994 (NOLOAD) : {     /* max-latch cell 0x94997         */
        __sup_latch_start = .; *(.sup_latch) . += 0x04; __sup_latch_end = .;
    } > RAM

    .sup_value 0xFFF94E5C (NOLOAD) : {     /* value cells .. 0x95010         */
        __sup_value_start = .; *(.sup_value) . += 0x1B8; __sup_value_end = .;
    } > RAM

    /* .data/.bss explicitly AFTER the last reservation: without the address,
     * ld back-fills the lowest gap and runs .bss over the pinned ranges. */
    .data 0xFFF95018 : {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        *(.ramfunc)                 /* code that must run from RAM (flash MAT switching) */
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT > APPROM
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    .noinit (NOLOAD) : {          /* survives watchdog resets (cleared only by power-off):
                                    * the E11 boot-window trip counter (spec 03 section 6.2h) */
        __noinit_start = .;
        *(.noinit .noinit.*)
        . = ALIGN(4);
        __noinit_end = .;
    } > RAM

    /* house rule (matches linker_ram.ld): never leave less than 4 KB of
     * stack below 0xFFFA0000 */
    ASSERT(__noinit_end <= 0xFFF9F000, "application data leaves less than 4 KB of stack")

    .app_tail : { KEEP(*(.app_tail)) } > APPTAIL
    .app_end : { KEEP(*(.app_end)) } > APPEND

    __vbr_base  = 0x00040070;   /* vector 4 at VBR + 0x10 = 0x40080 */
    __stack_top = 0xFFFA0000;

    /DISCARD/ : { *(.comment) *(.note*) }
}

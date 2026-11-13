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

    /* L0-sense: the stock's ADC DMA buffers/ring at their stock addresses
     * (plat_adcmux.c; adc_chain_port.md); the 80-byte mux sequence moved to
     * 0xFFF88500 (D1: the stock's 0xFFF8844C overlaps .sup_stage2) */
    .adc_dma_a 0xFFF88130 (NOLOAD) : { . += 0x3C; } > RAM   /* DMAC5 29 words + guard 0x8816A */
    .adc_dma_b 0xFFF884BA (NOLOAD) : { . += 0x22; } > RAM   /* DMAC4 16 words + guard 0x884DA */
    .adc_seq   0xFFF88500 (NOLOAD) : { . += 0x50; } > RAM   /* mux sequence (D1) */

    .sup_j7 0xFFF885E0 (NOLOAD) : {        /* JOB_NIB: 0000                  */
        __sup_j7_start = .; *(.sup_j7) . += 0x02; __sup_j7_end = .;
    } > RAM

    .adc_ring  0xFFF885E8 (NOLOAD) : { . += 0x142; } > RAM  /* A-DMAC ring 160 words + guard 0x88728 */
    .adc_cells 0xFFF90000 (NOLOAD) : { . += 0xB0; } > RAM   /* descriptor states + windows */
    .adc_out   0xFFF90184 (NOLOAD) : { . += 0x08; } > RAM   /* 0x90184..0x9018B */
    .adc_ms    0xFFF90628 (NOLOAD) : { . += 0x04; } > RAM

    .sup_walk 0xFFF90540 (NOLOAD) : {      /* walk reply cells .. 0x90551    */
        __sup_walk_start = .; *(.sup_walk) . += 0x14; __sup_walk_end = .;
    } > RAM

    .sup_cells 0xFFF90740 (NOLOAD) : {     /* protocol cells 0x907C8-0x9097B */
        __sup_cells_start = .; *(.sup_cells) . += 0x240; __sup_cells_end = .;
    } > RAM

    .adc_span  0xFFF910FC (NOLOAD) : { . += 0x0C; } > RAM   /* 0x910FC..0x91107 */

    .sup_era 0xFFF9240C (NOLOAD) : {       /* era FRT stamps + staged token  */
        __sup_era_start = .; *(.sup_era) . += 0x0C; __sup_era_end = .;
    } > RAM

    .sup_period 0xFFF9383C (NOLOAD) : {    /* guard cells 0x9383C-44 + period 0x93846/48 */
        __sup_period_start = .; *(.sup_period) . += 0x10; __sup_period_end = .;
    } > RAM

    .sup_fsm 0xFFF93B40 (NOLOAD) : {       /* walk FSM cells .. 0x93BA7      */
        __sup_fsm_start = .; *(.sup_fsm) . += 0x68; __sup_fsm_end = .;
    } > RAM

    .sup_s8 0xFFF9417C (NOLOAD) : {        /* state-8 budget cells 0x9417D/E */
        __sup_s8_start = .; *(.sup_s8) . += 0x04; __sup_s8_end = .;
    } > RAM

    .sup_use 0xFFF94218 (NOLOAD) : {       /* job use counters (14 × u16)    */
        __sup_use_start = .; *(.sup_use) . += 0x1C; __sup_use_end = .;
    } > RAM

    .sup_latch 0xFFF94994 (NOLOAD) : {     /* max-latch cell 0x94997         */
        __sup_latch_start = .; *(.sup_latch) . += 0x04; __sup_latch_end = .;
    } > RAM

    /* L1 (2026-11-13): the self-test ladder / sequencer / seq2 / pin-cut /
     * publisher cells at the stock addresses (sup_cells.h, sup_ladder.c),
     * and the 5 ms countdown service's whole range (0x7C7E4, table 0x45D30:
     * words [0x94734, 0x94786), bytes [0x94788, 0x94835)) — it decrements
     * every nonzero cell there, so nothing else may live in it. */
    .sup_l1a 0xFFF93118 (NOLOAD) : { . += 0x0C; } > RAM   /* 0x93118..0x93123 diag   */
    .sup_l1b 0xFFF93936 (NOLOAD) : { . += 0x0C; } > RAM   /* 0x93936..0x93941        */
    .sup_l1c 0xFFF939B4 (NOLOAD) : { . += 0x14; } > RAM   /* 0x939B4..0x939C7 (L3: +0x939C4) */
    .sup_l1d 0xFFF93A84 (NOLOAD) : { . += 0x10; } > RAM   /* 0x93A84..0x93A93        */
    .sup_l1e 0xFFF93AAC (NOLOAD) : { . += 0x06; } > RAM   /* 0x93AAC..0x93AB1        */
    .sup_l1f 0xFFF940F8 (NOLOAD) : { . += 0x64; } > RAM   /* 0x940F8..0x9415B        */
    .sup_cnt 0xFFF94734 (NOLOAD) : { . += 0x101; } > RAM  /* 0x94734..0x94834        */
    .sup_l1g 0xFFF94D48 (NOLOAD) : { . += 0x114; } > RAM  /* 0x94D48..0x94E5B pairs  */
    /* L3 (2026-11-13): the '3' diagnosis 0x85FC4 cells outside the L1 ranges */
    .sup_l3a 0xFFF94958 (NOLOAD) : { . += 0x08; } > RAM   /* 0x94958..0x9495F        */
    .sup_l3b 0xFFF84A24 (NOLOAD) : { . += 0x08; } > RAM   /* 0x84A24..0x84A2B        */
    /* mode 1 (2026-11-13, ~/tcu_sweep/mode1_port.md): the OS mode cells
     * 0x93B32/33/34 (boot state, mode, pending) and 0x848CF / 0x848E6 (the
     * boot-side mode copy and the state-0 gate) */
    .sup_os  0xFFF93B30 (NOLOAD) : { . += 0x10; } > RAM   /* 0x93B30..0x93B3F        */
    .sup_os2 0xFFF848C0 (NOLOAD) : { . += 0x28; } > RAM   /* 0x848C0..0x848E7        */
    .sup_os3 0xFFF9353C (NOLOAD) : { . += 0x24; } > RAM   /* 0x9353C..0x9355F task cd */
    /* our own mode-1 witnesses (not stock cells): placed in the free low
     * RAM below the pinned ranges, so they do not cost the stack guard */
    .sup_wit 0xFFF84C00 (NOLOAD) : {
        __sup_wit_start = .; *(.sup_wit) . = ALIGN(4); __sup_wit_end = .;
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

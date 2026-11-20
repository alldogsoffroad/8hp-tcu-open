/* SH7254R application block (spec 06): 0x040000-0x17FFFF, header at 0x40000.
 * RAM: the boot handover area 0xFFF84000-0xFFF8403F is not used (spec 06 section 5);
 * the application uses 0xFFF84100-0xFFF9FFFF. */
OUTPUT_FORMAT("elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)
/* r04r (~/tcu_sweep/r04r_port.md): the retired holder's .bss span, kept as
 * a pad so .bss / .noinit do not move (app/rq_retired.c) — a GC root */
EXTERN(_tcu_rq_retired_pad)

MEMORY
{
    APPHDR (rx)  : ORIGIN = 0x00040000, LENGTH = 0x80
    APPVEC (rx)  : ORIGIN = 0x00040080, LENGTH = 0x7F0      /* vectors 4..511 */
    APPROM (rx)  : ORIGIN = 0x00040870, LENGTH = 0x0017FB70 - 0x00040870
    APPTAIL(r)   : ORIGIN = 0x0017FB70, LENGTH = 0x8C
    APPEND (r)   : ORIGIN = 0x0017FBFC, LENGTH = 4
    RAM    (rwx) : ORIGIN = 0xFFF84100, LENGTH = 0xFFFA0000 - 0xFFF84100
    /* the stock's published range object 0xFFF8304C (app/rng_chain.c,
     * 0xC79B8): below the application RAM origin, in the boot service
     * window 0xFFF82000-0xFFF83A5F that the stock application overwrites
     * the same way every 10 ms (2026-11-17, range_chain_live_port.md) */
    RNG304C (rw) : ORIGIN = 0xFFF8304C, LENGTH = 0x18
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
    /* M4 (drv_temp.c): the ADC_A direct-scan sums the 1 ms ISR accumulates
     * (stock 0x8F058 tail, 0x900E8 + 4n, n = AN0..AN27) and the AN table the
     * reply proc's 0x8F70C publishes (0x9018C + 2n, mV) */
    .adc_sum   0xFFF900E8 (NOLOAD) : { . += 0x70; } > RAM   /* 0x900E8..0x90157 */
    .adc_an    0xFFF9018C (NOLOAD) : { . += 0x38; } > RAM   /* 0x9018C..0x901C3 */
    /* park position sensor (2026-11-16, ~/tcu_sweep/park_sensor_port.md):
     * the mux-15 accumulator of 0x8F058 / descriptor 0x40904 state —
     * 0x900B0 sum (u32), 0x900B4 count (u16), 0x900B6 status (u8) */
    .adc_m15   0xFFF900B0 (NOLOAD) : { . += 0x08; } > RAM   /* 0x900B0..0x900B7 */

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
    ASSERT(__sup_wit_end <= 0xFFF84C40, ".sup_wit runs into .drv_wit")
    /* r03b, the request law (drv_reqlaw.c; ~/tcu_sweep/request_law_port.md):
     * the stock cells outside .sup_value / .sup_cells, cleared by
     * drv_rq_init — 0x90230 currents + 0x90244 currents-as-sent, 0x90320
     * dither, the per-chip driver records 0x91108 + 0x1C·c, 0x9337C
     * corrections, 0x93978/7A/7C undervoltage / fault / full-on masks,
     * 0x93A12 last correction */
    .drv_rq1 0xFFF90230 (NOLOAD) : { . += 0x28; } > RAM   /* 0x90230..0x90257        */
    .drv_rq2 0xFFF90320 (NOLOAD) : { . += 0x14; } > RAM   /* 0x90320..0x90333        */
    .drv_rec 0xFFF91108 (NOLOAD) : { . += 0x8C; } > RAM   /* 0x91108..0x91193        */
    .drv_rq3 0xFFF9337C (NOLOAD) : { . += 0x14; } > RAM   /* 0x9337C..0x9338F        */
    .drv_rq4 0xFFF93978 (NOLOAD) : { . += 0x08; } > RAM   /* 0x93978..0x9397F (+0x9397E: 0x84392) */
    .drv_rq5 0xFFF93A12 (NOLOAD) : { . += 0x02; } > RAM   /* 0x93A12..0x93A13        */
    /* THE OUTPUT DIAGNOSIS (2026-11-17, bsw/drv/drv_odiag.c; ~/tcu_sweep/
     * output_diag_port.md): the stock cells of 0x904FA (0x9030C chip status),
     * 0x92298 (0x9062C), 0x7B6D8 (0x84890 request slots), 0x83DB0's enable
     * 0x84A22/23, 0x897AC (0x84A2D), 0x7BA2A (0x93004 / 0x93018), 0x8422E
     * (0x930F8 / 0x93100), 0x89F5C (0x93104, 0x93394.., 0x93980..0x939A9,
     * 0x93A..), 0x8A788 (0x93484.., 0x934E8..), 0x94930 / 0x94944 — the rest
     * lie in .sup_cells / .sup_l1c / .sup_l1f / .sup_l1g / .sup_cnt / .sup_os2
     * / .drv_rq4 / .drv_rec; cleared by drv_od_init. .od_93a04 and .od_93a14
     * frame .drv_rq5 (0x93A12); .od_93484 and .od_934e8 frame .pks_rat. */
    .od_9030c 0xFFF9030C (NOLOAD) : { . += 0x14; } > RAM   /* 0x9030C..0x9031F        */
    .od_9062c 0xFFF9062C (NOLOAD) : { . += 0x04; } > RAM   /* 0x9062C..0x9062F        */
    .od_84890 0xFFF84890 (NOLOAD) : { . += 0x14; } > RAM   /* 0x84890..0x848A3        */
    .od_84a22 0xFFF84A22 (NOLOAD) : { . += 0x02; } > RAM   /* 0x84A22..0x84A23        */
    .od_84a2c 0xFFF84A2C (NOLOAD) : { . += 0x04; } > RAM   /* 0x84A2C..0x84A2F        */
    .od_93004 0xFFF93004 (NOLOAD) : { . += 0x28; } > RAM   /* 0x93004..0x9302B        */
    .od_930f8 0xFFF930F8 (NOLOAD) : { . += 0x20; } > RAM   /* 0x930F8..0x93117        */
    .od_93374 0xFFF93374 (NOLOAD) : { . += 0x08; } > RAM   /* 0x93374..0x9337B        */
    .od_93394 0xFFF93394 (NOLOAD) : { . += 0xA0; } > RAM   /* 0x93394..0x93433        */
    .od_93484 0xFFF93484 (NOLOAD) : { . += 0x3C; } > RAM   /* 0x93484..0x934BF        */
    .od_934e8 0xFFF934E8 (NOLOAD) : { . += 0x50; } > RAM   /* 0x934E8..0x93537        */
    .od_93980 0xFFF93980 (NOLOAD) : { . += 0x2A; } > RAM   /* 0x93980..0x939A9        */
    .od_93a04 0xFFF93A04 (NOLOAD) : { . += 0x0E; } > RAM   /* 0x93A04..0x93A11        */
    .od_93a14 0xFFF93A14 (NOLOAD) : { . += 0x52; } > RAM   /* 0x93A14..0x93A65        */
    .od_93b0c 0xFFF93B0C (NOLOAD) : { . += 0x16; } > RAM   /* 0x93B0C..0x93B21        */
    .od_94930 0xFFF94930 (NOLOAD) : { . += 0x28; } > RAM   /* 0x94930..0x94957        */
    /* its witnesses (ours, not stock cells: DSM groups 1–20, the would-be
     * resets, reporter counts): free low RAM 0xFFF87A00..0xFFF87E00 (after
     * .tmp_wit / .kl15_wit). ld does not refuse overlapping NOLOAD output
     * sections, hence the explicit bounds */
    .od_wit 0xFFF87A00 (NOLOAD) : {
        __od_wit_start = .; *(.od_wit) . = ALIGN(4); __od_wit_end = .;
    } > RAM
    ASSERT(__od_wit_start >= 0xFFF87880 && __od_wit_end <= 0xFFF87E00, ".od_wit overflow")
    /* the bus-A ring and the output-cadence witnesses (ours, 2026-11-18,
     * drv_jobs.c; ~/tcu_sweep/output_cadence_port.md): free low RAM
     * 0xFFF87E00..0xFFF88000, below .adc_dma_a 0xFFF88130 */
    .out_wit 0xFFF87E00 (NOLOAD) : {
        __out_wit_start = .; *(.out_wit) . = ALIGN(4); __out_wit_end = .;
    } > RAM
    ASSERT(__out_wit_start >= __od_wit_end && __out_wit_end <= 0xFFF88000, ".out_wit overflow")
    /* M4 group A (drv_groupa.c / drv_temp.c; ~/tcu_sweep/groupA_park_port.md):
     * the stock cells outside .sup_value / .sup_cells / .sup_cnt / .sup_l1f,
     * cleared by drv_ga_init / drv_temp_init — 0x93A66 the switch-output law's
     * previous duty (0x8ADF4), 0x93A9C..0x93AAB the fluid-temperature
     * acquisition 0x8D5BA, 0x9416E/F the 0x887D8 source flags */
    .drv_ga1  0xFFF93A66 (NOLOAD) : { . += 0x0A; } > RAM   /* 0x93A66..0x93A6F        */
    .drv_tmp1 0xFFF93A9C (NOLOAD) : { . += 0x10; } > RAM   /* 0x93A9C..0x93AAB        */
    .drv_tmp2 0xFFF9416E (NOLOAD) : { . += 0x02; } > RAM   /* 0x9416E..0x9416F        */
    /* THE ECU TEMPERATURES' STOCK CELLS, RESERVED (2026-11-18,
     * ~/tcu_sweep/diag_temp_inputs_decode.md / diag_temp_shadow_port.md):
     * nothing of ours writes them yet (app/tsel_shadow.c keeps its own
     * copies in .tsel_shadow) — reserved so nothing else lands on them before
     * a live step: 0x93916 the die fault word (0x7C01E), 0x93AB2..0x93AC3 the
     * die / board producers 0x8DB5A / 0x8DD7C, 0x9415C..0x9415E the die
     * copies + status, 0x94730 the board's "never valid" word (not a
     * countdown). Already inside earlier reservations: 0x9393A..3F
     * (.sup_l1b), 0x940E0..E8 (.tmp_940e0), 0x940FC..0x94101 (.sup_l1f),
     * 0x9477A / 0x9477C / 0x94823 (.sup_cnt — the 0x7C7E4 ranges),
     * 0x93AF0..F7 (.pks_eval, unused by park), 0x94170 (.pks_ev2) */
    .tsel_93916 0xFFF93916 (NOLOAD) : { . += 0x02; } > RAM   /* 0x93916..0x93917      */
    .tsel_93ab2 0xFFF93AB2 (NOLOAD) : { . += 0x12; } > RAM   /* 0x93AB2..0x93AC3      */
    .tsel_9415c 0xFFF9415C (NOLOAD) : { . += 0x03; } > RAM   /* 0x9415C..0x9415E      */
    .tsel_94730 0xFFF94730 (NOLOAD) : { . += 0x04; } > RAM   /* 0x94730..0x94733      */
    ASSERT(ADDR(.tsel_93ab2) >= ADDR(.sup_l1e) + SIZEOF(.sup_l1e) &&
           ADDR(.tsel_93ab2) + SIZEOF(.tsel_93ab2) <= ADDR(.pks_eval), ".tsel_93ab2 overlaps .sup_l1e / .pks_eval")
    ASSERT(ADDR(.tsel_9415c) >= ADDR(.sup_l1f) + SIZEOF(.sup_l1f) &&
           ADDR(.tsel_9415c) + SIZEOF(.tsel_9415c) <= ADDR(.pks_ev1), ".tsel_9415c overlaps .sup_l1f / .pks_ev1")
    ASSERT(ADDR(.tsel_94730) + SIZEOF(.tsel_94730) <= ADDR(.sup_cnt), ".tsel_94730 overlaps .sup_cnt")
    ASSERT(ADDR(.tsel_93916) >= ADDR(.tmp_93914) + SIZEOF(.tmp_93914), ".tsel_93916 overlaps .tmp_93914")
    /* M4 range / park (app/range.c, app/park_law.c, app/park_cells.h;
     * ~/tcu_sweep/range_logic_port.md): the stock cells of the park module,
     * cleared by park_init — 0x851A8 output flags + complements, 0x89C6C /
     * 0x89CDC the target as read / with the diag overrides, 0x8AE28 the
     * direction + park request, 0x8BC70 the park object. (0x8D6D6 the range
     * and 0x8E549 the init arbiter's result now lie in the range chain's
     * .rng_d6ca / .rng_e530, 2026-11-17) */
    .park_out  0xFFF851A8 (NOLOAD) : { . += 0x0A; } > RAM   /* 0x851A8..0x851B1        */
    .park_rng  0xFFF89C6C (NOLOAD) : { . += 0x01; } > RAM   /* 0x89C6C                 */
    .park_rngd 0xFFF89CDC (NOLOAD) : { . += 0x01; } > RAM   /* 0x89CDC                 */
    .park_req  0xFFF8AE28 (NOLOAD) : { . += 0x02; } > RAM   /* 0x8AE28..0x8AE29        */
    .park_obj  0xFFF8BC70 (NOLOAD) : { . += 0x04; } > RAM   /* 0x8BC70..0x8BC73        */
    /* the fault-reaction park request (app/park_law.c park_law_aux, 0xB986C
     * +0xB4): 0x89CF0 = +0xB4, 0x89CF2 / 0x89CF3 its copies (+0xB6 / +0xB7;
     * 0x89CF1 = +0xB5 is not computed, left 0) — 2026-11-17 */
    .park_aux  0xFFF89CF0 (NOLOAD) : { . += 0x04; } > RAM   /* 0x89CF0..0x89CF3        */
    /* THE TERMINAL-15 OBJECT (safety/kl15.c; ~/tcu_sweep/keyoff_autop_port.md):
     * 0xFFF8E9D4..0xFFF8EA04 (0x140B80's object, cleared by kl15_init) and
     * the voltage copy 0xFFF938B4 (0x7A7E8's cell) */
    .kl15_obj  0xFFF8E9D4 (NOLOAD) : { . += 0x31; } > RAM   /* 0x8E9D4..0x8EA04        */
    .kl15_v    0xFFF938B4 (NOLOAD) : { . += 0x02; } > RAM   /* 0x938B4..0x938B5        */
    /* THE RANGE CHAIN (app/rng_chain.c, rng_chain.h; ~/tcu_sweep/
     * range_chain_live_port.md): the stock's cyclic 10 ms range chain, the
     * live owner of D6D6 — its cells at the stock addresses, one object per
     * section (cleared by rng_chain_init). The ASSERTs hold each object to
     * its stock extent */
    .rng_d5e4  0xFFF8D5E4 (NOLOAD) : { KEEP(*(.rng_d5e4)) } > RAM  /* 0x8D5E4..0x8D613 */
    .rng_d61a  0xFFF8D61A (NOLOAD) : { KEEP(*(.rng_d61a)) } > RAM  /* 0x8D61A..0x8D699 */
    .rng_d6ca  0xFFF8D6CA (NOLOAD) : { KEEP(*(.rng_d6ca)) } > RAM  /* 0x8D6CA..0x8D6DF (D6D6) */
    .rng_e530  0xFFF8E530 (NOLOAD) : { KEEP(*(.rng_e530)) } > RAM  /* 0x8E530..0x8E54B (E549) */
    .rng_e572  0xFFF8E572 (NOLOAD) : { KEEP(*(.rng_e572)) } > RAM  /* 0x8E572 */
    .rng_e64b  0xFFF8E64B (NOLOAD) : { KEEP(*(.rng_e64b)) } > RAM  /* 0x8E64B..0x8E68F */
    .rng_e912  0xFFF8E912 (NOLOAD) : { KEEP(*(.rng_e912)) } > RAM  /* 0x8E912..0x8E913 */
    .rng_893e0 0xFFF893E0 (NOLOAD) : { KEEP(*(.rng_893e0)) } > RAM  /* 0x893E0..0x893F7 */
    .rng_89418 0xFFF89418 (NOLOAD) : { KEEP(*(.rng_89418)) } > RAM  /* 0x89418..0x894BB */
    .rng_85160 0xFFF85160 (NOLOAD) : { KEEP(*(.rng_85160)) } > RAM  /* 0x85160..0x85183 */
    .rng_8975c 0xFFF8975C (NOLOAD) : { KEEP(*(.rng_8975c)) } > RAM  /* 0x8975C..0x8975F */
    .rng_304c  0xFFF8304C (NOLOAD) : { KEEP(*(.rng_304c)) } > RNG304C /* 0x8304C..0x83063 */
    ASSERT(SIZEOF(.rng_d5e4) == 0x30 && SIZEOF(.rng_d61a) == 0x80 && SIZEOF(.rng_d6ca) == 0x16 &&
           SIZEOF(.rng_e530) == 0x1C && SIZEOF(.rng_e572) == 0x01 && SIZEOF(.rng_e64b) == 0x45 &&
           SIZEOF(.rng_e912) == 0x02 && SIZEOF(.rng_893e0) == 0x18 && SIZEOF(.rng_89418) == 0xA4 &&
           SIZEOF(.rng_85160) == 0x24 && SIZEOF(.rng_8975c) == 0x04 && SIZEOF(.rng_304c) == 0x18,
           "a range-chain object is not its stock extent")
    ASSERT(_rc_d6ca == 0xFFF8D6CA && _rc_e530 == 0xFFF8E530 && _rc_304c == 0xFFF8304C &&
           _rc_d5e4 == 0xFFF8D5E4 && _rc_89418 == 0xFFF89418, "a range-chain object is off its stock address")
    /* park position sensor, READ-ONLY (2026-11-16; bsw/drv/drv_pksens.c,
     * safety/park_pair.c; ~/tcu_sweep/park_sensor_port.md): the stock cells
     * of the switch evaluator 0x8DF2E / monitor 0x88EF0 / copy 0x7B150 and
     * the safety-layer pair object 0xFFF8EA28 that lie outside .sup_value /
     * .sup_l1g / .sup_cells / .sup_cnt / .drv_tmp2; cleared and initialised
     * by drv_pksens_init / park_pair_init — 0x847F4 state/fault copies (the
     * stock's initialised-data block), 0x8EA28 the pair object, 0x92FE4 the
     * monitor's tested flags + error words, 0x934C4 ratios / error words /
     * tested words, 0x93942 the latched sensor-supply reference, 0x93AC4
     * states / complements / digital bits / bands / latch / previous bands,
     * 0x9415F..0x9416D and 0x94170..0x94173 the evaluator's bytes (0x9416E/F
     * are .drv_tmp2's, 0x887D8's own) */
    .pks_cp    0xFFF847F4 (NOLOAD) : { . += 0x10; } > RAM   /* 0x847F4..0x84803        */
    .pkp_obj   0xFFF8EA28 (NOLOAD) : { . += 0x24; } > RAM   /* 0x8EA28..0x8EA4B        */
    .pks_mon   0xFFF92FE4 (NOLOAD) : { . += 0x0C; } > RAM   /* 0x92FE4..0x92FEF        */
    .pks_rat   0xFFF934C4 (NOLOAD) : { . += 0x24; } > RAM   /* 0x934C4..0x934E7        */
    .pks_ref   0xFFF93942 (NOLOAD) : { . += 0x02; } > RAM   /* 0x93942..0x93943        */
    .pks_eval  0xFFF93AC4 (NOLOAD) : { . += 0x40; } > RAM   /* 0x93AC4..0x93B03        */
    .pks_ev1   0xFFF9415F (NOLOAD) : { . += 0x0F; } > RAM   /* 0x9415F..0x9416D        */
    .pks_ev2   0xFFF94170 (NOLOAD) : { . += 0x04; } > RAM   /* 0x94170..0x94173        */
    /* our park-sensor witnesses (fatal counters, group-98 verdict; not stock
     * cells): free low RAM like .drv_wit, so they do not cost the stack guard */
    .pk_wit 0xFFF84D00 (NOLOAD) : {
        __pk_wit_start = .; *(.pk_wit) . = ALIGN(4); __pk_wit_end = .;
    } > RAM
    ASSERT(__pk_wit_end <= 0xFFF84D40, ".pk_wit overflow")

    /* M5-0, the stock request producer (app/rq_prod.c; ~/tcu_sweep/
     * m5_0_port.md): the stock cells, cleared by rq_prod_init — 0x84B78 the
     * actuator currents + complements at +0x18 (0xD9F70), 0x8B154 the seven
     * actuator objects (0x15BF2C: 5 × 0x28 + 2 × 0x10), 0x8B7C8 the request
     * objects A..E (5 × 0x60), Sys 0x8B9A8, WK 0x8B9B8 (0x10 each); and the
     * WRITE-ONLY mirrors of the compile-time inputs 0x89C2B (gear code),
     * 0x89C30 (power flow), 0x89CF5 (Sys mode), 0x8BCB0 (line pressure) */
    .m5_cur  0xFFF84B78 (NOLOAD) : { . += 0x30; } > RAM   /* 0x84B78..0x84BA7        */
    .m5_act  0xFFF8B154 (NOLOAD) : { . += 0xE8; } > RAM   /* 0x8B154..0x8B23B        */
    .m5_req  0xFFF8B7C8 (NOLOAD) : { . += 0x200; } > RAM  /* 0x8B7C8..0x8B9C7        */
    .m5_in1  0xFFF89C2B (NOLOAD) : { . += 0x01; } > RAM   /* 0x89C2B                 */
    .m5_in2  0xFFF89C30 (NOLOAD) : { . += 0x01; } > RAM   /* 0x89C30                 */
    .m5_in3  0xFFF89CF5 (NOLOAD) : { . += 0x01; } > RAM   /* 0x89CF5                 */
    .m5_in4  0xFFF8BCB0 (NOLOAD) : { . += 0x02; } > RAM   /* 0x8BCB0..0x8BCB1        */
    /* our M5-0 witnesses (guard, faults, unported branches; not stock cells) */
    /* M5-1 (2026-11-16, ~/tcu_sweep/m5_1_fill_port.md): the element fill's
     * stock cells (0x851A2/3 fill bits, 0x851A5, 0xFFF8E384), the input
     * mirrors 0x89C13 / 0x89D10, and OUR fill recorder (2 KB of stock RAM
     * our firmware does not use; .noinit has no room under the stack guard) */
    .m5_fl   0xFFF851A2 (NOLOAD) : { . += 0x04; } > RAM   /* 0x851A2..0x851A5        */
    .m5_e384 0xFFF8E384 (NOLOAD) : { . += 0x01; } > RAM   /* 0x8E384                 */
    .m5_in5  0xFFF89C13 (NOLOAD) : { . += 0x01; } > RAM   /* 0x89C13                 */
    .m5_in6  0xFFF89D10 (NOLOAD) : { . += 0x01; } > RAM   /* 0x89D10                 */
    /* M5, the temperature byte live (2026-11-17; drv_temp.c, safety/
     * tmp_safe.c, app/tmp_chain.c; ~/tcu_sweep/temp_byte_live_port.md):
     * 0xB9646's two copies, the safety layer's objects 0xFFF8E998 (+0x2C..
     * +0x35) and 0xFFF8ED20 (+0xB8..+0xD8), 0x7BF8C's fault word and the
     * published bytes 0x940E0..0x940E8 (0x93AE8/EA lie in .pks_eval) */
    .tmp_89c57 0xFFF89C57 (NOLOAD) : { . += 0x01; } > RAM  /* 0x89C57                 */
    /* G5b (2026-11-19, ~/tcu_sweep/g5b_port.md; safety/os_sel.c): slot 26's
     * one-hot OS-mode flags at the stock addresses (GBR 0x89C3C +0 / +1 /
     * +2 and +0x17 / +0x18 / +0x19), cleared by os_sel_init. Free in the
     * r04t map (nm + the section list); the neighbours are asserted below */
    .os_flg1 0xFFF89C3C (NOLOAD) : { . += 0x03; } > RAM   /* 0x89C3C..0x89C3E        */
    .os_flg2 0xFFF89C53 (NOLOAD) : { . += 0x03; } > RAM   /* 0x89C53..0x89C55        */
    ASSERT(ADDR(.os_flg1) >= ADDR(.m5_in2) + SIZEOF(.m5_in2) &&
           ADDR(.os_flg2) >= ADDR(.os_flg1) + SIZEOF(.os_flg1) &&
           ADDR(.os_flg2) + SIZEOF(.os_flg2) <= ADDR(.tmp_89c57), ".os_flg1 / .os_flg2 overlap a neighbour")
    .tmp_89cd2 0xFFF89CD2 (NOLOAD) : { . += 0x01; } > RAM  /* 0x89CD2                 */
    .tmp_e9c4  0xFFF8E9C4 (NOLOAD) : { . += 0x0A; } > RAM  /* 0x8E9C4..0x8E9CD        */
    .tmp_edd8  0xFFF8EDD8 (NOLOAD) : { . += 0x21; } > RAM  /* 0x8EDD8..0x8EDF8        */
    .tmp_93914 0xFFF93914 (NOLOAD) : { . += 0x02; } > RAM  /* 0x93914..0x93915        */
    .tmp_940e0 0xFFF940E0 (NOLOAD) : { . += 0x09; } > RAM  /* 0x940E0..0x940E8        */
    .m5_rec  0xFFF86000 (NOLOAD) : {
        __m5_rec_start = .; *(.m5_rec) . = ALIGN(4); __m5_rec_end = .;
    } > RAM
    ASSERT(__m5_rec_end <= 0xFFF86800, ".m5_rec overflow (its upper 0x200 B is .sm_shadow since 2026-11-19)")
    /* THE SHIFT-MAP SHADOW (app/sm_shadow.c, ours — not stock cells;
     * 2026-11-19, ~/tcu_sweep/sm_shadow_port.md): struct sm_t (0x120) at
     * 0xFFF86800, in the retired M5 recorder's window (.m5_rec, empty since
     * r04r) below .fb_shadow 0xFFF86A00 — stock RAM the driving dump holds
     * all zero (0xFFF85400..0xFFF88100). The TU's zero-initialised statics
     * land here too, so .bss does not move. Cleared by sm_shadow_init. */
    .sm_shadow 0xFFF86800 (NOLOAD) : {
        __sm_shadow_start = .; *(.sm_shadow) *sm_shadow.c.obj(.bss .bss.* COMMON)
        . = ALIGN(4); __sm_shadow_end = .;
    } > RAM
    ASSERT(__sm_shadow_start >= __m5_rec_end, ".sm_shadow overlaps .m5_rec")
    ASSERT(__sm_shadow_end <= 0xFFF86A00, ".sm_shadow runs into .fb_shadow")
    ASSERT(__sm_shadow_start == 0xFFF86800 && SIZEOF(.sm_shadow) == 0x120, ".sm_shadow: sm_t is off 0xFFF86800 (witness_sm.py)")
    /* THE GEAR FEEDBACK CHAIN of the shift-map shadow (app/sm_gear.c, ours;
     * 2026-11-19, ~/tcu_sweep/gear_feedback_shadow_port.md): struct gf_t
     * (0x9C) right after .sm_shadow, still below .fb_shadow 0xFFF86A00, in
     * the same dump-zero window. Cleared by sm_gear_init. */
    .sm_gear 0xFFF86920 (NOLOAD) : {
        __sm_gear_start = .; *(.sm_gear) *sm_gear.c.obj(.bss .bss.* COMMON)
        . = ALIGN(4); __sm_gear_end = .;
    } > RAM
    ASSERT(__sm_gear_start >= __sm_shadow_end, ".sm_gear overlaps .sm_shadow")
    ASSERT(__sm_gear_end <= 0xFFF86A00, ".sm_gear runs into .fb_shadow")
    ASSERT(__sm_gear_start == 0xFFF86920 && SIZEOF(.sm_gear) == 0x9C, ".sm_gear: gf_t is off 0xFFF86920 (witness_sm.py)")
    .m5_wit 0xFFF84D40 (NOLOAD) : {
        __m5_wit_start = .; *(.m5_wit) . = ALIGN(4); __m5_wit_end = .;
    } > RAM
    ASSERT(__m5_wit_end <= 0xFFF84D80, ".m5_wit overflow")

    /* the range chain's recorder, witnesses and fault-manager model
     * (app/rng_chain.c, ours — not stock cells; the r03q..r03s shadow block,
     * 2026-11-17): free low RAM in 0xFFF86A00..0xFFF88130; cleared by
     * rng_chain_init; it does not cost the stack guard */
    .rng_wit 0xFFF87000 (NOLOAD) : {
        __rng_wit_start = .; *(.rng_wit) . = ALIGN(4); __rng_wit_end = .;
    } > RAM
    ASSERT(__rng_wit_end <= 0xFFF87800, ".rng_wit overflow")
    /* the temperature byte's task phases (app/tmp_chain.c, ours): free low
     * RAM after .rng_wit; set by tmp_chain_init */
    .tmp_wit 0xFFF87800 (NOLOAD) : {
        __tmp_wit_start = .; *(.tmp_wit) . = ALIGN(4); __tmp_wit_end = .;
    } > RAM
    ASSERT(__tmp_wit_end <= 0xFFF87840, ".tmp_wit overflow")
    /* the terminal-15 witnesses and the agreement shadow (safety/kl15.c,
     * ours): free low RAM after .tmp_wit */
    .kl15_wit 0xFFF87840 (NOLOAD) : {
        __kl15_wit_start = .; *(.kl15_wit) . = ALIGN(4); __kl15_wit_end = .;
    } > RAM
    ASSERT(__kl15_wit_end <= 0xFFF87880, ".kl15_wit overflow")

    /* G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): the garage-engagement shadow
     * (instance 1: .gs_req 0xFFF911A0, .gs_obj 0xFFF91440, .gs_rec
     * 0xFFF8EE00, .gs_rel 0xFFF8872C) is deleted (D-G6-7); its RAM holds the
     * monitor's two recorders now (.gsb_rec0 / .gsb_rec1, below), which had
     * to leave the stock cells they sat on (0xFFF8BCC0.., 0xFFF89D20..).
     *
     * THE ENGINE'S STOCK OBJECTS AT THE STOCK ADDRESSES (D-G6-9;
     * app/gs_engine.c gse_* types, every field at its stock offset, the
     * bytes between them padding of ours; binding 2 = app/gs_cand.c, the
     * only engine in the image). Each in its own NOLOAD section; cleared by
     * gs_cand_init. ld does NOT refuse overlapping NOLOAD sections: the
     * ASSERTs hold each object between its neighbours, and the post-build
     * section scan (tests/unit/g6c_sections.py) checks every allocated RAM
     * section of the image pairwise. */
    .gse_bd14 0xFFF8BD14 (NOLOAD) : { KEEP(*(.gse_bd14)) } > RAM   /* 0x8BD14..0x8BEB3 (+ block A 0x8BD6C) */
    /* r04s (~/tcu_sweep/r04s_port.md): the element sub-objects D0DC / D148 /
     * D1B4 (0x6C each, stock layout) and the {idx, blk} cells their +0x58 /
     * +0x5C / +0x60 point to (three cells of 8 per group; the groups'
     * stock neighbours 0x8CE74 / 0x8CF04 / 0x8CF8C belong to other objects
     * and are not ours). The pointer values the engine stores ARE these
     * addresses: each ADDR is asserted equal to the stock value. */
    .gse_pr60 0xFFF8CE7C (NOLOAD) : { KEEP(*(.gse_pr60)) } > RAM   /* 0x8CE7C..0x8CE93 */
    .gse_pr5c 0xFFF8CF0C (NOLOAD) : { KEEP(*(.gse_pr5c)) } > RAM   /* 0x8CF0C..0x8CF23 */
    .gse_pr58 0xFFF8CF94 (NOLOAD) : { KEEP(*(.gse_pr58)) } > RAM   /* 0x8CF94..0x8CFAB */
    .gse_d0dc 0xFFF8D0DC (NOLOAD) : { KEEP(*(.gse_d0dc)) } > RAM   /* 0x8D0DC..0x8D21F */
    .gse_e28e 0xFFF8E28E (NOLOAD) : { KEEP(*(.gse_e28e)) } > RAM   /* 0x8E28E..0x8E297 */
    .gse_e33c 0xFFF8E33C (NOLOAD) : { KEEP(*(.gse_e33c)) } > RAM   /* 0x8E33C..0x8E34F (r04s: + E34E) */
    .gse_e4a7 0xFFF8E4A7 (NOLOAD) : { KEEP(*(.gse_e4a7)) } > RAM   /* 0x8E4A7 */
    .gse_e4ac 0xFFF8E4AC (NOLOAD) : { KEEP(*(.gse_e4ac)) } > RAM   /* 0x8E4AC..0x8E4B5 */
    .gse_e69b 0xFFF8E69B (NOLOAD) : { KEEP(*(.gse_e69b)) } > RAM   /* 0x8E69B */
    .gse_e69f 0xFFF8E69F (NOLOAD) : { KEEP(*(.gse_e69f)) } > RAM   /* 0x8E69F */
    .gse_e6a1 0xFFF8E6A1 (NOLOAD) : { KEEP(*(.gse_e6a1)) } > RAM   /* 0x8E6A1 */
    .gse_e6a8 0xFFF8E6A8 (NOLOAD) : { KEEP(*(.gse_e6a8)) } > RAM   /* 0x8E6A8..0x8E6AC */
    .gse_e6c1 0xFFF8E6C1 (NOLOAD) : { KEEP(*(.gse_e6c1)) } > RAM   /* 0x8E6C1..0x8E6C2 (r04s: E6BF +2/+3, 0xDC0F8) */
    .gse_e6c5 0xFFF8E6C5 (NOLOAD) : { KEEP(*(.gse_e6c5)) } > RAM   /* 0x8E6C5 */
    .gse_e6ee 0xFFF8E6EE (NOLOAD) : { KEEP(*(.gse_e6ee)) } > RAM   /* 0x8E6EE */
    .gse_e6ef 0xFFF8E6EF (NOLOAD) : { KEEP(*(.gse_e6ef)) } > RAM   /* 0x8E6EF */
    ASSERT(SIZEOF(.gse_pr60) == 0x18 && SIZEOF(.gse_pr5c) == 0x18 && SIZEOF(.gse_pr58) == 0x18 &&
           SIZEOF(.gse_d0dc) == 0x144 && SIZEOF(.gse_e6c1) == 2,
           ".gse_pr* / .gse_d0dc / .gse_e6c1: not the stock size (app/gs_engine.c gs_sub_t / gse_pair_t)")
    ASSERT(ADDR(.gse_pr60) == 0xFFF8CE7C && ADDR(.gse_pr5c) == 0xFFF8CF0C && ADDR(.gse_pr58) == 0xFFF8CF94 &&
           ADDR(.gse_d0dc) == 0xFFF8D0DC, ".gse_pr* / .gse_d0dc: the stored pointers must equal the stock cells")
    ASSERT(ADDR(.gse_pr60) >= ADDR(.gse_bd14) + SIZEOF(.gse_bd14) &&
           ADDR(.gse_pr60) + SIZEOF(.gse_pr60) <= 0xFFF8CE94 &&
           ADDR(.gse_pr5c) >= ADDR(.gse_pr60) + SIZEOF(.gse_pr60) &&
           ADDR(.gse_pr5c) + SIZEOF(.gse_pr5c) <= 0xFFF8CF24 &&
           ADDR(.gse_pr58) >= ADDR(.gse_pr5c) + SIZEOF(.gse_pr5c) &&
           ADDR(.gse_pr58) + SIZEOF(.gse_pr58) <= 0xFFF8CFAC &&
           ADDR(.gse_d0dc) >= ADDR(.gse_pr58) + SIZEOF(.gse_pr58) &&
           ADDR(.gse_d0dc) + SIZEOF(.gse_d0dc) <= 0xFFF8D220 &&
           ADDR(.gse_d0dc) + SIZEOF(.gse_d0dc) <= ADDR(.rng_d5e4),
           ".gse_pr60 / pr5c / pr58 / d0dc overlap .gse_bd14 / each other / their stock groups / .rng_d5e4")
    ASSERT(SIZEOF(.gse_bd14) == 0x1A0 && SIZEOF(.gse_e28e) == 0x0A && SIZEOF(.gse_e33c) == 0x14 &&
           SIZEOF(.gse_e4a7) == 1 && SIZEOF(.gse_e4ac) == 0x0A && SIZEOF(.gse_e69b) == 1 &&
           SIZEOF(.gse_e69f) == 1 && SIZEOF(.gse_e6a1) == 1 && SIZEOF(.gse_e6a8) == 5 &&
           SIZEOF(.gse_e6c5) == 1 && SIZEOF(.gse_e6ee) == 1 && SIZEOF(.gse_e6ef) == 1,
           ".gse_*: an engine object is not the size of its stock layout (app/gs_engine.c gse_*)")
    ASSERT(ADDR(.gse_bd14) >= ADDR(.m5_in4) + SIZEOF(.m5_in4) &&
           ADDR(.gse_bd14) + SIZEOF(.gse_bd14) <= 0xFFF8BEB4 &&
           ADDR(.gse_bd14) + SIZEOF(.gse_bd14) <= ADDR(.rng_d5e4), ".gse_bd14 overlaps .m5_in4 / .rng_d5e4 or block B")
    ASSERT(ADDR(.gse_e28e) + SIZEOF(.gse_e28e) <= 0xFFF8E298 &&
           ADDR(.gse_e33c) >= ADDR(.gse_e28e) + SIZEOF(.gse_e28e) &&
           ADDR(.gse_e33c) + SIZEOF(.gse_e33c) <= ADDR(.m5_e384) &&
           ADDR(.gse_e4a7) >= ADDR(.m5_e384) + SIZEOF(.m5_e384) &&
           ADDR(.gse_e4ac) >= ADDR(.gse_e4a7) + SIZEOF(.gse_e4a7) &&
           ADDR(.gse_e4ac) + SIZEOF(.gse_e4ac) <= ADDR(.rng_e530),
           ".gse_e28e / e33c / e4a7 / e4ac overlap E298 / .m5_e384 / .rng_e530")
    ASSERT(ADDR(.gse_e69b) >= ADDR(.rng_e64b) + SIZEOF(.rng_e64b) &&
           ADDR(.gse_e69f) > ADDR(.gse_e69b) && ADDR(.gse_e6a1) > ADDR(.gse_e69f) &&
           ADDR(.gse_e6a8) > ADDR(.gse_e6a1) && ADDR(.gse_e6c1) >= ADDR(.gse_e6a8) + SIZEOF(.gse_e6a8) &&
           ADDR(.gse_e6c5) >= ADDR(.gse_e6c1) + SIZEOF(.gse_e6c1) &&
           ADDR(.gse_e6ee) > ADDR(.gse_e6c5) && ADDR(.gse_e6ef) > ADDR(.gse_e6ee) &&
           ADDR(.gse_e6ef) + SIZEOF(.gse_e6ef) <= ADDR(.kl15_obj),
           ".gse_e69b .. e6ef overlap .rng_e64b / each other / .kl15_obj")

    /* G6a — THE CANDIDATE (app/gs_cand.c + app/gs_cand_fill.c, instance 2 of
     * the garage engine template; ours, not stock cells; 2026-11-19,
     * ~/tcu_sweep/g6a_port.md): its request objects (7 x 0x60, addressed by
     * number: app/gs_cand_bind.h GSC_REQ_BASE) and every static of both
     * candidate TUs (no recorders: GSE_REC 0) in 0xFFF925C0..0xFFF92C68 —
     * stock RAM the driving dump holds all zero (0xFFF925BE..0xFFF92C68)
     * and no code of ours names (literal scan at the port), between
     * .sup_era and .pks_mon. Cleared by gs_cand_init (__gsc_obj_start..end;
     * NOLOAD is not cleared by start.S); .bss does not move. */
    .gsc_req 0xFFF925C0 (NOLOAD) : { __gsc_req_start = .; . += 0x2A0; __gsc_req_end = .; } > RAM
    .gsc_obj 0xFFF92860 (NOLOAD) : {
        __gsc_obj_start = .; *(.gsc_obj)
        *gs_cand.c.obj(.bss .bss.* COMMON) *gs_cand_fill.c.obj(.bss .bss.* COMMON)
        . = ALIGN(4); __gsc_obj_end = .;
    } > RAM
    ASSERT(__gsc_req_start == 0xFFF925C0 && __gsc_req_end == __gsc_obj_start,
           ".gsc_req is off app/gs_cand_bind.h GSC_REQ_BASE")
    ASSERT(__gsc_req_start >= ADDR(.sup_era) + SIZEOF(.sup_era), ".gsc_req overlaps .sup_era")
    ASSERT(__gsc_obj_end <= 0xFFF92C68, ".gsc_obj leaves the dump's zero run (0xFFF925BE..0xFFF92C68)")
    ASSERT(__gsc_obj_end <= ADDR(.pks_mon), ".gsc_obj runs into .pks_mon")
    /* G6a — THE COMPARATOR (app/g6a.c; ours): g6a_t (0x3A4; G6b 0x3F0: + the CPU block) in
     * 0xFFF93C08..0xFFF94000 — the dump's zero run 0xFFF93C04..0xFFF94006,
     * no code of ours names it (literal scan at the port), after .sup_fsm
     * and below .tmp_940e0. Cleared by g6a_init. */
    .g6a 0xFFF93C08 (NOLOAD) : {
        __g6a_start = .; KEEP(*(.g6a)) *g6a.c.obj(.bss .bss.* COMMON) . = ALIGN(4); __g6a_end = .;
    } > RAM
    ASSERT(__g6a_start >= ADDR(.sup_fsm) + SIZEOF(.sup_fsm) && __g6a_end <= 0xFFF94000,
           ".g6a overlaps .sup_fsm or leaves the dump's zero run")
    ASSERT(__g6a_end <= ADDR(.tmp_940e0), ".g6a runs into .tmp_940e0")
    ASSERT(SIZEOF(.g6a) == 0x3F0, ".g6a: g6a_t is off (tools/bench/witness_g6a.py)")

    /* THE GARAGE BRIDGE, STAGE G1 (app/gs_bridge.c; ours — not stock cells;
     * 2026-11-18, ~/tcu_sweep/garage_g1_port.md). Four NOLOAD objects:
     *   .gsb_win   gsb_win_t (20 B) at 0xFFF87200 — the .noinit windows: NOT
     *              cleared on a warm boot (gs_bridge_init(cold) decides), so
     *              they survive an MCU reset like .noinit; a power-on clears;
     *   .gsb_state gsb_t (212 B since G6b: + the monitor block; 224 B since r04q: + the boot
     *              allowance and the stall fence's pass count — ends at 0xFFF87300 = .gsb_rel)
     *              + the TU's zero-initialised data, after it;
     *              both in the free low RAM 0xFFF87200..0xFFF87800 between
     *              .rng_wit (ASSERTed <= 0xFFF87800) and .tmp_wit (0xFFF87800)
     *              — the stock driving dump holds zeros there;
     *   .gsb_rec0  112 x 36 B (the last D) at 0xFFF911A0 (G6c; r04o 0xFFF8BCC0);
     *   .gsb_rec1  112 x 36 B (the last R) at 0xFFF8EE00 (G6c; r04o 0xFFF89D20) —
     *              the deleted shadow's RAM (G6c), our firmware names no
     *              other cell there.
     * ld does NOT refuse overlapping NOLOAD sections: the ASSERTs hold each. */
    .gsb_win 0xFFF87200 (NOLOAD) : {
        __gsb_win_start = .; KEEP(*(.gsb_win)) . = ALIGN(4); __gsb_win_end = .;
    } > RAM
    .gsb_state 0xFFF87220 (NOLOAD) : {
        __gsb_state_start = .; KEEP(*(.gsb_state)) *gs_bridge.c.obj(.bss .bss.* COMMON)
        . = ALIGN(4); __gsb_state_end = .;
    } > RAM
    ASSERT(__gsb_win_start >= __rng_wit_end && __gsb_win_end <= __gsb_state_start, ".gsb_win overlaps")
    ASSERT(__gsb_state_end <= ADDR(.tmp_wit), ".gsb_state runs into .tmp_wit")
    ASSERT(SIZEOF(.gsb_win) == 0x14 && SIZEOF(.gsb_state) == 0xE0, ".gsb_*: gsb_win_t / gsb_t are off (witness_g1.py)")
    .gsb_rec0 0xFFF911A0 (NOLOAD) : {
        __gsb_rec0_start = .; KEEP(*(.gsb_rec0)) . = ALIGN(4); __gsb_rec0_end = .;
    } > RAM
    .gsb_rec1 0xFFF8EE00 (NOLOAD) : {
        __gsb_rec1_start = .; KEEP(*(.gsb_rec1)) . = ALIGN(4); __gsb_rec1_end = .;
    } > RAM
    /* G6c: off the stock cells (BD14.. / 0x89D5E.. 0xFFF8A004 / 0xFFF8A697, plan
     * R10) into the deleted shadow's RAM: rec0 where .gs_req / .gs_obj were
     * (0xFFF911A0..0xFFF92320), rec1 where .gs_rec was (0xFFF8EE00..) */
    ASSERT(__gsb_rec0_start >= ADDR(.drv_rec) + SIZEOF(.drv_rec) && __gsb_rec0_end <= ADDR(.sup_era),
           ".gsb_rec0 overlaps .drv_rec / .sup_era")
    ASSERT(__gsb_rec1_start >= ADDR(.tmp_edd8) + SIZEOF(.tmp_edd8) && __gsb_rec1_end <= ADDR(.adc_cells),
           ".gsb_rec1 overlaps .tmp_edd8 / .adc_cells")
    ASSERT(SIZEOF(.gsb_rec0) == 0xFC0 && SIZEOF(.gsb_rec1) == 0xFC0, ".gsb_rec*: 112 x gsb_rec_t (witness_g1.py)")
    /* STAGE G4b (2026-11-19, ~/tcu_sweep/g4b_release_bridge_port.md; ours,
     * not stock cells): the bridged release's state + recorder gsbr_t
     * (0x450 B) at 0xFFF87300 in the free low RAM after .gsb_state (ends
     * 0xFFF872CC) and below .tmp_wit (0xFFF87800); its estimate record
     * gsbr_est_t (0x1F4 B) at 0xFFF86D50 after .fb_shadow (ends 0xFFF86D4C)
     * and below .rng_wit (0xFFF87000) — both inside the stock driving
     * dump's all-zero run 0xFFF85400..0xFFF88100 (.fb_rec's note).
     * Cleared by gs_bridge_init (cold and warm boots alike). */
    .gsb_rel 0xFFF87300 (NOLOAD) : {
        __gsb_rel_start = .; KEEP(*(.gsb_rel)) . = ALIGN(4); __gsb_rel_end = .;
    } > RAM
    .gsb_rel_est 0xFFF86D50 (NOLOAD) : {
        __gsb_rel_est_start = .; KEEP(*(.gsb_rel_est)) . = ALIGN(4); __gsb_rel_est_end = .;
    } > RAM
    ASSERT(__gsb_rel_start >= __gsb_state_end && __gsb_rel_end <= ADDR(.tmp_wit), ".gsb_rel overlaps .gsb_state / .tmp_wit")
    ASSERT(__gsb_rel_est_start >= ADDR(.fb_shadow) + SIZEOF(.fb_shadow) && __gsb_rel_est_end <= ADDR(.rng_wit),
           ".gsb_rel_est overlaps .fb_shadow / .rng_wit")
    ASSERT(SIZEOF(.gsb_rel) == 0x450 && SIZEOF(.gsb_rel_est) == 0x1F4, ".gsb_rel*: gsbr_t / gsbr_est_t are off (tcu_bench.py gs-check)")

    /* THE PWM-FEEDBACK SHADOW (app/fb_shadow.c; ours — not stock cells;
     * 2026-11-18, ~/tcu_sweep/feedback_shadow_port.md): struct fbs_t (0x34C)
     * in free low RAM 0xFFF86A00..0xFFF87000 between .m5_rec (ASSERTed
     * <= 0xFFF86A00) and .rng_wit (0xFFF87000); the TU's zero-initialised
     * statics land here too, so .bss does not move. Cleared by
     * fb_shadow_init. */
    .fb_shadow 0xFFF86A00 (NOLOAD) : {
        __fb_shadow_start = .; *(.fb_shadow) *fb_shadow.c.obj(.bss .bss.* COMMON)
        . = ALIGN(4); __fb_shadow_end = .;
    } > RAM
    ASSERT(__fb_shadow_start >= __m5_rec_end && __fb_shadow_start >= __sm_shadow_end && __fb_shadow_start >= __sm_gear_end, ".fb_shadow overlaps .m5_rec / .sm_shadow / .sm_gear")
    ASSERT(__fb_shadow_end <= ADDR(.rng_wit), ".fb_shadow runs into .rng_wit")
    ASSERT(__fb_shadow_start == 0xFFF86A00 && SIZEOF(.fb_shadow) == 0x34C, ".fb_shadow: fbs_t is off 0xFFF86A00 (witness_fbshadow.py)")

    /* THE FEEDBACK DISAGREEMENT RECORDER (app/fb_shadow.c, ours — not a
     * stock cell; 2026-11-18, ~/tcu_sweep/r04d_recorder_pa7_shadow.md):
     * struct fbr_t (0xBE8) in 0xFFF85400..0xFFF86000 — stock RAM that the
     * stock driving dump shows all zero (0xFFF85400..0xFFF88100) and that
     * no section of ours uses (.park_out ends 0xFFF851B2, .m5_rec starts
     * 0xFFF86000). Cleared by fb_shadow_init; costs no stack guard. */
    .fb_rec 0xFFF85400 (NOLOAD) : {
        __fb_rec_start = .; *(.fb_rec) . = ALIGN(4); __fb_rec_end = .;
    } > RAM
    ASSERT(__fb_rec_start >= ADDR(.park_out) + SIZEOF(.park_out), ".fb_rec overlaps .park_out")
    ASSERT(__fb_rec_end <= ADDR(.m5_rec), ".fb_rec runs into .m5_rec")
    ASSERT(__fb_rec_start == 0xFFF85400 && SIZEOF(.fb_rec) == 0xBE8, ".fb_rec: fbr_t is off 0xFFF85400 (witness_fbshadow.py)")

    /* THE PA7 SHADOW (app/pa7_shadow.c, ours — not stock cells; 2026-11-18,
     * ~/tcu_sweep/r04d_recorder_pa7_shadow.md): struct pa7s_t (0x124) in the
     * free low RAM 0xFFF87880..0xFFF87A00 between .kl15_wit (ASSERTed
     * <= 0xFFF87880) and .od_wit (0xFFF87A00). Cleared by pa7_shadow_init. */
    .pa7_shadow 0xFFF87880 (NOLOAD) : {
        __pa7_shadow_start = .; *(.pa7_shadow) *pa7_shadow.c.obj(.bss .bss.* COMMON)
        . = ALIGN(4); __pa7_shadow_end = .;
    } > RAM
    ASSERT(__pa7_shadow_start >= __kl15_wit_end, ".pa7_shadow overlaps .kl15_wit")
    ASSERT(__pa7_shadow_end <= __od_wit_start, ".pa7_shadow runs into .od_wit")
    ASSERT(__pa7_shadow_start == 0xFFF87880 && SIZEOF(.pa7_shadow) == 0x124, ".pa7_shadow: pa7s_t is off 0xFFF87880 (witness_pa7.py)")

    /* THE ECU-TEMPERATURE SHADOW (app/tsel_shadow.c, ours — not stock cells;
     * 2026-11-18, ~/tcu_sweep/diag_temp_shadow_port.md): struct tsel_t
     * (0x148) in the free low RAM 0xFFF87C40..0xFFF87E00 between .od_wit
     * (ends 0xFFF87C38) and .out_wit (0xFFF87E00) — the stock driving dump
     * holds zeros there. Cleared by tsel_shadow_init. */
    .tsel_shadow 0xFFF87C40 (NOLOAD) : {
        __tsel_shadow_start = .; *(.tsel_shadow) *tsel_shadow.c.obj(.bss .bss.* COMMON)
        . = ALIGN(4); __tsel_shadow_end = .;
    } > RAM
    ASSERT(__tsel_shadow_start >= __od_wit_end, ".tsel_shadow overlaps .od_wit")
    ASSERT(__tsel_shadow_end <= __out_wit_start, ".tsel_shadow runs into .out_wit")
    ASSERT(__tsel_shadow_start == 0xFFF87C40 && SIZEOF(.tsel_shadow) == 0x148, ".tsel_shadow: tsel_t is off 0xFFF87C40 (witness_temps.py)")

    /* our request-law witnesses and the refresh snapshot (not stock cells;
     * free low RAM like .sup_wit, so they do not cost the stack guard) */
    .drv_wit 0xFFF84C40 (NOLOAD) : {
        __drv_wit_start = .; *(.drv_wit) . = ALIGN(4); __drv_wit_end = .;
    } > RAM
    ASSERT(__drv_wit_end <= 0xFFF84D00, ".drv_wit overflow")

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
    /* G5b (safety/os_sel.c OS_SEL_OBJ): the selector object 0xFFF9547C lives
     * in r04r's retired .bss pad — which must stay where r04r put it */
    ASSERT(_tcu_rq_retired_pad == 0xFFF95F7C && SIZEOF(.bss) > 0, "the retired pad moved: safety/os_sel.h OS_SEL_OBJ")

    .app_tail : { KEEP(*(.app_tail)) } > APPTAIL
    .app_end : { KEEP(*(.app_end)) } > APPEND

    __vbr_base  = 0x00040070;   /* vector 4 at VBR + 0x10 = 0x40080 */
    __stack_top = 0xFFFA0000;

    /DISCARD/ : { *(.comment) *(.note*) }
}

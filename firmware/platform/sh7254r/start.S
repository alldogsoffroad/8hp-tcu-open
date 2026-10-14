! Application entry for the SH7254R (spec 06 section 4).
! The Bosch boot side jumps to the address at header +0x7C with an unknown register state.
! This code: sets the stack, masks interrupts, points VBR at the vector table, sets the FPU
! mode expected by GCC -m2a, initialises RAM and calls main(). main() never returns.

        .section .text.start, "ax"
        .global _start
        .align  2
_start:
        mov.l   L_stack, r15
        mov.l   L_sr, r0
        ldc     r0, sr                  ! IMASK = 15: all maskable interrupts off
        mov.l   L_vbr, r0
        ldc     r0, vbr                 ! vector n at VBR + 4n; vector 4 = header +0x80
        mov.l   L_fpscr, r0
        lds     r0, fpscr               ! PR = 1 (double default for -m2a), DN = 1, RN

        ! copy initialised data from ROM to RAM
        mov.l   L_data_load, r1
        mov.l   L_data_start, r2
        mov.l   L_data_end, r3
1:      cmp/hs  r3, r2
        bt      2f
        mov.l   @r1+, r0
        mov.l   r0, @r2
        bra     1b
        add     #4, r2

        ! clear .bss
2:      mov.l   L_bss_start, r2
        mov.l   L_bss_end, r3
        mov     #0, r0
3:      cmp/hs  r3, r2
        bt      4f
        mov.l   r0, @r2
        bra     3b
        add     #4, r2

4:      mov.l   L_main, r0
        jsr     @r0
        nop
5:      bra     5b                      ! main() must not return; let the watchdog reset
        nop

        .align  2
L_stack:      .long   __stack_top
L_sr:         .long   0x000000F0
L_vbr:        .long   __vbr_base
L_fpscr:      .long   0x000C0000
L_data_load:  .long   __data_load
L_data_start: .long   __data_start
L_data_end:   .long   __data_end
L_bss_start:  .long   __bss_start
L_bss_end:    .long   __bss_end
L_main:       .long   _main

! Flasher entry (spec 16 section 3). The application copies the flasher image to its link address
! (0xFFF90000) and jumps here with interrupts masked. Stack at the top of RAM.

        .section .text.entry, "ax"
        .global _fl_entry
        .align  2
_fl_entry:
        mov.l   L_stack, r15
        mov.l   L_sr, r0
        ldc     r0, sr                  ! IMASK = 15
        mov.l   L_bss_start, r2
        mov.l   L_bss_end, r3
        mov     #0, r0
1:      cmp/hs  r3, r2
        bt      2f
        mov.l   r0, @r2
        bra     1b
        add     #4, r2
2:      mov.l   L_main, r0
        jsr     @r0
        nop
3:      bra     3b                      ! fl_cmain() does not return; the watchdog resets
        nop

        .align  2
L_stack:     .long   __fl_stack_top
L_sr:        .long   0x000000F0
L_bss_start: .long   __fl_bss_start
L_bss_end:   .long   __fl_bss_end
L_main:      .long   _fl_cmain

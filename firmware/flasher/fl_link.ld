/* Project flasher, linked to run from RAM (spec 16 section 3).
 * 0xFFF90000-0xFFF9EFFF image + .bss, 0xFFF9F000-0xFFF9FFFF stack. The application copies the
 * image (text, rodata, data in one block) to 0xFFF90000 and jumps to its first word. */
OUTPUT_FORMAT("elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_fl_entry)

MEMORY
{
    FLRAM (rwx) : ORIGIN = 0xFFF90000, LENGTH = 0xF000
}

SECTIONS
{
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        . = ALIGN(4);
    } > FLRAM

    .bss (NOLOAD) : {
        __fl_bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __fl_bss_end = .;
    } > FLRAM

    __fl_stack_top = 0xFFFA0000;

    /DISCARD/ : { *(.comment) *(.note*) }
}

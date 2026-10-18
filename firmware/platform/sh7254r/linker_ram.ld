/* RAM application (spec 18): the whole program runs from RAM, loaded by the data-flash loader.
 * The loadable part (vectors, code, constants, initialised data) is one contiguous block from
 * 0xFFF84100 (the image payload); .bss follows; the stack is at the top of RAM. The code-flash
 * header, tail and end marker are not used. */
OUTPUT_FORMAT("elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0xFFF84100, LENGTH = 0xFFFA0000 - 0xFFF84100
}

SECTIONS
{
    .image : {
        __image_start = .;
        __vbr_base = .;             /* vector 4 at VBR + 0x10 */
        . = . + 0x10;
        KEEP(*(.app_vectors))
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        *(.ramfunc)
        . = ALIGN(4);
        __data_end = .;
        . = ALIGN(128);             /* payload length: whole 128-byte data-flash units */
        __image_end = .;
    } > RAM
    __data_load = __data_start;     /* already in place: start.S copies onto itself */

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    __stack_top = 0xFFFA0000;
    ASSERT(__image_end - __image_start <= 0xFF80, "RAM application does not fit in DB00-DB07")
    ASSERT(__bss_end <= 0xFFF9F000, "RAM application data leaves less than 4 KB of stack")

    /DISCARD/ : { *(.app_header) *(.app_tail) *(.app_end) *(.comment) *(.note*) }
}

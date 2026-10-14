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

    .data : {
        __data_start = .;
        *(.data .data.*)
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

    .app_tail : { KEEP(*(.app_tail)) } > APPTAIL
    .app_end : { KEEP(*(.app_end)) } > APPEND

    __vbr_base  = 0x00040070;   /* vector 4 at VBR + 0x10 = 0x40080 */
    __stack_top = 0xFFFA0000;

    /DISCARD/ : { *(.comment) *(.note*) }
}

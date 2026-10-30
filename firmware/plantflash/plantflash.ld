/* Plant-flash engine image: flat load at 0xFFF80100 (low RAM window, keeps 0xFFF88000+ intact), driven through the session's engine
 * table (slot at +0x10 -> T). No .bss: the engine state block is initialised data inside
 * the download, so a 34/36 download always installs a known-good idle state. */
/* default emulation of sh-elf-ld */
OUTPUT_ARCH(sh)
ENTRY(_pf_exec)

SECTIONS
{
    . = 0xFFF8C400;
    .img : ALIGN(4)
    {
        KEEP(*(.img_pad))
        KEEP(*(.img_slot))
        KEEP(*(.img_tbl))
        *(.text .text.*)
        *(.rodata .rodata.*)
    }

    . = ALIGN(4);
    .img_state : ALIGN(4)
    {
        KEEP(*(.img_state))
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}

ASSERT(. - 0xFFF8C400 <= 0x3C00, "plantflash image exceeds the 0xFFF80100-0xFFF81FFF window")

/* Plant-session dumper image: flat load at 0xFFF88400, run via the session hook
 * (slot at +0x10 -> descriptor, entry at descriptor +8). No .bss/.data: all state
 * lives at a fixed scratch address inside the download window. */
/* default emulation of sh-elf-ld */
OUTPUT_ARCH(sh)
ENTRY(_pd_entry)

SECTIONS
{
    . = 0xFFF88400;
    .img : ALIGN(4)
    {
        KEEP(*(.img_pad))
        KEEP(*(.img_slot))
        KEEP(*(.img_desc))
        *(.text .text.*)
        *(.rodata .rodata.*)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}

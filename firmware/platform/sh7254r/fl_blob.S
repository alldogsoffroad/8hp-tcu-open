! The project flasher image (spec 16), built from firmware/flasher and linked for RAM 0xFFF90000.
        .section .rodata.flasher, "a"
        .global _fl_blob
        .global _fl_blob_end
        .align  2
_fl_blob:
        .incbin "tcu_flasher.bin"
        .align  2
_fl_blob_end:

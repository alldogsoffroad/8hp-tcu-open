#!/bin/sh
# Plant-flash engine build (spec 16 flasher in the plant session's F0F0 pipe). Produces:
#   build/plantflash.bin  flat image for the boot-MAT download window at 0xFFF88400
# Layout: +0x10 slot -> T (exec/ready/pump/stop/ident), state block as initialised data.
set -e
cd "$(dirname "$0")"
OUT="$PWD/../../build"
mkdir -p "$OUT"
CC="${SH_ELF_PREFIX:-$HOME/opt/sh-elf}/bin/sh-elf-gcc"
OC=/usr/bin/sh-elf-objcopy
CFLAGS="-m2a -mb -ffreestanding -fno-builtin -Os -Wall -Wextra"

$CC $CFLAGS -nostdlib -nostartfiles -Wl,-T,plantflash.ld -Wl,--build-id=none \
    -o "$OUT/plantflash.elf" pf.c
$OC -O binary "$OUT/plantflash.elf" "$OUT/plantflash.bin"
ls -l "$OUT/plantflash.bin"

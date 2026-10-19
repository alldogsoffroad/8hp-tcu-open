#!/bin/sh
# Plant-session dumper build (spec 17 follow-up). Produces:
#   build/plantdump.bin    full RAM dumper
#   build/plantcanary.bin  single-frame probe build (PD_CANARY)
# Both are flat images for the boot-MAT download window at 0xFFF88400.
set -e
cd "$(dirname "$0")"
OUT="$PWD/../../build"
mkdir -p "$OUT"
CC="${SH_ELF_PREFIX:-$HOME/opt/sh-elf}/bin/sh-elf-gcc"
OC=/usr/bin/sh-elf-objcopy
CFLAGS="-m2a -mb -ffreestanding -fno-builtin -Os -Wall -Wextra"

$CC $CFLAGS -nostdlib -nostartfiles -Wl,-T,plantdump.ld -Wl,--build-id=none \
    -o "$OUT/plantdump.elf" pd.c
$CC $CFLAGS -DPD_CANARY -nostdlib -nostartfiles -Wl,-T,plantdump.ld -Wl,--build-id=none \
    -o "$OUT/plantcanary.elf" pd.c
$OC -O binary "$OUT/plantdump.elf" "$OUT/plantdump.bin"
$OC -O binary "$OUT/plantcanary.elf" "$OUT/plantcanary.bin"
ls -l "$OUT"/plantdump.bin "$OUT"/plantcanary.bin

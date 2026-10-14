# Cross toolchain: GCC for SH-2A, big-endian, bare metal.
# Usage: cmake -B build-sh2a -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-sh2a.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR sh2a)

set(SH_ELF_PREFIX "$ENV{HOME}/opt/sh-elf" CACHE PATH "sh-elf GCC install prefix")
set(CMAKE_C_COMPILER   "${SH_ELF_PREFIX}/bin/sh-elf-gcc")
set(CMAKE_ASM_COMPILER "${SH_ELF_PREFIX}/bin/sh-elf-gcc")
set(CMAKE_OBJCOPY      "/usr/bin/sh-elf-objcopy" CACHE FILEPATH "")
set(CMAKE_SIZE         "/usr/bin/sh-elf-size" CACHE FILEPATH "")

set(CMAKE_C_FLAGS_INIT   "-m2a -mb -ffreestanding -fno-builtin -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "-m2a -mb")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -nostartfiles -Wl,--gc-sections")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

cmake_minimum_required(VERSION 3.20)
project(tcu C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wconversion -Wshadow -O2 -g)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

set(CORE_SOURCES
    bsw/sched/sched.c
    app/main.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # ---- Target image for the SH7254R --------------------------------------
    set(LDSCRIPT ${CMAKE_SOURCE_DIR}/platform/sh7254r/linker.ld)
    add_executable(tcu_app.elf
        platform/sh7254r/start.S
        platform/sh7254r/header.c
        platform/sh7254r/plat_sh7254r.c
        ${CORE_SOURCES}
    )
    target_link_options(tcu_app.elf PRIVATE -T ${LDSCRIPT} -Wl,-Map=tcu_app.map)
    target_link_libraries(tcu_app.elf PRIVATE gcc)
    set_target_properties(tcu_app.elf PROPERTIES LINK_DEPENDS ${LDSCRIPT})
    add_custom_command(TARGET tcu_app.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex --remove-section=.bss tcu_app.elf tcu_app.hex
        COMMAND ${CMAKE_SIZE} tcu_app.elf
        COMMENT "Writing application block as Intel HEX (load addresses)")
else()
    # ---- Host build: simulator and unit tests ------------------------------
    add_executable(tcu_sim platform/host/plat_host.c ${CORE_SOURCES})

    enable_testing()
    add_executable(test_sched tests/unit/test_sched.c bsw/sched/sched.c)
    add_test(NAME sched COMMAND test_sched)
endif()

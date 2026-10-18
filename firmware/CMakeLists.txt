cmake_minimum_required(VERSION 3.20)
project(tcu C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wconversion -Wshadow -O2 -g)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

set(CORE_SOURCES
    bsw/sched/sched.c
    bsw/sup/sup.c
    bsw/com/crc8.c
    bsw/com/com.c
    bsw/com/com_tx.c
    bsw/com/sigpack.c
    io/vehicle/vehicle.c
    io/vehicle/fca_tx.c
    io/sensors/adc.c
    io/sensors/speed.c
    io/sensors/tft.c
    io/sensors/sensors.c
    io/actuators/sol_drv.c
    app/bench_out.c
    app/bench_dump.c
    app/flash_entry.c
    io/vehicle/profile_fca.c
    bsw/meas/meas.c
    app/meas_table.c
    bsw/diag/isotp.c
    bsw/diag/uds.c
    bsw/diag/boot_record.c
    app/main.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # ---- Target image for the SH7254R --------------------------------------
    set(LDSCRIPT ${CMAKE_SOURCE_DIR}/platform/sh7254r/linker.ld)

    # Project flasher (spec 16): its own program, linked for RAM 0xFFF90000 without libc or
    # libgcc, so it cannot reach into the application or the flash it rewrites.
    set(FL_LDSCRIPT ${CMAKE_SOURCE_DIR}/flasher/fl_link.ld)
    add_executable(tcu_flasher.elf
        flasher/fl_start.S
        flasher/fl_entry.c
        flasher/fl_sh7254r.c
        flasher/fl_core.c
        flasher/fl_fcu.c
    )
    set_source_files_properties(flasher/fl_entry.c PROPERTIES
        COMPILE_OPTIONS -fno-tree-loop-distribute-patterns)
    target_link_options(tcu_flasher.elf PRIVATE -T ${FL_LDSCRIPT} -Wl,-Map=tcu_flasher.map)
    set_target_properties(tcu_flasher.elf PROPERTIES LINK_DEPENDS ${FL_LDSCRIPT})
    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/tcu_flasher.bin
        COMMAND ${CMAKE_OBJCOPY} -O binary --remove-section=.bss tcu_flasher.elf tcu_flasher.bin
        COMMAND ${CMAKE_SIZE} tcu_flasher.elf
        DEPENDS tcu_flasher.elf
        COMMENT "Flasher RAM image")
    add_custom_target(tcu_flasher_bin DEPENDS ${CMAKE_BINARY_DIR}/tcu_flasher.bin)
    set_source_files_properties(platform/sh7254r/fl_blob.S PROPERTIES
        OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/tcu_flasher.bin
        COMPILE_OPTIONS "-Wa,-I${CMAKE_BINARY_DIR}")

    add_executable(tcu_app.elf
        platform/sh7254r/start.S
        platform/sh7254r/fl_blob.S
        platform/sh7254r/header.c
        platform/sh7254r/plat_sh7254r.c
        platform/sh7254r/libc_min.c
        ${CORE_SOURCES}
    )
    # keep GCC from turning the memset/memcpy loops into calls to themselves
    set_source_files_properties(platform/sh7254r/libc_min.c PROPERTIES
        COMPILE_OPTIONS -fno-tree-loop-distribute-patterns)
    target_include_directories(tcu_app.elf PRIVATE ${CMAKE_SOURCE_DIR}/platform/sh7254r/include)
    target_link_options(tcu_app.elf PRIVATE -T ${LDSCRIPT} -Wl,-Map=tcu_app.map)
    target_link_libraries(tcu_app.elf PRIVATE gcc)
    add_dependencies(tcu_app.elf tcu_flasher_bin)
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
    add_executable(test_com tests/unit/test_com.c bsw/com/com.c bsw/com/crc8.c
        tests/unit/stub_plat.c)
    add_test(NAME com COMMAND test_com)
    add_executable(test_profile_fca tests/unit/test_profile_fca.c io/vehicle/profile_fca.c
        io/vehicle/vehicle.c
        bsw/com/com.c bsw/com/crc8.c tests/unit/stub_plat.c)
    add_test(NAME profile_fca COMMAND test_profile_fca)
    add_executable(test_uds tests/unit/test_uds.c bsw/diag/uds.c bsw/diag/isotp.c
        bsw/diag/boot_record.c tests/unit/stub_plat.c)
    add_test(NAME uds COMMAND test_uds)
    add_executable(test_meas tests/unit/test_meas.c bsw/meas/meas.c tests/unit/stub_plat.c)
    add_test(NAME meas COMMAND test_meas)
    add_executable(test_com_tx tests/unit/test_com_tx.c bsw/com/com_tx.c bsw/com/sigpack.c
        bsw/com/crc8.c io/vehicle/fca_tx.c tests/unit/stub_plat.c)
    add_test(NAME com_tx COMMAND test_com_tx)
    add_executable(test_sensors tests/unit/test_sensors.c io/sensors/adc.c io/sensors/speed.c
        io/sensors/tft.c io/sensors/sensors.c tests/unit/stub_plat.c)
    add_test(NAME sensors COMMAND test_sensors)
    add_executable(test_sup tests/unit/test_sup.c bsw/sup/sup.c tests/unit/stub_plat.c)
    add_test(NAME sup COMMAND test_sup)
    add_executable(test_sol tests/unit/test_sol.c io/actuators/sol_drv.c tests/unit/stub_plat.c)
    add_test(NAME sol COMMAND test_sol)
    add_executable(test_bench tests/unit/test_bench.c app/bench_out.c io/actuators/sol_drv.c
        bsw/sup/sup.c bsw/diag/uds.c bsw/diag/isotp.c bsw/diag/boot_record.c
        io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        tests/unit/stub_plat.c)
    add_test(NAME bench COMMAND test_bench)
    add_executable(test_dump tests/unit/test_dump.c app/bench_dump.c tests/unit/stub_plat.c)
    add_test(NAME dump COMMAND test_dump)
    # project flasher on the simulated flash controller (spec 16 section 7)
    set(FL_HOST_SOURCES flasher/fl_core.c flasher/fl_fcu.c tests/unit/fl_sim.c)
    add_executable(test_flasher tests/unit/test_flasher.c ${FL_HOST_SOURCES} app/flash_entry.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_flasher PRIVATE FL_HOST)
    add_test(NAME flasher COMMAND test_flasher)
    add_executable(tcu_flasher_sim tests/unit/fl_sim_main.c ${FL_HOST_SOURCES})
    target_compile_definitions(tcu_flasher_sim PRIVATE FL_HOST)
    add_test(NAME flash_tool
        COMMAND python3 ${CMAKE_SOURCE_DIR}/../tools/bench/test_tcu_flash.py
            $<TARGET_FILE:tcu_flasher_sim>)
    add_test(NAME tft_table_current
        COMMAND python3 ${CMAKE_SOURCE_DIR}/../tools/gen_tft_table.py ${CMAKE_BINARY_DIR}/tft_table.h)
    add_test(NAME tft_table_diff
        COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/tft_table.h
            ${CMAKE_SOURCE_DIR}/io/sensors/tft_table.h)
    set_tests_properties(tft_table_diff PROPERTIES DEPENDS tft_table_current)
    # the generated transmit table must match the CSV it was made from
    add_test(NAME fca_tx_fields_current
        COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/../tools/gen_fca_tx.py
            ${CMAKE_SOURCE_DIR}/../1034420267_can_tx_signals.csv ${CMAKE_BINARY_DIR}/fca_tx_fields.h)
    add_test(NAME fca_tx_fields_diff
        COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/fca_tx_fields.h
            ${CMAKE_SOURCE_DIR}/io/vehicle/fca_tx_fields.h)
    set_tests_properties(fca_tx_fields_diff PROPERTIES DEPENDS fca_tx_fields_current)
endif()

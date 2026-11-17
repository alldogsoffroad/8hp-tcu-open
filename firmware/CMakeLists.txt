cmake_minimum_required(VERSION 3.20)
project(tcu C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wconversion -Wshadow -O2 -g)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/platform)

set(CORE_SOURCES
    bsw/sched/sched.c
    bsw/sup/sup_jobs.c
    bsw/sup/sup_handlers.c
    bsw/sup/sup_walk.c
    bsw/sup/sup_batch.c
    bsw/sup/sup_ctrl.c
    bsw/sup/sup_letters.c
    bsw/sup/sup_ladder.c bsw/sup/sup_mode.c
    bsw/sup/sup_token.c
    bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c
    bsw/drv/drv_handlers.c
    bsw/drv/drv_groupa.c bsw/drv/drv_temp.c
    bsw/drv/drv_pksens.c
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
    io/actuators/sol_ctl.c
    app/bench_out.c
    app/range.c
    app/park_law.c
    app/park.c
    app/rq_prod.c
    app/rq_fill.c
    app/rq_fill_cal.c
    safety/park_mon.c
    safety/park_pair.c
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
        platform/sh7254r/plat_intc.c
        platform/sh7254r/plat_tick.c
        platform/sh7254r/plat_walktmr.c
        platform/sh7254r/plat_wdt.c
        platform/sh7254r/plat_rspi.c
        platform/sh7254r/plat_adcmux.c
        platform/sh7254r/libc_min.c
        ${CORE_SOURCES}
    )
    # keep GCC from turning the memset/memcpy loops into calls to themselves
    set_source_files_properties(platform/sh7254r/libc_min.c PROPERTIES
        COMPILE_OPTIONS -fno-tree-loop-distribute-patterns)
    target_include_directories(tcu_app.elf PRIVATE ${CMAKE_SOURCE_DIR}/platform/sh7254r/include)
    target_compile_definitions(tcu_app.elf PRIVATE TCU_AUTOENGAGE TCU_E60P)  # bench m4 series; E60x - the full walk, alive at last (boot death solved: no bit0 from word one)
    # M4 park outputs (bsw/drv/drv_park.h, drv_groupa.h): THE ONE STAGING
    # MACRO of the flashed application — 0 SAFE BUILD: both park outputs
    # (release ch 7, hold TOE31) held at 0, the logic runs at zero demand;
    # 1 (default) the park outputs FOLLOW THE PORTED RANGE/PARK LOGIC
    # (app/range.c, app/park_law.c), bounded to {0, 10000}, the release
    # under the D-M4-7 30 s on-time limit; 2 the rail-off group-A full-on
    # tests released, park outputs 0. -DDRV_GA_STAGE=0 at configure time
    # for a build that cannot drive any park current.
    set(DRV_GA_STAGE 1 CACHE STRING "M4 park-output stage of tcu_app.elf (0 / 1 / 2)")
    set_property(CACHE DRV_GA_STAGE PROPERTY STRINGS 0 1 2)
    target_compile_definitions(tcu_app.elf PRIVATE DRV_GA_STAGE=${DRV_GA_STAGE})
    target_link_options(tcu_app.elf PRIVATE -T ${LDSCRIPT} -Wl,-Map=tcu_app.map)
    target_link_libraries(tcu_app.elf PRIVATE gcc)
    add_dependencies(tcu_app.elf tcu_flasher_bin)
    set_target_properties(tcu_app.elf PROPERTIES LINK_DEPENDS ${LDSCRIPT})
    add_custom_command(TARGET tcu_app.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex --remove-section=.bss tcu_app.elf tcu_app.hex
        COMMAND ${CMAKE_SIZE} tcu_app.elf
        COMMENT "Writing application block as Intel HEX (load addresses)")

    # ---- Data-flash loader (spec 18): code flash at 0x40000, written with the Teensy flasher ----
    set(LD_LDSCRIPT ${CMAKE_SOURCE_DIR}/loader/ld_link.ld)
    add_executable(tcu_loader.elf
        platform/sh7254r/start.S
        platform/sh7254r/header.c
        platform/sh7254r/plat_sh7254r.c
        platform/sh7254r/libc_min.c
        loader/ld_main.c
        loader/ld_image.c
        flasher/fl_core.c
        flasher/fl_fcu.c
        flasher/fl_sh7254r.c
    )
    target_compile_definitions(tcu_loader.elf PRIVATE TCU_LOADER)
    target_include_directories(tcu_loader.elf PRIVATE ${CMAKE_SOURCE_DIR}/platform/sh7254r/include)
    target_link_options(tcu_loader.elf PRIVATE -T ${LD_LDSCRIPT} -Wl,-Map=tcu_loader.map)
    target_link_libraries(tcu_loader.elf PRIVATE gcc)
    set_target_properties(tcu_loader.elf PROPERTIES LINK_DEPENDS ${LD_LDSCRIPT})
    add_custom_command(TARGET tcu_loader.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex --remove-section=.bss tcu_loader.elf tcu_loader.hex
        COMMAND ${CMAKE_SIZE} tcu_loader.elf
        COMMENT "Loader block as Intel HEX (build the full flash image with build_image.py --stock-ident)")

    # ---- RAM application (spec 18): stored in data flash, run from RAM ---------------------
    set(RAM_LDSCRIPT ${CMAKE_SOURCE_DIR}/platform/sh7254r/linker_ram.ld)
    add_executable(tcu_app_ram.elf
        platform/sh7254r/start.S
        platform/sh7254r/header.c
        platform/sh7254r/plat_sh7254r.c
        platform/sh7254r/plat_intc.c
        platform/sh7254r/plat_tick.c
        platform/sh7254r/plat_walktmr.c
        platform/sh7254r/plat_wdt.c
        platform/sh7254r/plat_rspi.c
        platform/sh7254r/plat_adcmux.c
        platform/sh7254r/libc_min.c
        ${CORE_SOURCES}
    )
    target_compile_definitions(tcu_app_ram.elf PRIVATE TCU_RAM_APP)
    # M4b park outputs (docs/firmware/01-m4-range-park.md): the bench application drives
    # the park actuators; the code-flash application stays output-gated until the M4b
    # bench check passes. Overridable: -DM4B_OUTPUTS=OFF builds the RAM app gated too.
    option(M4B_OUTPUTS "Enable the M4b park outputs on the RAM application" ON)
    if(M4B_OUTPUTS)
        target_compile_definitions(tcu_app_ram.elf PRIVATE TCU_M4B_OUTPUTS)
    endif()
    target_include_directories(tcu_app_ram.elf PRIVATE ${CMAKE_SOURCE_DIR}/platform/sh7254r/include)
    target_link_options(tcu_app_ram.elf PRIVATE -T ${RAM_LDSCRIPT} -Wl,-Map=tcu_app_ram.map)
    target_link_libraries(tcu_app_ram.elf PRIVATE gcc)
    set_target_properties(tcu_app_ram.elf PROPERTIES LINK_DEPENDS ${RAM_LDSCRIPT})
    add_custom_command(TARGET tcu_app_ram.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary --only-section=.image tcu_app_ram.elf tcu_app_ram.bin
        COMMAND python3 ${CMAKE_SOURCE_DIR}/../tools/build_app_image.py --elf tcu_app_ram.elf
            --bin tcu_app_ram.bin -o tcu_app_df.bin
        COMMAND ${CMAKE_SIZE} tcu_app_ram.elf
        COMMENT "RAM application and its data-flash image tcu_app_df.bin")
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
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c
    bsw/drv/drv_handlers.c
    bsw/com/crc8.c io/vehicle/fca_tx.c tests/unit/stub_plat.c)
    add_test(NAME com_tx COMMAND test_com_tx)
    add_executable(test_sensors tests/unit/test_sensors.c io/sensors/adc.c io/sensors/speed.c
        io/sensors/tft.c io/sensors/sensors.c tests/unit/stub_plat.c)
    add_test(NAME sensors COMMAND test_sensors)
    add_executable(test_sup tests/unit/test_sup.c bsw/sup/sup.c bsw/sup/sup_engage.c tests/unit/stub_plat.c)
    add_test(NAME sup COMMAND test_sup)
    add_executable(test_sol tests/unit/test_sol.c io/actuators/sol_drv.c tests/unit/stub_plat.c)
    add_test(NAME sol COMMAND test_sol)
    add_executable(test_bench tests/unit/test_bench.c app/bench_out.c io/actuators/sol_ctl.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_handlers.c
        bsw/diag/uds.c bsw/diag/isotp.c bsw/diag/boot_record.c
        io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_bench PRIVATE TCU_M4B_OUTPUTS)
    add_test(NAME bench COMMAND test_bench)
    # M4 range / park (2026-11-14, ~/tcu_sweep/range_logic_port.md): the
    # stock arbiter 0xA557C, the output law 0xCFFDE / 0xE88FA / 0xE89DE /
    # 0xE8888, the cross-check, driven from CAN lever frames; the slot
    # getters into the request law's on-time limit
    add_executable(test_park tests/unit/test_park.c app/range.c app/park_law.c app/park.c
        safety/park_mon.c io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c
        bsw/com/crc8.c bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_park PRIVATE PARK_TEST_HOOKS)
    add_test(NAME park COMMAND test_park)
    # r03b: the stock's solenoid request law (drv_reqlaw.c) — the driving
    # dump reproduced, the zero-request content per seq2 state, the full-on
    # tests and our interlock, the undervoltage / minimum-current laws, the
    # 'U' gate, the same-tick refresh. Nonzero requests exist ONLY here.
    add_executable(test_reqlaw tests/unit/test_reqlaw.c bsw/drv/drv_jobs.c
        bsw/drv/drv_reqlaw.c bsw/drv/drv_handlers.c tests/unit/stub_plat.c)
    add_test(NAME reqlaw COMMAND test_reqlaw)
    # M5-0 (2026-11-16, ~/tcu_sweep/m5_0_port.md): the stock request
    # producer (app/rq_prod.c) in front of the law, built as the target
    # runs it (DRV_RQ_USE_PRODUCER): the P set, the dump's cells, the wire
    # byte-identical to the r03f constant requests, the guard, the table
    # law; --stock compares the const data / code constants with the
    # pristine image and the cells with the driving dump (skipped, exit 77,
    # when the files are absent)
    set(TCU_STOCK_IMAGE "$ENV{HOME}/Downloads/260924131233 1034420267_IFla.bin" CACHE FILEPATH
        "the pristine stock flash image (M5-0 byte checks)")
    set(TCU_STOCK_DUMP "${CMAKE_SOURCE_DIR}/../stockram_driving_20261104.bin" CACHE FILEPATH
        "the stock driving RAM dump (M5-0 cell checks)")
    add_executable(test_m5prod tests/unit/test_m5prod.c app/rq_prod.c app/rq_fill.c
        app/rq_fill_cal.c bsw/drv/drv_jobs.c
        bsw/drv/drv_reqlaw.c bsw/drv/drv_handlers.c tests/unit/stub_plat.c)
    target_compile_definitions(test_m5prod PRIVATE DRV_RQ_USE_PRODUCER=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME m5prod COMMAND test_m5prod)
    add_test(NAME m5_stock_bytes COMMAND test_m5prod --stock)
    set_tests_properties(m5_stock_bytes PROPERTIES SKIP_RETURN_CODE 77)
    # M5-1 (2026-11-16, ~/tcu_sweep/m5_1_fill_port.md): the element fill
    # (app/rq_fill.c) at the stock cadence — M5-1a (the boot fill of B, then
    # the r03n wire) and M5-1b (A's fill, B released) pass by pass, the
    # widened guard on both sides of each bound; --stock: the cal copies
    # against the image, the fill cells / fill times against the dump.
    # m5fill_mut: the guard's lower bound mutated to 499 must FAIL the test.
    set(M5F_SOURCES tests/unit/test_m5fill.c app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_handlers.c tests/unit/stub_plat.c)
    add_executable(test_m5fill ${M5F_SOURCES})
    target_compile_definitions(test_m5fill PRIVATE DRV_RQ_USE_PRODUCER=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME m5fill_1a COMMAND test_m5fill)
    add_test(NAME m5_fill_bytes COMMAND test_m5fill --stock)
    set_tests_properties(m5_fill_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_m5fill_1b ${M5F_SOURCES})
    target_compile_definitions(test_m5fill_1b PRIVATE DRV_RQ_USE_PRODUCER=1 RQ_M5_STAGE=0x1B)
    add_test(NAME m5fill_1b COMMAND test_m5fill_1b)
    add_executable(test_m5fill_mut ${M5F_SOURCES})
    target_compile_definitions(test_m5fill_mut PRIVATE DRV_RQ_USE_PRODUCER=1 DRV_RQ_FILL_MIN=499u)
    add_test(NAME m5fill_mut COMMAND test_m5fill_mut)
    set_tests_properties(m5fill_mut PROPERTIES WILL_FAIL TRUE)
    # M4 group A (2026-11-14, ~/tcu_sweep/groupA_park_port.md): the stock's
    # switch-output law 0x849D4 → 0x8ADF4 → 0x91500 and the hold-law
    # temperature 0x887D8 — host runtime path, then the three compile-time
    # stage configurations exactly as the target builds them
    set(GA_TEST_SOURCES tests/unit/test_groupa.c bsw/drv/drv_groupa.c bsw/drv/drv_temp.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_handlers.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c
        bsw/sup/sup_token.c tests/unit/stub_plat.c)
    add_executable(test_groupa ${GA_TEST_SOURCES})
    add_test(NAME groupa COMMAND test_groupa)
    foreach(st 0 1 2)
        add_executable(test_groupa_stage${st} ${GA_TEST_SOURCES})
        target_compile_definitions(test_groupa_stage${st} PRIVATE DRV_GA_COMPILE_TIME=1 DRV_GA_STAGE=${st})
        add_test(NAME groupa_stage${st} COMMAND test_groupa_stage${st})
    endforeach()
    # ... and the SH-only compile checks of the safety frame: with __sh__
    # (the cross compiler when present, else the host one with -D__sh__)
    # the control compiles, and every request source fails to compile
    find_program(SH_ELF_GCC sh-elf-gcc PATHS $ENV{HOME}/opt/sh-elf/bin NO_DEFAULT_PATH)
    if(SH_ELF_GCC)
        set(RQ_SH_CC ${SH_ELF_GCC} -m2a -mb -std=c11 -ffreestanding)
    else()
        set(RQ_SH_CC ${CMAKE_C_COMPILER} -std=c11 -D__sh__=1)
    endif()
    set(RQ_SH_ARGS -fsyntax-only -Werror -I${CMAKE_SOURCE_DIR} -I${CMAKE_SOURCE_DIR}/include)
    add_test(NAME rq_sh_guard_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard.c)
    add_test(NAME rq_sh_guard_setter
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_SETTER ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard.c)
    add_test(NAME rq_sh_guard_cell
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_CELL ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard.c)
    add_test(NAME rq_sh_guard_inject
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_INJECT ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard.c)
    add_test(NAME rq_sh_guard_source
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_source.c)
    # a refusal passes only when the compiler fails WITH the frame's reason
    # (not on some unrelated error): the regex is the diagnostic, and the
    # exit code must be nonzero (WILL_FAIL with FAIL_REGULAR_EXPRESSION
    # would accept any failure)
    set_tests_properties(rq_sh_guard_setter rq_sh_guard_inject PROPERTIES
        PASS_REGULAR_EXPRESSION "no solenoid request source on SH builds")
    set_tests_properties(rq_sh_guard_source PROPERTIES
        PASS_REGULAR_EXPRESSION "solenoid request source is refused on SH builds")
    set_tests_properties(rq_sh_guard_cell PROPERTIES
        PASS_REGULAR_EXPRESSION "read-only")
    # the park-release bound (2026-11-14, range/park logic port): stage 1
    # compiles; any bound but 0 / 10000, or a bound not implied by
    # DRV_GA_STAGE, does not
    add_test(NAME rq_sh_guard_park_live
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_STAGE=1 ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard.c)
    add_test(NAME rq_sh_guard_p7_value
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_STAGE=1 -DDRV_RQ_P7=5000u ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard.c)
    add_test(NAME rq_sh_guard_p7_stage
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_STAGE=0 -DDRV_RQ_P7=10000u ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard.c)
    set_tests_properties(rq_sh_guard_p7_value PROPERTIES
        PASS_REGULAR_EXPRESSION "nothing else")
    set_tests_properties(rq_sh_guard_p7_stage PROPERTIES
        PASS_REGULAR_EXPRESSION "follows DRV_GA_STAGE alone")
    # M5-0 (2026-11-16): the request producer's inputs are compile-time
    # constants on SH — the control (and the producer itself) compiles; a
    # runtime input path, a runtime input source, or a gear code / power
    # flow other than the M5-0 (dump) value does not
    set(M5_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_m5.c)
    add_test(NAME m5_sh_guard_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_producer
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/app/rq_prod.c)
    add_test(NAME m5_sh_guard_runtime
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_M5_RUNTIME ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_source
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_PROD_INPUT_SOURCE=1 ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_gear
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_GEAR=1 ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_pflow
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_PFLOW=1 ${M5_SH_SRC})
    set_tests_properties(m5_sh_guard_runtime PROPERTIES
        PASS_REGULAR_EXPRESSION "no runtime producer input on SH builds")
    set_tests_properties(m5_sh_guard_source PROPERTIES
        PASS_REGULAR_EXPRESSION "runtime producer input source is refused on SH builds")
    set_tests_properties(m5_sh_guard_gear PROPERTIES
        PASS_REGULAR_EXPRESSION "M5-1 gear code follows RQ_M5_STAGE alone")
    # M5-1: stage 1b compiles (the one-line switch); the fill files compile on
    # SH; a host fill-input path, a fill input other than the dump's, an
    # unknown stage or a guard fill bound above 8500 does not
    add_test(NAME m5_sh_guard_stage1b
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_STAGE=0x1B ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_fill
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/app/rq_fill.c)
    add_test(NAME m5_sh_guard_fillin
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_M5_FILLIN ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_temp
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_TEMP=0x50 ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_stagex
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_STAGE=0x1C ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_fillmax
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_RQ_FILL_MAX=8501u ${M5_SH_SRC})
    add_test(NAME m5_sh_guard_record
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_REC_STATE=0u ${M5_SH_SRC})
    set_tests_properties(m5_sh_guard_record PROPERTIES
        PASS_REGULAR_EXPRESSION "parameter record 0x1FD is the dump")
    set_tests_properties(m5_sh_guard_fillin PROPERTIES
        PASS_REGULAR_EXPRESSION "no runtime producer input on SH builds")
    set_tests_properties(m5_sh_guard_temp PROPERTIES
        PASS_REGULAR_EXPRESSION "every M5-1 fill input equals the stock driving dump")
    set_tests_properties(m5_sh_guard_stagex PROPERTIES
        PASS_REGULAR_EXPRESSION "RQ_M5_STAGE is RQ_M5_STAGE_1A or RQ_M5_STAGE_1B")
    set_tests_properties(m5_sh_guard_fillmax PROPERTIES
        PASS_REGULAR_EXPRESSION "fill range is the stock clamp")
    set_tests_properties(m5_sh_guard_pflow PROPERTIES
        PASS_REGULAR_EXPRESSION "M5-0 power-flow state is 0")
    # Timer E stage 0b (2026-11-13): the group-A duty setter (TOE31 park
    # hold) is refused on every SH build except the RAM application with
    # TCU_M4B_OUTPUTS; TCU_M4B_OUTPUTS on the flashed application is #error
    set(TOE_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_toe.c)
    add_test(NAME toe_sh_guard_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${TOE_SH_SRC})
    add_test(NAME toe_sh_guard_ram_m4b
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DTCU_RAM_APP -DTCU_M4B_OUTPUTS -DSH_GUARD_TOE_RAM ${TOE_SH_SRC})
    add_test(NAME toe_sh_guard_setter
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_TOE_SET ${TOE_SH_SRC})
    add_test(NAME toe_sh_guard_setter_loader
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DTCU_LOADER -DSH_GUARD_TOE_SET ${TOE_SH_SRC})
    add_test(NAME toe_sh_guard_m4b
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DTCU_M4B_OUTPUTS ${TOE_SH_SRC})
    set_tests_properties(toe_sh_guard_setter toe_sh_guard_setter_loader PROPERTIES
        PASS_REGULAR_EXPRESSION "no group-A Timer E duty source")
    set_tests_properties(toe_sh_guard_m4b PROPERTIES
        PASS_REGULAR_EXPRESSION "TCU_M4B_OUTPUTS is refused on the flashed SH application")
    # M4 group A safety frame: the stage-0 control (and stages 1 / 2) compile;
    # a demand source, the host setters and a foreign Timer E writer do not
    set(GA_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_ga.c)
    add_test(NAME ga_sh_guard_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_stage1
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_STAGE=1 ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_stage2
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_STAGE=2 ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_stage3
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_STAGE=3 ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_source
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_DEMAND_SOURCE=1 ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_setter
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_GA_SETTER ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_tests
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_GA_TESTS ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_writer
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_GA_WRITER ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_cycle
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_GA_CYCLE ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_d13
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_D13=10000u ${GA_SH_SRC})
    add_test(NAME ga_sh_guard_d11
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DDRV_GA_D11=5000u ${GA_SH_SRC})
    set_tests_properties(ga_sh_guard_stage3 PROPERTIES
        PASS_REGULAR_EXPRESSION "DRV_GA_STAGE must be 0, 1 or 2")
    set_tests_properties(ga_sh_guard_source PROPERTIES
        PASS_REGULAR_EXPRESSION "group-A demand source is refused on SH builds")
    set_tests_properties(ga_sh_guard_setter ga_sh_guard_tests PROPERTIES
        PASS_REGULAR_EXPRESSION "no group-A demand source on SH builds")
    set_tests_properties(ga_sh_guard_writer ga_sh_guard_cycle PROPERTIES
        PASS_REGULAR_EXPRESSION "only the group-A law")
    set_tests_properties(ga_sh_guard_d13 PROPERTIES
        PASS_REGULAR_EXPRESSION "only the park hold")
    set_tests_properties(ga_sh_guard_d11 PROPERTIES
        PASS_REGULAR_EXPRESSION "on/off value")
    # the park position sensor, READ-ONLY (2026-11-16,
    # ~/tcu_sweep/park_sensor_port.md): the evaluator 0x8DF2E / monitor
    # 0x88EF0 / copy 0x7B150 and the safety pair object 0x141BA2 / group 98
    # 0x141E06 at the bench mV values; and the park demands / arbiter target
    # bit-identical with the sensor in every state
    add_executable(test_pksens tests/unit/test_pksens.c bsw/drv/drv_pksens.c
        safety/park_pair.c app/range.c app/park_law.c app/park.c safety/park_mon.c
        io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    add_test(NAME pksens COMMAND test_pksens)
    # ... and its read-only frame: no output path names the sensor (source
    # scan; the negative run scans a fixture that does), and the sensor
    # headers refuse to compile in a TU that defines TCU_OUTPUT_PATH
    set(PKS_OUTPUT_PATHS
        ${CMAKE_SOURCE_DIR}/app/range.c ${CMAKE_SOURCE_DIR}/app/range.h
        ${CMAKE_SOURCE_DIR}/app/park_law.c ${CMAKE_SOURCE_DIR}/app/park_law.h
        ${CMAKE_SOURCE_DIR}/app/park.c ${CMAKE_SOURCE_DIR}/app/park.h
        ${CMAKE_SOURCE_DIR}/app/park_cells.h
        ${CMAKE_SOURCE_DIR}/safety/park_mon.c ${CMAKE_SOURCE_DIR}/safety/park_mon.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_park.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_reqlaw.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_reqlaw.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_jobs.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_jobs.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_groupa.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_groupa.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_cells.h
        ${CMAKE_SOURCE_DIR}/app/rq_prod.c ${CMAKE_SOURCE_DIR}/app/rq_prod.h
        ${CMAKE_SOURCE_DIR}/app/rq_fill.c ${CMAKE_SOURCE_DIR}/app/rq_fill.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_rqprod.h)
    string(REPLACE ";" "," PKS_OUTPUT_PATHS_ARG "${PKS_OUTPUT_PATHS}")
    add_test(NAME pksens_readonly_scan
        COMMAND ${CMAKE_COMMAND} "-DFILES=${PKS_OUTPUT_PATHS_ARG}"
            -P ${CMAKE_SOURCE_DIR}/tests/unit/pksens_readonly_scan.cmake)
    add_test(NAME pksens_readonly_scan_negative
        COMMAND ${CMAKE_COMMAND} -DFILES=${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_pksens.c
            -P ${CMAKE_SOURCE_DIR}/tests/unit/pksens_readonly_scan.cmake)
    set_tests_properties(pksens_readonly_scan_negative PROPERTIES
        PASS_REGULAR_EXPRESSION "park sensor read-only violation")
    add_test(NAME pks_sh_guard_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_pksens.c)
    add_test(NAME pks_sh_guard_output
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_PKS_OUTPUT ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_pksens.c)
    set_tests_properties(pks_sh_guard_output PROPERTIES
        PASS_REGULAR_EXPRESSION "park position sensor is read-only")
    add_executable(test_dump tests/unit/test_dump.c app/bench_dump.c tests/unit/stub_plat.c)
    add_executable(transcript tests/unit/transcript.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    add_test(NAME transcript COMMAND transcript)
    add_test(NAME transcript_28us COMMAND transcript 7168)
    # mode 1 (2026-11-13, ~/tcu_sweep/mode1_port.md): the same transcript
    # with startup modules 7 and 11 forced done at P50 and H5 released —
    # the stock's mode-0 → mode-1 switch, then the mode-1 schedule judged
    # pass by pass (J2 every 5 ms; J3 J4 F J6 J1 J5 F2 on the 10 ms pass)
    add_executable(transcript_mode1 tests/unit/transcript.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    target_compile_definitions(transcript_mode1 PRIVATE SUP_MODE_HOLD=0 TRANSCRIPT_MODE1=1)
    add_test(NAME transcript_mode1 COMMAND transcript_mode1)
    add_test(NAME transcript_mode1_28us COMMAND transcript_mode1 7168)
    add_test(NAME dump COMMAND test_dump)
    add_executable(test_rebuild tests/unit/test_rebuild.c bsw/sched/sched.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_walk.c bsw/sup/sup_batch.c
        bsw/sup/sup_ctrl.c bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    add_test(NAME rebuild COMMAND test_rebuild)
    # L1: the stock self-test ladder, held (sup_ladder.c) — driven with the
    # r02q bench inputs and a chip model of 0x14 bit 1 (level <= 4 AND the
    # TIOC clock present); asserts J2 TX 0x9000 through every path
    add_executable(test_ladder tests/unit/test_ladder.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    add_test(NAME ladder COMMAND test_ladder)
    # the negative test: phase 8's request hold released (L2) — the J2
    # invariant must see exactly the stock's one 0x9020 frame
    add_executable(test_ladder_l2 tests/unit/test_ladder.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_ladder_l2 PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0)
    add_test(NAME ladder_l2 COMMAND test_ladder_l2)
    # L3a (the target default): H1 + H2 released, H3 held — the ladder runs
    # phases 0x40 / 0x80 / 0x100 to 0xAA against a plant model of the
    # switched output; the 0x9020 window must sit in phases 0x40..0x80 and
    # the wire return to 0x9000 for good (the FSM stays 8)
    add_executable(test_ladder_l3a tests/unit/test_ladder.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_ladder_l3a PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0
        SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=1)
    add_test(NAME ladder_l3a COMMAND test_ladder_l3a)
    # L3b (not the target default): H3 released too — FSM 0x23 → 'E',
    # sequencer 'L', seq2 '3' → 'U', letter 'c', 0x9020 for good; asserts the
    # stock driving dump's end state
    add_executable(test_ladder_l3b tests/unit/test_ladder.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_ladder_l3b PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0
        SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0 SUP_L3_HOLD_SEQ_L=0)
    add_test(NAME ladder_l3b COMMAND test_ladder_l3b)
    # L3a-E (the recommended alternative to L3a): H3 released, H4 held — the
    # stock's post-ladder walk 0x23 → 'E' without the permanent letter
    add_executable(test_ladder_l3a_e tests/unit/test_ladder.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_ladder_l3a_e PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0
        SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0 SUP_L3_HOLD_SEQ_L=1)
    add_test(NAME ladder_l3a_e COMMAND test_ladder_l3a_e)
    # project flasher on the simulated flash controller (spec 16 section 7)
    set(FL_HOST_SOURCES flasher/fl_core.c flasher/fl_fcu.c tests/unit/fl_sim.c)
    add_executable(test_flasher tests/unit/test_flasher.c ${FL_HOST_SOURCES} app/flash_entry.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_letters.c tests/unit/stub_plat.c)
    target_compile_definitions(test_flasher PRIVATE FL_HOST)
    add_test(NAME flasher COMMAND test_flasher)
    add_executable(test_loader tests/unit/test_loader.c loader/ld_image.c)
    add_test(NAME loader COMMAND test_loader)
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

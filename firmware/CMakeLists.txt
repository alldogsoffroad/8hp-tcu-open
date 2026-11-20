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
    bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c
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
    app/rng_chain.c
    # r04r (~/tcu_sweep/r04r_port.md): the live holder (app/rq_prod.c +
    # its fill app/rq_fill.c) is retired from the applications; this pad
    # takes their place in the link so .bss / .noinit do not move. Both
    # stay as the host reference of the M5 tests (listed per test below).
    app/rq_retired.c
    app/rq_fill_cal.c
    app/tmp_chain.c
    app/gs_cal.c app/gs_bridge.c
    app/fb_shadow.c app/fb_cal.c app/fb_hook.c
    app/pa7_shadow.c app/pa7_hook.c bsw/drv/drv_pa7.c
    app/tsel_shadow.c app/tsel_hook.c app/tsel_live.c bsw/drv/drv_tsel.c
    app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c
    safety/park_mon.c
    safety/park_pair.c
    safety/kl15.c
    # G5b (~/tcu_sweep/g5b_port.md): the OS-mode request path (0x1418E2,
    # 0x68EA8, 0x68EFE, slot 26) — no .bss / .data (fixed addresses), so the
    # link order moves nothing; empty in the RAM application
    safety/os_sel.c
    safety/tmp_safe.c
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
        # G6a (2026-11-19, ~/tcu_sweep/g6a_port.md): the read-only CANDIDATE
        # (instance 2 of the garage engine template, its own RAM .gsc_req /
        # .gsc_obj) and the comparator (.g6a) — code-flash application only
        app/gs_cand.c app/gs_cand_fill.c app/g6a.c
        # F1 (2026-11-19, ~/tcu_sweep/f1_fr_shadow_port.md): the READ-ONLY
        # fault-reaction / gear-code shadow (its own RAM .fr_shadow / .fr_fm)
        # — code-flash application only
        app/fr_shadow.c app/fr_fm.c app/fr_cal.c app/fr_hook.c
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
    # G2 (user decision 2026-11-18): the garage bridge live for D and R on the
    # flashed application (bsw/drv/drv_gsbr.h; 0 = off, 3 = D and R)
    set(GS_LIVE_DIR 3 CACHE STRING "garage bridge stage of tcu_app.elf (0 off / 3 D and R)")
    target_compile_definitions(tcu_app.elf PRIVATE GS_LIVE_DIR=${GS_LIVE_DIR})
    # G5b (~/tcu_sweep/g5b_port.md, D-G5-8): the monitor's 0x1418E2 witness
    # becomes a counted cross-check of the live selector (safety/os_sel.c)
    target_compile_definitions(tcu_app.elf PRIVATE GS_M2_XCHECK=1)
    # G6b (2026-11-19, ~/tcu_sweep/g6b_port.md): THE SWITCH — the seam carries
    # the candidate (the stock engine + holder, its own RAM) under the monitor
    # (bsw/drv/drv_gsbr.h GS_G6B; 1 needs GS_LIVE_DIR 3; SH asserts it)
    set(GS_G6B 1 CACHE STRING "G6b: the candidate under the monitor drives A..E (1) / the r04n bridge (0, host only)")
    target_compile_definitions(tcu_app.elf PRIVATE GS_G6B=${GS_G6B})
    target_link_options(tcu_app.elf PRIVATE -T ${LDSCRIPT} -Wl,-Map=tcu_app.map)
    target_link_libraries(tcu_app.elf PRIVATE gcc)
    add_dependencies(tcu_app.elf tcu_flasher_bin)
    set_target_properties(tcu_app.elf PROPERTIES LINK_DEPENDS ${LDSCRIPT})
    # G6a: the link-map scan of the candidate and the comparator (no EEPROM /
    # NVM / DSM / event-log / fatal path; tests/unit/g6a_scan.py) — the image
    # is refused if it fails
    string(REPLACE "objcopy" "nm" SH_NM ${CMAKE_OBJCOPY})
    string(REPLACE "objcopy" "objdump" SH_OBJDUMP ${CMAKE_OBJCOPY})
    add_custom_command(TARGET tcu_app.elf POST_BUILD
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py link ${SH_NM}
            CMakeFiles/tcu_app.elf.dir/app/gs_cand.c.obj CMakeFiles/tcu_app.elf.dir/app/gs_cand_fill.c.obj
            CMakeFiles/tcu_app.elf.dir/app/g6a.c.obj
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py nvm ${SH_NM}
            CMakeFiles/tcu_app.elf.dir/app/fr_shadow.c.obj CMakeFiles/tcu_app.elf.dir/app/fr_fm.c.obj
            CMakeFiles/tcu_app.elf.dir/app/fr_cal.c.obj CMakeFiles/tcu_app.elf.dir/app/fr_hook.c.obj
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6c_sections.py ${SH_OBJDUMP} tcu_app.elf
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py retired ${SH_NM} tcu_app.elf
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py image ${SH_NM} tcu_app.elf
        COMMAND ${CMAKE_OBJCOPY} -O ihex --remove-section=.bss tcu_app.elf tcu_app.hex
        COMMAND ${CMAKE_SIZE} tcu_app.elf
        COMMENT "G6a link-map scan; G6c RAM-section overlap scan; writing application block as Intel HEX (load addresses)")

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
    add_executable(tcu_sim platform/host/plat_host.c ${CORE_SOURCES}
        app/gs_cand.c app/gs_cand_fill.c app/g6a.c
        app/fr_shadow.c app/fr_fm.c app/fr_cal.c app/fr_hook.c)   # F1: as the image
    target_compile_definitions(tcu_sim PRIVATE GSC_G6A=1)   # the image's candidate form

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
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c
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
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
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
        bsw/com/crc8.c bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    add_test(NAME park COMMAND test_park)
    # r03b: the stock's solenoid request law (drv_reqlaw.c) — the driving
    # dump reproduced, the zero-request content per seq2 state, the full-on
    # tests and our interlock, the undervoltage / minimum-current laws, the
    # 'U' gate, the same-tick refresh. Nonzero requests exist ONLY here.
    add_executable(test_reqlaw tests/unit/test_reqlaw.c bsw/drv/drv_jobs.c
        bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c tests/unit/stub_plat.c)
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
        bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c tests/unit/stub_plat.c)
    target_compile_definitions(test_m5prod PRIVATE DRV_RQ_USE_PRODUCER=1 RQ_M5_STAGE=0x1A
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
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c tests/unit/stub_plat.c)
    add_executable(test_m5fill ${M5F_SOURCES})
    target_compile_definitions(test_m5fill PRIVATE DRV_RQ_USE_PRODUCER=1 RQ_M5_STAGE=0x1A
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME m5fill_1a COMMAND test_m5fill)
    add_test(NAME m5_fill_bytes COMMAND test_m5fill --stock)
    set_tests_properties(m5_fill_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_m5fill_1b ${M5F_SOURCES})
    target_compile_definitions(test_m5fill_1b PRIVATE DRV_RQ_USE_PRODUCER=1 RQ_M5_STAGE=0x1B)
    add_test(NAME m5fill_1b COMMAND test_m5fill_1b)
    add_executable(test_m5fill_mut ${M5F_SOURCES})
    target_compile_definitions(test_m5fill_mut PRIVATE DRV_RQ_USE_PRODUCER=1 RQ_M5_STAGE=0x1A DRV_RQ_FILL_MIN=499u)
    add_test(NAME m5fill_mut COMMAND test_m5fill_mut)
    set_tests_properties(m5fill_mut PROPERTIES WILL_FAIL TRUE)
    # M5, the temperature byte LIVE (2026-11-17, ~/tcu_sweep/
    # temp_byte_live_port.md): the stock chain 0x887D8 → 0x7C100 → the
    # safety layer → 0xB9646 → 0xB986C, its schedule and invalid paths, the
    # producer (M5-1b) over every byte 0..255 inside the guard, the dump's
    # 0x43 reproduced from the dump's own gathered inputs (--stock)
    add_executable(test_tmpchain tests/unit/test_tmpchain.c app/tmp_chain.c safety/tmp_safe.c
        bsw/drv/drv_temp.c app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c
        bsw/sup/sup_token.c tests/unit/stub_plat.c)
    target_compile_definitions(test_tmpchain PRIVATE DRV_RQ_USE_PRODUCER=1 RQ_M5_STAGE=0x1B
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME tmpchain COMMAND test_tmpchain)
    add_test(NAME tmpchain_stock COMMAND test_tmpchain --stock)
    set_tests_properties(tmpchain_stock PROPERTIES SKIP_RETURN_CODE 77)
    # M4 group A (2026-11-14, ~/tcu_sweep/groupA_park_port.md): the stock's
    # switch-output law 0x849D4 → 0x8ADF4 → 0x91500 and the hold-law
    # temperature 0x887D8 — host runtime path, then the three compile-time
    # stage configurations exactly as the target builds them
    set(GA_TEST_SOURCES tests/unit/test_groupa.c bsw/drv/drv_groupa.c bsw/drv/drv_temp.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
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
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_STAGE=0x1A -DRQ_M5_GEAR=1 ${M5_SH_SRC})
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
        PASS_REGULAR_EXPRESSION "the temperature byte 0x89C13 / 0x89CD2 is live")
    set_tests_properties(m5_sh_guard_stagex PROPERTIES
        PASS_REGULAR_EXPRESSION "RQ_M5_STAGE is RQ_M5_STAGE_1A or RQ_M5_STAGE_1B")
    set_tests_properties(m5_sh_guard_fillmax PROPERTIES
        PASS_REGULAR_EXPRESSION "fill range is the stock clamp")
    # the temperature byte (2026-11-17): the chain compiles on SH; a revived
    # compile-time temperature input (RQ_M5_TEMP / RQ_M5_CD2) does not
    add_test(NAME m5_sh_guard_tmpchain
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/app/tmp_chain.c)
    add_test(NAME m5_sh_guard_tmpsafe
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/safety/tmp_safe.c)
    add_test(NAME m5_sh_guard_cd2
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DRQ_M5_CD2=0x43 ${M5_SH_SRC})
    set_tests_properties(m5_sh_guard_cd2 PROPERTIES
        PASS_REGULAR_EXPRESSION "the temperature byte 0x89C13 / 0x89CD2 is live")
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
        app/rng_chain.c safety/kl15.c bsw/sched/sched.c
        io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    add_test(NAME pksens COMMAND test_pksens)
    # ... and its read-only frame: no output path names the sensor (source
    # scan; the negative run scans a fixture that does), and the sensor
    # headers refuse to compile in a TU that defines TCU_OUTPUT_PATH
    set(PKS_OUTPUT_PATHS
        ${CMAKE_SOURCE_DIR}/app/range.c ${CMAKE_SOURCE_DIR}/app/range.h
        ${CMAKE_SOURCE_DIR}/app/rng_chain.c ${CMAKE_SOURCE_DIR}/app/rng_chain.h
        ${CMAKE_SOURCE_DIR}/app/park_law.c ${CMAKE_SOURCE_DIR}/app/park_law.h
        ${CMAKE_SOURCE_DIR}/app/park.c ${CMAKE_SOURCE_DIR}/app/park.h
        ${CMAKE_SOURCE_DIR}/app/park_cells.h
        ${CMAKE_SOURCE_DIR}/safety/park_mon.c ${CMAKE_SOURCE_DIR}/safety/park_mon.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_park.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_reqlaw.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_reqlaw.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_odiag.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_odiag.h
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
    # THE RANGE CHAIN, LIVE (2026-11-17, ~/tcu_sweep/range_chain_live_port.md;
    # the r03q..r03s read-only shadow made the live owner of D6D6): the stock
    # cyclic range chain 0x9B306/0x9B322/0x9B34A at the stock cell addresses
    # → the park law → the two park demands; the dump-stand-in answer, the
    # brake + engine release, the U1267 debounce and gaps; park_mon's
    # independent checks with mutations of the chain's output
    set(RNG_TEST_SOURCES app/rng_chain.c safety/kl15.c
        app/range.c app/park_law.c app/park.c safety/park_mon.c bsw/sched/sched.c
        io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    add_executable(test_rng_chain tests/unit/test_rng_chain.c ${RNG_TEST_SOURCES})
    target_compile_definitions(test_rng_chain PRIVATE RNG_CHAIN_TEST_HOOKS=1)
    add_test(NAME rng_chain COMMAND test_rng_chain)
    # KEY-OFF AUTO-P (2026-11-17, ~/tcu_sweep/keyoff_autop_port.md): the
    # terminal-15 object 0x140B80 (safety/kl15.c) feeding D641 / D642 / D643,
    # the chain's 0xC286C at key-off, the voltage path, the fault-reaction
    # park request 0x89CF2 and the DSM terminal re-cycle; park_mon silent
    add_executable(test_keyoff tests/unit/test_keyoff.c ${RNG_TEST_SOURCES})
    add_test(NAME keyoff COMMAND test_keyoff)
    # G5b (2026-11-19, ~/tcu_sweep/g5b_port.md): THE OS-MODE REQUEST PATH
    # (safety/os_sel.c: 0x1418E2, 0x68EA8, 0x68EFE, slot 26) with
    # bsw/sup/sup_mode.c's 0x6F0B4 / 0x66E06, the real CAN receive and kl15:
    # the law against the decode's table, plain boot (mode 1), sims from
    # power-on (mode 2, latched), the sim after boot, a stray frame, key-off
    # / key-on, the turbine term, the supply limits
    add_executable(test_os_sel tests/unit/test_os_sel.c safety/os_sel.c bsw/sup/sup_mode.c ${RNG_TEST_SOURCES})
    target_compile_definitions(test_os_sel PRIVATE SUP_L3_HOLD_SEQ_L=0)
    add_test(NAME g5b_os_sel COMMAND test_os_sel)
    # ... "the only output effect is the park demands": the whole 10 ms
    # output path (request producer + fill → request law → bus-A cyclic /
    # config words, group-A slots) traced over the r03s scenarios and
    # compared, masked at the park-release / park-hold positions only, with
    # the trace of HEAD 91cc956 (r03s, the shadow; the golden file was
    # produced by this same program built against that tree)
    add_executable(test_rng_wire tests/unit/test_rng_wire.c app/rng_chain.c safety/kl15.c
        app/range.c app/park_law.c app/park.c safety/park_mon.c bsw/sched/sched.c
        io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_rng_wire PRIVATE DRV_RQ_USE_PRODUCER=1)
    # THE OUTPUT DIAGNOSIS (2026-11-17, bsw/drv/drv_odiag.c; ~/tcu_sweep/
    # output_diag_port.md): the bench timeline at the healthy chips' replies
    # vs the trace of HEAD a6ec870 (r03v, no diagnosis; the golden file was
    # produced by this same program built against that tree with -DOD_HEAD),
    # then injected chip faults and the witness-only fatal checks
    add_executable(test_odiag tests/unit/test_odiag.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_odiag PRIVATE DRV_RQ_USE_PRODUCER=1)
    # 2026-11-18 (output_cadence_port.md): re-baselined with a CONTENT-only
    # comparator — the last cyclic / config / ID on the wire (setpoint fields
    # masked: the 0x9090A phase) and the law cells per 5 ms tick, against the
    # same program built at HEAD ebf475a with -DOD_OLD (refresh + periodic).
    # The old per-tick wire hash (odiag_head_a6ec870.txt) pinned the old
    # cadence itself and can no longer match by construction.
    add_test(NAME odiag_content_vs_ebf475a
        COMMAND test_odiag ${CMAKE_SOURCE_DIR}/tests/unit/golden/odiag_content_head_ebf475a.txt)
    add_test(NAME rng_wire_vs_r03s
        COMMAND test_rng_wire ${CMAKE_SOURCE_DIR}/tests/unit/golden/rng_wire_r03s.txt)
    # ... and the WHOLE trace (park demands included) byte for byte against
    # HEAD 1087992 — the bench case is unchanged by the terminal-15 object
    # (2026-11-17, ~/tcu_sweep/keyoff_autop_port.md)
    add_test(NAME rng_wire_vs_head
        COMMAND test_rng_wire --exact ${CMAKE_SOURCE_DIR}/tests/unit/golden/rng_wire_head_1087992.txt)
    # ... the writer guard (ours): nothing but app/rng_chain.c writes D6D6 /
    # D6D7 / D6D9, E549..E54B or 0xFFF8304C (source scan of every firmware
    # source, comments stripped; a negative fixture), the read-only cell
    # macros refuse a write, and the chain may not include an output-side
    # header (SH compile checks)
    add_test(NAME range_chain_writer_scan
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/range_chain_writer_scan.py ${CMAKE_SOURCE_DIR})
    add_test(NAME range_chain_writer_scan_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/range_chain_writer_scan.py ${CMAKE_SOURCE_DIR}
            --only ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_rng.c)
    set_tests_properties(range_chain_writer_scan_negative PROPERTIES WILL_FAIL TRUE)
    set(RNG_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_rng.c)
    add_test(NAME rng_sh_guard_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${RNG_SH_SRC})
    add_test(NAME rng_sh_guard_chain
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform/sh7254r/include ${CMAKE_SOURCE_DIR}/app/rng_chain.c)
    add_test(NAME rng_sh_guard_write
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_RNG_WRITE ${RNG_SH_SRC})
    add_test(NAME rng_sh_guard_isolated
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_RNG_ISOLATED ${RNG_SH_SRC})
    add_test(NAME rng_sh_guard_hooks
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform/sh7254r/include -DRNG_CHAIN_TEST_HOOKS=1 ${CMAKE_SOURCE_DIR}/app/rng_chain.c)
    set_tests_properties(rng_sh_guard_write PROPERTIES
        PASS_REGULAR_EXPRESSION "read-only")
    set_tests_properties(rng_sh_guard_isolated PROPERTIES
        PASS_REGULAR_EXPRESSION "the range chain is isolated: it may not include an output-side header")
    set_tests_properties(rng_sh_guard_hooks PROPERTIES
        PASS_REGULAR_EXPRESSION "RNG_CHAIN_TEST_HOOKS .input overrides. is host-only")
    # THE GARAGE-ENGAGEMENT SHADOW (2026-11-17, ~/tcu_sweep/garage_shadow_port.md):
    # READ-ONLY. (a) the live path bit-identical with and without it over the
    # r03t/r03w scenarios (whole cell window hashed per tick); (b)/(c) P→D /
    # P→R with brake + engine 700 under the forced commit; (d) the stock-
    # faithful variant never commits; (e) (2026-11-18, garage_shadow_v2_port.md)
    # the dry-bench completion through the ported synchronisation progress
    # (normal, ≈ 0.95 s) and the 0xE469C fill-time term, at 38.9 and 21 °C;
    # (g) the 250-pass timeout with a turbine that never synchronises; (n) the
    # old stand-ins must fail (e); reaction 37 host-forced.
    set(GS_TEST_SOURCES tests/unit/test_gs_shadow.c tests/unit/gs_ref_shadow.c app/gs_fill.c app/gs_cal.c app/gs_bridge.c
        app/rng_chain.c safety/kl15.c
        app/range.c app/park_law.c app/park.c safety/park_mon.c bsw/sched/sched.c
        io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    add_executable(test_gs_shadow ${GS_TEST_SOURCES})
    target_compile_definitions(test_gs_shadow PRIVATE DRV_RQ_USE_PRODUCER=1 GS_FORCED=1)
    add_test(NAME gs_shadow COMMAND test_gs_shadow)
    add_executable(test_gs_shadow_faithful ${GS_TEST_SOURCES})
    target_compile_definitions(test_gs_shadow_faithful PRIVATE DRV_RQ_USE_PRODUCER=1 GS_FORCED=0)
    add_test(NAME gs_shadow_faithful COMMAND test_gs_shadow_faithful)
    # G6-0 (2026-11-19, ~/tcu_sweep/g6_0_port.md): the engine is a TEMPLATE
    # (app/gs_engine.c) compiled twice — instance 1 = tests/unit/gs_ref_shadow.c (the
    # shadow, today's RAM and symbols, the only one in the SH image) and
    # instance 2 = app/gs_cand.c + app/gs_cand_fill.c (the future G6a
    # candidate, HOST-ONLY here, its own request objects and fill instance).
    # gs_dual: instance 2 driven with instance 1's gs_in_t every pass of every
    # scenario of test_gs_shadow (the gs-check P D P R P with sims, the
    # engagements, the 120-pass releases, the release interruptions) and of
    # test_gs_bridge_logic (every abort case, G1 and GS_LIVE_DIR 3): every
    # state byte and output identical. Negatives (must FAIL): the candidate's
    # fill instance bound one object off its engine; the candidate bound onto
    # instance 1's request objects.
    set(GS_DUAL_SOURCES app/gs_cand.c app/gs_cand_fill.c tests/unit/gs_dual.c)
    add_executable(test_gs_dual ${GS_TEST_SOURCES} ${GS_DUAL_SOURCES})
    target_compile_definitions(test_gs_dual PRIVATE DRV_RQ_USE_PRODUCER=1 GS_FORCED=1 GS_DUAL=1)
    add_test(NAME gs_dual_identity COMMAND test_gs_dual)
    add_executable(test_gs_dual_faithful ${GS_TEST_SOURCES} ${GS_DUAL_SOURCES})
    target_compile_definitions(test_gs_dual_faithful PRIVATE DRV_RQ_USE_PRODUCER=1 GS_FORCED=0 GS_DUAL=1)
    add_test(NAME gs_dual_identity_faithful COMMAND test_gs_dual_faithful)
    add_executable(test_gs_dual_neg_fill ${GS_TEST_SOURCES} ${GS_DUAL_SOURCES})
    target_compile_definitions(test_gs_dual_neg_fill PRIVATE DRV_RQ_USE_PRODUCER=1 GS_FORCED=1 GS_DUAL=1
        GSC_NEG_FILL_SHIFT=1)
    add_test(NAME gs_dual_neg_fill_binding COMMAND test_gs_dual_neg_fill)
    set_tests_properties(gs_dual_neg_fill_binding PROPERTIES WILL_FAIL TRUE)
    add_executable(test_gs_dual_neg_alias ${GS_TEST_SOURCES} ${GS_DUAL_SOURCES})
    target_compile_definitions(test_gs_dual_neg_alias PRIVATE DRV_RQ_USE_PRODUCER=1 GS_FORCED=1 GS_DUAL=1
        GSC_NEG_ALIAS=1)
    add_test(NAME gs_dual_neg_alias_binding COMMAND test_gs_dual_neg_alias)
    set_tests_properties(gs_dual_neg_alias_binding PROPERTIES WILL_FAIL TRUE)
    add_executable(test_gs_cal tests/unit/test_gs_cal.c app/gs_cal.c)
    target_compile_definitions(test_gs_cal PRIVATE "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"")
    add_test(NAME gs_cal_bytes COMMAND test_gs_cal)
    set_tests_properties(gs_cal_bytes PROPERTIES SKIP_RETURN_CODE 77)
    # ... read-only enforcement: the source scans (every output path may not
    # name the shadow; the shadow may not name a live header, a live cell
    # address or a live writer), each with a negative fixture, and the SH
    # compile checks (the shadow compiles under sh-elf-gcc -Werror; an output
    # TU including the shadow header, the shadow including a live header and
    # the host hooks on SH must each FAIL with the frame's message)
    set(GS_OUTPUT_PATHS
        ${CMAKE_SOURCE_DIR}/app/park.c ${CMAKE_SOURCE_DIR}/app/park_law.c
        ${CMAKE_SOURCE_DIR}/app/range.c ${CMAKE_SOURCE_DIR}/app/rng_chain.c
        ${CMAKE_SOURCE_DIR}/app/rq_prod.c ${CMAKE_SOURCE_DIR}/app/rq_fill.c
        ${CMAKE_SOURCE_DIR}/safety/park_mon.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_jobs.c
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_reqlaw.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_groupa.c
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_odiag.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_handlers.c
        ${CMAKE_SOURCE_DIR}/bsw/sup/sup_cells.h)
    add_test(NAME gs_shadow_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_shadow_scan.py outputs ${GS_OUTPUT_PATHS})
    add_test(NAME gs_shadow_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_shadow_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/gs_scan_fixture_output.c)
    set_tests_properties(gs_shadow_scan_outputs_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME gs_shadow_scan_shadow
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/tests/unit/gs_ref_shadow.c ${CMAKE_SOURCE_DIR}/app/gs_fill.c
            ${CMAKE_SOURCE_DIR}/app/gs_shadow.h ${CMAKE_SOURCE_DIR}/app/gs_fill.h
            ${CMAKE_SOURCE_DIR}/app/gs_engine.c ${CMAKE_SOURCE_DIR}/app/gs_cand.c
            ${CMAKE_SOURCE_DIR}/app/gs_cand_fill.c ${CMAKE_SOURCE_DIR}/app/gs_cand.h
            ${CMAKE_SOURCE_DIR}/app/gs_cand_bind.h)
    add_test(NAME gs_shadow_scan_shadow_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/tests/unit/gs_scan_fixture_shadow.c)
    set_tests_properties(gs_shadow_scan_shadow_negative PROPERTIES WILL_FAIL TRUE)
    set(GS_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_gs.c)
    add_test(NAME gs_sh_guard_control COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${GS_SH_SRC})
    # G6c: instance 1 (the shadow) is a host-only test reference: SH refuses it
    add_test(NAME gs_sh_guard_shadow
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/tests/unit/gs_ref_shadow.c)
    add_test(NAME gs_sh_guard_fill
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/gs_fill.c)
    set_tests_properties(gs_sh_guard_shadow PROPERTIES
        PASS_REGULAR_EXPRESSION "instance 1 .the shadow. is a host-only test reference since G6c")
    set_tests_properties(gs_sh_guard_fill PROPERTIES
        PASS_REGULAR_EXPRESSION "instance 1's fill .the shadow. is a host-only test reference since G6c")
    add_test(NAME gs_sh_guard_output
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_GS_OUTPUT ${GS_SH_SRC})
    add_test(NAME gs_sh_guard_live
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_GS_LIVE ${GS_SH_SRC})
    add_test(NAME gs_sh_guard_hooks
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_SHADOW_HOST_HOOKS ${GS_SH_SRC})
    add_test(NAME gs_sh_guard_forced
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_FORCED=2 ${GS_SH_SRC})
    add_test(NAME gs_sh_guard_r37
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_R37=1 ${CMAKE_SOURCE_DIR}/app/gs_cand.c)
    # G6-0: the template does not compile alone; the candidate is host-only
    add_test(NAME gs_sh_guard_engine_alone
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${CMAKE_SOURCE_DIR}/app/gs_engine.c)
    # G6a: the candidate compiles for SH (its SH form) under -Werror; refused
    # in the RAM application, with GS_FORCED 0, with a host negative, and
    # included by an output path; the comparator compiles for SH
    add_test(NAME gs_sh_guard_cand
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/gs_cand.c)
    add_test(NAME gs_sh_guard_cand_fill
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/gs_cand_fill.c)
    add_test(NAME g6a_sh_guard_cmp
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform -Wall -Wextra -Wconversion
            ${CMAKE_SOURCE_DIR}/app/g6a.c)
    add_test(NAME g6a_sh_guard_cand_ram
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DTCU_RAM_APP ${CMAKE_SOURCE_DIR}/app/gs_cand.c)
    add_test(NAME g6a_sh_guard_cand_forced
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_FORCED=0 ${CMAKE_SOURCE_DIR}/app/gs_cand.c)
    add_test(NAME g6a_sh_guard_cand_neg
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGSC_NEG_C3D=1 ${CMAKE_SOURCE_DIR}/app/gs_cand.c)
    # G5c (~/tcu_sweep/g5c_port.md): 0x89C3D is honest in the image — the
    # forced 0 (GS_C3D_FORCED 1) is refused for SH
    add_test(NAME g5c_sh_guard_c3d_forced
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_C3D_FORCED=1 ${CMAKE_SOURCE_DIR}/app/gs_cand.c)
    set_tests_properties(g5c_sh_guard_c3d_forced PROPERTIES
        PASS_REGULAR_EXPRESSION "0x89C3D is honest in the image since G5c")
    add_test(NAME g6a_sh_guard_output
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DTCU_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/app/g6a.c)
    set_tests_properties(gs_sh_guard_engine_alone PROPERTIES
        PASS_REGULAR_EXPRESSION "gs_engine.c is the garage engine TEMPLATE")
    # r04r (~/tcu_sweep/r04r_port.md): the retired holder cannot come back into
    # the code-flash application (GS_G6B): app/rq_prod.c and its own fill
    # app/rq_fill.c refuse to compile there; the RAM-app form (empty) and the
    # host reference still compile (the M5 tests and these controls)
    add_test(NAME g6r_sh_guard_holder
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_G6B=1 ${CMAKE_SOURCE_DIR}/app/rq_prod.c)
    add_test(NAME g6r_sh_guard_holder_fill
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_G6B=1 ${CMAKE_SOURCE_DIR}/app/rq_fill.c)
    set_tests_properties(g6r_sh_guard_holder PROPERTIES
        PASS_REGULAR_EXPRESSION "the live holder is retired from the code-flash application since r04r")
    set_tests_properties(g6r_sh_guard_holder_fill PROPERTIES
        PASS_REGULAR_EXPRESSION "the live holder's fill instance is retired from the code-flash application since r04r")
    add_test(NAME g6r_sh_guard_holder_ram_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_G6B=1 -DTCU_RAM_APP ${CMAKE_SOURCE_DIR}/app/rq_prod.c)
    set_tests_properties(g6a_sh_guard_cand_ram PROPERTIES
        PASS_REGULAR_EXPRESSION "the G6a candidate is for the code-flash application only")
    set_tests_properties(g6a_sh_guard_cand_forced PROPERTIES
        PASS_REGULAR_EXPRESSION "GS_TARGET_1ST must be 1")
    set_tests_properties(g6a_sh_guard_cand_neg PROPERTIES
        PASS_REGULAR_EXPRESSION "the binding negatives are host-only tests")
    set_tests_properties(g6a_sh_guard_output PROPERTIES
        PASS_REGULAR_EXPRESSION "the G6a candidate is read-only: an output path may not include g6a.h")
    set_tests_properties(gs_sh_guard_output PROPERTIES
        PASS_REGULAR_EXPRESSION "the garage shadow is read-only: an output path may not include gs_shadow.h")
    set_tests_properties(gs_sh_guard_live PROPERTIES
        PASS_REGULAR_EXPRESSION "the garage shadow is read-only: it may not include a live output header")
    set_tests_properties(gs_sh_guard_hooks PROPERTIES
        PASS_REGULAR_EXPRESSION "the host stand-in overrides do not exist on SH builds")
    set_tests_properties(gs_sh_guard_forced PROPERTIES
        PASS_REGULAR_EXPRESSION "GS_FORCED is 0 .stock-faithful. or 1 .forced commit., nothing else")
    set_tests_properties(gs_sh_guard_r37 PROPERTIES
        PASS_REGULAR_EXPRESSION "reaction 37 is a compile-time stand-in 0 on SH builds")
    # THE PWM-FEEDBACK SHADOW (2026-11-18, ~/tcu_sweep/feedback_shadow_port.md):
    # READ-ONLY. (1) the stock driving dump reproduced on the Timer F / Timer D
    # model (classes, duties, estimates, resistances); (2) the timeout,
    # saturation, 950 permil, mixed-hold, R-cadence and sampler-rate paths;
    # (3) the negative controls (swapped polarity, swapped pins) must FAIL;
    # the calibration byte-checked against the stock image; the live path
    # (whole cell window + every bus-A word + the cadence witnesses, parts
    # 1-4 of test_odiag) byte-identical with and without the shadow; the
    # source scans and the SH compile checks of the read-only frame.
    set(FB_TEST_SOURCES tests/unit/test_fb_shadow.c app/fb_shadow.c app/fb_cal.c tests/unit/fb_model.c
        bsw/drv/drv_temp.c tests/unit/stub_plat.c)
    add_executable(test_fb_shadow ${FB_TEST_SOURCES})
    add_test(NAME fb_shadow COMMAND test_fb_shadow)
    foreach(neg POLARITY PINS)
        add_executable(test_fb_shadow_neg_${neg} ${FB_TEST_SOURCES})
        target_compile_definitions(test_fb_shadow_neg_${neg} PRIVATE FBS_NEG_${neg}=1)
        add_test(NAME fb_shadow_neg_${neg} COMMAND test_fb_shadow_neg_${neg} --neg)
        set_tests_properties(fb_shadow_neg_${neg} PROPERTIES WILL_FAIL TRUE)
    endforeach()
    # (4) the cold boot: the r04b defect restored (no wait for a valid 0x94F9C
    # before the R model's seed) must FAIL
    add_executable(test_fb_shadow_neg_COLDSEED ${FB_TEST_SOURCES})
    target_compile_definitions(test_fb_shadow_neg_COLDSEED PRIVATE FBS_NEG_COLDSEED=1)
    add_test(NAME fb_shadow_neg_COLDSEED COMMAND test_fb_shadow_neg_COLDSEED --cold)
    set_tests_properties(fb_shadow_neg_COLDSEED PROPERTIES WILL_FAIL TRUE)
    add_executable(test_fb_cal tests/unit/test_fb_cal.c app/fb_cal.c)
    target_compile_definitions(test_fb_cal PRIVATE "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"")
    add_test(NAME fb_cal_bytes COMMAND test_fb_cal)
    set_tests_properties(fb_cal_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_odiag_fbs tests/unit/test_odiag.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c app/fb_hook.c tests/unit/fb_model.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_odiag_fbs PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1)
    add_test(NAME fb_shadow_identity
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag> $<TARGET_FILE:test_odiag_fbs>)
    add_executable(test_odiag_fbs_neg tests/unit/test_odiag.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c app/fb_hook.c tests/unit/fb_model.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_odiag_fbs_neg PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_FBS_NEG=1)
    add_test(NAME fb_shadow_identity_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag> $<TARGET_FILE:test_odiag_fbs_neg>)
    set_tests_properties(fb_shadow_identity_negative PROPERTIES WILL_FAIL TRUE)
    set(FB_OUTPUT_PATHS ${GS_OUTPUT_PATHS}
        ${CMAKE_SOURCE_DIR}/bsw/sup/sup_batch.c ${CMAKE_SOURCE_DIR}/io/actuators/sol_ctl.c
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_cells.h ${CMAKE_SOURCE_DIR}/bsw/drv/drv_odiag.h
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_reqlaw.h ${CMAKE_SOURCE_DIR}/bsw/drv/drv_jobs.h)
    add_test(NAME fb_shadow_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_shadow_scan.py outputs ${FB_OUTPUT_PATHS})
    add_test(NAME fb_shadow_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_shadow_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/fb_scan_fixture_output.c)
    set_tests_properties(fb_shadow_scan_outputs_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME fb_shadow_scan_shadow
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/app/fb_shadow.c ${CMAKE_SOURCE_DIR}/app/fb_shadow.h
            ${CMAKE_SOURCE_DIR}/app/fb_cal.c ${CMAKE_SOURCE_DIR}/app/fb_cal.h)
    add_test(NAME fb_shadow_scan_shadow_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/tests/unit/fb_scan_fixture_shadow.c)
    set_tests_properties(fb_shadow_scan_shadow_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME fb_shadow_scan_hook
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/app/fb_hook.c)
    add_test(NAME fb_shadow_scan_hook_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/tests/unit/fb_scan_fixture_hook.c)
    set_tests_properties(fb_shadow_scan_hook_negative PROPERTIES WILL_FAIL TRUE)
    set(FB_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_fb.c)
    add_test(NAME fb_sh_guard_control COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${FB_SH_SRC})
    add_test(NAME fb_sh_guard_shadow
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/fb_shadow.c)
    add_test(NAME fb_sh_guard_hook
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/fb_hook.c)
    add_test(NAME fb_sh_guard_output COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_FB_OUTPUT ${FB_SH_SRC})
    add_test(NAME fb_sh_guard_live COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_FB_LIVE ${FB_SH_SRC})
    add_test(NAME fb_sh_guard_cal COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_FB_CAL ${FB_SH_SRC})
    set_tests_properties(fb_sh_guard_output PROPERTIES
        PASS_REGULAR_EXPRESSION "the feedback shadow is read-only: an output path may not include fb_shadow.h")
    set_tests_properties(fb_sh_guard_live PROPERTIES
        PASS_REGULAR_EXPRESSION "the feedback shadow is read-only: it may not include a live output header")
    set_tests_properties(fb_sh_guard_cal PROPERTIES
        PASS_REGULAR_EXPRESSION "the feedback shadow is read-only: an output path may not include fb_cal.h")
    # THE PA7 SHADOW (2026-11-18, ~/tcu_sweep/r04d_recorder_pa7_shadow.md):
    # READ-ONLY. (1) the plain P boot reproduces the stock driving dump's PA7
    # cells; (2) / (3) the park-check lever sequence without and with the
    # brake + engine sims through the real range chain; (4) the range part
    # and the law on direct inputs (invalid range → state 3, the sensor's P
    # → state 2, D713); (5) the AN7 line checks; the calibration against the
    # stock image; the broken-law negative must FAIL; the live identity
    # (test_odiag with the PA7 hooks at their slots, and a PA7-leak negative);
    # the source scans and the SH compile checks of the read-only frame.
    set(PA7_TEST_SOURCES tests/unit/test_pa7_shadow.c app/pa7_shadow.c app/pa7_hook.c ${RNG_TEST_SOURCES})
    add_executable(test_pa7_shadow ${PA7_TEST_SOURCES})
    target_compile_definitions(test_pa7_shadow PRIVATE "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"")
    add_test(NAME pa7_shadow COMMAND test_pa7_shadow)
    add_test(NAME pa7_cal_bytes COMMAND test_pa7_shadow --cal)
    set_tests_properties(pa7_cal_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_pa7_shadow_neg_LAW ${PA7_TEST_SOURCES})
    target_compile_definitions(test_pa7_shadow_neg_LAW PRIVATE PA7_NEG_LAW=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"")
    add_test(NAME pa7_shadow_neg_LAW COMMAND test_pa7_shadow_neg_LAW)
    set_tests_properties(pa7_shadow_neg_LAW PROPERTIES WILL_FAIL TRUE)
    set(ODIAG_PA7_SOURCES tests/unit/test_odiag.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c app/fb_hook.c tests/unit/fb_model.c
        app/pa7_shadow.c app/pa7_hook.c
        tests/unit/stub_plat.c)
    add_executable(test_odiag_pa7 ${ODIAG_PA7_SOURCES})
    target_compile_definitions(test_odiag_pa7 PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1 TEST_ODIAG_PA7=1)
    add_test(NAME pa7_shadow_identity
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag> $<TARGET_FILE:test_odiag_pa7> --pa7)
    add_executable(test_odiag_pa7_neg ${ODIAG_PA7_SOURCES})
    target_compile_definitions(test_odiag_pa7_neg PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_PA7=1 TEST_ODIAG_PA7_NEG=1)
    add_test(NAME pa7_shadow_identity_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag> $<TARGET_FILE:test_odiag_pa7_neg> --pa7)
    set_tests_properties(pa7_shadow_identity_negative PROPERTIES WILL_FAIL TRUE)
    set(PA7_OUTPUT_PATHS ${FB_OUTPUT_PATHS}
        ${CMAKE_SOURCE_DIR}/bsw/sup/sup_ctrl.c ${CMAKE_SOURCE_DIR}/bsw/sup/sup_ctrl.h
        ${CMAKE_SOURCE_DIR}/bsw/sup/sup_handlers.c ${CMAKE_SOURCE_DIR}/bsw/sup/sup_jobs.c
        ${CMAKE_SOURCE_DIR}/platform/sh7254r/plat_sh7254r.c)
    add_test(NAME pa7_shadow_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/pa7_shadow_scan.py outputs ${PA7_OUTPUT_PATHS})
    add_test(NAME pa7_shadow_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/pa7_shadow_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/pa7_scan_fixture_output.c)
    set_tests_properties(pa7_shadow_scan_outputs_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME pa7_shadow_scan_shadow
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/pa7_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/app/pa7_shadow.c ${CMAKE_SOURCE_DIR}/app/pa7_shadow.h)
    add_test(NAME pa7_shadow_scan_shadow_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/pa7_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/tests/unit/pa7_scan_fixture_shadow.c)
    set_tests_properties(pa7_shadow_scan_shadow_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME pa7_shadow_scan_hook
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/pa7_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/app/pa7_hook.c)
    add_test(NAME pa7_shadow_scan_hook_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/pa7_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/tests/unit/pa7_scan_fixture_hook.c)
    set_tests_properties(pa7_shadow_scan_hook_negative PROPERTIES WILL_FAIL TRUE)
    set(PA7_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_pa7.c)
    add_test(NAME pa7_sh_guard_control COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${PA7_SH_SRC})
    add_test(NAME pa7_sh_guard_shadow
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/pa7_shadow.c)
    add_test(NAME pa7_sh_guard_hook
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/pa7_hook.c)
    add_test(NAME pa7_sh_guard_ctrl
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform ${CMAKE_SOURCE_DIR}/bsw/sup/sup_ctrl.c)
    add_test(NAME pa7_sh_guard_output COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_PA7_OUTPUT ${PA7_SH_SRC})
    add_test(NAME pa7_sh_guard_ctrlinc COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform
        -DSH_GUARD_PA7_CTRL ${PA7_SH_SRC})
    foreach(hdr CELLS CTRL RNG PARK PLAT)
        add_test(NAME pa7_sh_guard_live_${hdr}
            COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform -DSH_GUARD_PA7_LIVE_${hdr} ${PA7_SH_SRC})
        set_tests_properties(pa7_sh_guard_live_${hdr} PROPERTIES
            PASS_REGULAR_EXPRESSION "the PA7 shadow is read-only: it may not include a live header")
    endforeach()
    set_tests_properties(pa7_sh_guard_output pa7_sh_guard_ctrlinc PROPERTIES
        PASS_REGULAR_EXPRESSION "the PA7 shadow is read-only: an output path may not include pa7_shadow.h")
    # THE ECU-TEMPERATURE SHADOW (2026-11-18, ~/tcu_sweep/diag_temp_shadow_port.md,
    # stage 1): READ-ONLY. (1) the producers 0x8DB28 / 0x8DB5A / 0x8DD40 /
    # 0x8DD7C / 0x88DA8 / 0x88BAC / 0x88BD0 + the flag publishers reproduce
    # the stock driving dump and the not-ready / 0x8001 / hold / fallback
    # cases (tsel_shadow; tsel_cal_bytes against the image; a mutated
    # selection must FAIL); (2) the live identity: test_odiag with the hub
    # hook at its slot vs the same seeded build without it (and a leak
    # negative); (3) the scans (outputs / shadow / hook, negative fixtures);
    # (4) the SH compile guards. STAGE 1b (the risk gate): test_diag_temps —
    # the dry run with 0x940FC forced valid and 0x9393A from the shadow.
    set(TSEL_TEST_SOURCES tests/unit/test_tsel_shadow.c app/tsel_shadow.c)
    add_executable(test_tsel_shadow ${TSEL_TEST_SOURCES})
    target_compile_definitions(test_tsel_shadow PRIVATE
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME tsel_shadow COMMAND test_tsel_shadow)
    add_test(NAME tsel_cal_bytes COMMAND test_tsel_shadow --cal)
    set_tests_properties(tsel_cal_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_tsel_shadow_neg_SEL ${TSEL_TEST_SOURCES})
    target_compile_definitions(test_tsel_shadow_neg_SEL PRIVATE TSEL_NEG_SEL=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME tsel_shadow_neg_SEL COMMAND test_tsel_shadow_neg_SEL)
    set_tests_properties(tsel_shadow_neg_SEL PROPERTIES WILL_FAIL TRUE)
    set(ODIAG_TSEL_SOURCES tests/unit/test_odiag.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c app/fb_hook.c tests/unit/fb_model.c
        app/tsel_shadow.c app/tsel_hook.c
        tests/unit/stub_plat.c)
    add_executable(test_odiag_tselbase ${ODIAG_TSEL_SOURCES})
    target_compile_definitions(test_odiag_tselbase PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_TSEL_SEED=1)
    add_executable(test_odiag_tsel ${ODIAG_TSEL_SOURCES})
    target_compile_definitions(test_odiag_tsel PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_TSEL_SEED=1 TEST_ODIAG_TSEL=1)
    add_test(NAME tsel_shadow_identity
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag_tselbase> $<TARGET_FILE:test_odiag_tsel> --tsel)
    # STAGE 2a (2026-11-18): the hub hook with TSEL_LIVE=1 (the selection's
    # cells written by the real output path; the feedback chain on the live
    # 0x9393A) — the window hash then EXCLUDES the stage-2a cells and their
    # by-design consumers (test_odiag.c TEST_ODIAG_EXCL2A, both builds), every
    # bus-A word, cadence witness and every other cell must stay identical,
    # and the live cells must carry the shadow's values every pass
    add_executable(test_odiag_tselbase_x ${ODIAG_TSEL_SOURCES})
    target_compile_definitions(test_odiag_tselbase_x PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_TSEL_SEED=1 TEST_ODIAG_EXCL2A=1)
    add_executable(test_odiag_tsel_live ${ODIAG_TSEL_SOURCES} app/tsel_live.c bsw/drv/drv_tsel.c)
    target_compile_definitions(test_odiag_tsel_live PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_TSEL_SEED=1 TEST_ODIAG_TSEL=1 TEST_ODIAG_EXCL2A=1 TSEL_LIVE=1 FBS_T_ECU_LIVE=1
        TSEL_LIVE_FLAGS=0)
    add_test(NAME tsel_live_identity
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag_tselbase_x> $<TARGET_FILE:test_odiag_tsel_live> --tsel --tsel-live)
    add_executable(test_odiag_tsel_live_neg ${ODIAG_TSEL_SOURCES} app/tsel_live.c bsw/drv/drv_tsel.c)
    target_compile_definitions(test_odiag_tsel_live_neg PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_TSEL_SEED=1 TEST_ODIAG_TSEL=1 TEST_ODIAG_EXCL2A=1 TSEL_LIVE=1 FBS_T_ECU_LIVE=1
        TSEL_LIVE_FLAGS=0 TEST_ODIAG_TSEL_LIVE_NEG=1)
    add_test(NAME tsel_live_identity_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag_tselbase_x> $<TARGET_FILE:test_odiag_tsel_live_neg> --tsel --tsel-live)
    set_tests_properties(tsel_live_identity_negative PROPERTIES WILL_FAIL TRUE)
    # STAGE 2b (2026-11-19, ~/tcu_sweep/temp_stage2b_port.md): HEAD's
    # stage-2a build (TSEL_LIVE=1, the flags at the 0x80 stand-in) against the
    # stage-2b one (TSEL_LIVE_FLAGS=1), both running 0x7C100 and the safety
    # layer at their slots (TEST_ODIAG_TMP); the window hash excludes ONLY
    # the four flags and their direct consumers (TEST_ODIAG_EXCL2B, listed in
    # test_odiag.c) — the fluid byte 0xFFF8E9C8, 0x89C13, the fill, the law
    # and every bus-A word must stay identical; the 2b build must end in the
    # stock's healthy flag state. Negatives: the stand-ins (TSEL_LIVE_FLAGS=0)
    # in the second build; a leak of the board byte into 0xFFF8E9C8.
    set(ODIAG_T2B_SOURCES ${ODIAG_TSEL_SOURCES} app/tsel_live.c bsw/drv/drv_tsel.c
        bsw/drv/drv_temp.c app/tmp_chain.c safety/tmp_safe.c)
    set(ODIAG_T2B_DEFS DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1 TEST_ODIAG_TSEL_SEED=1 TEST_ODIAG_TSEL=1
        TEST_ODIAG_TMP=1 TEST_ODIAG_EXCL2B=1 TSEL_LIVE=1 FBS_T_ECU_LIVE=1)
    add_executable(test_odiag_tsel2a_t ${ODIAG_T2B_SOURCES})
    target_compile_definitions(test_odiag_tsel2a_t PRIVATE ${ODIAG_T2B_DEFS} TSEL_LIVE_FLAGS=0)
    add_executable(test_odiag_tsel2b_t ${ODIAG_T2B_SOURCES})
    target_compile_definitions(test_odiag_tsel2b_t PRIVATE ${ODIAG_T2B_DEFS} TSEL_LIVE_FLAGS=1)
    add_test(NAME tsel2b_identity
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag_tsel2a_t> $<TARGET_FILE:test_odiag_tsel2b_t> --tsel --tsel-live --tsel2b)
    add_test(NAME tsel2b_identity_negative_standin
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag_tsel2a_t> $<TARGET_FILE:test_odiag_tsel2a_t> --tsel --tsel-live --tsel2b)
    set_tests_properties(tsel2b_identity_negative_standin PROPERTIES WILL_FAIL TRUE)
    add_executable(test_odiag_tsel2b_leak ${ODIAG_T2B_SOURCES})
    target_compile_definitions(test_odiag_tsel2b_leak PRIVATE ${ODIAG_T2B_DEFS} TSEL_LIVE_FLAGS=1
        TEST_ODIAG_T2B_LEAK=1)
    add_test(NAME tsel2b_identity_negative_leak
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag_tsel2a_t> $<TARGET_FILE:test_odiag_tsel2b_leak> --tsel --tsel-live --tsel2b)
    set_tests_properties(tsel2b_identity_negative_leak PROPERTIES WILL_FAIL TRUE)
    add_executable(test_odiag_tsel_neg ${ODIAG_TSEL_SOURCES})
    target_compile_definitions(test_odiag_tsel_neg PRIVATE DRV_RQ_USE_PRODUCER=1 TEST_ODIAG_FBS=1
        TEST_ODIAG_TSEL_SEED=1 TEST_ODIAG_TSEL=1 TEST_ODIAG_TSEL_NEG=1)
    add_test(NAME tsel_shadow_identity_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fb_identity.py
            $<TARGET_FILE:test_odiag_tselbase> $<TARGET_FILE:test_odiag_tsel_neg> --tsel)
    set_tests_properties(tsel_shadow_identity_negative PROPERTIES WILL_FAIL TRUE)
    set(TSEL_OUTPUT_PATHS ${PA7_OUTPUT_PATHS}
        ${CMAKE_SOURCE_DIR}/bsw/drv/drv_temp.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_temp.h
        ${CMAKE_SOURCE_DIR}/safety/tmp_safe.c ${CMAKE_SOURCE_DIR}/app/tmp_chain.c
        ${CMAKE_SOURCE_DIR}/app/fb_shadow.c ${CMAKE_SOURCE_DIR}/app/fb_hook.c
        ${CMAKE_SOURCE_DIR}/bsw/sup/sup_ladder.c ${CMAKE_SOURCE_DIR}/bsw/drv/drv_tsel.c)
    add_test(NAME tsel_shadow_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/tsel_shadow_scan.py outputs ${TSEL_OUTPUT_PATHS})
    add_test(NAME tsel_shadow_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/tsel_shadow_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/tsel_scan_fixture_output.c)
    set_tests_properties(tsel_shadow_scan_outputs_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME tsel_shadow_scan_shadow
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/tsel_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/app/tsel_shadow.c ${CMAKE_SOURCE_DIR}/app/tsel_shadow.h)
    add_test(NAME tsel_shadow_scan_shadow_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/tsel_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/tests/unit/tsel_scan_fixture_shadow.c)
    set_tests_properties(tsel_shadow_scan_shadow_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME tsel_shadow_scan_hook
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/tsel_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/app/tsel_hook.c ${CMAKE_SOURCE_DIR}/app/tsel_live.c)
    add_test(NAME tsel_shadow_scan_hook_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/tsel_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/tests/unit/tsel_scan_fixture_hook.c)
    set_tests_properties(tsel_shadow_scan_hook_negative PROPERTIES WILL_FAIL TRUE)
    set(TSEL_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_tsel.c)
    add_test(NAME tsel_sh_guard_control COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${TSEL_SH_SRC})
    add_test(NAME tsel_sh_guard_shadow
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/tsel_shadow.c)
    add_test(NAME tsel_sh_guard_hook
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform -Wall -Wextra -Wconversion
            ${CMAKE_SOURCE_DIR}/app/tsel_hook.c)
    add_test(NAME tsel_sh_guard_output COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_TSEL_OUTPUT ${TSEL_SH_SRC})
    # stage 2a: the live writer may not name the shadow; TSEL_LIVE is refused
    # outside the code-flash application; the live TUs compile on SH
    add_test(NAME tsel_sh_guard_drvtsel COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_TSEL_DRVTSEL ${TSEL_SH_SRC})
    set_tests_properties(tsel_sh_guard_drvtsel PROPERTIES
        PASS_REGULAR_EXPRESSION "an output path may not include tsel_shadow.h")
    add_test(NAME tsel_sh_guard_ramlive COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_TSEL_RAMLIVE ${TSEL_SH_SRC})
    set_tests_properties(tsel_sh_guard_ramlive PROPERTIES
        PASS_REGULAR_EXPRESSION "TSEL_LIVE is for the code-flash application only")
    # stage 2b: the live flags need stage 2a, and never in the RAM app
    add_test(NAME tsel_sh_guard_ramflags COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_TSEL_RAMFLAGS ${TSEL_SH_SRC})
    set_tests_properties(tsel_sh_guard_ramflags PROPERTIES
        PASS_REGULAR_EXPRESSION "TSEL_LIVE_FLAGS is for the code-flash application only")
    add_test(NAME tsel_sh_guard_flagsnolive COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_TSEL_FLAGSNOLIVE ${TSEL_SH_SRC})
    set_tests_properties(tsel_sh_guard_flagsnolive PROPERTIES
        PASS_REGULAR_EXPRESSION "TSEL_LIVE_FLAGS \\(stage 2b\\) needs TSEL_LIVE")
    add_test(NAME tsel_sh_guard_drvtemp_tu
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion -I${CMAKE_SOURCE_DIR}/platform
            ${CMAKE_SOURCE_DIR}/bsw/drv/drv_temp.c)
    add_test(NAME tsel_sh_guard_live_tu
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/tsel_live.c)
    add_test(NAME tsel_sh_guard_drvtsel_tu
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/bsw/drv/drv_tsel.c)
    add_test(NAME tsel_sh_guard_outtu COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform
        -DSH_GUARD_TSEL_OUTTU ${TSEL_SH_SRC})
    set_tests_properties(tsel_sh_guard_output tsel_sh_guard_outtu PROPERTIES
        PASS_REGULAR_EXPRESSION "the temperature-selection shadow is read-only: an output path may not include tsel_shadow.h")
    foreach(hdr CELLS DCELLS TEMP ODIAG PLAT)
        add_test(NAME tsel_sh_guard_live_${hdr}
            COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform -DSH_GUARD_TSEL_LIVE_${hdr} ${TSEL_SH_SRC})
        set_tests_properties(tsel_sh_guard_live_${hdr} PROPERTIES
            PASS_REGULAR_EXPRESSION "the temperature-selection shadow is read-only: it may not include a live header")
    endforeach()
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
    # the J3 average seed at boot (stock 0x91ACA's 0x9095B gate, 0x91DAA's
    # tail; ~/tcu_sweep/temp_stage2a_port.md §1): the r04h boot sequence on
    # our batch + the tsel hook — no implausible selection; a negative
    # control (the old seeding) inside the test must reproduce r04h's −24 °C
    add_executable(test_j3seed tests/unit/test_j3seed.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_walk.c bsw/sup/sup_batch.c
        bsw/sup/sup_ctrl.c bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_token.c
        app/tsel_shadow.c app/tsel_hook.c tests/unit/stub_plat.c)
    add_test(NAME j3seed COMMAND test_j3seed)
    # STAGE 2b (2026-11-19, ~/tcu_sweep/temp_stage2b_port.md): the die /
    # board fault flags live — the dump's flags and consumers, the fluid-
    # sensor failure (0x89C13 → the board byte, the stock's 0x1405FA rule),
    # fault injection with the stock's 600 / 40 ms holds. The stand-in build
    # (TSEL_LIVE_FLAGS=0) must FAIL.
    set(TSEL2B_SOURCES tests/unit/test_tsel2b.c app/tsel_shadow.c app/tsel_hook.c app/tsel_live.c
        bsw/drv/drv_tsel.c bsw/drv/drv_temp.c app/tmp_chain.c safety/tmp_safe.c tests/unit/stub_plat.c)
    add_executable(test_tsel2b ${TSEL2B_SOURCES})
    target_compile_definitions(test_tsel2b PRIVATE TSEL_LIVE=1 TSEL_LIVE_FLAGS=1
        "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME tsel2b COMMAND test_tsel2b)
    add_executable(test_tsel2b_neg_standin ${TSEL2B_SOURCES})
    target_compile_definitions(test_tsel2b_neg_standin PRIVATE TSEL_LIVE=1 TSEL_LIVE_FLAGS=0
        "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME tsel2b_neg_standin COMMAND test_tsel2b_neg_standin)
    set_tests_properties(tsel2b_neg_standin PROPERTIES WILL_FAIL TRUE)
    add_executable(test_j3seed_live tests/unit/test_j3seed.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_walk.c bsw/sup/sup_batch.c
        bsw/sup/sup_ctrl.c bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_token.c
        app/tsel_shadow.c app/tsel_hook.c app/tsel_live.c bsw/drv/drv_tsel.c tests/unit/stub_plat.c)
    target_compile_definitions(test_j3seed_live PRIVATE TSEL_LIVE=1)
    add_test(NAME j3seed_live COMMAND test_j3seed_live)
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
    # the stock's rail-off full-on tests (ladder phases 4 / 0x10) against our
    # rail-open guard and the 0x84644 witness: the real supervisor (L3b, the
    # target configuration) + the real bus-A chain in main.c's schedule,
    # bus A in time (~/tcu_sweep/fullon_interlock_decode.md)
    add_executable(test_fullon tests/unit/test_fullon.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_fullon PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0
        SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0 SUP_L3_HOLD_SEQ_L=0)
    add_test(NAME fullon COMMAND test_fullon)
    # HOST DRY-RUN (2026-11-18, ~/tcu_sweep/diag_real_feedback_dryrun.md):
    # test_fullon's supervisor + bus-A chain with the feedback shadow on the
    # Timer F / D model, the consumers fed (test-only, at the 0x838B0 slot)
    # from the REAL chain vs the stand-in, over the bench waveforms
    add_executable(test_diag_realfb tests/unit/test_diag_realfb.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c tests/unit/fb_model.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_diag_realfb PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0
        SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0 SUP_L3_HOLD_SEQ_L=0)
    add_test(NAME diag_realfb_dryrun COMMAND test_diag_realfb)
    # STAGE 1b — THE RISK GATE (2026-11-18, ~/tcu_sweep/diag_temp_shadow_port.md):
    # the same dry run with 0x940FC forced valid and the ECU-temperature
    # shadow's 0x9393A fed to the feedback chain; zero 0x100 / 0x200 over
    # cold boot → ladder → 'U' + A's fill → P set → release D/P/R/P →
    # the garage-shift trajectories; a forced D estimate below 7300 must
    # set 0x100 and leave D driven
    add_executable(test_diag_temps tests/unit/test_diag_realfb.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c tests/unit/fb_model.c app/tsel_shadow.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_diag_temps PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0
        SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0 SUP_L3_HOLD_SEQ_L=0
        DIAG_TEMPS=1 FBS_9393A_HOST_SRC=g_fbs_9393a_host)
    add_test(NAME diag_temps_dryrun COMMAND test_diag_temps)
    # STAGE 2a (2026-11-18, ~/tcu_sweep/temp_stage2a_port.md): the same risk
    # gate with the selection LIVE through the real output path
    # (app/tsel_live.c → bsw/drv/drv_tsel.c, TSEL_LIVE=1), the feedback chain
    # on the live 0x9393A (FBS_T_ECU_LIVE=1) and drv_temp's pre-ADC
    # substitute window modelled
    add_executable(test_diag_temps_live tests/unit/test_diag_realfb.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c tests/unit/fb_model.c app/tsel_shadow.c
        app/tsel_live.c bsw/drv/drv_tsel.c
        tests/unit/stub_plat.c)
    target_compile_definitions(test_diag_temps_live PRIVATE SUP_L1_HOLD_PHASE8_REQUEST=0
        SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0 SUP_L3_HOLD_SEQ_L=0
        DIAG_TEMPS=1 TSEL_LIVE=1 FBS_T_ECU_LIVE=1)
    add_test(NAME diag_temps_dryrun_live COMMAND test_diag_temps_live)
    # G6c: the harness of test_g6b / test_g6b_gate* (the r04n-form builds of
    # this harness — test_gs_bridge_chain, test_gs_bridge_g2* — went with the
    # r04n bridge; their goldens stay, test_g6b_gate* compares against them).
    # THE GARAGE BRIDGE, STAGE G1 (2026-11-18, ~/tcu_sweep/garage_g1_port.md):
    # test_diag_realfb's supervisor + real bus-A chain + main.c's 10 ms task
    # (producer, vehicle sims, kl15, the range chain, park_tick, the garage
    # shadow, the bridge) over the gs-check (P D P R P, brake + engine 700).
    # (1) GS_LIVE_DIR 0: every bus-A word, the cell window and the park
    # demands identical with and without the bridge, frame by frame; the
    # recorder against the shadow's own. (2) GS_LIVE_DIR 3 (HOST ONLY — the
    # G2 gate): the bridged currents applied through the seam, the output
    # diagnosis on the real feedback silent; _temps with 0x940FC forced 8
    # (the current bounds).
    set(GSB_CHAIN_SOURCES tests/unit/test_gs_bridge_chain.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/fb_shadow.c app/fb_cal.c tests/unit/fb_model.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        app/gs_cal.c app/gs_bridge.c
        app/rng_chain.c safety/kl15.c app/range.c app/park_law.c app/park.c safety/park_mon.c
        bsw/sched/sched.c io/vehicle/profile_fca.c io/vehicle/vehicle.c bsw/com/com.c bsw/com/crc8.c
        tests/unit/stub_plat.c)
    set(GSB_CHAIN_DEFS SUP_L1_HOLD_PHASE8_REQUEST=0 SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0
        SUP_L3_HOLD_SEQ_L=0 DRV_RQ_USE_PRODUCER=1 GS_FORCED=1)
    # G4a (2026-11-19, ~/tcu_sweep/g4a_release_port.md): the live path with
    # the bridge LIVE (the SH configuration) against the per-frame trace of
    # HEAD bccfb9d (r04j, the instant-release stand-in): every frame of the
    # long-P runs (P >= release + B's ramp), every frame before the first
    # post-release commit of the 1 s / 0.3 s-P runs. The negative applies the
    # shadow's release through the seam and must FAIL.
    # G4b (2026-11-19, ~/tcu_sweep/g4b_release_bridge_port.md): the bridged
    # release. Each G2 gate runs against (1) the r04k REFERENCE trace (no
    # bridged release; written by the GSB_HOST_NO_REL_BRIDGE build, whose
    # own trace reproduces HEAD bccfb9d under the G4a rule — the *_r04k_ref
    # tests) with the differences confined to the release windows on the
    # wire, and (2) the exact G4b trace. The negative: the r04k behaviour
    # against the exact G4b trace must FAIL.
    set(GSB_GOLD ${CMAKE_SOURCE_DIR}/tests/unit/golden)
    # stage 2b (2026-11-19): TSEL_LIVE=1 now brings the fault flags live too
    # (TSEL_LIVE_FLAGS defaults to it — the image's configuration); the
    # stage-2a goldens are kept, the four flag cells hashed at their 2a value
    # 0 (GSB_HASH_FLAGS_AS_2A, test_gs_bridge_chain.c), everything else exact
    # stage 2b negative: the same build WITHOUT the 2a mask must FAIL against
    # the exact golden (the flags really are written in this harness)
    # G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): the G6a dual-run tests
    # (test_g6a*: the candidate beside the r04n bridge of the shadow) went
    # with the shadow and the r04n bridge form (D-G6-7); the image's form is
    # tested by test_g6b / test_g6b_gate* below.
    # the read-only frame of the candidate and the comparator: the source
    # scans (outputs may not name them, the comparator only reads) and THE
    # LINK-MAP SCAN of their objects (every undefined symbol in the allowed
    # set; no EEPROM / NVM / DSM / event-log / fatal routine; no EEPROM-cell
    # literal) — each with a negative fixture; the SH image runs the same
    # link scan as a POST_BUILD step of tcu_app.elf
    set(G6A_OUTPUT_PATHS ${GS_OUTPUT_PATHS} ${CMAKE_SOURCE_DIR}/bsw/sup/sup_mode.c
        ${CMAKE_SOURCE_DIR}/bsw/sup/sup_batch.c ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    add_test(NAME g6a_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py outputs ${G6A_OUTPUT_PATHS})
    add_test(NAME g6a_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan_fixture_output.c)
    set_tests_properties(g6a_scan_outputs_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME g6a_scan_cmp
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py cmp ${CMAKE_SOURCE_DIR}/app/g6a.c)
    # G6b: the monitor's rule (app/gs_bridge.c, inside the outputs scan) and its negative
    add_test(NAME g6b_scan_monitor_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py monitor
            ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan_fixture_monitor.c)
    set_tests_properties(g6b_scan_monitor_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME g6b_scan_monitor
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py monitor ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    add_test(NAME g6a_scan_cmp_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py cmp
            ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan_fixture_cmp.c)
    set_tests_properties(g6a_scan_cmp_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME g6a_scan_link
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py link nm
            ${CMAKE_BINARY_DIR}/CMakeFiles/test_g6b.dir/app/gs_cand.c.o
            ${CMAKE_BINARY_DIR}/CMakeFiles/test_g6b.dir/app/gs_cand_fill.c.o
            ${CMAKE_BINARY_DIR}/CMakeFiles/test_g6b.dir/app/g6a.c.o)
    add_library(g6a_link_fixture OBJECT tests/unit/g6a_scan_fixture_link.c)
    # G6c: the monitor's .noinit windows over a cold / warm / torn reset
    # (gs_bridge_init; the r04n bridge-logic test that carried it is gone)
    add_executable(test_g6c_windows tests/unit/test_g6c_windows.c app/gs_bridge.c)
    add_test(NAME g6c_monitor_windows_reset COMMAND test_g6c_windows)
    # G6c: the RAM-section overlap scan (POST_BUILD of tcu_app.elf) on its fixtures
    add_test(NAME g6c_sections_scan
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6c_sections.py --fixture
            ${CMAKE_SOURCE_DIR}/tests/unit/g6c_sections_fixture_ok.txt)
    add_test(NAME g6c_sections_scan_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6c_sections.py --fixture
            ${CMAKE_SOURCE_DIR}/tests/unit/g6c_sections_fixture_overlap.txt)
    set_tests_properties(g6c_sections_scan_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME g6a_scan_link_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py link nm $<TARGET_OBJECTS:g6a_link_fixture>)
    set_tests_properties(g6a_scan_link_negative PROPERTIES WILL_FAIL TRUE)
    # G6b (2026-11-19, ~/tcu_sweep/g6b_port.md): THE SWITCH on the real chain.
    # (1) THE G2 GATE IN G6b FORM (test_gs_bridge_chain.c's main, G6B_GATE):
    # the candidate drives A..E through the monitor; every frame against the
    # r04n goldens (the bridge of the shadow: the exact G4b traces, both
    # parities, the long-P / 2 s / 0.3 s P holds), the output diagnosis on the
    # real feedback silent (no fault word, switch-off, DSM report, would-be
    # reset); _bounds / _bounds_live with the current bounds (0x940FC 8).
    set(G6B_GATE_SOURCES ${GSB_CHAIN_SOURCES} app/gs_cand.c app/gs_cand_fill.c app/g6a.c)
    set(G6B_GATE_DEFS ${GSB_CHAIN_DEFS} GS_LIVE_DIR=3 GSC_G6A=1 GS_G6B=1 G6A_TEST=1 G6B_GATE=1)
    add_executable(test_g6b_gate ${G6B_GATE_SOURCES})
    target_compile_definitions(test_g6b_gate PRIVATE ${G6B_GATE_DEFS})
    add_test(NAME g6b_gate COMMAND test_g6b_gate
        ${GSB_GOLD}/gsb_live_g2_ref_r04k.txt ${GSB_GOLD}/gsb_live_g2_g4b.txt)
    add_executable(test_g6b_gate_bounds ${G6B_GATE_SOURCES} app/tsel_shadow.c)
    target_compile_definitions(test_g6b_gate_bounds PRIVATE ${G6B_GATE_DEFS}
        DIAG_TEMPS=1 FBS_9393A_HOST_SRC=g_fbs_9393a_host)
    add_test(NAME g6b_gate_bounds COMMAND test_g6b_gate_bounds
        ${GSB_GOLD}/gsb_live_g2_temps_ref_r04k.txt ${GSB_GOLD}/gsb_live_g2_temps_g4b.txt)
    add_executable(test_g6b_gate_bounds_live ${G6B_GATE_SOURCES} app/tsel_shadow.c
        app/tsel_live.c bsw/drv/drv_tsel.c)
    target_compile_definitions(test_g6b_gate_bounds_live PRIVATE ${G6B_GATE_DEFS}
        DIAG_TEMPS=1 TSEL_LIVE=1 FBS_T_ECU_LIVE=1 GSB_HASH_FLAGS_AS_2A=1)
    add_test(NAME g6b_gate_bounds_live COMMAND test_g6b_gate_bounds_live
        ${GSB_GOLD}/gsb_live_g2_temps_live_ref_r04k.txt ${GSB_GOLD}/gsb_live_g2_temps_live_g4b.txt)
    # (2) test_g6b: every G6a scenario in the r04n form and the G6b form in
    # one binary — wire + cell window identical frame for frame; the G6b
    # comparator (UNEXP 0, candidate == shadow), the E28E fence (boot, engine
    # stall), the diagnosis silent; the monitor negatives (bounds, envelope,
    # windows, permits, idle content, shadow) each caught in its pass and
    # latched. _englim: the 3.5 s limit at 50 passes, both forms. _bypass
    # (GSM_HOST_BYPASS: the monitor's checks skipped) must FAIL.
    set(G6B_SOURCES ${G6B_GATE_SOURCES})
    list(REMOVE_ITEM G6B_SOURCES tests/unit/test_gs_bridge_chain.c)
    list(APPEND G6B_SOURCES tests/unit/test_g6b.c)
    set(G6B_DEFS ${GSB_CHAIN_DEFS} GS_LIVE_DIR=3 GSC_G6A=1 GS_G6B=1)
    add_executable(test_g6b ${G6B_SOURCES})
    target_compile_definitions(test_g6b PRIVATE ${G6B_DEFS})
    # G6c (2026-11-19, ~/tcu_sweep/g6c_port.md): the r04o goldens of every
    # test_g6b run (written by HEAD fff32bd's G6b form, tests/unit/g6c_gold.h)
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_SOURCE_DIR}/tests/unit/golden/g6c_r04o_test_g6b.tgz
        DESTINATION ${CMAKE_BINARY_DIR}/g6c_r04o)
    # r04q (D-G6-13/14, ~/tcu_sweep/r04q_port.md): the boot fence is lifted
    # (the stock boot all-release + A's fill on the wire) and the stall
    # fence's source is the P content — test_g6b now compares with the r04q
    # goldens (this build's G6C_GOLD_OUT); g6q_vs_r04p_goldens proves the
    # r04p -> r04q difference of every run is confined to the boot window
    # (channel A only) and the stall fence's source label; the strict form
    # (nothing allowed) must FAIL
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_SOURCE_DIR}/tests/unit/golden/g6q_r04q_test_g6b.tgz
        DESTINATION ${CMAKE_BINARY_DIR}/g6q_r04q)
    # r04r (~/tcu_sweep/r04r_port.md): the live holder is retired from the
    # image (the engine is the sole producer of ch 0..6). test_g6b now
    # compares with the r04r goldens (written by this build, the harness
    # replaying the retired holder so its cells hold r04q's values:
    # G6R_HOLDER_REPLICA 1). g6r_vs_r04q_goldens proves the r04q -> r04r
    # difference of every run is ONLY gsb_t's agree_n / disagree_n (the
    # agreement witness's reference is the P content now; their sum
    # identical) — every frame (wire AND cell hash), tick, window, release
    # header and the end-of-run window byte-identical; strict must FAIL.
    # test_g6b_norep: the image's configuration (no holder at all) against
    # the same goldens — the wire hash identical on every frame, the end
    # window identical outside the holder's cells (tests/unit/g6c_gold.h
    # G6R_NOREP), every tick / gsb_t / window / release header exact.
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_SOURCE_DIR}/tests/unit/golden/g6r_r04r_test_g6b.tgz
        DESTINATION ${CMAKE_BINARY_DIR}/g6r_r04r)
    # r04s (~/tcu_sweep/r04s_port.md): the pinned element sub-objects,
    # 0xDC0F8 (E6C1 / E6C2) with the reversal's consumers, 0x106698's
    # prologue / epilogue. test_g6b now compares with the r04s goldens
    # (gold/, this build's G6C_GOLD_OUT; + the new run "R -> D direct").
    # g6s_vs_r04r_goldens proves the r04r -> r04s difference of every run is
    # confined to the "D -> R direct" reversal (channels D and B from its R
    # start, the two engagement timers, D's fill time −10 ms) and that the
    # new run is identical to what the r04r engine does (gold_r04r_extra/,
    # written by HEAD 2884c9b with only the scenario added); strict must FAIL
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_SOURCE_DIR}/tests/unit/golden/g6s_r04s_test_g6b.tgz
        DESTINATION ${CMAKE_BINARY_DIR}/g6s_r04s)
    set(G6C_GOLD_ENV "G6C_GOLD_IN=${CMAKE_BINARY_DIR}/g6s_r04s/gold")
    add_test(NAME g6s_vs_r04r_goldens
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6s_gold_diff.py ${CMAKE_BINARY_DIR}/g6r_r04r/gold
            ${CMAKE_BINARY_DIR}/g6s_r04s/gold_r04r_extra ${CMAKE_BINARY_DIR}/g6s_r04s/gold)
    add_test(NAME g6s_vs_r04r_goldens_strict_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6s_gold_diff.py ${CMAKE_BINARY_DIR}/g6r_r04r/gold
            ${CMAKE_BINARY_DIR}/g6s_r04s/gold_r04r_extra ${CMAKE_BINARY_DIR}/g6s_r04s/gold --strict)
    set_tests_properties(g6s_vs_r04r_goldens_strict_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME g6r_vs_r04q_goldens
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6r_gold_diff.py ${CMAKE_BINARY_DIR}/g6q_r04q/gold
            ${CMAKE_BINARY_DIR}/g6r_r04r/gold)
    add_test(NAME g6r_vs_r04q_goldens_strict_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6r_gold_diff.py ${CMAKE_BINARY_DIR}/g6q_r04q/gold
            ${CMAKE_BINARY_DIR}/g6r_r04r/gold --strict)
    set_tests_properties(g6r_vs_r04q_goldens_strict_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME g6q_vs_r04p_goldens
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6q_gold_diff.py ${CMAKE_BINARY_DIR}/g6c_r04o/gold
            ${CMAKE_BINARY_DIR}/g6q_r04q/gold --skip-tag candidate____shadow_2_passes__C_fill_)
    add_test(NAME g6q_vs_r04p_goldens_strict_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6q_gold_diff.py ${CMAKE_BINARY_DIR}/g6c_r04o/gold
            ${CMAKE_BINARY_DIR}/g6q_r04q/gold --strict --skip-tag candidate____shadow_2_passes__C_fill_)
    set_tests_properties(g6q_vs_r04p_goldens_strict_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME g6b_switch COMMAND test_g6b)
    add_executable(test_g6b_englim ${G6B_SOURCES})
    target_compile_definitions(test_g6b_englim PRIVATE ${G6B_DEFS} GSB_ENG_PASSES=50u G6A_ENGLIM=1)
    add_test(NAME g6b_switch_englim COMMAND test_g6b_englim)
    set_tests_properties(g6b_switch g6b_switch_englim PROPERTIES ENVIRONMENT "${G6C_GOLD_ENV}")
    # r04r: the image's configuration — no holder linked, no replica
    set(G6B_NOREP_SOURCES ${G6B_SOURCES})
    list(REMOVE_ITEM G6B_NOREP_SOURCES app/rq_prod.c app/rq_fill.c)
    add_executable(test_g6b_norep ${G6B_NOREP_SOURCES})
    target_compile_definitions(test_g6b_norep PRIVATE ${G6B_DEFS} G6R_HOLDER_REPLICA=0 G6R_NOREP=1)
    add_test(NAME g6r_switch_norep COMMAND test_g6b_norep)
    add_executable(test_g6b_norep_englim ${G6B_NOREP_SOURCES})
    target_compile_definitions(test_g6b_norep_englim PRIVATE ${G6B_DEFS} G6R_HOLDER_REPLICA=0 G6R_NOREP=1
        GSB_ENG_PASSES=50u G6A_ENGLIM=1)
    add_test(NAME g6r_switch_norep_englim COMMAND test_g6b_norep_englim)
    set_tests_properties(g6r_switch_norep g6r_switch_norep_englim PROPERTIES ENVIRONMENT "${G6C_GOLD_ENV}")
    # G5b (2026-11-19, ~/tcu_sweep/g5b_port.md): THE OS MODE LIVE ON THE REAL
    # CHAIN — test_g6b_norep (the image's configuration) with app/main.c's
    # selector / echo / slot-26 flags / hand-off (G5B_SEL, safety/os_sel.c)
    # and the monitor's cross-check (GS_M2_XCHECK); every run of test_g6b +
    # two G5b runs (the 0x108 sim started after boot, one stray frame)
    # (G5B_SCEN). Compared with the r04t goldens (written by HEAD 986dfc3's
    # test_g6b_norep with the same harness and the same mask, G5B_HASH_MASK:
    # the OS-mode cells and the selector object, gsb_t.m2): every bus-A word
    # and the park demands per frame (the wire), every OTHER byte of the
    # cell window per frame and at the end, every tick, gsb_t, the windows,
    # the release header — identical. Plus per run: the live mode (2 at the
    # first mode-1 batch with the sims, latched; 1 without), the flags, the
    # m2 cross-check with zero disagreeing passes.
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_SOURCE_DIR}/tests/unit/golden/g5b_r04t_test_g6b.tgz
        DESTINATION ${CMAKE_BINARY_DIR}/g5b_r04t)
    set(G5B_SOURCES ${G6B_NOREP_SOURCES} safety/os_sel.c)
    set(G5B_DEFS ${G6B_DEFS} G6R_HOLDER_REPLICA=0 G6R_NOREP=1 G5B_SEL=1 G5B_SCEN=1 G5B_HASH_MASK=1
        GS_M2_XCHECK=1)
    # G5c: the G5b runs are now part of test_g5c below (the request objects'
    # +0x57 differs at the end since 0x89C3D is honest, and the stray-frame
    # run's D commit waits for the settle counter — both by design); the
    # r04t goldens are kept unpacked for the record only
    # G5c (2026-11-19, ~/tcu_sweep/g5c_port.md): 0x89C3D HONEST — the engine
    # reads slot 26's live flag. test_g5c = test_g5b (the image's
    # configuration, the live selector) + the G5c cases (G5C_SCEN) + the
    # request objects' +0x57 not compared at the end (G5C_MASK, counted),
    # against the r04u goldens (written by HEAD 5698464's engine with this
    # harness, G6C_GOLD_OUT; g5c_r04u_test_g6b.tgz): every from-power-on run
    # and every monitor negative must stay IDENTICAL (wire, ticks, gsb_t,
    # windows, cell window per frame and at the end); the five G5c cases
    # (mode 2 not granted, lever D inside the 240 ms settle, the stray frame,
    # the two key-off cycles) must follow the stock's rule (g5c_check). The
    # negative: the forced 0 (GS_C3D_FORCED 1) must FAIL.
    file(ARCHIVE_EXTRACT INPUT ${CMAKE_SOURCE_DIR}/tests/unit/golden/g5c_r04u_test_g6b.tgz
        DESTINATION ${CMAKE_BINARY_DIR}/g5c_r04u)
    set(G5C_DEFS ${G5B_DEFS} G5C_SCEN=1 G5C_MASK=1)
    add_executable(test_g5c ${G5B_SOURCES})
    target_compile_definitions(test_g5c PRIVATE ${G5C_DEFS})
    add_test(NAME g5c_wire_vs_r04u COMMAND test_g5c)
    add_executable(test_g5c_englim ${G5B_SOURCES})
    target_compile_definitions(test_g5c_englim PRIVATE ${G5C_DEFS} GSB_ENG_PASSES=50u G6A_ENGLIM=1)
    add_test(NAME g5c_wire_vs_r04u_englim COMMAND test_g5c_englim)
    add_executable(test_g5c_neg_forced ${G5B_SOURCES})
    target_compile_definitions(test_g5c_neg_forced PRIVATE ${G5C_DEFS} GS_C3D_FORCED=1)
    add_test(NAME g5c_forced_c3d_negative COMMAND test_g5c_neg_forced)
    set_tests_properties(g5c_forced_c3d_negative PROPERTIES WILL_FAIL TRUE)
    # the G5b negative, carried over: a mode-2-dependent output (park_law's
    # c3c fed "mode 3" in mode 2) must FAIL the same comparison
    add_executable(test_g5c_neg_park ${G5B_SOURCES})
    target_compile_definitions(test_g5c_neg_park PRIVATE ${G5C_DEFS} G5B_NEG_MODE2_PARK=1)
    add_test(NAME g5c_wire_vs_r04u_negative COMMAND test_g5c_neg_park)
    set_tests_properties(g5c_wire_vs_r04u_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(g5c_wire_vs_r04u g5c_wire_vs_r04u_englim g5c_forced_c3d_negative
        g5c_wire_vs_r04u_negative PROPERTIES
        ENVIRONMENT "G6C_GOLD_IN=${CMAKE_BINARY_DIR}/g5c_r04u/gold")
    # THE SHIFT-MAP SHADOW (2026-11-19, ~/tcu_sweep/sm_shadow_port.md): the
    # stock's shift target (30 ms entries 5 / 14 / 15 / 16 → A697 / A0AA /
    # 0x89CE8) as a read-only shadow (app/sm_shadow.c). sm_shadow: the dump
    # cells, standstill, the decode's §7 sweeps, the step limits, invariants;
    # sm_cal_bytes: every cal copy against the image; _neg_MAP (one map cell
    # +1) must FAIL. sm_identity: test_g6b_norep (the image's configuration,
    # the r04r goldens: every bus-A word, the park demands, the cell window,
    # every tick) with the shadow at main.c's 30 ms slot must pass unchanged
    # and show the shadow ran; the leak negative (the shadow's target written
    # into the live 0x89CE8) must FAIL. The scans and the SH compile checks.
    set(SM_TEST_SOURCES tests/unit/test_sm_shadow.c app/sm_shadow.c app/sm_cal.c app/sm_gear.c)
    add_executable(test_sm_shadow ${SM_TEST_SOURCES})
    target_compile_definitions(test_sm_shadow PRIVATE _POSIX_C_SOURCE=200809L
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME sm_shadow COMMAND test_sm_shadow)
    add_test(NAME sm_cal_bytes COMMAND test_sm_shadow --cal)
    set_tests_properties(sm_cal_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_sm_shadow_neg_MAP ${SM_TEST_SOURCES})
    target_compile_definitions(test_sm_shadow_neg_MAP PRIVATE _POSIX_C_SOURCE=200809L SM_NEG_MAP=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME sm_shadow_neg_MAP COMMAND test_sm_shadow_neg_MAP)
    set_tests_properties(sm_shadow_neg_MAP PROPERTIES WILL_FAIL TRUE)
    # the gear feedback chain (2026-11-19, ~/tcu_sweep/gear_feedback_shadow_port.md):
    # its tests are part of test_sm_shadow ((6) boot / D / R / P, (6f) the
    # drive ramp, (6g) the dump); the cal mutation (the next gear 1 → 2 made
    # 1) must FAIL; the hook-side negatives: the gear gather reading a cell the
    # chain models, and an output path naming the chain
    add_executable(test_sm_shadow_neg_GNEXT ${SM_TEST_SOURCES})
    target_compile_definitions(test_sm_shadow_neg_GNEXT PRIVATE _POSIX_C_SOURCE=200809L SM_NEG_GNEXT=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME sm_gear_neg_GNEXT COMMAND test_sm_shadow_neg_GNEXT)
    set_tests_properties(sm_gear_neg_GNEXT PROPERTIES WILL_FAIL TRUE)
    add_test(NAME sm_gear_scan_hook_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/tests/unit/sm_gear_fixture_hook.c)
    add_test(NAME sm_gear_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/sm_gear_fixture_output.c)
    set_tests_properties(sm_gear_scan_hook_negative sm_gear_scan_outputs_negative PROPERTIES WILL_FAIL TRUE)
    add_executable(test_g6b_norep_gfleak ${G6B_NOREP_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c)
    target_compile_definitions(test_g6b_norep_gfleak PRIVATE ${G6B_DEFS} G6R_HOLDER_REPLICA=0 G6R_NOREP=1
        SM_TEST=1 SM_HOOK_NO_GATHER=1 SM_NEG_GLEAK=1)
    add_test(NAME sm_gear_identity_negative_leak COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_identity.py
        $<TARGET_FILE:test_g6b_norep_gfleak>)
    set_tests_properties(sm_gear_identity_negative_leak PROPERTIES WILL_FAIL TRUE ENVIRONMENT "${G6C_GOLD_ENV}")
    add_executable(test_g6b_norep_sm ${G6B_NOREP_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c)
    target_compile_definitions(test_g6b_norep_sm PRIVATE ${G6B_DEFS} G6R_HOLDER_REPLICA=0 G6R_NOREP=1
        SM_TEST=1 SM_HOOK_NO_GATHER=1)
    add_test(NAME sm_identity COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_identity.py
        $<TARGET_FILE:test_g6b_norep_sm>)
    add_executable(test_g6b_norep_smleak ${G6B_NOREP_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c)
    target_compile_definitions(test_g6b_norep_smleak PRIVATE ${G6B_DEFS} G6R_HOLDER_REPLICA=0 G6R_NOREP=1
        SM_TEST=1 SM_HOOK_NO_GATHER=1 SM_NEG_LEAK=1)
    add_test(NAME sm_identity_negative_leak COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_identity.py
        $<TARGET_FILE:test_g6b_norep_smleak>)
    # the replica form (test_g6b: the retired holder replayed so the per-frame
    # cell-window hash is compared too, not only the wire and the end window)
    add_executable(test_g6b_sm ${G6B_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c)
    target_compile_definitions(test_g6b_sm PRIVATE ${G6B_DEFS} SM_TEST=1 SM_HOOK_NO_GATHER=1)
    add_test(NAME sm_identity_cells COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_identity.py
        $<TARGET_FILE:test_g6b_sm>)
    add_executable(test_g6b_smleak ${G6B_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c)
    target_compile_definitions(test_g6b_smleak PRIVATE ${G6B_DEFS} SM_TEST=1 SM_HOOK_NO_GATHER=1
        SM_NEG_LEAK=1)
    add_test(NAME sm_identity_cells_negative_leak COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_identity.py
        $<TARGET_FILE:test_g6b_smleak>)
    set_tests_properties(sm_identity_cells_negative_leak PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(sm_identity sm_identity_negative_leak sm_identity_cells sm_identity_cells_negative_leak
        PROPERTIES ENVIRONMENT "${G6C_GOLD_ENV}")
    set_tests_properties(sm_identity_negative_leak PROPERTIES WILL_FAIL TRUE)
    set(SM_OUTPUT_PATHS ${TSEL_OUTPUT_PATHS}
        ${CMAKE_SOURCE_DIR}/app/gs_cand.c ${CMAKE_SOURCE_DIR}/app/gs_engine.c ${CMAKE_SOURCE_DIR}/app/gs_bridge.c
        ${CMAKE_SOURCE_DIR}/app/g6a.c ${CMAKE_SOURCE_DIR}/app/rng_chain.c ${CMAKE_SOURCE_DIR}/app/park.c
        ${CMAKE_SOURCE_DIR}/app/park_law.c ${CMAKE_SOURCE_DIR}/app/tmp_chain.c)
    add_test(NAME sm_shadow_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py outputs ${SM_OUTPUT_PATHS})
    add_test(NAME sm_shadow_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/sm_scan_fixture_output.c)
    add_test(NAME sm_shadow_scan_shadow
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/app/sm_shadow.c ${CMAKE_SOURCE_DIR}/app/sm_shadow.h
            ${CMAKE_SOURCE_DIR}/app/sm_cal.c ${CMAKE_SOURCE_DIR}/app/sm_cal.h
            ${CMAKE_SOURCE_DIR}/app/sm_gear.c)
    add_test(NAME sm_shadow_scan_shadow_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/tests/unit/sm_scan_fixture_shadow.c)
    add_test(NAME sm_shadow_scan_hook
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py hook ${CMAKE_SOURCE_DIR}/app/sm_hook.c)
    add_test(NAME sm_shadow_scan_hook_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/sm_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/tests/unit/sm_scan_fixture_hook.c)
    set_tests_properties(sm_shadow_scan_outputs_negative sm_shadow_scan_shadow_negative
        sm_shadow_scan_hook_negative PROPERTIES WILL_FAIL TRUE)
    set(SM_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_sm.c)
    add_test(NAME sm_sh_guard_control COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${SM_SH_SRC})
    add_test(NAME sm_sh_guard_shadow
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/sm_shadow.c)
    add_test(NAME sm_sh_guard_cal
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/sm_cal.c)
    add_test(NAME sm_sh_guard_gear
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion -Werror ${CMAKE_SOURCE_DIR}/app/sm_gear.c)
    add_test(NAME sm_sh_guard_hook
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform -Wall -Wextra -Wconversion
            ${CMAKE_SOURCE_DIR}/app/sm_hook.c)
    foreach(g OUTPUT OUTCAL OUTTU)
        add_test(NAME sm_sh_guard_${g} COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform
            -DSH_GUARD_SM_${g} ${SM_SH_SRC})
        set_tests_properties(sm_sh_guard_${g} PROPERTIES
            PASS_REGULAR_EXPRESSION "the shift-map shadow is read-only: an output path may not include")
    endforeach()
    foreach(hdr sup_cells drv_cells drv_odiag drv_temp platform)
        add_test(NAME sm_sh_guard_live_${hdr} COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform
            -DSH_GUARD_SM_LIVE_${hdr} ${SM_SH_SRC})
        set_tests_properties(sm_sh_guard_live_${hdr} PROPERTIES
            PASS_REGULAR_EXPRESSION "the shift-map shadow is read-only: it may not include a live header")
    endforeach()
    # THE FAULT-REACTION SHADOW (F1, 2026-11-19, ~/tcu_sweep/f1_fr_shadow_port.md):
    # the RX status, the comm monitor, the generic fault manager (RAM only, all
    # 195 configs), the substitute-gear code, 0x89CF5 and the fault-fed hinge as
    # a read-only shadow (app/fr_shadow.c, fr_fm.c). fr_shadow: the RX status,
    # the plain boot / sims / heal / key-cycle timelines, the dump's masks, the
    # cell window untouched; fr_cal_bytes; _neg_CFG (group 136's fail time +2)
    # must FAIL. fr_identity: test_g6b_norep / test_g6b with the shadow ticking
    # (the goldens: every bus-A word, the park demands, the cell window) must
    # pass unchanged, the shadow must have run, its U1267 / P0850 verdicts must
    # agree with rng_chain's pass for pass; the leak negative must FAIL. The
    # scans (outputs / shadow / cal / hook, nvm) and the SH compile checks.
    set(FR_TEST_SOURCES tests/unit/test_fr_shadow.c app/fr_shadow.c app/fr_fm.c app/fr_cal.c app/fr_hook.c
        app/sm_cal.c bsw/com/crc8.c)
    add_executable(test_fr_shadow ${FR_TEST_SOURCES})
    target_compile_definitions(test_fr_shadow PRIVATE _POSIX_C_SOURCE=200809L FR_HOOK_NO_GATHER=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"" "TCU_STOCK_DUMP=\"${TCU_STOCK_DUMP}\"")
    add_test(NAME fr_shadow COMMAND test_fr_shadow)
    add_test(NAME fr_cal_bytes COMMAND test_fr_shadow --cal)
    set_tests_properties(fr_cal_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_fr_shadow_neg_CFG ${FR_TEST_SOURCES})
    target_compile_definitions(test_fr_shadow_neg_CFG PRIVATE _POSIX_C_SOURCE=200809L FR_HOOK_NO_GATHER=1
        FR_NEG_CFG=1)
    add_test(NAME fr_shadow_neg_CFG COMMAND test_fr_shadow_neg_CFG)
    set_tests_properties(fr_shadow_neg_CFG PROPERTIES WILL_FAIL TRUE)
    set(FR_APP_SOURCES app/fr_shadow.c app/fr_fm.c app/fr_cal.c app/fr_hook.c)
    add_executable(test_g6b_norep_fr ${G6B_NOREP_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c
        ${FR_APP_SOURCES})
    target_compile_definitions(test_g6b_norep_fr PRIVATE ${G6B_DEFS} G6R_HOLDER_REPLICA=0 G6R_NOREP=1
        SM_TEST=1 SM_HOOK_NO_GATHER=1 FR_TEST=1 FR_HOOK_NO_GATHER=1)
    add_test(NAME fr_identity COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_identity.py
        $<TARGET_FILE:test_g6b_norep_fr>)
    add_executable(test_g6b_fr ${G6B_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c
        ${FR_APP_SOURCES})
    target_compile_definitions(test_g6b_fr PRIVATE ${G6B_DEFS} SM_TEST=1 SM_HOOK_NO_GATHER=1
        FR_TEST=1 FR_HOOK_NO_GATHER=1)
    add_test(NAME fr_identity_cells COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_identity.py
        $<TARGET_FILE:test_g6b_fr>)
    add_executable(test_g6b_frleak ${G6B_SOURCES} app/sm_shadow.c app/sm_cal.c app/sm_gear.c app/sm_hook.c
        ${FR_APP_SOURCES})
    target_compile_definitions(test_g6b_frleak PRIVATE ${G6B_DEFS} SM_TEST=1 SM_HOOK_NO_GATHER=1
        FR_TEST=1 FR_HOOK_NO_GATHER=1 FR_NEG_LEAK=1)
    add_test(NAME fr_identity_negative_leak COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_identity.py
        $<TARGET_FILE:test_g6b_frleak>)
    set_tests_properties(fr_identity fr_identity_cells fr_identity_negative_leak
        PROPERTIES ENVIRONMENT "${G6C_GOLD_ENV}")
    set_tests_properties(fr_identity_negative_leak PROPERTIES WILL_FAIL TRUE)
    set(FR_OUTPUT_PATHS ${SM_OUTPUT_PATHS} ${CMAKE_SOURCE_DIR}/app/sm_shadow.c ${CMAKE_SOURCE_DIR}/app/sm_gear.c
        ${CMAKE_SOURCE_DIR}/app/sm_hook.c ${CMAKE_SOURCE_DIR}/bsw/com/com.c ${CMAKE_SOURCE_DIR}/io/vehicle/profile_fca.c)
    add_test(NAME fr_shadow_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py outputs ${FR_OUTPUT_PATHS})
    add_test(NAME fr_shadow_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/fr_scan_fixture_output.c)
    add_test(NAME fr_shadow_scan_shadow
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/app/fr_shadow.c ${CMAKE_SOURCE_DIR}/app/fr_shadow.h
            ${CMAKE_SOURCE_DIR}/app/fr_fm.c ${CMAKE_SOURCE_DIR}/app/fr_cal.h)
    add_test(NAME fr_shadow_scan_shadow_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py shadow
            ${CMAKE_SOURCE_DIR}/tests/unit/fr_scan_fixture_shadow.c)
    add_test(NAME fr_shadow_scan_cal
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py cal ${CMAKE_SOURCE_DIR}/app/fr_cal.c)
    add_test(NAME fr_shadow_scan_cal_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py cal
            ${CMAKE_SOURCE_DIR}/tests/unit/fr_scan_fixture_cal.c)
    add_test(NAME fr_shadow_scan_hook
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py hook ${CMAKE_SOURCE_DIR}/app/fr_hook.c)
    add_test(NAME fr_shadow_scan_hook_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/fr_shadow_scan.py hook
            ${CMAKE_SOURCE_DIR}/tests/unit/fr_scan_fixture_hook.c)
    set_tests_properties(fr_shadow_scan_outputs_negative fr_shadow_scan_shadow_negative
        fr_shadow_scan_cal_negative fr_shadow_scan_hook_negative PROPERTIES WILL_FAIL TRUE)
    # the NVM / EEPROM / DSM-record / fatal scan (g6a_scan.py nvm) on the host
    # objects (the SH objects: tcu_app.elf's POST_BUILD) and its negative
    add_library(fr_nvm_objs OBJECT ${FR_APP_SOURCES})
    target_compile_definitions(fr_nvm_objs PRIVATE FR_HOOK_NO_GATHER=1)
    add_test(NAME fr_scan_nvm
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py nvm nm $<TARGET_OBJECTS:fr_nvm_objs>
        COMMAND_EXPAND_LISTS)
    add_library(fr_nvm_fixture OBJECT tests/unit/fr_nvm_fixture.c)
    add_test(NAME fr_scan_nvm_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py nvm nm $<TARGET_OBJECTS:fr_nvm_fixture>)
    set_tests_properties(fr_scan_nvm_negative PROPERTIES WILL_FAIL TRUE)
    set(FR_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_fr.c)
    add_test(NAME fr_sh_guard_control COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${FR_SH_SRC})
    add_test(NAME fr_sh_guard_shadow
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/fr_shadow.c)
    add_test(NAME fr_sh_guard_fm
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/fr_fm.c)
    add_test(NAME fr_sh_guard_cal
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/fr_cal.c)
    add_test(NAME fr_sh_guard_hook
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform -Wall -Wextra -Wconversion
            ${CMAKE_SOURCE_DIR}/app/fr_hook.c)
    foreach(g OUTPUT OUTCAL OUTTU)
        add_test(NAME fr_sh_guard_${g} COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform
            -DSH_GUARD_FR_${g} ${FR_SH_SRC})
        set_tests_properties(fr_sh_guard_${g} PROPERTIES
            PASS_REGULAR_EXPRESSION "the fault-reaction shadow is read-only: an output path may not include")
    endforeach()
    foreach(hdr sup_cells drv_cells drv_odiag drv_temp platform)
        add_test(NAME fr_sh_guard_live_${hdr} COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -I${CMAKE_SOURCE_DIR}/platform
            -DSH_GUARD_FR_LIVE_${hdr} ${FR_SH_SRC})
        set_tests_properties(fr_sh_guard_live_${hdr} PROPERTIES
            PASS_REGULAR_EXPRESSION "the fault-reaction shadow is read-only: it may not include a live header")
    endforeach()
    # r04r: the retired holder stays out of the image (the POST_BUILD scan of
    # tcu_app.elf runs the same rule on the real link map); its fixtures
    add_test(NAME g6r_scan_retired_ok
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py retired --nm-text
            ${CMAKE_SOURCE_DIR}/tests/unit/g6r_retired_fixture_ok.txt)
    add_test(NAME g6r_scan_retired_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py retired --nm-text
            ${CMAKE_SOURCE_DIR}/tests/unit/g6r_retired_fixture_nm.txt)
    set_tests_properties(g6r_scan_retired_negative PROPERTIES WILL_FAIL TRUE)
    # G5b (D-G5-7): the whole-image scan for the EEPROM / NVM routines and
    # the mode-2-only functions kept out (POST_BUILD of tcu_app.elf); its
    # fixtures (the ok one is the retired-holder ok fixture: a clean excerpt)
    add_test(NAME g5b_scan_image_ok
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py image --nm-text
            ${CMAKE_SOURCE_DIR}/tests/unit/g6r_retired_fixture_ok.txt)
    add_test(NAME g5b_scan_image_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/g6a_scan.py image --nm-text
            ${CMAKE_SOURCE_DIR}/tests/unit/g5b_image_fixture_nm.txt)
    set_tests_properties(g5b_scan_image_negative PROPERTIES WILL_FAIL TRUE)
    add_executable(test_g6b_bypass ${G6B_SOURCES})
    target_compile_definitions(test_g6b_bypass PRIVATE ${G6B_DEFS} GSM_HOST_BYPASS=1 G6B_BYPASS=1)
    add_test(NAME g6b_monitor_bypass_negative COMMAND test_g6b_bypass)
    set_tests_properties(g6b_monitor_bypass_negative PROPERTIES ENVIRONMENT "${G6C_GOLD_ENV}")
    set_tests_properties(g6b_monitor_bypass_negative PROPERTIES WILL_FAIL TRUE)
    # G6c: the r04n bridge's logic tests (test_gs_bridge_logic / _live /
    # _mut / _engt / _rellen / _rel_norel / _rel_fresh, test_gs_dual_bridge_*)
    # went with the r04n bridge form; the monitor's equivalents (bounds,
    # envelope, 125-pass release limit, 3.5 s engagement limit, windows,
    # permits) are test_g6b's negatives
    # the compile-time element bounds against the image's bytes (role tables
    # 0x6312F / 0x63139 / 0x63143, table 0x505A4); the mutated table must FAIL
    set(GSB_TABLE_SOURCES tests/unit/test_gs_bridge_tables.c app/gs_bridge.c tests/unit/gs_ref_shadow.c
        app/gs_fill.c app/gs_cal.c app/rq_fill_cal.c tests/unit/stub_plat.c)
    add_executable(test_gs_bridge_tables ${GSB_TABLE_SOURCES})
    target_compile_definitions(test_gs_bridge_tables PRIVATE GS_FORCED=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"")
    add_test(NAME gs_bridge_tables_bytes COMMAND test_gs_bridge_tables)
    set_tests_properties(gs_bridge_tables_bytes PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(test_gs_bridge_tables_mut ${GSB_TABLE_SOURCES})
    target_compile_definitions(test_gs_bridge_tables_mut PRIVATE GS_FORCED=1 GSB_HOST_MUT_TABLE=1
        "TCU_STOCK_IMAGE=\"${TCU_STOCK_IMAGE}\"")
    add_test(NAME gs_bridge_tables_bytes_mutation COMMAND test_gs_bridge_tables_mut)
    set_tests_properties(gs_bridge_tables_bytes_mutation PROPERTIES WILL_FAIL TRUE)
    # the ONE SEAM: no output path names the bridge but drv_reqlaw.c's one
    # drv_gsbr_take( call inside `#if GS_LIVE_DIR != 0`; the bridge reads
    # live cells only through const volatile and calls no live writer; each
    # scan with a negative fixture; the SH compile checks
    set(GSB_OUTPUT_PATHS ${FB_OUTPUT_PATHS})
    add_test(NAME gs_bridge_scan_outputs
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan.py outputs ${GSB_OUTPUT_PATHS})
    add_test(NAME gs_bridge_scan_outputs_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan.py outputs
            ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan_fixture_output.c)
    set_tests_properties(gs_bridge_scan_outputs_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME gs_bridge_scan_seam_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan.py seam
            ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan_fixture_seam.c)
    set_tests_properties(gs_bridge_scan_seam_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME gs_bridge_scan_bridge
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan.py bridge
            ${CMAKE_SOURCE_DIR}/app/gs_bridge.c ${CMAKE_SOURCE_DIR}/app/gs_bridge.h)
    add_test(NAME gs_bridge_scan_bridge_negative
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan.py bridge
            ${CMAKE_SOURCE_DIR}/tests/unit/gs_bridge_scan_fixture_bridge.c)
    set_tests_properties(gs_bridge_scan_bridge_negative PROPERTIES WILL_FAIL TRUE)
    set(GSB_SH_SRC ${CMAKE_SOURCE_DIR}/tests/unit/sh_guard_gsbr.c)
    add_test(NAME gs_bridge_sh_guard_control COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_bridge
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    add_test(NAME gs_bridge_sh_guard_output
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DSH_GUARD_GSBR_OUTPUT ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_g1
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_LIVE_DIR=1 ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_dir2
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_LIVE_DIR=2 ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_dir4
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_LIVE_DIR=4 ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_forced
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_LIVE_DIR=3 -DGS_FORCED=0 ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    add_test(NAME gs_bridge_sh_guard_ch5
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGSB_SH_MUT_CH=5 ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    add_test(NAME gs_bridge_sh_guard_mut
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGSB_HOST_MUT_TABLE ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_engt
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGSB_ENG_PASSES=50 ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_rellen
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGSB_REL_PASSES=50 ${GSB_SH_SRC})
    add_test(NAME gs_bridge_sh_guard_relneg
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGSB_HOST_NO_REL_BRIDGE ${GSB_SH_SRC})
    set_tests_properties(gs_bridge_sh_guard_rellen PROPERTIES
        PASS_REGULAR_EXPRESSION "GSB_REL_PASSES is a host-only test override")
    set_tests_properties(gs_bridge_sh_guard_relneg PROPERTIES
        PASS_REGULAR_EXPRESSION "the release negatives are host-only tests")
    # G6b SH guards: the monitor bypass is host-only; a live seam on SH is the
    # G6b form; G6b needs the seam; the control (G6b, -Wall -Wextra -Wconversion)
    add_test(NAME g6b_sh_guard_control
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -Wall -Wextra -Wconversion -DGS_LIVE_DIR=3 -DGS_G6B=1
            ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    # r04q: the boot allowance's bound is a host-only override (15 on SH)
    add_test(NAME g6q_sh_guard_bootlen
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_LIVE_DIR=3 -DGS_G6B=1 -DGSB_BOOT_PASSES=5u
            ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    set_tests_properties(g6q_sh_guard_bootlen PROPERTIES
        PASS_REGULAR_EXPRESSION "GSB_BOOT_PASSES is a host-only test override")
    add_test(NAME g6b_sh_guard_bypass
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_LIVE_DIR=3 -DGS_G6B=1 -DGSM_HOST_BYPASS
            ${CMAKE_SOURCE_DIR}/app/gs_bridge.c)
    set_tests_properties(g6b_sh_guard_bypass PROPERTIES
        PASS_REGULAR_EXPRESSION "the monitor bypass is a host-only negative test")
    add_test(NAME g6b_sh_guard_r04n_form
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_LIVE_DIR=3 -DGS_G6B=0 ${GSB_SH_SRC})
    set_tests_properties(g6b_sh_guard_r04n_form PROPERTIES
        PASS_REGULAR_EXPRESSION "on SH builds a live seam carries the candidate under the monitor")
    add_test(NAME g6b_sh_guard_noseam
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DGS_G6B=1 ${GSB_SH_SRC})
    set_tests_properties(g6b_sh_guard_noseam PROPERTIES
        PASS_REGULAR_EXPRESSION "G6b .GS_G6B 1. needs the seam for D and R")
    add_test(NAME gs_bridge_sh_guard_ramapp
        COMMAND ${RQ_SH_CC} ${RQ_SH_ARGS} -DTCU_RAM_APP -DGS_LIVE_DIR=3 ${GSB_SH_SRC})
    set_tests_properties(gs_bridge_sh_guard_output PROPERTIES
        PASS_REGULAR_EXPRESSION "the garage bridge is read-only in G1: an output path may not include gs_bridge.h")
    set_tests_properties(gs_bridge_sh_guard_g1 PROPERTIES
        PASS_REGULAR_EXPRESSION "stage G2 - on SH builds the bridge is off .0. or live for D and R .3.")
    set_tests_properties(gs_bridge_sh_guard_dir2 gs_bridge_sh_guard_dir4 PROPERTIES
        PASS_REGULAR_EXPRESSION "GS_LIVE_DIR is 0 .off., 1 .D only. or 3 .D and R., nothing else")
    set_tests_properties(gs_bridge_sh_guard_forced PROPERTIES
        PASS_REGULAR_EXPRESSION "a live bridge needs the forced garage shadow")
    set_tests_properties(gs_bridge_sh_guard_ch5 PROPERTIES
        PASS_REGULAR_EXPRESSION "only channels 0..4 .A D B E C. may be bridged")
    set_tests_properties(gs_bridge_sh_guard_mut PROPERTIES
        PASS_REGULAR_EXPRESSION "the table mutation is a host-only negative test")
    set_tests_properties(gs_bridge_sh_guard_engt PROPERTIES
        PASS_REGULAR_EXPRESSION "GSB_ENG_PASSES is a host-only test override")
    set_tests_properties(gs_bridge_sh_guard_ramapp PROPERTIES
        PASS_REGULAR_EXPRESSION "the RAM application and the loader carry no garage bridge")
    # THE BUS-A OUTPUT CADENCE (2026-11-18, ~/tcu_sweep/output_cadence_port.md):
    # one law pass + one F stage per output tick (5 ms mode 0, 10 ms mode 1),
    # cyclic -> config -> ID per F pass through the ring, nothing lost, the
    # diag job queued in its window, 0x9090A per pass, producer before the
    # law, no launch on a busy channel / no staging torn in flight; the
    # reaper on a stalled cyclic. Four negative builds must FAIL.
    set(BUSA_SOURCES tests/unit/transcript_busa.c
        bsw/sup/sup_jobs.c bsw/sup/sup_handlers.c bsw/sup/sup_ctrl.c bsw/sup/sup_batch.c
        bsw/sup/sup_letters.c bsw/sup/sup_ladder.c bsw/sup/sup_mode.c bsw/sup/sup_walk.c bsw/sup/sup_token.c
        bsw/drv/drv_jobs.c bsw/drv/drv_reqlaw.c bsw/drv/drv_odiag.c bsw/drv/drv_handlers.c
        app/rq_prod.c app/rq_fill.c app/rq_fill_cal.c
        tests/unit/stub_plat.c)
    set(BUSA_DEFS SUP_L1_HOLD_PHASE8_REQUEST=0 SUP_L1_HOLD_PHASE40=0 SUP_L1_HOLD_FAIL_FSM=0
        SUP_L3_HOLD_SEQ_L=0 DRV_RQ_USE_PRODUCER=1)
    add_executable(transcript_busa ${BUSA_SOURCES})
    target_compile_definitions(transcript_busa PRIVATE ${BUSA_DEFS})
    add_test(NAME transcript_busa COMMAND transcript_busa)
    foreach(neg NEG_PERIODIC DRV_RING_DISABLE NEG_LAW5 NEG_NOFREE)
        string(TOLOWER ${neg} negl)
        add_executable(transcript_busa_${negl} ${BUSA_SOURCES})
        target_compile_definitions(transcript_busa_${negl} PRIVATE ${BUSA_DEFS} ${neg}=1)
        add_test(NAME transcript_busa_${negl} COMMAND transcript_busa_${negl})
        set_tests_properties(transcript_busa_${negl} PROPERTIES WILL_FAIL TRUE)
    endforeach()
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

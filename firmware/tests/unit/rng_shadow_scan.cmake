# rng_shadow_scan.cmake — the range shadow is READ-ONLY, both directions
# (2026-11-17, ~/tcu_sweep/range_shadow_port.md). Run with
#   cmake -DFILES="a.c,b.h,..." -DSIDE=output|shadow -P rng_shadow_scan.cmake
# SIDE=output: the listed live output-path sources must not use the shadow
#   (its header, its API, its cells, its RAM block 0xFFF87000..0xFFF877FF).
#   The live cell headers' own #error guard names TCU_RNG_SHADOW_TU and is
#   allowed.
# SIDE=shadow: the shadow's sources must not reach a live cell — no live
#   cell header, no live cell macro, no absolute-address pointer, no call
#   into the output, supervisor or driver layers.
# Fails (FATAL_ERROR "range shadow read-only violation") on any hit.
if(SIDE STREQUAL "output")
    set(PATTERNS
        "#include[ ]*\"app/rng_shadow\\.h\"" "rng_shadow_[a-z_0-9]+[ ]*\\("
        "rng_shadow_cells_t" "rng_shadow_wit_t" "rng_shadow_rec_t"
        "s_rs[^a-zA-Z0-9]" "FFF87[0-7][0-9A-Fa-f][0-9A-Fa-f]")
elseif(SIDE STREQUAL "shadow")
    set(PATTERNS
        "#include[ ]*\"app/park" "#include[ ]*\"app/rq_" "#include[ ]*\"bsw/drv/"
        "#include[ ]*\"bsw/sup/" "#include[ ]*\"safety/"
        "PCELL_" "CELL_[A-Z]" "volatile" "uintptr_t"
        "[^a-z_]drv_[a-z_0-9]+[ ]*\\(" "[^a-z_]sup_[a-z_0-9]+[ ]*\\("
        "[^a-z_]park_[a-z_0-9]+[ ]*\\(" "[^a-z_]rq_[a-z_0-9]+[ ]*\\("
        "[^a-z_]plat_[a-z_0-9]+[ ]*\\(")
else()
    message(FATAL_ERROR "rng_shadow_scan: SIDE must be output or shadow")
endif()
string(REPLACE "," ";" FILES "${FILES}")
set(bad "")
foreach(f IN LISTS FILES)
    file(READ "${f}" txt)
    foreach(p IN LISTS PATTERNS)
        if(txt MATCHES "${p}")
            list(APPEND bad "${f}: ${p}")
        endif()
    endforeach()
endforeach()
if(bad)
    string(REPLACE ";" "\n  " msg "${bad}")
    message(FATAL_ERROR "range shadow read-only violation (${SIDE}):\n  ${msg}")
endif()
list(LENGTH FILES n)
message(STATUS "range shadow read-only scan (${SIDE}): ${n} sources clean")

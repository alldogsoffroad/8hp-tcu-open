# pksens_readonly_scan.cmake — the park position sensor is READ-ONLY
# (2026-11-16, ~/tcu_sweep/park_sensor_port.md §4). Run with
#   cmake -DFILES="a.c,b.c,..." -P pksens_readonly_scan.cmake
# Fails (FATAL_ERROR, message "read-only violation") if any listed source
# names the sensor modules, their cell macros, or any of the sensor's stock
# cell addresses. The list is every output path: the arbiter, the park law,
# the park demands, the cross-check, bus-A staging, the request law and the
# group-A / TOE31 law. The headers drv_pksens.h / park_pair.h additionally
# #error under TCU_OUTPUT_PATH, which each of these files defines.
set(PATTERNS
    "drv_pksens" "park_pair" "CELL_PKS_" "CELL_PKP_" "PKP_"
    # the sensor's stock cells (case-insensitive below)
    "FFF907E8" "FFF900B[0-7]" "FFF93942" "FFF934C[4-9A-F]" "FFF934D[0-9A-F]" "FFF934E[0-7]"
    "FFF93A[C-F][0-9A-F]" "FFF93B0[0-3]" "FFF9415F" "FFF9416[0-9A-D]"
    "FFF9417[0-3]" "FFF94F8[89A-F]" "FFF94F9[0-3]" "FFF94E0[0-9A-B]"
    "FFF9411D" "FFF9477E" "FFF92FE[4-9A-F]" "FFF847F[4-9A-F]" "FFF8480[0-3]"
    "FFF8EA[2-4][0-9A-F]")
string(REPLACE "," ";" FILES "${FILES}")
set(bad "")
foreach(f IN LISTS FILES)
    file(READ "${f}" txt)
    string(TOUPPER "${txt}" up)
    foreach(p IN LISTS PATTERNS)
        string(TOUPPER "${p}" pu)
        if(up MATCHES "${pu}")
            list(APPEND bad "${f}: ${p}")
        endif()
    endforeach()
endforeach()
if(bad)
    string(REPLACE ";" "\n  " msg "${bad}")
    message(FATAL_ERROR "park sensor read-only violation:\n  ${msg}")
endif()
list(LENGTH FILES n)
message(STATUS "park sensor read-only scan: ${n} output-path sources clean")

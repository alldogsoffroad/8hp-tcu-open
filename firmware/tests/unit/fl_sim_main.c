/* tcu_flasher_sim: the project flasher (fl_core + fl_fcu) on the simulated flash controller,
 * talking ISO-TP frames over stdin/stdout (one frame per line, hex bytes; requests from the
 * tester in, responses out). Used by tools/bench/test_tcu_flash.py (spec 16 section 7).
 *
 *   tcu_flasher_sim ROM_IN ROM_OUT [--cut N] [--no-fwe]
 * ROM_IN/ROM_OUT: flash contents 0x000000-0x1FFFFF (a longer file is truncated). On exit (reset
 * request, power cut, or end of input) ROM_OUT is written. Exit code 3 = a flash controller rule
 * was broken by the flasher. */
#include "tests/unit/fl_sim.h"
#include "flasher/fl_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    FILE *f;

    if (argc < 3) {
        fprintf(stderr, "usage: %s ROM_IN ROM_OUT [--cut N] [--no-fwe]\n", argv[0]);
        return 2;
    }
    fl_sim_init();
    sim.bridge = true;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--cut") == 0 && i + 1 < argc) {
            sim.cut_after = atol(argv[++i]);
        } else if (strcmp(argv[i], "--no-fwe") == 0) {
            sim.fwe = false;
        }
    }
    f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror(argv[1]);
        return 2;
    }
    if (fread(sim.rom, 1, SIM_ROM_SIZE, f) != SIM_ROM_SIZE) {
        fprintf(stderr, "%s: shorter than 2 MB\n", argv[1]);
        return 2;
    }
    fclose(f);
    fl_core_init();
    fl_main_loop();
    f = fopen(argv[2], "wb");
    if (f == NULL || fwrite(sim.rom, 1, SIM_ROM_SIZE, f) != SIM_ROM_SIZE) {
        perror(argv[2]);
        return 2;
    }
    fclose(f);
    fprintf(stderr, "tcu_flasher_sim: erases %u, programs %u, FCU resets %u, violations %u%s\n",
            sim.erases, sim.programs, sim.resets_fcu, sim.violations, sim.cut ? ", POWER CUT" : "");
    return sim.violations != 0u ? 3 : 0;
}

/* cal_inplace.c — the HOST view of the stock calibration (app/cal_inplace.h).
 * The stock flash image is read once, on first use, into a buffer indexed by
 * the stock flash address. Path: the environment variable TCU_STOCK_IMAGE, else
 * the compile-time default (the CMake cache variable of the same name). A
 * missing / short file is never silent: a loud SKIP line on stderr and exit
 * status 77 (every ctest entry maps 77 to "skipped"). Host builds only: the
 * target maps the stock bytes at their flash addresses. */
#if !defined(__sh__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/cal_inplace.h"

#ifndef TCU_STOCK_IMAGE_DEFAULT
#define TCU_STOCK_IMAGE_DEFAULT ""
#endif

static uint8_t s_img[CAL_FLASH_END];
static int s_loaded;

/* mutation negatives (ctests *_neg_*): one stock byte of a shadow's calibration is moved,
 * as the generated copies' SM_NEG_* / FR_NEG_* / GP_NEG_* macros used to */
static void mutate(void)
{
#if defined(SM_NEG_MAP)
    /* map 1 (0x1E9A9C), line 8 (down 2->1) at pedal 18, z cell 7 of row 0: 125 -> 126 */
    s_img[CAL_SM_MAP0 + CAL_SM_MAP_LEN + 8u + 2u * (14u + 12u) + 2u * 7u + 1u] += 1u;
#endif
#if defined(FR_NEG_CFG)
    /* group 136's fail time (cfg +3): 0xC2 -> 0xC4 */
    s_img[CAL_FR_GRP + CAL_FR_GRP_ROW * 135u + 3u] += 2u;
#endif
#if defined(GP_NEG_CLASS)
    /* the down-shift f8 array t[1] = 1 -> 0 */
    s_img[CAL_GP_F8_DN + 4u + 1u] -= 1u;
#endif
}

const uint8_t *cal_img(void)
{
    if (!s_loaded) {
        const char *path = getenv("TCU_STOCK_IMAGE");
        if (path == NULL || path[0] == '\0') {
            path = TCU_STOCK_IMAGE_DEFAULT;
        }
        FILE *f = (path[0] != '\0') ? fopen(path, "rb") : NULL;
        if (f == NULL || fread(s_img, 1u, sizeof s_img, f) != sizeof s_img) {
            fprintf(stderr, "SKIP: the stock flash image is needed (the shadows read their calibration in place): "
                            "'%s' missing or short; set TCU_STOCK_IMAGE\n", path);
            exit(77);
        }
        fclose(f);
        mutate();
        s_loaded = 1;
    }
    return s_img;
}

#endif /* !__sh__ */

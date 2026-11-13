/* drv_jobs.h — bus-A driver job service (design §8, spec 22 §2.2). */
#ifndef TCU_DRV_JOBS_H
#define TCU_DRV_JOBS_H

#include <stdint.h>
#include <stdbool.h>

#define DRV_ID     16
#define DRV_DIAG   17
#define DRV_CFG    18
#define DRV_CYC    19

bool drv_job_queue(uint8_t id);
bool drv_job_check_free(uint8_t id);
void drv_jobs_init(void);
void drv_cyclic_build(const uint16_t *setpoints, uint8_t n_ch);
void drv_id_kick(void);

/* reply buffer of a finished bus-A job (bench FE12 results; NULL outside) */
const uint16_t *drv_job_rx(uint8_t id);
const uint16_t *drv_job_tx(uint8_t id);   /* r02v: the staged words (tests/witness) */

#endif

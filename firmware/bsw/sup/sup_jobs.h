/* sup_jobs.h — the bus-B job engine: descriptors, queues, one-in-flight (design §7.1-7.3). */
#ifndef TCU_SUP_JOBS_H
#define TCU_SUP_JOBS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* job IDs (index into the descriptor table) */
enum {
    JOB_CTRL = 0,       /* J1: 4f B1,B0,B1,B0  {8E|ctrl, C9, 22, 09}      */
    JOB_EN,             /* J2: 2f B1,B1        {90|en, 24}                  */
    JOB_POS,            /* J3: 2f B1,B1        {28, 2E}                     */
    JOB_SSLB2,          /* J4: 1f B2(mode 0)   {00FF}                        */
    JOB_ID,             /* J5: 4f B1,B0,B1,B0  {06, 03, 00, 00}             */
    JOB_STAT,           /* J6: 5f B1,B0,B1,B0,B1 {18, 0C, 1E, 06, 14}      */
    JOB_NIB,            /* J7: 1f B0           {00}                          */
    JOB_DUTY,           /* W1: 7f B1×7         {88|t0..t3, 12, 0C, 0A}     */
    JOB_LEVEL,          /* W2: 4f B1×4         {84|lvl, 0A, 12, 0C}        */
    JOB_FLT,            /* F:  2f B1,B1        {90|en, 24} (fault twin)   */
    JOB_COUNT
};

/* bus-A job IDs (same engine, different table section) */
enum {
    DRV_JOB_ID = 16,    /* 10f five-select {06×5, 00×5}                    */
    DRV_JOB_DIAG,       /* 15f {18, 1E, 28} per chip                       */
    DRV_JOB_CFG,        /* 40f config writes/reads                          */
    DRV_JOB_CYC,        /* 45f cyclic setpoints                             */
};

/* select-config tables (flash const, SPCMD values) */
extern const uint16_t SEL_B1[];          /* {AF11} — supervisor only      */
extern const uint16_t SEL_B1_B0[];       /* {AF11, AF01} — alternation    */
extern const uint16_t SEL_B0[];          /* {AF01} — SSLB0 only          */
extern const uint16_t SEL_B2[];          /* {AF20} — SSLB2 (mode 0)      */
extern const uint16_t SEL_A5[];          /* {AF31,AF21,AF11,AF01,AF51} — bus A */

/* queue routing (design §7.1; the class field's operational rule) */
#define JOBQ_CYCLIC 0    /* 10 slots: J1-J7, F, bus-A jobs */
#define JOBQ_WALK   1    /* 5 slots: W1, W2 only */

/* descriptor (design §7.1; flash-resident const). The in-flight flag is
 * NOT in the descriptor: it is per-job runtime state and lives in the
 * engine's RAM status array (sup_job_release / sup_job_check_free) — the
 * stock's flash/RAM split (flash descriptors 0x409F4.., RAM status cells
 * 0xFFF908xx). */
typedef struct {
    uint16_t class_field;       /* 0x0100 cyclic | 0x0101 walk | 0x0000 bus-A */
    uint8_t  frames;
    uint8_t  n_sel;
    const uint16_t *tx_tmpl;   /* flash template (reference/initial image) */
    uint16_t *tx_stage;        /* RAM staging buffer — the stagers own it */
    uint16_t *rx;              /* RAM reply buffer */
    const uint16_t *sel_cfg;   /* SPCMD table */
    uint8_t  queue;            /* JOBQ_* */
    uint8_t  bus;              /* 0=A, 1=B */
} sup_job_desc_t;

extern const sup_job_desc_t sup_job_table[JOB_COUNT];

/* engine interface */
void sup_jobs_init(void);
bool sup_job_queue(uint8_t id);         /* false if in-flight or queued */
bool sup_job_check_free(uint8_t id);    /* the stock 0x93B8E */
void sup_job_release(uint8_t id);       /* clear in-flight (handlers, ISR ctx) */
void sup_job_engine_pump(void);         /* launch head of each queue if bus idle */

/* lab job (design §7.2; FE13 and magic-frame experiments) */
bool sup_lab_job_queue(uint16_t *tx, uint16_t *rx, uint8_t frames,
                       const uint16_t *sel_cfg, uint8_t n_sel);

/* reply buffer of a finished job (bench FE13 results read it; NULL outside
 * the table) */
const uint16_t *sup_job_rx(uint8_t id);

#endif

/* Project measurement index list for the measurement protocol (bsw/meas). */
#ifndef TCU_MEAS_TABLE_H
#define TCU_MEAS_TABLE_H

void meas_table_init(void);
/* Refresh the measured copies; call every 10 ms before meas_tick_10ms(). */
void meas_table_update(void);

#endif

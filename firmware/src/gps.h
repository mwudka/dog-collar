#ifndef GPS_H_
#define GPS_H_

#include <gps.h>
#include <lte_lc.h>

int gps_control_init(gps_trigger_handler_t handler);
int gps_control_stop(void);
int gps_control_start(void);

#endif
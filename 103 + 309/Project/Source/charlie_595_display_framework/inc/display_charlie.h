#ifndef DISPLAY_CHARLIE_H
#define DISPLAY_CHARLIE_H

#include <stdint.h>
#include "display_types.h"

void display_init(void);
void display_set_value(uint8_t value, uint8_t charge_on, uint8_t percent_on);
void display_scan_task_1ms(void);

#endif

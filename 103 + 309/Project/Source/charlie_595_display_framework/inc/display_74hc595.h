#ifndef DISPLAY_74HC595_H
#define DISPLAY_74HC595_H

#include <stdint.h>

void disp595_init(void);
void disp595_write_byte(uint8_t data);
void disp595_clear_all(void);

#endif

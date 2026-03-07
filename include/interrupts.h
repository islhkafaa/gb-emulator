#ifndef GB_INTERRUPTS_H
#define GB_INTERRUPTS_H

#include "types.h"

#define INT_VBLANK (1 << 0)
#define INT_LCD_STAT (1 << 1)
#define INT_TIMER (1 << 2)
#define INT_SERIAL (1 << 3)
#define INT_JOYPAD (1 << 4)

struct GB;

void cpu_request_interrupt(struct GB *gb, u8 interrupt);

#endif

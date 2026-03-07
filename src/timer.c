#include "../include/timer.h"
#include "../include/gb.h"
#include "../include/interrupts.h"

void timer_tick(struct GB *gb, int m_cycles) {
  int t_cycles = m_cycles * 4;

  gb->div_counter += t_cycles;
  while (gb->div_counter >= 256) {
    gb->div_counter -= 256;
    gb->mem.io[0x04]++;
  }

  u8 tac = gb->mem.io[0x07];
  if (tac & 0x04) {
    gb->timer_counter += t_cycles;

    int freq = 1024;
    switch (tac & 0x03) {
    case 0:
      freq = 1024;
      break;
    case 1:
      freq = 16;
      break;
    case 2:
      freq = 64;
      break;
    case 3:
      freq = 256;
      break;
    }

    while (gb->timer_counter >= freq) {
      gb->timer_counter -= freq;
      u8 tima = gb->mem.io[0x05];
      if (tima == 0xFF) {
        gb->mem.io[0x05] = gb->mem.io[0x06];
        cpu_request_interrupt(gb, INT_TIMER);
      } else {
        gb->mem.io[0x05] = tima + 1;
      }
    }
  }
}

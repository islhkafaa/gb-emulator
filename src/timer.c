#include "../include/timer.h"
#include "../include/gb.h"
#include "../include/interrupts.h"

void timer_tick(struct GB *gb, int m_cycles) {
  int t_cycles = m_cycles * 4;

  if (gb->tima_overflow_delay > 0) {
    gb->tima_overflow_delay -= t_cycles;
    if (gb->tima_overflow_delay <= 0) {
      gb->mem.io[0x05] = gb->mem.io[0x06];
      cpu_request_interrupt(gb, INT_TIMER);
      gb->tima_overflow_delay = 0;
    }
  }

  u16 old_div = gb->div_counter;
  gb->div_counter += t_cycles;
  gb->mem.io[0x04] = gb->div_counter >> 8;

  u8 tac = gb->mem.io[0x07];
  if (tac & 0x04) {
    int freq_shifts[] = {9, 3, 5, 7};
    int shift = freq_shifts[tac & 0x03];

    int old_bit = (old_div >> shift) & 1;
    int new_bit = (gb->div_counter >> shift) & 1;

    if (old_bit && !new_bit) {
      if (gb->mem.io[0x05] == 0xFF) {
        gb->mem.io[0x05] = 0;
        gb->tima_overflow_delay = 4;
      } else {
        gb->mem.io[0x05]++;
      }
    }
  }
}

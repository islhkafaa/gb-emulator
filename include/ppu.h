#ifndef GB_PPU_H
#define GB_PPU_H

#include "types.h"

#define PPU_MODE_HBLANK 0
#define PPU_MODE_VBLANK 1
#define PPU_MODE_OAM 2
#define PPU_MODE_TRANSFER 3

struct GB;

typedef struct {
  u32 frame_buffer[160 * 144];
  int mode;
  int mode_clock;
  bool_t stat_irq_line;
} PPU;

void ppu_step(struct GB *gb, int m_cycles);
void ppu_render_scanline(struct GB *gb);

#endif

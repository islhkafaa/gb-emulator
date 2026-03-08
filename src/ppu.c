#include "../include/ppu.h"
#include "../include/gb.h"
#include "../include/interrupts.h"

static const u32 PALETTE[4] = {0xFF9BBC0F, 0xFF8BAC0F, 0xFF306230, 0xFF0F380F};

void ppu_render_scanline(GB *gb) {
  u8 lcdc = gb->mem.io[0x40];
  u8 scy = gb->mem.io[0x42];
  u8 scx = gb->mem.io[0x43];
  u8 ly = gb->mem.io[0x44];
  u8 bgp = gb->mem.io[0x47];

  u16 tile_map = (lcdc & 0x08) ? 0x9C00 : 0x9800;
  int is_signed = !(lcdc & 0x10);

  u8 wy = gb->mem.io[0x4A];
  u8 wx = gb->mem.io[0x4B];

  bool_t window_drawn = FALSE;
  u8 *bg_ci = gb->ppu.bg_color_index;
  for (int i = 0; i < 160; i++)
    bg_ci[i] = 0;

  for (int pixel = 0; pixel < 160; pixel++) {
    u8 color_idx = 0;

    if ((lcdc & 0x20) && ly >= wy && (pixel + 7 >= wx)) {
      window_drawn = TRUE;
      int win_x = pixel - (wx - 7);
      int win_y = gb->ppu.window_line_counter;
      u16 win_tile_map = (lcdc & 0x40) ? 0x9C00 : 0x9800;
      u16 win_offset = win_tile_map + (win_y / 8) * 32 + (win_x / 8);

      u8 tile_num = gb->mem.vram[win_offset - 0x8000];
      u16 tile_addr =
          is_signed ? (0x9000 + (i8)tile_num * 16) : (0x8000 + tile_num * 16);
      u8 win_line = (win_y % 8) * 2;
      u8 wd1 = gb->mem.vram[tile_addr - 0x8000 + win_line];
      u8 wd2 = gb->mem.vram[tile_addr - 0x8000 + win_line + 1];

      int win_bit = 7 - (win_x % 8);
      color_idx = ((wd2 >> win_bit) & 1) << 1 | ((wd1 >> win_bit) & 1);
    } else if (lcdc & 0x01) {
      u8 x_pos = (scx + pixel) & 0xFF;
      u8 y_pos = (scy + ly) & 0xFF;
      u16 offset = tile_map + (y_pos / 8) * 32 + (x_pos / 8);

      u8 tile_num = gb->mem.vram[offset - 0x8000];
      u16 tile_addr =
          is_signed ? (0x9000 + (i8)tile_num * 16) : (0x8000 + tile_num * 16);
      u8 line = (y_pos % 8) * 2;
      u8 data1 = gb->mem.vram[tile_addr - 0x8000 + line];
      u8 data2 = gb->mem.vram[tile_addr - 0x8000 + line + 1];

      int bit = 7 - (x_pos % 8);
      color_idx = ((data2 >> bit) & 1) << 1 | ((data1 >> bit) & 1);
    }

    int mapped_col = (bgp >> (color_idx * 2)) & 0x03;
    gb->ppu.frame_buffer[ly * 160 + pixel] = PALETTE[mapped_col];
    bg_ci[pixel] = color_idx;
  }

  if (window_drawn) {
    gb->ppu.window_line_counter++;
  }

  if ((lcdc & 0x02) == 0)
    return;

  int obj_size = (lcdc & 0x04) ? 16 : 8;
  int obj_count = 0;

  typedef struct {
    int y, x, tile, flags, oam_idx;
  } Sprite;
  Sprite sprites[10];

  for (int i = 0; i < 40 && obj_count < 10; i++) {
    u16 oam_addr = i * 4;
    int obj_y = gb->mem.oam[oam_addr] - 16;
    if (ly < obj_y || ly >= obj_y + obj_size)
      continue;
    sprites[obj_count].y = obj_y;
    sprites[obj_count].x = gb->mem.oam[oam_addr + 1] - 8;
    sprites[obj_count].tile = gb->mem.oam[oam_addr + 2];
    sprites[obj_count].flags = gb->mem.oam[oam_addr + 3];
    sprites[obj_count].oam_idx = i;
    obj_count++;
  }

  for (int i = 0; i < obj_count - 1; i++) {
    for (int j = i + 1; j < obj_count; j++) {
      if (sprites[j].x < sprites[i].x ||
          (sprites[j].x == sprites[i].x &&
           sprites[j].oam_idx < sprites[i].oam_idx)) {
        Sprite tmp = sprites[i];
        sprites[i] = sprites[j];
        sprites[j] = tmp;
      }
    }
  }

  for (int s = obj_count - 1; s >= 0; s--) {
    int obj_y = sprites[s].y;
    int obj_x = sprites[s].x;
    u8 tile_num = (u8)sprites[s].tile;
    u8 flags = (u8)sprites[s].flags;

    int line = ly - obj_y;
    if (flags & 0x40)
      line = (obj_size - 1) - line;

    if (obj_size == 16) {
      if (line < 8) {
        tile_num &= 0xFE;
      } else {
        tile_num |= 0x01;
        line -= 8;
      }
    }

    line *= 2;

    u16 tile_addr = 0x8000 + (tile_num * 16) + line;
    u8 data1 = gb->mem.vram[tile_addr - 0x8000];
    u8 data2 = gb->mem.vram[tile_addr - 0x8000 + 1];

    u8 pal = (flags & 0x10) ? gb->mem.io[0x49] : gb->mem.io[0x48];

    for (int p_x = 0; p_x < 8; p_x++) {
      int screen_x = obj_x + p_x;
      if (screen_x < 0 || screen_x >= 160)
        continue;

      int color_bit = (flags & 0x20) ? p_x : 7 - p_x;
      int color_idx =
          ((data2 >> color_bit) & 1) << 1 | ((data1 >> color_bit) & 1);

      if (color_idx == 0)
        continue;

      if ((flags & 0x80) != 0) {
        if (gb->ppu.bg_color_index[screen_x] != 0)
          continue;
      }

      int mapped_col = (pal >> (color_idx * 2)) & 0x03;
      gb->ppu.frame_buffer[ly * 160 + screen_x] = PALETTE[mapped_col];
    }
  }
}

void ppu_step(GB *gb, int m_cycles) {
  int t_cycles = m_cycles * 4;
  gb->ppu.mode_clock += t_cycles;

  u8 stat = gb->mem.io[0x41];
  u8 ly = gb->mem.io[0x44];
  u8 lyc = gb->mem.io[0x45];

  u8 lcdc = gb->mem.io[0x40];

  if ((lcdc & 0x80) == 0) {
    gb->ppu.mode_clock = 0;
    gb->ppu.mode = PPU_MODE_OAM;
    gb->mem.io[0x44] = 0;
    gb->ppu.window_line_counter = 0;
    stat = (stat & ~0x03) | PPU_MODE_OAM;
    gb->mem.io[0x41] = stat;
    return;
  }

  switch (gb->ppu.mode) {
  case PPU_MODE_OAM:
    if (gb->ppu.mode_clock >= 80) {
      gb->ppu.mode_clock -= 80;
      gb->ppu.mode = PPU_MODE_TRANSFER;
    }
    break;

  case PPU_MODE_TRANSFER:
    if (gb->ppu.mode_clock >= 172) {
      gb->ppu.mode_clock -= 172;
      gb->ppu.mode = PPU_MODE_HBLANK;
      ppu_render_scanline(gb);
    }
    break;

  case PPU_MODE_HBLANK:
    if (gb->ppu.mode_clock >= 204) {
      gb->ppu.mode_clock -= 204;
      ly++;

      if (ly == 144) {
        gb->ppu.mode = PPU_MODE_VBLANK;
        cpu_request_interrupt(gb, INT_VBLANK);
      } else {
        gb->ppu.mode = PPU_MODE_OAM;
      }
    }
    break;

  case PPU_MODE_VBLANK:
    if (gb->ppu.mode_clock >= 456) {
      gb->ppu.mode_clock -= 456;
      ly++;

      if (ly > 153) {
        gb->ppu.mode = PPU_MODE_OAM;
        gb->ppu.window_line_counter = 0;
        ly = 0;
      }
    }
    break;
  }

  gb->mem.io[0x44] = ly;
  stat = (stat & ~0x03) | (gb->ppu.mode & 0x03);

  if (ly == lyc) {
    stat |= 0x04;
  } else {
    stat &= ~0x04;
  }

  bool_t stat_line = FALSE;
  if ((stat & 0x40) && (stat & 0x04))
    stat_line = TRUE;
  if ((stat & 0x20) && (gb->ppu.mode == PPU_MODE_OAM))
    stat_line = TRUE;
  if ((stat & 0x10) && (gb->ppu.mode == PPU_MODE_VBLANK))
    stat_line = TRUE;
  if ((stat & 0x08) && (gb->ppu.mode == PPU_MODE_HBLANK))
    stat_line = TRUE;

  if (stat_line && !gb->ppu.stat_irq_line) {
    cpu_request_interrupt(gb, INT_LCD_STAT);
  }
  gb->ppu.stat_irq_line = stat_line;

  gb->mem.io[0x41] = stat;
}

#include "../include/ppu.h"
#include "../include/gb.h"
#include "../include/interrupts.h"

static const u32 PALETTE[4] = {0xFF9BBC0F, 0xFF8BAC0F, 0xFF306230, 0xFF0F380F};

void ppu_render_scanline(GB *gb) {
  u8 lcdc = bus_read(&gb->mem, gb->rom, 0xFF40);
  if ((lcdc & 0x01) == 0)
    return;

  u8 scy = bus_read(&gb->mem, gb->rom, 0xFF42);
  u8 scx = bus_read(&gb->mem, gb->rom, 0xFF43);
  u8 ly = bus_read(&gb->mem, gb->rom, 0xFF44);
  u8 bgp = bus_read(&gb->mem, gb->rom, 0xFF47);

  u16 tile_map = (lcdc & 0x08) ? 0x9C00 : 0x9800;
  u16 tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;
  int is_signed = !(lcdc & 0x10);

  u8 y_pos = scy + ly;
  u16 tile_row = (y_pos / 8) * 32;

  for (int pixel = 0; pixel < 160; pixel++) {
    u8 x_pos = scx + pixel;
    u16 map_offset = tile_map + tile_row + (x_pos / 8);

    i16 tile_num = bus_read(&gb->mem, gb->rom, map_offset);
    if (is_signed && tile_num < 128) {
      tile_num += 256;
    }

    u16 tile_addr = tile_data + (tile_num * 16);
    u8 line = (y_pos % 8) * 2;
    u8 data1 = bus_read(&gb->mem, gb->rom, tile_addr + line);
    u8 data2 = bus_read(&gb->mem, gb->rom, tile_addr + line + 1);

    int color_bit = 7 - (x_pos % 8);
    int color_idx =
        ((data2 >> color_bit) & 1) << 1 | ((data1 >> color_bit) & 1);
    int mapped_col = (bgp >> (color_idx * 2)) & 0x03;

    gb->ppu.frame_buffer[ly * 160 + pixel] = PALETTE[mapped_col];
  }

  if ((lcdc & 0x02) == 0)
    return;

  int obj_size = (lcdc & 0x04) ? 16 : 8;
  int obj_count = 0;

  for (u16 oam_addr = 0xFE00; oam_addr < 0xFEA0; oam_addr += 4) {
    if (obj_count >= 10)
      break;

    int obj_y = bus_read(&gb->mem, gb->rom, oam_addr) - 16;
    int obj_x = bus_read(&gb->mem, gb->rom, oam_addr + 1) - 8;
    u8 tile_num = bus_read(&gb->mem, gb->rom, oam_addr + 2);
    u8 flags = bus_read(&gb->mem, gb->rom, oam_addr + 3);

    if (ly >= obj_y && ly < (obj_y + obj_size)) {
      obj_count++;

      if (obj_size == 16) {
        tile_num &= 0xFE;
      }

      int line = ly - obj_y;
      if (flags & 0x40) {
        line = (obj_size - 1) - line;
      }
      line *= 2;

      u16 tile_addr = 0x8000 + (tile_num * 16) + line;
      u8 data1 = bus_read(&gb->mem, gb->rom, tile_addr);
      u8 data2 = bus_read(&gb->mem, gb->rom, tile_addr + 1);

      u8 pal = (flags & 0x10) ? bus_read(&gb->mem, gb->rom, 0xFF49)
                              : bus_read(&gb->mem, gb->rom, 0xFF48);

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
          u32 current_bg = gb->ppu.frame_buffer[ly * 160 + screen_x];
          if (current_bg != PALETTE[(bgp & 0x03)])
            continue;
        }

        int mapped_col = (pal >> (color_idx * 2)) & 0x03;
        gb->ppu.frame_buffer[ly * 160 + screen_x] = PALETTE[mapped_col];
      }
    }
  }
}

void ppu_step(GB *gb, int m_cycles) {
  int t_cycles = m_cycles * 4;
  gb->ppu.mode_clock += t_cycles;

  u8 lcdc = bus_read(&gb->mem, gb->rom, 0xFF40);
  if ((lcdc & 0x80) == 0) {
    gb->ppu.mode_clock = 0;
    gb->ppu.mode = PPU_MODE_HBLANK;
    bus_write(&gb->mem, gb->rom, 0xFF44, 0);
    return;
  }

  u8 ly = bus_read(&gb->mem, gb->rom, 0xFF44);

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
        ly = 0;
      }
    }
    break;
  }

  bus_write(&gb->mem, gb->rom, 0xFF44, ly);

  u8 stat = bus_read(&gb->mem, gb->rom, 0xFF41);
  stat &= ~0x03;
  stat |= (gb->ppu.mode & 0x03);

  u8 lyc = bus_read(&gb->mem, gb->rom, 0xFF45);
  if (ly == lyc) {
    stat |= 0x04;
  } else {
    stat &= ~0x04;
  }
  bus_write(&gb->mem, gb->rom, 0xFF41, stat);
}

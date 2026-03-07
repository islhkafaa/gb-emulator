#ifndef GB_MEMORY_H
#define GB_MEMORY_H

#include "types.h"

#define MEM_ROM_BANK0_START 0x0000
#define MEM_ROM_BANK0_END 0x3FFF
#define MEM_ROM_BANKN_START 0x4000
#define MEM_ROM_BANKN_END 0x7FFF
#define MEM_VRAM_START 0x8000
#define MEM_VRAM_END 0x9FFF
#define MEM_EXT_RAM_START 0xA000
#define MEM_EXT_RAM_END 0xBFFF
#define MEM_WRAM_START 0xC000
#define MEM_WRAM_END 0xDFFF
#define MEM_ECHO_START 0xE000
#define MEM_ECHO_END 0xFDFF
#define MEM_OAM_START 0xFE00
#define MEM_OAM_END 0xFE9F
#define MEM_UNUSABLE_START 0xFEA0
#define MEM_UNUSABLE_END 0xFEFF
#define MEM_IO_START 0xFF00
#define MEM_IO_END 0xFF7F
#define MEM_HRAM_START 0xFF80
#define MEM_HRAM_END 0xFFFE
#define MEM_IE 0xFFFF

#define MEM_VRAM_SIZE 0x2000
#define MEM_EXT_RAM_SIZE 0x20000
#define MEM_WRAM_SIZE 0x2000
#define MEM_OAM_SIZE 0x00A0
#define MEM_IO_SIZE 0x0080
#define MEM_HRAM_SIZE 0x007F

typedef struct {
  u8 type;
  u8 rom_bank;
  u8 ram_bank;
  u8 ram_enable;
  u8 banking_mode;
  u8 rom_bank_hi;
} MBC;

typedef struct {
  u8 vram[MEM_VRAM_SIZE];
  u8 ext_ram[MEM_EXT_RAM_SIZE];
  u8 wram[MEM_WRAM_SIZE];
  u8 oam[MEM_OAM_SIZE];
  u8 io[MEM_IO_SIZE];
  u8 hram[MEM_HRAM_SIZE];
  u8 ie;
  void *gb_ptr;
  MBC mbc;
} Memory;

void memory_init(Memory *mem, const u8 *rom);
u8 bus_read(Memory *mem, const u8 *rom, u16 addr);
void bus_write(Memory *mem, const u8 *rom, u16 addr, u8 val);

#endif

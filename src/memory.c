#include "../include/memory.h"

u8 bus_read(Memory *mem, const u8 *rom, u16 addr) {
  if (addr <= MEM_ROM_BANKN_END) {
    return rom ? rom[addr] : 0xFF;
  }
  if (addr <= MEM_VRAM_END) {
    return mem->vram[addr - MEM_VRAM_START];
  }
  if (addr <= MEM_EXT_RAM_END) {
    return mem->ext_ram[addr - MEM_EXT_RAM_START];
  }
  if (addr <= MEM_WRAM_END) {
    return mem->wram[addr - MEM_WRAM_START];
  }
  if (addr <= MEM_ECHO_END) {
    return mem->wram[addr - MEM_ECHO_START];
  }
  if (addr <= MEM_OAM_END) {
    return mem->oam[addr - MEM_OAM_START];
  }
  if (addr <= MEM_UNUSABLE_END) {
    return 0xFF;
  }
  if (addr <= MEM_IO_END) {
    return mem->io[addr - MEM_IO_START];
  }
  if (addr <= MEM_HRAM_END) {
    return mem->hram[addr - MEM_HRAM_START];
  }
  return mem->ie;
}

void bus_write(Memory *mem, const u8 *rom, u16 addr, u8 val) {
  if (addr <= MEM_ROM_BANKN_END) {
    return;
  }
  if (addr <= MEM_VRAM_END) {
    mem->vram[addr - MEM_VRAM_START] = val;
    return;
  }
  if (addr <= MEM_EXT_RAM_END) {
    mem->ext_ram[addr - MEM_EXT_RAM_START] = val;
    return;
  }
  if (addr <= MEM_WRAM_END) {
    mem->wram[addr - MEM_WRAM_START] = val;
    return;
  }
  if (addr <= MEM_ECHO_END) {
    mem->wram[addr - MEM_ECHO_START] = val;
    return;
  }
  if (addr <= MEM_OAM_END) {
    mem->oam[addr - MEM_OAM_START] = val;
    return;
  }
  if (addr <= MEM_UNUSABLE_END) {
    return;
  }
  if (addr == 0xFF46) {
    u16 src = val << 8;
    for (int i = 0; i < 0xA0; i++) {
      mem->oam[i] = bus_read(mem, rom, src + i);
    }
    mem->io[addr - MEM_IO_START] = val;
    return;
  }
  if (addr <= MEM_IO_END) {
    mem->io[addr - MEM_IO_START] = val;
    return;
  }
  if (addr <= MEM_HRAM_END) {
    mem->hram[addr - MEM_HRAM_START] = val;
    return;
  }
  mem->ie = val;
}

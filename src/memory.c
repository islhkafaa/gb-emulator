#include "../include/memory.h"
#include "../include/apu.h"
#include "../include/joypad.h"

struct GB;

void memory_init(Memory *mem, const u8 *rom) {
  if (rom) {
    mem->mbc.type = rom[0x0147];
  } else {
    mem->mbc.type = 0;
  }
  mem->mbc.rom_bank = 1;
  mem->mbc.ram_bank = 0;
  mem->mbc.ram_enable = 0;
  mem->mbc.banking_mode = 0;
}

u8 bus_read(Memory *mem, const u8 *rom, u16 addr) {
  if (addr <= MEM_ROM_BANK0_END) {
    return rom ? rom[addr] : 0xFF;
  }
  if (addr <= MEM_ROM_BANKN_END) {
    if (!rom)
      return 0xFF;
    u16 bank = mem->mbc.rom_bank;
    return rom[(bank * 0x4000) + (addr - 0x4000)];
  }
  if (addr <= MEM_VRAM_END) {
    return mem->vram[addr - MEM_VRAM_START];
  }
  if (addr <= MEM_EXT_RAM_END) {
    if (mem->mbc.ram_enable) {
      u16 bank = mem->mbc.banking_mode ? mem->mbc.ram_bank : 0;
      return mem->ext_ram[(bank * 0x2000) + (addr - MEM_EXT_RAM_START)];
    }
    return 0xFF;
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
  if (addr == 0xFF00) {
    return joypad_read((struct GB *)mem->gb_ptr);
  }
  if (addr >= 0xFF10 && addr <= 0xFF26) {
    return apu_read((struct GB *)mem->gb_ptr, addr);
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
  if (addr <= 0x1FFF) {
    if (mem->mbc.type >= 1 && mem->mbc.type <= 3) {
      mem->mbc.ram_enable = ((val & 0x0F) == 0x0A) ? 1 : 0;
    }
    return;
  }
  if (addr <= 0x3FFF) {
    if (mem->mbc.type >= 1 && mem->mbc.type <= 3) {
      mem->mbc.rom_bank = (mem->mbc.rom_bank & 0xE0) | (val & 0x1F);
      if (mem->mbc.rom_bank == 0)
        mem->mbc.rom_bank = 1;
    }
    return;
  }
  if (addr <= 0x5FFF) {
    if (mem->mbc.type >= 1 && mem->mbc.type <= 3) {
      if (mem->mbc.banking_mode == 0) {
        mem->mbc.rom_bank = (mem->mbc.rom_bank & 0x1F) | ((val & 0x03) << 5);
        if (mem->mbc.rom_bank == 0)
          mem->mbc.rom_bank = 1;
      } else {
        mem->mbc.ram_bank = val & 0x03;
      }
    }
    return;
  }
  if (addr <= 0x7FFF) {
    if (mem->mbc.type >= 1 && mem->mbc.type <= 3) {
      mem->mbc.banking_mode = val & 0x01;
    }
    return;
  }
  if (addr <= MEM_VRAM_END) {
    mem->vram[addr - MEM_VRAM_START] = val;
    return;
  }
  if (addr <= MEM_EXT_RAM_END) {
    if (mem->mbc.ram_enable) {
      u16 bank = mem->mbc.banking_mode ? mem->mbc.ram_bank : 0;
      mem->ext_ram[(bank * 0x2000) + (addr - MEM_EXT_RAM_START)] = val;
    }
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
  if (addr == 0xFF00) {
    joypad_write((struct GB *)mem->gb_ptr, val);
    return;
  }
  if (addr >= 0xFF10 && addr <= 0xFF3F) {
    apu_write((struct GB *)mem->gb_ptr, addr, val);
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

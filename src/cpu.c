#include "../include/cpu.h"
#include "../include/gb.h"
#include "../include/memory.h"
#include <stdio.h>

void cpu_init(CPU *cpu) {
  cpu->af = 0x01B0;
  cpu->bc = 0x0013;
  cpu->de = 0x00D8;
  cpu->hl = 0x014D;
  cpu->sp = 0xFFFE;
  cpu->pc = 0x0100;

  cpu->ime = FALSE;
  cpu->halted = FALSE;
}

static u8 fetch8(struct GB *gb) {
  u8 val = bus_read(&gb->mem, gb->rom, gb->cpu.pc);
  gb->cpu.pc++;
  return val;
}

static u16 fetch16(struct GB *gb) {
  u8 lo = fetch8(gb);
  u8 hi = fetch8(gb);
  return (u16)(lo | (hi << 8));
}

static void push16(struct GB *gb, u16 val) {
  gb->cpu.sp -= 2;
  bus_write(&gb->mem, gb->cpu.sp, (u8)(val & 0xFF));
  bus_write(&gb->mem, gb->cpu.sp + 1, (u8)((val >> 8) & 0xFF));
}

static u16 pop16(struct GB *gb) {
  u8 lo = bus_read(&gb->mem, gb->rom, gb->cpu.sp);
  u8 hi = bus_read(&gb->mem, gb->rom, gb->cpu.sp + 1);
  gb->cpu.sp += 2;
  return (u16)(lo | (hi << 8));
}

int cpu_step(struct GB *gb) {
  u8 opcode = fetch8(gb);

  switch (opcode) {
  case 0x00:
    return 4;

  case 0x01:
    gb->cpu.bc = fetch16(gb);
    return 12;

  case 0x06:
    gb->cpu.b = fetch8(gb);
    return 8;

  case 0x0B:
    gb->cpu.bc--;
    return 8;

  case 0x0D:
    gb->cpu.f &= FLAG_C;
    gb->cpu.f |= FLAG_N;
    if ((gb->cpu.c & 0x0F) == 0)
      gb->cpu.f |= FLAG_H;
    gb->cpu.c--;
    if (gb->cpu.c == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x0E:
    gb->cpu.c = fetch8(gb);
    return 8;

  case 0x11:
    gb->cpu.de = fetch16(gb);
    return 12;

  case 0x16:
    gb->cpu.d = fetch8(gb);
    return 8;

  case 0x1E:
    gb->cpu.e = fetch8(gb);
    return 8;

  case 0x20: {
    i8 rel = (i8)fetch8(gb);
    if (!(gb->cpu.f & FLAG_Z)) {
      gb->cpu.pc += rel;
      return 12;
    }
    return 8;
  }

  case 0x21:
    gb->cpu.hl = fetch16(gb);
    return 12;

  case 0x22:
    bus_write(&gb->mem, gb->cpu.hl, gb->cpu.a);
    gb->cpu.hl++;
    return 8;

  case 0x26:
    gb->cpu.h = fetch8(gb);
    return 8;

  case 0x28: {
    i8 rel = (i8)fetch8(gb);
    if (gb->cpu.f & FLAG_Z) {
      gb->cpu.pc += rel;
      return 12;
    }
    return 8;
  }

  case 0x2E:
    gb->cpu.l = fetch8(gb);
    return 8;

  case 0x31:
    gb->cpu.sp = fetch16(gb);
    return 12;

  case 0x3E:
    gb->cpu.a = fetch8(gb);
    return 8;

  case 0x77:
    bus_write(&gb->mem, gb->cpu.hl, gb->cpu.a);
    return 8;

  case 0x78:
    gb->cpu.a = gb->cpu.b;
    return 4;

  case 0xAF:
    gb->cpu.a ^= gb->cpu.a;
    gb->cpu.f = FLAG_Z;
    return 4;

  case 0xB1:
    gb->cpu.a |= gb->cpu.c;
    gb->cpu.f = (gb->cpu.a == 0 ? FLAG_Z : 0);
    return 4;

  case 0xC1:
    gb->cpu.bc = pop16(gb);
    return 12;

  case 0xC3:
    gb->cpu.pc = fetch16(gb);
    return 16;

  case 0xC5:
    push16(gb, gb->cpu.bc);
    return 16;

  case 0xC9:
    gb->cpu.pc = pop16(gb);
    return 16;

  case 0xCD: {
    u16 addr = fetch16(gb);
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = addr;
    return 24;
  }

  case 0xD1:
    gb->cpu.de = pop16(gb);
    return 12;

  case 0xD5:
    push16(gb, gb->cpu.de);
    return 16;

  case 0xE0:
    bus_write(&gb->mem, 0xFF00 + fetch8(gb), gb->cpu.a);
    return 12;

  case 0xE1:
    gb->cpu.hl = pop16(gb);
    return 12;

  case 0xE5:
    push16(gb, gb->cpu.hl);
    return 16;

  case 0xE6:
    gb->cpu.a &= fetch8(gb);
    gb->cpu.f = (gb->cpu.a == 0 ? FLAG_Z : 0) | FLAG_H;
    return 8;

  case 0xF0:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, 0xFF00 + fetch8(gb));
    return 12;

  case 0xF1:
    gb->cpu.af = pop16(gb) & 0xFFF0;
    return 12;

  case 0xF3:
    gb->cpu.ime = FALSE;
    return 4;

  case 0xF5:
    push16(gb, gb->cpu.af);
    return 16;

  case 0xFA:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, fetch16(gb));
    return 16;

  case 0xFB:
    gb->cpu.ime = TRUE;
    return 4;

  default:
    fprintf(stderr, "cpu_step: Unhandled opcode 0x%02X at PC 0x%04X\n", opcode,
            gb->cpu.pc - 1);
    return 0;
  }
}

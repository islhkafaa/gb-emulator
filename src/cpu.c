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

static void alu_add(struct GB *gb, u8 val) {
  u16 res = gb->cpu.a + val;
  gb->cpu.f = (res & 0xFF) == 0 ? FLAG_Z : 0;
  if ((gb->cpu.a & 0x0F) + (val & 0x0F) > 0x0F)
    gb->cpu.f |= FLAG_H;
  if (res > 0xFF)
    gb->cpu.f |= FLAG_C;
  gb->cpu.a = (u8)res;
}

static void alu_adc(struct GB *gb, u8 val) {
  u8 carry = (gb->cpu.f & FLAG_C) ? 1 : 0;
  u16 res = gb->cpu.a + val + carry;
  gb->cpu.f = (res & 0xFF) == 0 ? FLAG_Z : 0;
  if ((gb->cpu.a & 0x0F) + (val & 0x0F) + carry > 0x0F)
    gb->cpu.f |= FLAG_H;
  if (res > 0xFF)
    gb->cpu.f |= FLAG_C;
  gb->cpu.a = (u8)res;
}

static void alu_sub(struct GB *gb, u8 val) {
  u16 res = gb->cpu.a - val;
  gb->cpu.f = FLAG_N;
  if ((res & 0xFF) == 0)
    gb->cpu.f |= FLAG_Z;
  if ((gb->cpu.a & 0x0F) < (val & 0x0F))
    gb->cpu.f |= FLAG_H;
  if (gb->cpu.a < val)
    gb->cpu.f |= FLAG_C;
  gb->cpu.a = (u8)res;
}

static void alu_sbc(struct GB *gb, u8 val) {
  u8 carry = (gb->cpu.f & FLAG_C) ? 1 : 0;
  u16 res = gb->cpu.a - val - carry;
  gb->cpu.f = FLAG_N;
  if ((res & 0xFF) == 0)
    gb->cpu.f |= FLAG_Z;
  if ((gb->cpu.a & 0x0F) < (val & 0x0F) + carry)
    gb->cpu.f |= FLAG_H;
  if (gb->cpu.a < (u16)val + carry)
    gb->cpu.f |= FLAG_C;
  gb->cpu.a = (u8)res;
}

static void alu_and(struct GB *gb, u8 val) {
  gb->cpu.a &= val;
  gb->cpu.f = FLAG_H | (gb->cpu.a == 0 ? FLAG_Z : 0);
}

static void alu_or(struct GB *gb, u8 val) {
  gb->cpu.a |= val;
  gb->cpu.f = gb->cpu.a == 0 ? FLAG_Z : 0;
}

static void alu_xor(struct GB *gb, u8 val) {
  gb->cpu.a ^= val;
  gb->cpu.f = gb->cpu.a == 0 ? FLAG_Z : 0;
}

static void alu_cp(struct GB *gb, u8 val) {
  u16 res = gb->cpu.a - val;
  gb->cpu.f = FLAG_N;
  if ((res & 0xFF) == 0)
    gb->cpu.f |= FLAG_Z;
  if ((gb->cpu.a & 0x0F) < (val & 0x0F))
    gb->cpu.f |= FLAG_H;
  if (gb->cpu.a < val)
    gb->cpu.f |= FLAG_C;
}

void cpu_request_interrupt(struct GB *gb, u8 interrupt) {
  u8 if_flag = bus_read(&gb->mem, gb->rom, 0xFF0F);
  if_flag |= interrupt;
  bus_write(&gb->mem, 0xFF0F, if_flag);
}

static int execute_cb(struct GB *gb) {
  u8 opcode = fetch8(gb);

  switch (opcode) {
  case 0x37: {
    u8 val = gb->cpu.a;
    gb->cpu.a = (val >> 4) | (val << 4);
    gb->cpu.f = (gb->cpu.a == 0 ? FLAG_Z : 0);
    return 8;
  }

  default:
    fprintf(stderr, "execute_cb: Unhandled opcode 0xCB 0x%02X at PC 0x%04X\n",
            opcode, gb->cpu.pc - 2);
    return 0;
  }
}

int cpu_step(struct GB *gb) {
  u8 ie = bus_read(&gb->mem, gb->rom, 0xFFFF);
  u8 if_flag = bus_read(&gb->mem, gb->rom, 0xFF0F);
  u8 pending = ie & if_flag & 0x1F;

  if (gb->cpu.halted) {
    if (pending) {
      gb->cpu.halted = FALSE;
    } else {
      return 4;
    }
  }

  if (gb->cpu.ime && pending) {
    u8 interrupt = 0;
    u16 vector = 0;

    for (int i = 0; i < 5; i++) {
      if (pending & (1 << i)) {
        interrupt = 1 << i;
        vector = 0x0040 + (i * 8);
        break;
      }
    }

    gb->cpu.ime = FALSE;
    if_flag &= ~interrupt;
    bus_write(&gb->mem, 0xFF0F, if_flag);

    push16(gb, gb->cpu.pc);
    gb->cpu.pc = vector;
    return 20;
  }

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

  case 0x2F:
    gb->cpu.a = ~gb->cpu.a;
    gb->cpu.f |= (FLAG_N | FLAG_H);
    return 4;

  case 0x31:
    gb->cpu.sp = fetch16(gb);
    return 12;

  case 0x3D:
    gb->cpu.f &= FLAG_C;
    gb->cpu.f |= FLAG_N;
    if ((gb->cpu.a & 0x0F) == 0)
      gb->cpu.f |= FLAG_H;
    gb->cpu.a--;
    if (gb->cpu.a == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x3E:
    gb->cpu.a = fetch8(gb);
    return 8;

  case 0x47:
    gb->cpu.b = gb->cpu.a;
    return 4;

  case 0x4F:
    gb->cpu.c = gb->cpu.a;
    return 4;

  case 0x76:
    gb->cpu.halted = TRUE;
    return 4;

  case 0x77:
    bus_write(&gb->mem, gb->cpu.hl, gb->cpu.a);
    return 8;

  case 0x78:
    gb->cpu.a = gb->cpu.b;
    return 4;

  case 0x79:
    gb->cpu.a = gb->cpu.c;
    return 4;

  case 0x80:
    alu_add(gb, gb->cpu.b);
    return 4;
  case 0x81:
    alu_add(gb, gb->cpu.c);
    return 4;
  case 0x82:
    alu_add(gb, gb->cpu.d);
    return 4;
  case 0x83:
    alu_add(gb, gb->cpu.e);
    return 4;
  case 0x84:
    alu_add(gb, gb->cpu.h);
    return 4;
  case 0x85:
    alu_add(gb, gb->cpu.l);
    return 4;
  case 0x86:
    alu_add(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0x87:
    alu_add(gb, gb->cpu.a);
    return 4;

  case 0x88:
    alu_adc(gb, gb->cpu.b);
    return 4;
  case 0x89:
    alu_adc(gb, gb->cpu.c);
    return 4;
  case 0x8A:
    alu_adc(gb, gb->cpu.d);
    return 4;
  case 0x8B:
    alu_adc(gb, gb->cpu.e);
    return 4;
  case 0x8C:
    alu_adc(gb, gb->cpu.h);
    return 4;
  case 0x8D:
    alu_adc(gb, gb->cpu.l);
    return 4;
  case 0x8E:
    alu_adc(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0x8F:
    alu_adc(gb, gb->cpu.a);
    return 4;

  case 0x90:
    alu_sub(gb, gb->cpu.b);
    return 4;
  case 0x91:
    alu_sub(gb, gb->cpu.c);
    return 4;
  case 0x92:
    alu_sub(gb, gb->cpu.d);
    return 4;
  case 0x93:
    alu_sub(gb, gb->cpu.e);
    return 4;
  case 0x94:
    alu_sub(gb, gb->cpu.h);
    return 4;
  case 0x95:
    alu_sub(gb, gb->cpu.l);
    return 4;
  case 0x96:
    alu_sub(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0x97:
    alu_sub(gb, gb->cpu.a);
    return 4;

  case 0x98:
    alu_sbc(gb, gb->cpu.b);
    return 4;
  case 0x99:
    alu_sbc(gb, gb->cpu.c);
    return 4;
  case 0x9A:
    alu_sbc(gb, gb->cpu.d);
    return 4;
  case 0x9B:
    alu_sbc(gb, gb->cpu.e);
    return 4;
  case 0x9C:
    alu_sbc(gb, gb->cpu.h);
    return 4;
  case 0x9D:
    alu_sbc(gb, gb->cpu.l);
    return 4;
  case 0x9E:
    alu_sbc(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0x9F:
    alu_sbc(gb, gb->cpu.a);
    return 4;

  case 0xA0:
    alu_and(gb, gb->cpu.b);
    return 4;
  case 0xA1:
    alu_and(gb, gb->cpu.c);
    return 4;
  case 0xA2:
    alu_and(gb, gb->cpu.d);
    return 4;
  case 0xA3:
    alu_and(gb, gb->cpu.e);
    return 4;
  case 0xA4:
    alu_and(gb, gb->cpu.h);
    return 4;
  case 0xA5:
    alu_and(gb, gb->cpu.l);
    return 4;
  case 0xA6:
    alu_and(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0xA7:
    alu_and(gb, gb->cpu.a);
    return 4;

  case 0xA8:
    alu_xor(gb, gb->cpu.b);
    return 4;
  case 0xA9:
    alu_xor(gb, gb->cpu.c);
    return 4;
  case 0xAA:
    alu_xor(gb, gb->cpu.d);
    return 4;
  case 0xAB:
    alu_xor(gb, gb->cpu.e);
    return 4;
  case 0xAC:
    alu_xor(gb, gb->cpu.h);
    return 4;
  case 0xAD:
    alu_xor(gb, gb->cpu.l);
    return 4;
  case 0xAE:
    alu_xor(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0xAF:
    alu_xor(gb, gb->cpu.a);
    return 4;

  case 0xB0:
    alu_or(gb, gb->cpu.b);
    return 4;
  case 0xB1:
    alu_or(gb, gb->cpu.c);
    return 4;
  case 0xB2:
    alu_or(gb, gb->cpu.d);
    return 4;
  case 0xB3:
    alu_or(gb, gb->cpu.e);
    return 4;
  case 0xB4:
    alu_or(gb, gb->cpu.h);
    return 4;
  case 0xB5:
    alu_or(gb, gb->cpu.l);
    return 4;
  case 0xB6:
    alu_or(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0xB7:
    alu_or(gb, gb->cpu.a);
    return 4;

  case 0xB8:
    alu_cp(gb, gb->cpu.b);
    return 4;
  case 0xB9:
    alu_cp(gb, gb->cpu.c);
    return 4;
  case 0xBA:
    alu_cp(gb, gb->cpu.d);
    return 4;
  case 0xBB:
    alu_cp(gb, gb->cpu.e);
    return 4;
  case 0xBC:
    alu_cp(gb, gb->cpu.h);
    return 4;
  case 0xBD:
    alu_cp(gb, gb->cpu.l);
    return 4;
  case 0xBE:
    alu_cp(gb, bus_read(&gb->mem, gb->rom, gb->cpu.hl));
    return 8;
  case 0xBF:
    alu_cp(gb, gb->cpu.a);
    return 4;

  case 0xC1:
    gb->cpu.bc = pop16(gb);
    return 12;

  case 0xC2: {
    u16 addr = fetch16(gb);
    if (!(gb->cpu.f & FLAG_Z)) {
      gb->cpu.pc = addr;
      return 16;
    }
    return 12;
  }

  case 0xC3:
    gb->cpu.pc = fetch16(gb);
    return 16;

  case 0xCA: {
    u16 addr = fetch16(gb);
    if (gb->cpu.f & FLAG_Z) {
      gb->cpu.pc = addr;
      return 16;
    }
    return 12;
  }

  case 0xC4: {
    u16 addr = fetch16(gb);
    if (!(gb->cpu.f & FLAG_Z)) {
      push16(gb, gb->cpu.pc);
      gb->cpu.pc = addr;
      return 24;
    }
    return 12;
  }

  case 0xC5:
    push16(gb, gb->cpu.bc);
    return 16;

  case 0xC9:
    gb->cpu.pc = pop16(gb);
    return 16;

  case 0xCB:
    return execute_cb(gb);

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

  case 0xFE:
    alu_cp(gb, fetch8(gb));
    return 8;

  default:
    fprintf(stderr, "cpu_step: Unhandled opcode 0x%02X at PC 0x%04X\n", opcode,
            gb->cpu.pc - 1);
    return 0;
  }
}

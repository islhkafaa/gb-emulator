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
  bus_write(&gb->mem, gb->rom, gb->cpu.sp, (u8)(val & 0xFF));
  bus_write(&gb->mem, gb->rom, gb->cpu.sp + 1, (u8)((val >> 8) & 0xFF));
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
  bus_write(&gb->mem, gb->rom, 0xFF0F, if_flag);
}

static u8 *get_reg8(struct GB *gb, int idx) {
  switch (idx) {
  case 0:
    return &gb->cpu.b;
  case 1:
    return &gb->cpu.c;
  case 2:
    return &gb->cpu.d;
  case 3:
    return &gb->cpu.e;
  case 4:
    return &gb->cpu.h;
  case 5:
    return &gb->cpu.l;
  case 7:
    return &gb->cpu.a;
  default:
    return NULL;
  }
}

static u8 cb_read(struct GB *gb, int idx) {
  if (idx == 6)
    return bus_read(&gb->mem, gb->rom, gb->cpu.hl);
  return *get_reg8(gb, idx);
}

static void cb_write(struct GB *gb, int idx, u8 val) {
  if (idx == 6)
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, val);
  else
    *get_reg8(gb, idx) = val;
}

static int execute_cb(struct GB *gb) {
  u8 opcode = fetch8(gb);
  int reg_idx = opcode & 0x07;
  int bit = (opcode >> 3) & 0x07;
  u8 val = cb_read(gb, reg_idx);
  int cycles = (reg_idx == 6) ? 16 : 8;

  if (opcode < 0x40) {
    switch (opcode >> 3) {
    case 0: { // RLC
      u8 c = (val & 0x80) >> 7;
      val = (val << 1) | c;
      gb->cpu.f = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    } break;
    case 1: { // RRC
      u8 c = val & 0x01;
      val = (val >> 1) | (c << 7);
      gb->cpu.f = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    } break;
    case 2: { // RL
      u8 old_c = (gb->cpu.f & FLAG_C) ? 1 : 0;
      u8 c = (val & 0x80) >> 7;
      val = (val << 1) | old_c;
      gb->cpu.f = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    } break;
    case 3: { // RR
      u8 old_c = (gb->cpu.f & FLAG_C) ? 0x80 : 0;
      u8 c = val & 0x01;
      val = (val >> 1) | old_c;
      gb->cpu.f = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    } break;
    case 4: { // SLA
      u8 c = (val & 0x80) >> 7;
      val <<= 1;
      gb->cpu.f = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    } break;
    case 5: { // SRA
      u8 c = val & 0x01;
      val = (i8)val >> 1;
      gb->cpu.f = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    } break;
    case 6: { // SWAP
      val = (val >> 4) | (val << 4);
      gb->cpu.f = (val == 0 ? FLAG_Z : 0);
    } break;
    case 7: { // SRL
      u8 c = val & 0x01;
      val >>= 1;
      gb->cpu.f = (val == 0 ? FLAG_Z : 0) | (c ? FLAG_C : 0);
    } break;
    }
    cb_write(gb, reg_idx, val);
  } else if (opcode < 0x80) { // BIT
    gb->cpu.f = (gb->cpu.f & FLAG_C) | FLAG_H;
    if (!(val & (1 << bit)))
      gb->cpu.f |= FLAG_Z;
    cycles = (reg_idx == 6) ? 12 : 8;
  } else if (opcode < 0xC0) { // RES
    cb_write(gb, reg_idx, val & ~(1 << bit));
  } else { // SET
    cb_write(gb, reg_idx, val | (1 << bit));
  }
  return cycles;
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
    bus_write(&gb->mem, gb->rom, 0xFF0F, if_flag);

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

  case 0x02:
    bus_write(&gb->mem, gb->rom, gb->cpu.bc, gb->cpu.a);
    return 8;

  case 0x03:
    gb->cpu.bc++;
    return 8;

  case 0x04:
    gb->cpu.f &= FLAG_C;
    if ((gb->cpu.b & 0x0F) == 0x0F)
      gb->cpu.f |= FLAG_H;
    gb->cpu.b++;
    if (gb->cpu.b == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x05:
    gb->cpu.f &= FLAG_C;
    gb->cpu.f |= FLAG_N;
    if ((gb->cpu.b & 0x0F) == 0)
      gb->cpu.f |= FLAG_H;
    gb->cpu.b--;
    if (gb->cpu.b == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x06:
    gb->cpu.b = fetch8(gb);
    return 8;

  case 0x07: {
    u8 carry = (gb->cpu.a & 0x80) >> 7;
    gb->cpu.a = (gb->cpu.a << 1) | carry;
    gb->cpu.f = (carry ? FLAG_C : 0);
    return 4;
  }

  case 0x08: {
    u16 addr = fetch16(gb);
    bus_write(&gb->mem, gb->rom, addr, (u8)(gb->cpu.sp & 0xFF));
    bus_write(&gb->mem, gb->rom, addr + 1, (u8)((gb->cpu.sp >> 8) & 0xFF));
    return 20;
  }

  case 0x09: {
    u32 res = (u32)gb->cpu.hl + (u32)gb->cpu.bc;
    gb->cpu.f &= FLAG_Z;
    if (((gb->cpu.hl & 0x0FFF) + (gb->cpu.bc & 0x0FFF)) > 0x0FFF)
      gb->cpu.f |= FLAG_H;
    if (res > 0xFFFF)
      gb->cpu.f |= FLAG_C;
    gb->cpu.hl = (u16)res;
    return 8;
  }

  case 0x0A:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, gb->cpu.bc);
    return 8;

  case 0x0B:
    gb->cpu.bc--;
    return 8;

  case 0x0C:
    gb->cpu.f &= FLAG_C;
    if ((gb->cpu.c & 0x0F) == 0x0F)
      gb->cpu.f |= FLAG_H;
    gb->cpu.c++;
    if (gb->cpu.c == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

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

  case 0x0F: {
    u8 carry = (gb->cpu.a & 0x01);
    gb->cpu.a = (gb->cpu.a >> 1) | (carry << 7);
    gb->cpu.f = (carry ? FLAG_C : 0);
    return 4;
  }

  case 0x10:
    fetch8(gb);
    return 4;

  case 0x11:
    gb->cpu.de = fetch16(gb);
    return 12;

  case 0x12:
    bus_write(&gb->mem, gb->rom, gb->cpu.de, gb->cpu.a);
    return 8;

  case 0x13:
    gb->cpu.de++;
    return 8;

  case 0x14:
    gb->cpu.f &= FLAG_C;
    if ((gb->cpu.d & 0x0F) == 0x0F)
      gb->cpu.f |= FLAG_H;
    gb->cpu.d++;
    if (gb->cpu.d == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x15:
    gb->cpu.f &= FLAG_C;
    gb->cpu.f |= FLAG_N;
    if ((gb->cpu.d & 0x0F) == 0)
      gb->cpu.f |= FLAG_H;
    gb->cpu.d--;
    if (gb->cpu.d == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x16:
    gb->cpu.d = fetch8(gb);
    return 8;

  case 0x17: {
    u8 old_carry = (gb->cpu.f & FLAG_C) ? 1 : 0;
    u8 carry = (gb->cpu.a & 0x80) >> 7;
    gb->cpu.a = (gb->cpu.a << 1) | old_carry;
    gb->cpu.f = (carry ? FLAG_C : 0);
    return 4;
  }

  case 0x18: {
    i8 rel = (i8)fetch8(gb);
    gb->cpu.pc += rel;
    return 12;
  }

  case 0x19: {
    u32 res = (u32)gb->cpu.hl + (u32)gb->cpu.de;
    gb->cpu.f &= FLAG_Z;
    if (((gb->cpu.hl & 0x0FFF) + (gb->cpu.de & 0x0FFF)) > 0x0FFF)
      gb->cpu.f |= FLAG_H;
    if (res > 0xFFFF)
      gb->cpu.f |= FLAG_C;
    gb->cpu.hl = (u16)res;
    return 8;
  }

  case 0x1A:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, gb->cpu.de);
    return 8;

  case 0x1B:
    gb->cpu.de--;
    return 8;

  case 0x1C:
    gb->cpu.f &= FLAG_C;
    if ((gb->cpu.e & 0x0F) == 0x0F)
      gb->cpu.f |= FLAG_H;
    gb->cpu.e++;
    if (gb->cpu.e == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x1D:
    gb->cpu.f &= FLAG_C;
    gb->cpu.f |= FLAG_N;
    if ((gb->cpu.e & 0x0F) == 0)
      gb->cpu.f |= FLAG_H;
    gb->cpu.e--;
    if (gb->cpu.e == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x1E:
    gb->cpu.e = fetch8(gb);
    return 8;

  case 0x1F: {
    u8 old_carry = (gb->cpu.f & FLAG_C) ? 0x80 : 0;
    u8 carry = (gb->cpu.a & 0x01);
    gb->cpu.a = (gb->cpu.a >> 1) | old_carry;
    gb->cpu.f = (carry ? FLAG_C : 0);
    return 4;
  }

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
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.a);
    gb->cpu.hl++;
    return 8;

  case 0x23:
    gb->cpu.hl++;
    return 8;

  case 0x24:
    gb->cpu.f &= FLAG_C;
    if ((gb->cpu.h & 0x0F) == 0x0F)
      gb->cpu.f |= FLAG_H;
    gb->cpu.h++;
    if (gb->cpu.h == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x25:
    gb->cpu.f &= FLAG_C;
    gb->cpu.f |= FLAG_N;
    if ((gb->cpu.h & 0x0F) == 0)
      gb->cpu.f |= FLAG_H;
    gb->cpu.h--;
    if (gb->cpu.h == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x26:
    gb->cpu.h = fetch8(gb);
    return 8;

  case 0x27: {
    u8 a = gb->cpu.a;
    u8 adjust = 0;
    if (gb->cpu.f & FLAG_H || (!(gb->cpu.f & FLAG_N) && (a & 0x0F) > 9))
      adjust |= 0x06;
    if (gb->cpu.f & FLAG_C || (!(gb->cpu.f & FLAG_N) && a > 0x99)) {
      adjust |= 0x60;
      gb->cpu.f |= FLAG_C;
    }
    a += (gb->cpu.f & FLAG_N) ? -adjust : adjust;
    gb->cpu.f &= ~(FLAG_H | FLAG_Z);
    if (a == 0)
      gb->cpu.f |= FLAG_Z;
    gb->cpu.a = a;
    return 4;
  }

  case 0x28: {
    i8 rel = (i8)fetch8(gb);
    if (gb->cpu.f & FLAG_Z) {
      gb->cpu.pc += rel;
      return 12;
    }
    return 8;
  }

  case 0x29: {
    u32 res = (u32)gb->cpu.hl + (u32)gb->cpu.hl;
    gb->cpu.f &= FLAG_Z;
    if (((gb->cpu.hl & 0x0FFF) + (gb->cpu.hl & 0x0FFF)) > 0x0FFF)
      gb->cpu.f |= FLAG_H;
    if (res > 0xFFFF)
      gb->cpu.f |= FLAG_C;
    gb->cpu.hl = (u16)res;
    return 8;
  }

  case 0x2A:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    gb->cpu.hl++;
    return 8;

  case 0x2B:
    gb->cpu.hl--;
    return 8;

  case 0x2C:
    gb->cpu.f &= FLAG_C;
    if ((gb->cpu.l & 0x0F) == 0x0F)
      gb->cpu.f |= FLAG_H;
    gb->cpu.l++;
    if (gb->cpu.l == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x2D:
    gb->cpu.f &= FLAG_C;
    gb->cpu.f |= FLAG_N;
    if ((gb->cpu.l & 0x0F) == 0)
      gb->cpu.f |= FLAG_H;
    gb->cpu.l--;
    if (gb->cpu.l == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

  case 0x2E:
    gb->cpu.l = fetch8(gb);
    return 8;

  case 0x2F:
    gb->cpu.a = ~gb->cpu.a;
    gb->cpu.f |= (FLAG_N | FLAG_H);
    return 4;

  case 0x30: {
    i8 rel = (i8)fetch8(gb);
    if (!(gb->cpu.f & FLAG_C)) {
      gb->cpu.pc += rel;
      return 12;
    }
    return 8;
  }

  case 0x31:
    gb->cpu.sp = fetch16(gb);
    return 12;

  case 0x32:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.a);
    gb->cpu.hl--;
    return 8;

  case 0x33:
    gb->cpu.sp++;
    return 8;

  case 0x34: {
    u8 val = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    u8 h_flag = (val & 0x0F) == 0x0F ? FLAG_H : 0;
    val++;
    gb->cpu.f = (gb->cpu.f & FLAG_C) | (val == 0 ? FLAG_Z : 0) | h_flag;
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, val);
    return 12;
  }

  case 0x35: {
    u8 val = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    u8 h_flag = (val & 0x0F) == 0 ? FLAG_H : 0;
    val--;
    gb->cpu.f =
        (gb->cpu.f & FLAG_C) | FLAG_N | (val == 0 ? FLAG_Z : 0) | h_flag;
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, val);
    return 12;
  }

  case 0x36:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, fetch8(gb));
    return 12;

  case 0x38: {
    i8 rel = (i8)fetch8(gb);
    if (gb->cpu.f & FLAG_C) {
      gb->cpu.pc += rel;
      return 12;
    }
    return 8;
  }

  case 0x39: {
    u32 res = (u32)gb->cpu.hl + (u32)gb->cpu.sp;
    gb->cpu.f &= FLAG_Z;
    if (((gb->cpu.hl & 0x0FFF) + (gb->cpu.sp & 0x0FFF)) > 0x0FFF)
      gb->cpu.f |= FLAG_H;
    if (res > 0xFFFF)
      gb->cpu.f |= FLAG_C;
    gb->cpu.hl = (u16)res;
    return 8;
  }

  case 0x3A:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    gb->cpu.hl--;
    return 8;

  case 0x3B:
    gb->cpu.sp--;
    return 8;

  case 0x3C:
    gb->cpu.f &= FLAG_C;
    if ((gb->cpu.a & 0x0F) == 0x0F)
      gb->cpu.f |= FLAG_H;
    gb->cpu.a++;
    if (gb->cpu.a == 0)
      gb->cpu.f |= FLAG_Z;
    return 4;

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

  case 0x3F:
    gb->cpu.f = (gb->cpu.f & (FLAG_Z | FLAG_C)) ^ FLAG_C;
    return 4;

  case 0x37:
    gb->cpu.f = (gb->cpu.f & FLAG_Z) | FLAG_C;
    return 4;

  case 0x40:
    return 4;
  case 0x41:
    gb->cpu.b = gb->cpu.c;
    return 4;
  case 0x42:
    gb->cpu.b = gb->cpu.d;
    return 4;
  case 0x43:
    gb->cpu.b = gb->cpu.e;
    return 4;
  case 0x44:
    gb->cpu.b = gb->cpu.h;
    return 4;
  case 0x45:
    gb->cpu.b = gb->cpu.l;
    return 4;
  case 0x46:
    gb->cpu.b = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    return 8;

  case 0x48:
    gb->cpu.c = gb->cpu.b;
    return 4;
  case 0x49:
    return 4;
  case 0x4A:
    gb->cpu.c = gb->cpu.d;
    return 4;
  case 0x4B:
    gb->cpu.c = gb->cpu.e;
    return 4;
  case 0x4C:
    gb->cpu.c = gb->cpu.h;
    return 4;
  case 0x4D:
    gb->cpu.c = gb->cpu.l;
    return 4;
  case 0x4E:
    gb->cpu.c = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    return 8;

  case 0x50:
    gb->cpu.d = gb->cpu.b;
    return 4;
  case 0x51:
    gb->cpu.d = gb->cpu.c;
    return 4;
  case 0x52:
    return 4;
  case 0x53:
    gb->cpu.d = gb->cpu.e;
    return 4;
  case 0x54:
    gb->cpu.d = gb->cpu.h;
    return 4;
  case 0x55:
    gb->cpu.d = gb->cpu.l;
    return 4;
  case 0x56:
    gb->cpu.d = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    return 8;
  case 0x57:
    gb->cpu.d = gb->cpu.a;
    return 4;

  case 0x58:
    gb->cpu.e = gb->cpu.b;
    return 4;
  case 0x59:
    gb->cpu.e = gb->cpu.c;
    return 4;
  case 0x5A:
    gb->cpu.e = gb->cpu.d;
    return 4;
  case 0x5B:
    return 4;
  case 0x5C:
    gb->cpu.e = gb->cpu.h;
    return 4;
  case 0x5D:
    gb->cpu.e = gb->cpu.l;
    return 4;
  case 0x5E:
    gb->cpu.e = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    return 8;
  case 0x5F:
    gb->cpu.e = gb->cpu.a;
    return 4;

  case 0x70:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.b);
    return 8;
  case 0x71:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.c);
    return 8;
  case 0x72:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.d);
    return 8;
  case 0x73:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.e);
    return 8;
  case 0x74:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.h);
    return 8;
  case 0x75:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.l);
    return 8;

  case 0x7E:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    return 8;

  case 0x7A:
    gb->cpu.a = gb->cpu.d;
    return 4;
  case 0x7B:
    gb->cpu.a = gb->cpu.e;
    return 4;
  case 0x7F:
    return 4;

  case 0x47:
    gb->cpu.b = gb->cpu.a;
    return 4;

  case 0x4F:
    gb->cpu.c = gb->cpu.a;
    return 4;

  case 0x60:
    gb->cpu.h = gb->cpu.b;
    return 4;

  case 0x61:
    gb->cpu.h = gb->cpu.c;
    return 4;

  case 0x62:
    gb->cpu.h = gb->cpu.d;
    return 4;

  case 0x63:
    gb->cpu.h = gb->cpu.e;
    return 4;

  case 0x64:
    gb->cpu.h = gb->cpu.h;
    return 4;

  case 0x65:
    gb->cpu.h = gb->cpu.l;
    return 4;

  case 0x66:
    gb->cpu.h = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    return 8;

  case 0x67:
    gb->cpu.h = gb->cpu.a;
    return 4;

  case 0x68:
    gb->cpu.l = gb->cpu.b;
    return 4;

  case 0x69:
    gb->cpu.l = gb->cpu.c;
    return 4;

  case 0x6A:
    gb->cpu.l = gb->cpu.d;
    return 4;

  case 0x6B:
    gb->cpu.l = gb->cpu.e;
    return 4;

  case 0x6C:
    gb->cpu.l = gb->cpu.h;
    return 4;

  case 0x6D:
    gb->cpu.l = gb->cpu.l;
    return 4;

  case 0x6E:
    gb->cpu.l = bus_read(&gb->mem, gb->rom, gb->cpu.hl);
    return 8;

  case 0x6F:
    gb->cpu.l = gb->cpu.a;
    return 4;

  case 0x76:
    gb->cpu.halted = TRUE;
    return 4;

  case 0x77:
    bus_write(&gb->mem, gb->rom, gb->cpu.hl, gb->cpu.a);
    return 8;

  case 0x78:
    gb->cpu.a = gb->cpu.b;
    return 4;

  case 0x79:
    gb->cpu.a = gb->cpu.c;
    return 4;

  case 0x7C:
    gb->cpu.a = gb->cpu.h;
    return 4;

  case 0x7D:
    gb->cpu.a = gb->cpu.l;
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

  case 0xC0:
    if (!(gb->cpu.f & FLAG_Z)) {
      gb->cpu.pc = pop16(gb);
      return 20;
    }
    return 8;

  case 0xC6:
    alu_add(gb, fetch8(gb));
    return 8;

  case 0xC7:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0000;
    return 16;

  case 0xCC: {
    u16 addr = fetch16(gb);
    if (gb->cpu.f & FLAG_Z) {
      push16(gb, gb->cpu.pc);
      gb->cpu.pc = addr;
      return 24;
    }
    return 12;
  }

  case 0xCE:
    alu_adc(gb, fetch8(gb));
    return 8;

  case 0xCF:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0008;
    return 16;

  case 0xD0:
    if (!(gb->cpu.f & FLAG_C)) {
      gb->cpu.pc = pop16(gb);
      return 20;
    }
    return 8;

  case 0xD2: {
    u16 addr = fetch16(gb);
    if (!(gb->cpu.f & FLAG_C)) {
      gb->cpu.pc = addr;
      return 16;
    }
    return 12;
  }

  case 0xD4: {
    u16 addr = fetch16(gb);
    if (!(gb->cpu.f & FLAG_C)) {
      push16(gb, gb->cpu.pc);
      gb->cpu.pc = addr;
      return 24;
    }
    return 12;
  }

  case 0xD6:
    alu_sub(gb, fetch8(gb));
    return 8;

  case 0xD7:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0010;
    return 16;

  case 0xD8:
    if (gb->cpu.f & FLAG_C) {
      gb->cpu.pc = pop16(gb);
      return 20;
    }
    return 8;

  case 0xD9:
    gb->cpu.pc = pop16(gb);
    gb->cpu.ime = TRUE;
    return 16;

  case 0xDA: {
    u16 addr = fetch16(gb);
    if (gb->cpu.f & FLAG_C) {
      gb->cpu.pc = addr;
      return 16;
    }
    return 12;
  }

  case 0xDC: {
    u16 addr = fetch16(gb);
    if (gb->cpu.f & FLAG_C) {
      push16(gb, gb->cpu.pc);
      gb->cpu.pc = addr;
      return 24;
    }
    return 12;
  }

  case 0xDE:
    alu_sbc(gb, fetch8(gb));
    return 8;

  case 0xDF:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0018;
    return 16;

  case 0xE7:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0020;
    return 16;

  case 0xE8: {
    i8 rel = (i8)fetch8(gb);
    u16 old_sp = gb->cpu.sp;
    gb->cpu.sp = old_sp + rel;
    gb->cpu.f = 0;
    if ((old_sp & 0x000F) + (rel & 0x0F) > 0x000F)
      gb->cpu.f |= FLAG_H;
    if ((old_sp & 0x00FF) + (rel & 0xFF) > 0x00FF)
      gb->cpu.f |= FLAG_C;
    return 16;
  }

  case 0xEE:
    alu_xor(gb, fetch8(gb));
    return 8;

  case 0xEF:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0028;
    return 16;

  case 0xF2:
    gb->cpu.a = bus_read(&gb->mem, gb->rom, 0xFF00 + gb->cpu.c);
    return 8;

  case 0xF7:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0030;
    return 16;

  case 0xF8: {
    i8 rel = (i8)fetch8(gb);
    u16 old_sp = gb->cpu.sp;
    gb->cpu.hl = old_sp + rel;
    gb->cpu.f = 0;
    if ((old_sp & 0x000F) + (rel & 0x0F) > 0x000F)
      gb->cpu.f |= FLAG_H;
    if ((old_sp & 0x00FF) + (rel & 0xFF) > 0x00FF)
      gb->cpu.f |= FLAG_C;
    return 12;
  }

  case 0xF9:
    gb->cpu.sp = gb->cpu.hl;
    return 8;

  case 0xFF:
    push16(gb, gb->cpu.pc);
    gb->cpu.pc = 0x0038;
    return 16;

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

  case 0xC8:
    if (gb->cpu.f & FLAG_Z) {
      gb->cpu.pc = pop16(gb);
      return 20;
    }
    return 8;

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
    bus_write(&gb->mem, gb->rom, 0xFF00 + fetch8(gb), gb->cpu.a);
    return 12;

  case 0xE1:
    gb->cpu.hl = pop16(gb);
    return 12;

  case 0xE2:
    bus_write(&gb->mem, gb->rom, 0xFF00 + gb->cpu.c, gb->cpu.a);
    return 8;

  case 0xE5:
    push16(gb, gb->cpu.hl);
    return 16;

  case 0xE6:
    gb->cpu.a &= fetch8(gb);
    gb->cpu.f = (gb->cpu.a == 0 ? FLAG_Z : 0) | FLAG_H;
    return 8;

  case 0xEA:
    bus_write(&gb->mem, gb->rom, fetch16(gb), gb->cpu.a);
    return 16;

  case 0xE9:
    gb->cpu.pc = gb->cpu.hl;
    return 4;

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

  case 0xF6:
    alu_or(gb, fetch8(gb));
    return 8;

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

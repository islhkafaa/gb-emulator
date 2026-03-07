#ifndef GB_CPU_H
#define GB_CPU_H

#include "types.h"

#define FLAG_Z (1 << 7)
#define FLAG_N (1 << 6)
#define FLAG_H (1 << 5)
#define FLAG_C (1 << 4)

typedef struct {
  union {
    struct {
      u8 f;
      u8 a;
    };
    u16 af;
  };
  union {
    struct {
      u8 c;
      u8 b;
    };
    u16 bc;
  };
  union {
    struct {
      u8 e;
      u8 d;
    };
    u16 de;
  };
  union {
    struct {
      u8 l;
      u8 h;
    };
    u16 hl;
  };

  u16 sp;
  u16 pc;

  bool_t ime;
  bool_t halted;
} CPU;

struct GB;

void cpu_init(CPU *cpu);
int cpu_step(struct GB *gb);

#endif

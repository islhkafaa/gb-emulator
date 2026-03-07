#ifndef GB_H
#define GB_H

#include "cpu.h"
#include "memory.h"
#include "ppu.h"
#include "types.h"
#include <SDL2/SDL.h>
#include <stddef.h>

#define GB_WIDTH 160
#define GB_HEIGHT 144
#define GB_SCALE 4

typedef struct GB {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  bool_t running;
  Memory mem;
  u8 *rom;
  size_t rom_size;
  CPU cpu;
  PPU ppu;
  int div_counter;
  int timer_counter;
} GB;

int gb_init(GB *gb);
void gb_run(GB *gb);
void gb_quit(GB *gb);

#endif

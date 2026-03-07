#ifndef GB_JOYPAD_H
#define GB_JOYPAD_H

#include "types.h"
#include <SDL2/SDL.h>

struct GB;

typedef struct {
  u8 action_keys;
  u8 dir_keys;
} Joypad;

void joypad_press(struct GB *gb, SDL_Keycode key);
void joypad_release(struct GB *gb, SDL_Keycode key);
u8 joypad_read(struct GB *gb);
void joypad_write(struct GB *gb, u8 val);

#endif

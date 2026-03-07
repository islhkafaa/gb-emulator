#include "../include/joypad.h"
#include "../include/gb.h"
#include "../include/interrupts.h"

void joypad_press(GB *gb, SDL_Keycode key) {
  u8 prev_action = gb->joypad.action_keys;
  u8 prev_dir = gb->joypad.dir_keys;

  switch (key) {
  case SDLK_z:
    gb->joypad.action_keys &= ~(1 << 1);
    break;
  case SDLK_x:
    gb->joypad.action_keys &= ~(1 << 0);
    break;
  case SDLK_RSHIFT:
  case SDLK_LSHIFT:
    gb->joypad.action_keys &= ~(1 << 2);
    break;
  case SDLK_RETURN:
    gb->joypad.action_keys &= ~(1 << 3);
    break;

  case SDLK_RIGHT:
    gb->joypad.dir_keys &= ~(1 << 0);
    break;
  case SDLK_LEFT:
    gb->joypad.dir_keys &= ~(1 << 1);
    break;
  case SDLK_UP:
    gb->joypad.dir_keys &= ~(1 << 2);
    break;
  case SDLK_DOWN:
    gb->joypad.dir_keys &= ~(1 << 3);
    break;
  }

  if (prev_action != gb->joypad.action_keys ||
      prev_dir != gb->joypad.dir_keys) {
    cpu_request_interrupt(gb, INT_JOYPAD);
  }
}

void joypad_release(GB *gb, SDL_Keycode key) {
  switch (key) {
  case SDLK_z:
    gb->joypad.action_keys |= (1 << 1);
    break;
  case SDLK_x:
    gb->joypad.action_keys |= (1 << 0);
    break;
  case SDLK_RSHIFT:
  case SDLK_LSHIFT:
    gb->joypad.action_keys |= (1 << 2);
    break;
  case SDLK_RETURN:
    gb->joypad.action_keys |= (1 << 3);
    break;

  case SDLK_RIGHT:
    gb->joypad.dir_keys |= (1 << 0);
    break;
  case SDLK_LEFT:
    gb->joypad.dir_keys |= (1 << 1);
    break;
  case SDLK_UP:
    gb->joypad.dir_keys |= (1 << 2);
    break;
  case SDLK_DOWN:
    gb->joypad.dir_keys |= (1 << 3);
    break;
  }
}

u8 joypad_read(GB *gb) {
  u8 mode = gb->mem.io[0x00];
  u8 ret = 0xCF | mode;

  if ((mode & 0x10) == 0) {
    ret &= gb->joypad.dir_keys;
  }
  if ((mode & 0x20) == 0) {
    ret &= gb->joypad.action_keys;
  }

  return ret;
}

void joypad_write(GB *gb, u8 val) {
  gb->mem.io[0x00] = (gb->mem.io[0x00] & 0xCF) | (val & 0x30);
}

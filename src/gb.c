#include "../include/gb.h"
#include "../include/ppu.h"
#include "../include/rom.h"
#include "../include/savestate.h"
#include "../include/timer.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

int gb_init(GB *gb, const char *path) {
  if (path) {
    strncpy(gb->rom_path, path, sizeof(gb->rom_path) - 1);
    gb->rom_path[sizeof(gb->rom_path) - 1] = '\0';
  } else {
    gb->rom_path[0] = '\0';
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 0;
  }

  gb->window = SDL_CreateWindow("GB Emulator", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, GB_WIDTH * GB_SCALE,
                                GB_HEIGHT * GB_SCALE, SDL_WINDOW_SHOWN);

  if (!gb->window) {
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
    return 0;
  }

  gb->renderer = SDL_CreateRenderer(gb->window, -1, SDL_RENDERER_ACCELERATED);

  if (!gb->renderer) {
    fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(gb->window);
    SDL_Quit();
    return 0;
  }

  gb->texture =
      SDL_CreateTexture(gb->renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, GB_WIDTH, GB_HEIGHT);

  if (!gb->texture) {
    fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
    SDL_DestroyRenderer(gb->renderer);
    SDL_DestroyWindow(gb->window);
    SDL_Quit();
    return 0;
  }

  SDL_RenderSetLogicalSize(gb->renderer, GB_WIDTH, GB_HEIGHT);

  cpu_init(&gb->cpu);

  gb->mem.gb_ptr = gb;
  memory_init(&gb->mem, gb->rom);

  if (gb->rom) {
    u8 type = gb->rom[0x0147];
    if (type == 0x03 || type == 0x06 || type == 0x09 || type == 0x10 ||
        type == 0x13 || type == 0x1B || type == 0x1E || type == 0xFF) {
      ram_load(gb->rom_path, gb->mem.ext_ram, MEM_EXT_RAM_SIZE);
    }
  }

  apu_init(gb);

  gb->joypad.action_keys = 0x0F;
  gb->joypad.dir_keys = 0x0F;

  gb->running = TRUE;
  return 1;
}

void gb_run(GB *gb) {
  SDL_Event event;
  u32 start_ticks = SDL_GetTicks();
  u64 frames = 0;

  while (gb->running) {
    u32 target_ticks = start_ticks + (u32)(frames * (1000.0 / 59.7275));
    u32 current_ticks = SDL_GetTicks();

    if (current_ticks < target_ticks) {
      SDL_Delay(target_ticks - current_ticks);
    }

    int cycles = 0;
    while (cycles < 17556) {
      int consumed = cpu_step(gb);
      if (consumed == 0) {
        gb->running = FALSE;
        break;
      }
      timer_tick(gb, consumed);
      ppu_step(gb, consumed);
      apu_step(gb, consumed);
      cycles += consumed;
    }
    frames++;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        gb->running = FALSE;
      }
      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          gb->running = FALSE;
        } else if (event.key.keysym.sym == SDLK_F5) {
          gb_savestate(gb);
        } else if (event.key.keysym.sym == SDLK_F8) {
          gb_loadstate(gb);
        } else {
          joypad_press(gb, event.key.keysym.sym);
        }
      }
      if (event.type == SDL_KEYUP) {
        joypad_release(gb, event.key.keysym.sym);
      }
    }

    SDL_UpdateTexture(gb->texture, NULL, gb->ppu.frame_buffer,
                      GB_WIDTH * sizeof(u32));

    SDL_RenderClear(gb->renderer);
    SDL_RenderCopy(gb->renderer, gb->texture, NULL, NULL);
    SDL_RenderPresent(gb->renderer);
  }
}

void gb_quit(GB *gb) {
  if (gb->rom) {
    u8 type = gb->rom[0x0147];
    if (type == 0x03 || type == 0x06 || type == 0x09 || type == 0x10 ||
        type == 0x13 || type == 0x1B || type == 0x1E || type == 0xFF) {
      ram_save(gb->rom_path, gb->mem.ext_ram, MEM_EXT_RAM_SIZE);
    }
  }

  apu_quit(gb);

  if (gb->texture) {
    SDL_DestroyTexture(gb->texture);
  }
  if (gb->renderer) {
    SDL_DestroyRenderer(gb->renderer);
    gb->renderer = NULL;
  }
  if (gb->window) {
    SDL_DestroyWindow(gb->window);
    gb->window = NULL;
  }
  SDL_Quit();
}

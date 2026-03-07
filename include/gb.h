#ifndef GB_H
#define GB_H

#include <SDL2/SDL.h>
#include "types.h"

#define GB_WIDTH  160
#define GB_HEIGHT 144
#define GB_SCALE  4

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    bool_t        running;
} GB;

int  gb_init(GB *gb);
void gb_run(GB *gb);
void gb_quit(GB *gb);

#endif

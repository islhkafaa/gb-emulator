#include <stdio.h>
#include <SDL2/SDL.h>
#include "gb.h"

int gb_init(GB *gb) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 0;
    }

    gb->window = SDL_CreateWindow(
        "GB Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        GB_WIDTH  * GB_SCALE,
        GB_HEIGHT * GB_SCALE,
        SDL_WINDOW_SHOWN
    );

    if (!gb->window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    gb->renderer = SDL_CreateRenderer(
        gb->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!gb->renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(gb->window);
        SDL_Quit();
        return 0;
    }

    SDL_RenderSetLogicalSize(gb->renderer, GB_WIDTH, GB_HEIGHT);

    gb->running = TRUE;
    return 1;
}

void gb_run(GB *gb) {
    SDL_Event event;

    while (gb->running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                gb->running = FALSE;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                gb->running = FALSE;
            }
        }

        SDL_SetRenderDrawColor(gb->renderer, 0x9B, 0xBC, 0x0F, 0xFF);
        SDL_RenderClear(gb->renderer);
        SDL_RenderPresent(gb->renderer);
    }
}

void gb_quit(GB *gb) {
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

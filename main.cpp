#include "cartridge.h"
#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "joypad.h"
#include <iostream>
#include <SDL2/SDL.h>

const int GB_WIDTH = 160;
const int GB_HEIGHT = 144;
const int SCALE = 4;

int main() {

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "GameBoy",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        GB_WIDTH * SCALE,
        GB_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        GB_WIDTH,
        GB_HEIGHT
    );

    uint32_t frameBuffer[GB_WIDTH * GB_HEIGHT];

    uint32_t palette[4] = {
    0xFFFFFFFF, // white
    0xFFAAAAAA, // light gray
    0xFF555555, // dark gray
    0xFF000000  // black
    };


    Joypad joypad;
    APU apu;
    Memory memory(joypad, apu);
    joypad.connectToMemory(&memory);
    apu.connectToMemory(&memory);

    if (!memory.loadCartridge("pokemon.gb")) {
        std::cerr << "Failed to load cartridge" << std::endl;
        return -1;
    }

    
    CPU cpu(memory);
    PPU ppu(memory);

    SDL_Event e;
    bool running = true;

    const double CPU_FREQ = 4194304.0;
    const double FRAME_TIME = 1.0 / 59.7275;
    uint64_t lastTime = SDL_GetPerformanceCounter();
    double cycleBudget = 0;

    while (running) {

        uint64_t frameStart = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        const Uint8* state = SDL_GetKeyboardState(NULL);
        
        // Get keyboard data and store into joypad for reading
        joypad.update(state);

        uint64_t now = SDL_GetPerformanceCounter();
        double elapsed = (double)(now - lastTime) / SDL_GetPerformanceFrequency();

        lastTime = now;

        cycleBudget += elapsed * CPU_FREQ;

        while (cycleBudget >= 4) {

            int cycles = cpu.step();
            apu.step(cycles);
            ppu.step(cycles);

         if (ppu.getFrameReady()) {

            for (int y = 0; y < GB_HEIGHT; y++) {
                for (int x = 0; x < GB_WIDTH; x++) {
                    frameBuffer[y*GB_WIDTH + x] =
                        palette[ ppu.getFrameBuffer()[y * GB_WIDTH + x]];
                }
            }

            SDL_UpdateTexture(texture, NULL, frameBuffer, GB_WIDTH * sizeof(uint32_t));

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);

            ppu.setFrameReady(false);
        }           

            cycleBudget -= cycles;
        }
        uint64_t frameEnd = SDL_GetPerformanceCounter();
        double frameElapsed = (double)(frameEnd - frameStart) / SDL_GetPerformanceFrequency();

        if (frameElapsed < FRAME_TIME) {
            SDL_Delay((FRAME_TIME - frameElapsed) * 1000.0);
        }

        frameStart = SDL_GetPerformanceCounter();
    }


    return 0;
}

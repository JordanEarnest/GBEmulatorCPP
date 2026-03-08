/*
    Description: The PPU is the pixel processing chip that reads from memory to create images on the visual display of the Game Boy
    Author: Jordan Earnest
*/
#pragma once
#include "memory.h"

// Note: the Game Boy's display is 160 x 144 pixels

struct Sprite {
    uint8_t y;
    uint8_t x;
    uint8_t tile;
    uint8_t attr;
};

class PPU {
private:
    Memory& memory;

    // Stores all visible sprites that intersected LY at OAM Scan 
    std::vector<Sprite> spriteBuffer;
    int spriteCount;

    // Stores entire frame to be displayed on LCD screen when ready
    uint8_t frameBuffer[144][160] = {};

    // Tracks how many t-cycles have happened in time frame
    int cycleCounter;

    // Getters for all PPU related registers
    uint8_t getLY();
    uint8_t getLYC();
    uint8_t getSTAT();
    uint8_t getLCDC(); 
    uint8_t getSCY(); // background scroll y
    uint8_t getSCX(); // background scroll x
    uint8_t getBGP(); // background pallete

    // Setters for all PPU related registers
    void setLY(uint8_t value);
    void setLYC(uint8_t value);
    void setSTAT(uint8_t value);
    void setLCDC(uint8_t value);
    void setSCY(uint8_t value); // background scroll y
    void setSCX(uint8_t value); // background scroll x
    void setBGP(uint8_t value); // background pallete

    // Set mode of PPU
    void setPPUMode(int value);

    // VBlankInterrupt
    void requestVBlankInterrupt();
    // STAT Interrupt
    void requestSTATInterrupt();

    // Helper method to see if sprite intersects with LY
    bool spriteIntersectsLY(uint8_t y);

public:
    PPU(Memory& memory);

    // Draw one frame
    // 1 T cycle = 1 PPU cycle
    void step(int tCycles);
};
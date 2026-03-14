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

    bool frameReady;

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
    uint8_t getWX();
    uint8_t getWY();
    uint8_t getBGP(); // background pallete
    uint8_t getOBP0();
    uint8_t getOBP1(); 


    // Setters for all PPU related registers
    void setLY(uint8_t value);
    void setLYC(uint8_t value);
    void setSTAT(uint8_t value);
    void setLCDC(uint8_t value);
    void setSCY(uint8_t value); // background scroll y
    void setSCX(uint8_t value); // background scroll x
    void setWX(uint8_t value);
    void setWY(uint8_t value);
    void setBGP(uint8_t value); // background pallete
    void setOBP0(uint8_t value);
    void setOBP1(uint8_t value); 


    // Set mode of PPU
    void setPPUMode(int value);

    // VBlankInterrupt
    void requestVBlankInterrupt();
    // STAT Interrupt
    void requestSTATInterrupt();

    // Helper method to see if sprite intersects with LY
    bool spriteIntersectsLY(uint8_t y);

    // intialize hardware
    void initializeHardware();

    void fillScanLineWithBackground(uint8_t x, uint8_t scx, uint8_t scy, uint8_t ly, uint8_t lcdc, uint8_t bgp);

    void fillScanLineWithWindow(uint8_t x, uint8_t wx, uint8_t wy, uint8_t ly, uint8_t lcdc, uint8_t bgp);

    void fillScanLineWithSpritesFromBuffer(uint8_t ly, uint8_t lcdc);

    // render to terminal
    void render();

public:
    PPU(Memory& memory);

    bool getFrameReady();
    void setFrameReady(bool value);

    uint8_t* getFrameBuffer();
    

    // Draw one frame
    // 1 T cycle = 1 PPU cycle
    void step(int tCycles);
};
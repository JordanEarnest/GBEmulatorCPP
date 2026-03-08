#include "ppu.h"
#include <iostream>


PPU::PPU(Memory& mem) : memory(mem), spriteBuffer(10), spriteCount(0) {
    initializeHardware();
}

void PPU::initializeHardware() {
    // Assign default values
    setLCDC(0x91);
    setSCY(0x00);
    setSCX(0x00);
    setLY(0x00);
    setLYC(0x00);
    setBGP(0xFC);
    setSTAT(0x85);
}

void PPU::step(int tCycles) {
    // If LCD display is off
    if (!(getLCDC() & 0x80)) {
        setLY(0);
        setPPUMode(0);
        cycleCounter = 0;
        return;
    }

    cycleCounter += tCycles;

    uint8_t ly = getLY();
    //std::cout << "LY: " << static_cast<int>(ly) << std::endl;
    uint8_t lyc = getLYC();
    uint8_t currentMode = getSTAT() & 0x03;

    // VBlank lines
    if (ly >= 144) {
        if (currentMode != 1) {
            setPPUMode(1);
        }
    }
    // Mode 2 - OAM Scan (fill sprite buffer)
    else if(cycleCounter < 80) {
        // Set STAT register to mode 2
        if (currentMode != 2) {
            setPPUMode(2);

            // Do OAM Scan
            // Go through all 40 possible sprites in OAM memory
            spriteCount = 0;
            for (int i = 0; i < 40; i++) {
                uint16_t addr = 0xFE00 + (i * 4); // 4 byte offset since each sprite has 4 bytes of information

                uint8_t y = memory.read(addr);
                uint8_t x = memory.read(addr + 1);
                uint8_t tile = memory.read(addr + 2);
                uint8_t attr = memory.read(addr + 3);
                
                // Check if current sprite intersects scanline
                if (spriteIntersectsLY(y)) { 
                    spriteBuffer[spriteCount++] = {y, x, tile, attr}; // create sprite struct in the sprite buffer

                    if (spriteCount == 10) // only 10 sprites can be in the buffer at a time
                        break;
                }

            }
        }
    } 
    // Mode 3 - Pixel Transfer (fill current scanline in framebuffer)
    else if (cycleCounter < 80+172) {
        // Set STAT register to mode 3
        if (currentMode != 3) {
            setPPUMode(3);

            uint8_t scx = getSCX();
            uint8_t scy = getSCY();
            uint8_t bgp = getBGP();
            uint8_t lcdc = getLCDC();

            // Compute all 160 pixels for scanline
            for (int x = 0; x < 160; x++) {
                // IF LCDC BIT = 0, BACKGROUND DISABLED
                if (!(lcdc & 0x01)) {
                    frameBuffer[ly][x] = 0x00;
                    continue;
                }
                
                // calculate background pos considering scroll
                uint8_t bgX = (x + scx) & 255; // "& 255" technically not needed since uint8_t auto wraps with overflow, but shows intention
                uint8_t bgY = (ly + scy) & 255;

                uint8_t tileX = bgX / 8;
                uint8_t tileY = bgY / 8;

                uint16_t tileMapBase;
                // which map to use? 0 or 1 in vram? lcdc bit 3 determines this
                if (!(lcdc & 0x08)) 
                    tileMapBase = 0x9800; // start of map 0
                else 
                    tileMapBase = 0x9C00; // start of map 1

                // get memory location of the tile being looked for later vram read
                uint16_t tileIndexAddress = tileMapBase + tileY * 32 + tileX;
                // get what type of tile it is 
                uint8_t tileIndex = memory.read(tileIndexAddress);

                // what pixel are we actually looking for within the tile
                uint8_t tilePixelX = bgX & 7;
                uint8_t tilePixelY = bgY & 7;

                
                uint16_t tileDataAddress;
                // Which partition to use in vram for tile data? signed or unsigned mode?
                // lddc determines whether we use signedIndex or not and changes tileDataAddress
                if (lcdc & 0x10) {
                    tileDataAddress = 0x8000 + tileIndex * 16;
                } else {
                    int8_t signedIndex = (int8_t)tileIndex;
                    tileDataAddress = 0x9000 + signedIndex * 16;
                }

                // Which row are we in within the tile data structure
                uint16_t row = tilePixelY * 2;

                uint8_t lowBits = memory.read(tileDataAddress + row);
                uint8_t highBits = memory.read(tileDataAddress + (row + 1));

                uint8_t bit = 7 - tilePixelX;

                uint8_t lowBit  = (lowBits  >> bit) & 1;
                uint8_t highBit = (highBits >> bit) & 1;

                uint8_t pixelColor = (highBit << 1) | lowBit;


                uint8_t shade = (bgp >> (pixelColor * 2)) & 0x03;

                frameBuffer[ly][x] = shade;
            }

        }   
    } 
    // Mode 0 - HBlank
    else if (cycleCounter < 456) {
        // Set STAT register to mode 0
        if (currentMode != 0) {
            setPPUMode(0);
        }
    }

    // Scanline finished
    if (cycleCounter >= 456) {
        cycleCounter -= 456; // reset back to 0

        ly++;

        uint8_t stat = getSTAT();

        if (ly == lyc) {
            stat |= 0x04;  // set coincidence flag

            if (stat & 0x40) { // LYC interrupt enabled
                requestSTATInterrupt();
            }
        } else {
            stat &= ~0x04; // clear coincidence flag
        }

        setSTAT(stat);
    

        // ly ranges from 0-143 for actual visible display
        if (ly == 144) { 
            setPPUMode(1); // VBlank
            requestVBlankInterrupt();
            setFrameReady(true);
        }

        if (ly > 153) {
            ly = 0;
        }
        // Update LY register into memory
        setLY(ly);
    }
}

bool PPU::spriteIntersectsLY(uint8_t y) {
    uint8_t lcdc = memory.read(0xFF40); // 0xFF40 location of LCDC register
    uint8_t ly = getLY();

    uint8_t spriteHeight = (lcdc & 0x04) ? 16 : 8;

    return (ly >= y - 16) && (ly < y - 16 + spriteHeight);
}

uint8_t* PPU::getFrameBuffer() {
    return &frameBuffer[0][0];
}

void PPU::render() {
    char shade[4] = { ' ', '.', '*', '#' };

    std::cout << "\033[H";   // move cursor to top

    for (int i = 0; i < 144; i++) {
        for (int j = 0; j < 160; j++) {
            std::cout << shade[frameBuffer[i][j]];
        }
        std::cout << '\n';
    }

    std::cout.flush();
}

void PPU::setFrameReady(bool value) {
    frameReady = value;
}

bool PPU::getFrameReady() {
    return frameReady;
}

void PPU::requestVBlankInterrupt() {
    // Set IF register or Interrupt Flag
    // Note: Bit 0 == VBlank
    // Note: Bit 1 == LCD STAT
    // Note: Bit 2 == Timer
    // Note: Bit 3 == Serial
    // Note: Bit 4 == Joypad
    uint8_t IF = memory.read(0xFF0F); // IF register is located at 0xFF0F
    IF |= 0x01; // VBlank Interrupt
    memory.write(0xFF0F, IF);
}

void PPU::requestSTATInterrupt() {
    uint8_t IF = memory.read(0xFF0F); // IF register is located at 0xFF0F
    IF |= 0x02; // LCD STAT Interrupt
    memory.write(0xFF0F, IF);
}

// Getters for PPU registers
uint8_t PPU::getLY() {
    return memory.read(0xFF44);
}
uint8_t PPU::getLYC() {
    return memory.read(0xFF45);
}
uint8_t PPU::getSTAT() {
    return memory.read(0xFF41);
} 
uint8_t PPU::getLCDC() {
    return memory.read(0xFF40);
} 
uint8_t PPU::getSCY() {
    return memory.read(0xFF42);
}
uint8_t PPU::getSCX() {
    return memory.read(0xFF43);
}
uint8_t PPU::getBGP() {
    return memory.read(0xFF47);
}

// Setters for PPU registers
void PPU::setLY(uint8_t value) {
    return memory.write(0xFF44, value);
}
void PPU::setLYC(uint8_t value) {
    return memory.write(0xFF45, value);
}
void PPU::setSTAT(uint8_t value) {
    return memory.write(0xFF41, value);
} 
void PPU::setLCDC(uint8_t value) {
    return memory.write(0xFF40, value);
} 
void PPU::setSCY(uint8_t value) {
    memory.write(0xFF42,value);
}
void PPU::setSCX(uint8_t value) {
    memory.write(0xFF43,value);
}
void PPU::setBGP(uint8_t value) {
    memory.write(0xFF47, value);
}


void PPU::setPPUMode(int value) {
    if (value < 0 || value > 3)
        return;

    uint8_t stat = getSTAT();
    uint8_t currentMode = stat & 0x03;

    if (currentMode == value)
        return;

    if (value == 0 && (stat & 0x08)) {
        requestSTATInterrupt();
    }
    else if (value == 1 && (stat & 0x10)) {
        requestSTATInterrupt();
    }
    else if (value == 2 && (stat & 0x20)) {
        requestSTATInterrupt();
    }

    stat = (stat & 0xFC) | value;
    setSTAT(stat);
}
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
    setWX(0x00);
    setWY(0x00);
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
            uint8_t wx = getWX();
            uint8_t wy = getWY();

            bool windowEnabled = lcdc & 0x20;
            int windowStartX = (int)wx - 7;

            // Compute all 160 pixels for scanline
            for (int x = 0; x < 160; x++) {
                // IF LCDC BIT = 0, BACKGROUND DISABLED
                if (!(lcdc & 0x01)) {
                    frameBuffer[ly][x] = 0x00;
                    continue;
                }
                
                bool usingWindow = windowEnabled && ly >= wy && x >= windowStartX;

                // Background and Window -- HIGH PRIORITY
                if (!usingWindow)
                    fillScanLineWithBackground(x, scx, scy, ly, lcdc, bgp);
                else
                    fillScanLineWithWindow(x,  wx, wy, ly, lcdc, bgp);
            }
            // Sprites -- MEDIUM PRIORITY
            fillScanLineWithSpritesFromBuffer(ly, lcdc);

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

void PPU::fillScanLineWithBackground(uint8_t x, uint8_t scx, uint8_t scy, uint8_t ly, uint8_t lcdc, uint8_t bgp) {
    // calculate background pos considering scroll
    uint8_t bgX = (x + scx) & 255; // "& 255" technically not needed since uint8_t auto wraps with overflow, but shows intention
    uint8_t bgY = (ly + scy) & 255;

    uint8_t tileX = bgX / 8;
    uint8_t tileY = bgY / 8;

    uint16_t tileMapBase;
    // which map to use? 0 or 1 in vram? lcdc bit 3 (background) determines this and lcdc bit 6 (window) determines this
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

void PPU::fillScanLineWithWindow(uint8_t x, uint8_t wx, uint8_t wy, uint8_t ly, uint8_t lcdc, uint8_t bgp) {
    // calculate window pos 
    int winX = x - (wx-7); // "& 255" technically not needed since uint8_t auto wraps with overflow, but shows intention
    int winY = ly - wy;

    uint8_t tileX = winX / 8;
    uint8_t tileY = winY / 8;

    uint16_t tileMapBase;
    // which map to use? 0 or 1 in vram? lcdc bit 3 (background) determines this and lcdc bit 6 (window) determines this
    if (lcdc & 0x40) 
        tileMapBase = 0x9C00;
    else 
        tileMapBase = 0x9800;

    // get memory location of the tile being looked for later vram read
    uint16_t tileIndexAddress = tileMapBase + tileY * 32 + tileX;
    // get what type of tile it is 
    uint8_t tileIndex = memory.read(tileIndexAddress);

    // what pixel are we actually looking for within the tile
    uint8_t tilePixelX = winX & 7;
    uint8_t tilePixelY = winY & 7;

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

void PPU::fillScanLineWithSpritesFromBuffer(uint8_t ly, uint8_t lcdc) {
    // Sprite only renders if this is set in lcdc
    if (!(lcdc & 0x02))
        return;

    for (int i = spriteCount - 1; i >=0; i--) {
        Sprite& sprite = spriteBuffer[i];

        int spriteHeight = (lcdc & 0x04) ? 16 : 8;

        int x = sprite.x - 8;
        int y = sprite.y - 16;

        int row = ly - y;

        if (row < 0 || row >= spriteHeight)
            continue;

        if (sprite.attr & 0x40)
            row = spriteHeight - 1 - row;

        uint8_t tileIndex = sprite.tile;
        uint16_t tileAddress;

        if (spriteHeight == 16) {
            tileIndex &= 0xFE;
            tileAddress = 0x8000 + (tileIndex + (row / 8)) * 16;
            row %= 8;
        } else {
            tileAddress = 0x8000 + tileIndex * 16;
        }

        uint16_t rowAddress = tileAddress + row * 2;

        uint8_t lowBits = memory.read(rowAddress);
        uint8_t highBits = memory.read(rowAddress + 1);

        for (int px = 0; px < 8; px++) {
            int screenX = x + px;
            
            // If out of screen, don't bother rendering
            if (screenX < 0 || screenX >= 160) {
                continue;
            }

            // Handles X flip
            uint8_t bit = (sprite.attr & 0x20) ? px : (7 - px);

            uint8_t lowBit  = (lowBits >> bit) & 1;
            uint8_t highBit = (highBits >> bit) & 1;
            uint8_t color = (highBit << 1) | lowBit;
 
            // don't draw transparent pixels
            if (color == 0)
                continue;

            uint8_t palette = (sprite.attr & 0x10) ? getOBP1() : getOBP0();
            uint8_t shade = (palette >> (color * 2)) & 3;

            bool priority = sprite.attr & 0x80;

            // Handle sprite/background priority
            // bit7 -> BG Priority, if set sprite appears behind BG colors 1-3
            if (priority && frameBuffer[ly][screenX] != 0)
                continue;

            frameBuffer[ly][screenX] = shade;
        }
    }
}

bool PPU::spriteIntersectsLY(uint8_t y) {
    uint8_t lcdc = memory.read(0xFF40);
    uint8_t ly = getLY();

    int spriteHeight = (lcdc & 0x04) ? 16 : 8;

    int spriteY = (int)y - 16;

    return (ly >= spriteY) && (ly < spriteY + spriteHeight);
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
uint8_t PPU::getWX() {
    return memory.read(0xFF4B);
}
uint8_t PPU::getWY() {
    return memory.read(0xFF4A);
}
uint8_t PPU::getBGP() {
    return memory.read(0xFF47);
}
uint8_t PPU::getOBP0() {
    return memory.read(0xFF48);
}
uint8_t PPU::getOBP1() {
    return memory.read(0xFF49);
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
void PPU::setWX(uint8_t value) {
    memory.write(0xFF4B,value);
}
void PPU::setWY(uint8_t value) {
    memory.write(0xFF4A,value);
}
void PPU::setBGP(uint8_t value) {
    memory.write(0xFF47, value);
}
void PPU::setOBP0(uint8_t value) {
    memory.write(0xFF48,value);
}
void PPU::setOBP1(uint8_t value) {
    memory.write(0xFF49, value);
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
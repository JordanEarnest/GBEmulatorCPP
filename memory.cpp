#include "memory.h"
#include <iostream>

bool Memory::loadCartridge(const std::string& path) {
    return cartridge.loadROM(path);
}

uint8_t Memory::read(uint16_t address) {
    // ROM
    if (address >= 0x0000 && address <= 0x7FFF) {
        return cartridge.read(address);
    }
    // External RAM (a part of cartridge)
    else if (address >= 0xA000 && address <= 0xBFFF) {
        return cartridge.read(address);
    }
    // VRAM
    else if (address >= 0x8000 && address <= 0x9FFF) {
        return vram[address - 0x8000];
    }
    // WRAM
    else if (address >= 0xC000 && address <= 0xDFFF) {
        return wram[address - 0xC000];
    }
    // WRAM echo
    else if (address >= 0xE000 && address <= 0xFDFF) {
        return wram[address - 0x2000];
    }
    // OAM
    else if (address >= 0xFE00 && address <= 0xFE9F) {
        return oam[address - 0xFE00];
    }
    // Unused portion of memory
    else if (address >= 0xFEA0 && address <= 0xFEFF) {
        return 0xFF; // safe default value
    }
    // I/O
    else if (address >= 0xFF00 && address <= 0xFF7F) {
        return io[address - 0xFF00];
    }
    // HRAM
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        return hram[address - 0xFF80];
    }
    // Interrupt Enable Register
    else if (address == 0xFFFF) {
        return ie;
    // Outside memory allocation (segfault)
    } else {
        return 0xFF; // error default value
    }
}

void Memory::write(uint16_t address, uint8_t value) {
    // Cartridge Data (ROM)
    if (address >= 0x0000 && address <= 0x7FFF) {
        return cartridge.write(address, value);
    }
    // External RAM (a part of cartridge)
    else if (address >= 0xA000 && address <= 0xBFFF) {
        return cartridge.write(address, value);
    }
    // VRAM
    else if (address >= 0x8000 && address <= 0x9FFF) {

        vram[address - 0x8000] = value;
    }
    // WRAM
    else if (address >= 0xC000 && address <= 0xDFFF) {
        wram[address - 0xC000] = value;
    }
    // WRAM echo
    else if (address >= 0xE000 && address <= 0xFDFF) {
        wram[address - 0x2000] = value;
    }
    // OAM
    else if (address >= 0xFE00 && address <= 0xFE9F) {
        oam[address - 0xFE00] = value;
    }
    // Unused portion of memory
    else if (address >= 0xFEA0 && address <= 0xFEFF) {
        return; 
    }
    // I/O
    else if (address >= 0xFF00 && address <= 0xFF7F) {
        io[address - 0xFF00] = value;
    }
    // HRAM
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        hram[address - 0xFF80] = value;
    }
    // Interrupt Enable Register
    else if (address == 0xFFFF) {
        ie = value;
    // Outside memory allocation (segfault)
    } else {
        return;
    }
}

Memory::Memory() {
    // Initialize size of memory allocations
    vram.resize(0x2000);
    wram.resize(0x2000);
    oam.resize(0xA0);
    io.resize(0x80);
    hram.resize(0x7F);

    // Default Interrupt Enable Register to 0x00
    ie = 0x00;
}
/*
    Description: Interface between ROM and CPU
    Author: Jordan Earnest
*/

#pragma once

#include "cartridge.h"
#include <stdint.h>

class Memory {
private:
    // ROM Data and External RAM
    Cartridge cartridge;         // 0x0000-0x7FFF  Total Bytes = 0x8000 (32KB)
                                 // 0xA000-0xBFFF  Total Bytes = 0x2000 (8KB)

    // Video RAM 
    std::vector<uint8_t> vram;   // 0x8000–0x9FFF  Total Bytes = 0x2000 (8KB)
    // Work RAM (not echo)
    std::vector<uint8_t> wram;   // 0xC000–0xDFFF  Total Bytes = 0x2000 (8KB)
    // Sprite Attribute Table
    std::vector<uint8_t> oam;    // 0xFE00–0xFE9F  Total Bytes = 0xA0   (160B)
    // I/O Memory
    std::vector<uint8_t> io;     // 0xFF00-0xFF7F  Total Bytes = 0x80   (128B)
    // High RAM
    std::vector<uint8_t> hram;   // 0xFF80–0xFFFE  Total Bytes = 0x7F   (127B)
    // Interrupt Enable Register
    uint8_t ie;                  // 0xFFFF         Total Bytes = 0x01   (1B)

public:
    // Load ROM data into cartridge
    bool loadCartridge(const std::string& path);

    // Read address from memory given 16bit addressable
    uint8_t read(uint16_t address);

    // Write a byte to memory at a 16bit addressable
    void write(uint16_t address, uint8_t value);

    Memory();
};
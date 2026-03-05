#include "cartridge.h"
#include <fstream>
#include <iostream>

bool Cartridge::loadROM(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    std::streampos pos = file.tellg();
    if (pos == std::streampos(-1))
        return false;

    size_t size = static_cast<size_t>(pos);
    if (size < 0x150)
        return false;

    file.clear(); 
    file.seekg(0, std::ios::beg);

    rom_size = size;
    rom.resize(rom_size);

    if (!file.read(reinterpret_cast<char*>(rom.data()), size))
        return false;

    setBankSizes();
    allocateRAM();
    initializeHardware();
    print();

    return true;
}

void Cartridge::initializeHardware() {
    uint8_t header_value = read(TYPE_HEADER_ADDR);

    rom_bank = 1;
    ram_bank = 0;

    ram_enabled = false;

    rtc_latched = false;
    rtc_selected = false;

    last_latch_write = 0x00;

    rtc_register_selected = 0x08;

    // live rtc
    rtc_seconds = 0x00;
    rtc_minutes = 0x00;
    rtc_hours = 0x00;
    rtc_day_lo = 0x00;
    rtc_day_hi = 0x00;

    // latched rtc
    latched_rtc_seconds = 0x00;
    latched_rtc_minutes = 0x00;
    latched_rtc_hours = 0x00;
    latched_rtc_day_lo = 0x00;
    latched_rtc_day_hi = 0x00;


    has_RTC = false;
    has_battery = false;
    has_ram = false;
    
    switch (header_value) {
        case 0x13:
            mbc = MBCType::MBC3;
            has_ram = true;
            has_battery = true;
            break;
    }
}

void Cartridge::allocateRAM() {
    ram_size = getRamSize();
    ram.resize(ram_size);
}

void Cartridge::setBankSizes() {
    rom_bank_count = getRomBankCount();
    ram_bank_count = getRamBankCount();
}

void Cartridge::print() {
    // Print all information about the cartridge 
    std::cout << "===================== Cartridge Info =====================" << std::endl;
    std::cout << "CART TYPE            : " << toString(mbc) << std::endl;
    std::cout << "HAS RAM              : " << has_ram << std::endl;
    std::cout << "HAS RTC              : " << has_RTC << std::endl;
    std::cout << "HAS BAT              : " << has_battery << std::endl;
    std::cout << std::endl;
    std::cout << "ROM SIZE             : " << rom_size << " B" << std::endl;
    std::cout << "ROM BANKS            : " << rom_bank_count << " (16 KB each)" << std::endl;
    std::cout << std::endl;
    std::cout << "RAM SIZE             : " << ram_size << " B" << std::endl;
    std::cout << "RAM BANKS            : " << ram_bank_count << " (8 KB each)" << std::endl;
    std::cout << "BYTE AT 0x1000       : " << std::hex << (int)read(0x1000) << std::endl;

}

const char* Cartridge::toString(MBCType type) {
    switch (type) {
        case MBCType::ROM_ONLY: return "ROM ONLY";
        case MBCType::MBC1:     return "MBC1";
        case MBCType::MBC2:     return "MBC2";
        case MBCType::MBC3:     return "MBC3";
        case MBCType::MBC5:     return "MBC5";
        default:                return "UNKNOWN";
    }
}

uint8_t Cartridge::read(uint16_t address) {
    // ROM Bank 0
    if (address >= 0x0000 && address <= 0x3FFF) {
        return rom[address];
    }
    // ROM Switchable Bank
    else if (address >= 0x4000 && address <= 0x7FFF) {
        return rom[(rom_bank * 0x4000) + (address - 0x4000)]; // 0x4000 = 16 KB
    }
    // External RAM
    else if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled || !has_ram)
            return 0xFF; // default output for non-access

        return ram[(ram_bank * 0x2000) + (address - 0xA000)]; // 0x2000 = 8KB
    } else {
        return 0xFF;
    }
}

void Cartridge::write(uint16_t address, uint8_t value) {
    // MBC Logic
    // ROM Bank 0, WE CAN NEVER MODIFY THE ROM, THESE ARE COMMANDS
    if (address >= 0x0000 && address <= 0x1FFF) {
        // 0x0A: enable RAM/RTC command
        if ((value & 0x0F) == 0x0A) {
            ram_enabled = true;
        } else {
            ram_enabled = false;
        }
    }
    // ROM Bank 0
    else if (address >= 0x2000 && address <= 0x3FFF) {
        // 0x00-0x80: ROM bank select (1-127)
        value &= 0x7F;
        rom_bank = (value == 0) ? 1 : value;
    }
    // ROM Switchable Bank
    else if (address >= 0x4000 && address <= 0x5FFF) {
        // 0x00-0x03: RAM bank select
        if (value >= 0x00 && value <= 0x03) {
            ram_bank = value;
            rtc_selected = false;
        // 0x08-0x0C: RTC register select
        } else if (value >= 0x08 && value <= 0x0C) {
            rtc_register_selected = value;
            rtc_selected = true;
        }
    }
    // ROM Switchable Bank
    else if (address >= 0x6000 && address <= 0x7FFF) {
        if (last_latch_write == 0x00 && value == 0x01) {
            // latch RTC, copy live rtc -> latched rtc
            latched_rtc_seconds = rtc_seconds;
            latched_rtc_minutes = rtc_minutes;
            latched_rtc_hours   = rtc_hours;
            latched_rtc_day_lo  = rtc_day_lo;
            latched_rtc_day_hi  = rtc_day_hi;
        }
            
        last_latch_write = value;
    }
    // =========================
    // External RAM Data Writes
    else if (address >= 0xA000 && address <= 0xBFFF) {
        if (!ram_enabled) 
            return;

        if (rtc_selected) {
            // writes to RTC register
            switch (rtc_register_selected) {
                case 0x08: rtc_seconds = value % 60; break;
                case 0x09: rtc_minutes = value % 60; break;
                case 0x0A: rtc_hours   = value % 24; break;
                case 0x0B: rtc_day_lo  = value; break;
                case 0x0C: rtc_day_hi  = value & 0xC1; break;
            }
        } else {
            // writes to external RAM
            ram[(ram_bank * 0x2000) + (address - 0xA000)] = value;
        }
    }

    
}

size_t Cartridge::getRamSize() {
    uint8_t header_value = read(RAM_HEADER_ADDR);

    switch (header_value) {
        case 0x00: 
            return 0;        // No RAM
        case 0x01: 
            return 0x800;    // 2 KB (rare)
        case 0x02: 
            return 0x2000;   // 8 KB
        case 0x03: 
            return 0x8000;   // 32 KB
        case 0x04: 
            return 0x20000;  // 128 KB
        case 0x05: 
            return 0x10000;  // 64 KB
        default:   
            return 0;
    }
}

size_t Cartridge::getRomBankCount() {
    uint8_t header_value = read(ROM_HEADER_ADDR);

    // Standard sizes
    if (header_value <= 0x7) {
        return 2u << header_value;
    }

    // Special non-power-of-two sizes
    switch (header_value) {
        case 0x52: 
            return 72;  // 1.1 MB
        case 0x53: 
            return 80;  // 1.2 MB
        case 0x54: 
            return 96;  // 1.5 MB
        default:
            return rom.size() / 0x4000; // fallback
    }
}

size_t Cartridge::getRamBankCount() {
    uint8_t header_value = read(RAM_HEADER_ADDR);

    switch (header_value) {
        case 0x00: 
            return 0;   // No RAM
        case 0x01: 
            return 1;   // 2 KB (special, not banked)
        case 0x02: 
            return 1;   // 8 KB
        case 0x03: 
            return 4;   // 32 KB
        case 0x04: 
            return 16;  // 128 KB
        case 0x05: 
            return 8;   // 64 KB
        default:
            return 0;
    }
}
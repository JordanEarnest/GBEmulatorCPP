/*
    Description: Stores and manages ROM data from the emulated cartridge
    Author: Jordan Earnest
*/
#pragma once

#include <stdint.h>
#include <vector>
#include <string>

#define TYPE_HEADER_ADDR 0x0147 // tells what MBC chip is used 
#define ROM_HEADER_ADDR  0x0148 // tells size of rom from cartridge
#define RAM_HEADER_ADDR  0x0149 // tells size of ram from cartridge

enum class MBCType {
    NONE,
    ROM_ONLY,
    MBC1,
    MBC2,
    MBC3,
    MBC5
};

class Cartridge {
private:
    // Dynamic array of single bytes
    std::vector<uint8_t> rom;
    // Changeable portion of memory of cartridge, can hold the save data (external memory)
    std::vector<uint8_t> ram;
    // Number of banks for ram, 8KB = 1 Bank
    uint16_t ram_bank_count;
    // Number of banks for rom, 16KB = 1 Bank
    uint16_t rom_bank_count;
    // Size of ram in bytes
    size_t ram_size;
    // Size of rom in bytes
    size_t rom_size;

    // Type of cartridge
    MBCType mbc;

    // Which rom bank are we using?
    uint8_t rom_bank;
    // Which ram bank are we using?
    uint8_t ram_bank;

    // RAM Switch
    bool ram_enabled;
    
    uint8_t rtc_register_selected;

    bool rtc_latched;
    bool rtc_selected;

    uint8_t last_latch_write;

    uint8_t rtc_seconds;
    uint8_t rtc_minutes;
    uint8_t rtc_hours;
    uint8_t rtc_day_lo;
    uint8_t rtc_day_hi;

    // latched RTC
    uint8_t latched_rtc_seconds, latched_rtc_minutes, latched_rtc_hours;
    uint8_t latched_rtc_day_lo, latched_rtc_day_hi;

    // Is there actual external ram physically in the cartridge?
    bool has_ram;
    // Does the cartridge have a battery?
    bool has_battery;
    // Does the cartridge have a RTC?
    bool has_RTC;

public:
    // Load the data from the .gb file into rom vector array
    bool loadROM(const std::string& path);

    // Allocate cartridge RAM from RAM Header information
    void allocateRAM();

    // Sets ram_bank_count and rom_bank_count to correct values using headers in ROM
    void setBankSizes();

    // Configures and handles the cartridge type from 0x00 - 0x1B, assigns MBC chip, ram_enabled etc
    void initializeHardware();

    // Print rom data for debug purposes
    void print();

    // Get byte data from ROM using PC (16bit addressable)
    uint8_t read(uint16_t address);

    // Write to ROM or RAM -> controlling banks (MCBs)
    void write(uint16_t address, uint8_t value);

private:
    // Look at ROM header at address 0x0149 to determine the RAM size of cartridge
    size_t getRamSize();
    // Get number of rom banks from 0x0148
    size_t getRomBankCount();
    // Get number of ram banks from 0x0149
    size_t getRamBankCount();

    // Converts enum to string to print for debugging
    const char* toString(MBCType type);
};
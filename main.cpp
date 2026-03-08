#include "cartridge.h"
#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include <iostream>

int main() {
    Memory memory;

    if (!memory.loadCartridge("pokemon.gb")) {
        std::cerr << "Failed to load cartridge" << std::endl;
        return -1;
    }

    CPU cpu(memory);
    PPU ppu(memory);

    cpu.step();

    return 0;
}
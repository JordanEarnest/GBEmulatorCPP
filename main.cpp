#include "cartridge.h"
#include "memory.h"
#include "cpu.h"
#include <iostream>

int main() {
    Memory memory;

    if (!memory.loadCartridge("pokemon.gb")) {
        std::cerr << "Failed to load cartridge" << std::endl;
        return -1;
    }

    CPU cpu(memory);

    for (int i = 0; i < 10; i++)
         cpu.step();

    return 0;
}
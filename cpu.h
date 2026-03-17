/*
    Description: Emulates the "Sharp SM83 CPU" within the SoC of the original Game Boy
    Author: Jordan Earnest
*/
#pragma once
#include <stdint.h>
#include <iostream>
#include <iomanip>
#include "memory.h"

// Registers for the Sharp SM83 CPU core
struct Registers {
    // A = Accumulator, F = Flags
    uint8_t A, F;
    // BC, DE, HL = General-purpose register pairs
    uint8_t B, C;
    uint8_t D, E;
    uint8_t H, L;
    // PC = Program Counter, SP = Stack Pointer
    uint16_t PC, SP;
};

class CPU {
private:
    Memory& memory;

    // Determines if CPU is in a STOPPED state (low power mode for saving)
    bool haulted;
    // Interrupt Master Enable Flag
    bool ime;
    
public:
    // Defines the CPU cycles per second
    // "constexpr" ensures value is known at compile time
    static constexpr uint32_t CLOCK_RATE = 4'194'304;

    Registers regs;

    CPU(Memory& memory);

    // Run a singular instruction on CPU and return number of cycles
    int step();
    // Fetch opcode from ROM
    uint8_t fetch();
    // Execute unique unprefixed opcodes, returns number of cycles executing opcode requires
    int execute(uint8_t opcode);
    // Execute unique unprefixed opcodes, returns number of cycles executing opcode requires
    int executePrefixed(uint8_t opcode);

    int handleInterrupts();

private:
    // Helper methods to get 16 bit register pairs
    uint16_t unsigned_16(uint8_t r1, uint8_t r2);
    uint16_t regAF();
    uint16_t regBC();
    uint16_t regDE();
    uint16_t regHL();
};


#include "cpu.h"

CPU::CPU(Memory& mem) : memory(mem) {   
    // Initialize default values for registers
    regs.A = 0x01;
    regs.F = 0xB0;

    regs.B = 0x00;
    regs.C = 0x13;

    regs.D = 0x00;
    regs.E = 0xD8;

    regs.H = 0x01;
    regs.L = 0x4D;

    regs.IE = 0x00;

    // SP starts at 0xFFFE
    regs.SP = 0xFFFE;
    // PC starts at 0x0100 for actual game code, everything before is boot ROM data
    regs.PC = 0x0100;

    // Determines if CPU is in a STOPPED state (low power mode for saving)
    stopped = false;
};

int CPU::step() {
    // Read opcode from memory and increment PC to next byte
    uint8_t opcode = fetch();

    // Run opcode and return number of cycles taken
    int cycles = execute(opcode);

    printf("%02X\n", opcode);

    return cycles;
}

uint8_t CPU::fetch() {
    return memory.read(regs.PC++);
}

int CPU::execute(uint8_t opcode) {
    /*
        Flags: stored in bits 7, 6, 5, 4 in reg F respectively
        Z = result == 0
        N = SUBSTRACT == 1
        H = lower nibble overflow
        C = carry
    */
    switch (opcode) {
        // NOP
        case 0x00:
            return 4; // # of cycles
        // LD BC, n16
        case 0x01: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            regs.B = msb;
            regs.C = lsb;
            return 12;
        }
        // LD [BC], A
        case 0x02: {
            memory.write(regBC(), regs.A);
            return 8;
        }
        // INC BC
        case 0x03: {
            uint16_t BC = regBC();
            BC++;
            regs.B = BC >> 8; // msb
            regs.C = BC & 0xFF; // lsb
            return 8;
        }
        // INC B
        case 0x04: {
            uint8_t result = regs.B + 1;

            // Z flag
            if (result == 0) 
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag
            if ((regs.B & 0xFF) + 1 > 0x0F)
                regs.F |= 0x20; 
            else
                regs.F &= ~0x20;

            regs.B = result;
            
            return 4;
        }
        // DEC B
        case 0x05: {
            uint8_t result = regs.B - 1;

            // Z flag
            if (result == 0) 
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.B & 0xFF) == 0)
                regs.F |= 0x20; 
            else
                regs.F &= ~0x20;

            regs.B = result;
            
            return 4;
        }
        // LD, B, n
        case 0x06: {
            regs.B = memory.read(regs.PC++); // get next byte which is immediate value n
            return 8; // # of cycles
        }
        // RLCA 
        case 0x07: {
            uint8_t carry = regs.A & 0x80;
            uint8_t shiftL = regs.A << 1;

            // add carry to most significant bit (wrap-around)
            uint8_t result = shiftL | (carry >> 7);

            // Flag Z set 0
            regs.F &= ~0x80;

            // Flag N set 0
            regs.F &= ~0x40;

            // Flag H set 0
            regs.F &= ~0x20;

            // Flag C
            if (carry == 0x80)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // LD [a16], SP
        case 0x08: {
            uint16_t nn_lsb = memory.read(regs.PC++);
            uint16_t nn_msb = memory.read(regs.PC++);
            uint16_t nn = unsigned_16(nn_msb, nn_lsb);
            
            memory.write(nn++, (regs.SP & 0xFF)); // get SP lsb bits
            memory.write(nn, (regs.SP >> 8)); // get SP msb bits

            return 20; // # of cycles
        }
        // ADD HL, BC
        case 0x09: {
            uint32_t result = regHL() + regBC();

            // N flag set to 0
            regs.F &= ~0x40;


            // H flag 1 carry from bit 11 to 12
            if ((regHL() & 0x0FFF) + (regBC() & 0x0FFF) > 0x0FFF)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag carry from bit 15
            if (result > 0xFFFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            // write result back to H
            regs.H = (result >> 8) & 0xFF;
            regs.L = (result & 0xFF);

            return 8;
        }
        // LD A, [BC]
        case 0x0A: {
            regs.A = memory.read(regBC());
            return 8;
        }
        // DEC BC
        case 0x0B: {
            uint16_t result = regBC() - 1;

            regs.B = result >> 8;
            regs.C = result & 0xFF;
            
            return 8;
        }
        // INC C
        case 0x0C: {
            uint8_t result = regs.C + 1;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag, carry from bit 3 to bit 4
            if ((regs.C & 0x0F) + 1 > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag, none

            regs.C = result;

            return 4;
        }
        // DEC C
        case 0x0D: {
            uint8_t result = regs.C - 1;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag, carry from bit 3 to bit 4 and vice versa
            if ((regs.C & 0x0F) == 0)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag, none

            regs.C = result;

            return 4;
        }
        // LD C, n8
        case 0x0E: {
            uint8_t n8 = memory.read(regs.PC++);
            regs.C = n8;
            return 8;
        }
        // RRCA
        case 0x0F: {
            uint8_t carry = regs.A & 0x01;
            uint8_t shiftR = regs.A >> 1;

            // add carry to most significant bit (wrap-around)
            uint8_t result = shiftR | (carry << 7);

            // Flag Z set 0
            regs.F &= ~0x80;

            // Flag N set 0
            regs.F &= ~0x40;

            // Flag H set 0
            regs.F &= ~0x20;

            // Flag C
            if (carry == 0x01)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // STOP n8
        case 0x10: {
            stopped = true;
            regs.PC++; // skip padding byte n8, usually 0x00
            return 4;
        }
        // LD DE, n16
        case 0x11: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            regs.D = msb;
            regs.E = lsb;
            return 12;
        }
        // LD [DE], A
        case 0x12: {
            memory.write(regDE(), regs.A);
            return 8;
        }
        // INC DE
        case 0x13: {
            uint16_t DE = regDE();
            DE++;
            regs.D = DE >> 8; // msb
            regs.E = DE & 0xFF; // lsb
            return 8;
        }
        // INC D
        case 0x14: {
            uint8_t result = regs.D + 1;

            // Z flag
            if (result == 0) 
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag
            if ((regs.D & 0xFF) + 1 > 0x0F)
                regs.F |= 0x20; 
            else
                regs.F &= ~0x20;

            regs.D = result;
            
            return 4;
        }
        // DEC D
        case 0x15: {
            uint8_t result = regs.D - 1;

            // Z flag
            if (result == 0) 
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.D & 0xFF) == 0)
                regs.F |= 0x20; 
            else
                regs.F &= ~0x20;

            regs.D = result;
            
            return 4;
        }
        // LD D, n8
        case 0x16: {
            regs.D = memory.read(regs.PC++); // get next byte which is immediate value n
            return 8; // # of cycles
        }
        // RLA
        case 0x17: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t newCarry = (regs.A & 0x80) >> 7;
            uint8_t shiftL = regs.A << 1;

            // add carry to most significant bit (wrap-around)
            uint8_t result = shiftL | oldCarry;

            // Flag Z set 0
            regs.F &= ~0x80;

            // Flag N set 0
            regs.F &= ~0x40;

            // Flag H set 0
            regs.F &= ~0x20;

            // Flag C
            if (newCarry)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }

        // Load Register, example: LD, B, C
        case 0x41:
            regs.B = regs.C;
            return 4; // # of cycles



        // Load Register (indirect HL), example: LD, B, (HL)
        // Load to the 8-bit register B, data from the absolute address specified by the 16-bit register HL
        case 0x46:
            regs.B = memory.read(regHL());
            return 8; // # of cycles

        // Load From Register (indirect HL), example: LD, (HL), B
        // Load to the absolute address specified by the 16-bit register HL, data from the 8-bit register r
        case 0x70:
            memory.write(regHL(), regs.B);
            return 8; // # of cycles

        // Load From Immediate Data (indirect HL)
        // Load to the absolute address specified by the 16-bit register HL, the immediate data n
        case 0x36: {
            uint8_t n = memory.read(regs.PC++);
            memory.write(regHL(), n);
            return 12; // # of cycles
        }

        // Load Accumulator (indirect DE), example: LD A, (DE)
        // Load to the 8-bit A register, data from the absolute address specified by the 16-bit register DE
        case 0x1A:
            regs.A = memory.read(regDE());
            return 8; // # of cycles


        // Load Accumulator (direct), example: LD A, (nn)
        // Load to the 8-bit A register, data from the absolute address specified by the 16-bit operand nn
        // Length 3 bytes: opcode + LSB(nn) + MSB(nn)
        case 0xFA: {
             uint8_t nn_lsb = memory.read(regs.PC++);
             uint8_t nn_msb = memory.read(regs.PC++);
             uint16_t nn = unsigned_16(nn_msb, nn_lsb);
             regs.A = memory.read(nn);
            return 16; // # of cycles
        }

        // Load From Accumulator (direct), example: LD (nn), A
        // Load to the absolute address specified by the 16-bit operand nn, data from the 8-bit A register
        // Length 3 bytes: opcode + LSB(nn) + MSB(nn) 
        case 0xEA: {
             uint8_t nn_lsb = memory.read(regs.PC++);
             uint8_t nn_msb = memory.read(regs.PC++);
             uint16_t nn = unsigned_16(nn_msb, nn_lsb);
             memory.write(nn, regs.A);
            return 16; // # of cycles
        }

        // Load Accumulator (indirect 0xFF00+C), example: LDH A, (C)
        // Load to the 8-bit A register, data from the address specified by the 8-bit C register. The full
        // 16-bit absolute address is obtained by setting the most significant byte to 0xFF and the least
        // significant byte to the value of C, so the possible range is 0xFF00-0xFFFF
        case 0xF2:
             regs.A = memory.read(unsigned_16(0xFF, regs.C));
            return 8; // # of cycles

        // Load From Accumulator (indirect 0xFF00+C), example: LDH (C), A
        // Load to the address specified by the 8-bit C register, data from the 8-bit A register. The full
        // 16-bit absolute address is obtained by setting the most significant byte to 0xFF and the least
        // significant byte to the value of C, so the possible range is 0xFF00-0xFFFF
        case 0xE2:
             memory.write(unsigned_16(0xFF, regs.C), regs.A);
            return 8; // # of cycles

        // Load Accumulator (indirect 0xFF00+n), example: LDH A, (n)
        // Load to the 8-bit A register, data from the address specified by the 8-bit immediate data n. The
        // full 16-bit absolute address is obtained by setting the most significant byte to 0xFF and the
        // least significant byte to the value of n, so the possible range is 0xFF00-0xFFFF
        // Length 2 bytes: opcode + n
        case 0xF0: {
            uint8_t n = memory.read(regs.PC++); 
            regs.A = memory.read(unsigned_16(0xFF, n));
            return 12; // # of cycles
        }

        // Load From Accumulator (indirect 0xFF00+n), example: LDH (n), A
        // Load to the address specified by the 8-bit immediate data n, data from the 8-bit A register. The
        // full 16-bit absolute address is obtained by setting the most significant byte to 0xFF and the
        // least significant byte to the value of n, so the possible range is 0xFF00-0xFFFF
        // Length 2 bytes: opcode + n
        case 0xE0: {
            uint8_t n = memory.read(regs.PC++); 
            memory.write(unsigned_16(0xFF, n), regs.A);
            return 12; // # of cycles
        }

        // Load Accumulator (indirect HL, decrement), example: LD A, (HL-)
        // Load to the 8-bit A register, data from the absolute address specified by the 16-bit register HL.
        // The value of HL is decremented after the memory read
        case 0x3A: {
            uint16_t HL = regHL();
            regs.A = memory.read(HL); 
            // decrement HL
            HL--;
            regs.H = (uint8_t)(HL >> 8); // drops low byte
            regs.L = (uint8_t)(HL & 0xFF); // drops high byte
            return 8; // # of cycles
        }

        // Load From Accumulator (indirect HL, decrement), example: LD (HL-), A
        // Load to the absolute address specified by the 16-bit register HL, data from the 8-bit A register.
        // The value of HL is decremented after the memory write.
        case 0x32: {
            uint16_t HL = regHL();
            memory.write(HL, regs.A);
            // decrement HL
            HL--;
            regs.H = (uint8_t)(HL >> 8); // drops low byte
            regs.L = (uint8_t)(HL & 0xFF); // drops high byte
            return 8; // # of cycles
        }

        // Load Accumulator (indirect HL, increment), example: LD A, (HL+)
        // Load to the 8-bit A register, data from the absolute address specified by the 16-bit register HL.
        // The value of HL is incremented after the memory read.
        case 0x2A: {
            uint16_t HL = regHL();
            regs.A = memory.read(HL);
            // increment HL
            HL++;
            regs.H = (uint8_t)(HL >> 8); // drops low byte
            regs.L = (uint8_t)(HL & 0xFF); // drops high byte
            return 8; // # of cycles
        }

        // Load From Accumulator (indirect HL, increment), example: LD (HL+), A
        // Load to the absolute address specified by the 16-bit register HL, data from the 8-bit A register.
        // The value of HL is incremented after the memory write.
        case 0x22: {
            uint16_t HL = regHL();
            memory.write(HL, regs.A);
            // increment HL
            HL++;
            regs.H = (uint8_t)(HL >> 8); // drops low byte
            regs.L = (uint8_t)(HL & 0xFF); // drops high byte
            return 8; // # of cycles
        }

        // 6.3 16-bit load instructions ********************************************


        // Load Stack Pointer from HL, example: LD SP, HL
        // Load to the 16-bit SP register, data from the 16-bit HL register
        case 0xF9: {
            regs.SP = regHL();
            return 8; // # of cycles
        }

        // Push to stack, example: PUSH BC
        // Push to the stack memory, data from the 16-bit register rr
        case 0xC5: {
            memory.write(--regs.SP, (regBC() >> 8));
            memory.write(--regs.SP, (regBC() & 0xFF));

            return 16; // # of cycles
        }

        // Push to stack, example: PUSH DE
        // Push to the stack memory, data from the 16-bit register rr
        case 0xD5: {
            memory.write(--regs.SP, (regDE() >> 8));
            memory.write(--regs.SP, (regDE() & 0xFF));

            return 16; // # of cycles
        }

        // Push to stack, example: PUSH HL
        // Push to the stack memory, data from the 16-bit register rr
        case 0xE5: {
            memory.write(--regs.SP, (regHL() >> 8));
            memory.write(--regs.SP, (regHL() & 0xFF));

            return 16; // # of cycles
        }

        // Push to stack, example: PUSH AF
        // Push to the stack memory, data from the 16-bit register rr
        case 0xF5: {
            // F register lower 4 bits should always be masked to 0
            regs.F &= 0xF0;

            memory.write(--regs.SP, (regAF() >> 8));
            memory.write(--regs.SP, (regAF() & 0xFF));

            return 16; // # of cycles
        }


        // Pop from stack, example: POP BC
        // Pops to the 16-bit register rr, data from the stack memory
        case 0xC1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.B = msb;
            regs.C = lsb;
            return 12; // # of cycles
        }

        // Pop from stack, example: POP DE
        // Pops to the 16-bit register rr, data from the stack memory
        case 0xD1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.D = msb;
            regs.E = lsb;
            return 12; // # of cycles
        }

        // Pop from stack, example: POP HL
        // Pops to the 16-bit register rr, data from the stack memory
        case 0xE1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.H = msb;
            regs.L = lsb;
            return 12; // # of cycles
        }

        // Pop from stack, example: POP AF (lower 4 bits of F MUST be masked to 0)
        // Pops to the 16-bit register rr, data from the stack memory
        case 0xF1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);

            regs.A = msb;
            regs.F = lsb;

            // Always mask last 4 bits of F to 0
            regs.F &= 0xF0;

            return 12; // # of cycles
        }

        





        









        default:
            return 4; 
    }
}

uint16_t CPU::unsigned_16(uint8_t r1, uint8_t r2) {
    // Type cast to (uint16_t) since the << operator turns the value to a regular integer
    return (uint16_t)((r1 << 8) | r2);
}
uint16_t CPU::regAF() {
    // Type cast to (uint16_t) since the << operator turns the value to a regular integer
    return (uint16_t)((regs.A << 8) | regs.F);
}
uint16_t CPU::regBC() {
    return (uint16_t)((regs.B << 8) | regs.C);
}
uint16_t CPU::regDE() {
    return (uint16_t)((regs.D << 8) | regs.E);
}
uint16_t CPU::regHL() {
    return (uint16_t)((regs.H << 8) | regs.L);
}
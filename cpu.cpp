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
            if ((regs.B & 0x0F) + 1 > 0x0F)
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
            if ((regs.B & 0x0F) == 0)
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
            uint8_t nn_lsb = memory.read(regs.PC++);
            uint8_t nn_msb = memory.read(regs.PC++);
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
            if ((regs.D & 0x0F) + 1 > 0x0F)
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
            if ((regs.D & 0x0F) == 0)
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
        // JR e8
        case 0x18: {
            int8_t e8 = memory.read(regs.PC++); // must be signed int8 to allow jumps backward
            regs.PC += e8;
            return 12;
        }
        // ADD HL, DE
        case 0x19: {
            uint32_t result = regHL() + regDE();

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag 1 carry from bit 11 to 12
            if ((regHL() & 0x0FFF) + (regDE() & 0x0FFF) > 0x0FFF)
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
        // LD A, [DE]
        case 0x1A: {
            regs.A = memory.read(regDE());
            return 8;  
        }
        // DEC DE
        case 0x1B: {
            uint16_t result = regDE() - 1;

            regs.D = result >> 8;
            regs.E = result & 0xFF;
            
            return 8;
        }
        // INC E
        case 0x1C: {
            uint8_t result = regs.E + 1;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag, carry from bit 3 to bit 4
            if ((regs.E & 0x0F) + 1 > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag, none

            regs.E = result;

            return 4;
        }
        // DEC E
        case 0x1D: {
            uint8_t result = regs.E - 1;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag, carry from bit 3 to bit 4 and vice versa
            if ((regs.E & 0x0F) == 0)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag, none

            regs.E = result;

            return 4;
        }
        // LD E, n8
        case 0x1E: {
            uint8_t n8 = memory.read(regs.PC++);
            regs.E = n8;
            return 8;
        }
        // RRA
        case 0x1F: {
            uint8_t oldCarry = (regs.F & 0x10) << 3;
            uint8_t newCarry = (regs.A & 0x01);
            uint8_t shiftR = regs.A >> 1;

            // add carry to most significant bit (wrap-around)
            uint8_t result = shiftR | oldCarry;

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
        // JR NZ, e8
        case 0x20: {
            int8_t e8 = memory.read(regs.PC++);

            // if flag Z == 1, don't jump
            if (regs.F & 0x80)
                return 8;

            // do jump
            regs.PC += e8;
            return 12;
        }
        // LD HL, n16
        case 0x21: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            regs.H = msb;
            regs.L = lsb;
            return 12;
        }
        // LD [HL+], A
        case 0x22: {
            uint16_t HL = regHL();
            memory.write(HL, regs.A);
            HL++;
            regs.H = HL >> 8; // msb
            regs.L = HL & 0xFF; // lsb
            return 8;
        }
        // INC HL
        case 0x23: {
            uint16_t HL = regHL();
            HL++;
            regs.H = HL >> 8; // msb
            regs.L = HL & 0xFF; // lsb
            return 8;
        }
        // INC H
        case 0x24: {
            uint8_t result = regs.H + 1;

            // Z flag
            if (result == 0) 
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag
            if ((regs.H & 0x0F) + 1 > 0x0F)
                regs.F |= 0x20; 
            else
                regs.F &= ~0x20;

            regs.H = result;
            
            return 4;
        }
        // DEC H
        case 0x25: {
            uint8_t result = regs.H - 1;

            // Z flag
            if (result == 0) 
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.H & 0x0F) == 0)
                regs.F |= 0x20; 
            else
                regs.F &= ~0x20;

            regs.H = result;
            
            return 4;
        }
        // LD H, n8
        case 0x26: {
            regs.H = memory.read(regs.PC++); // get next byte which is immediate value n
            return 8; // # of cycles
        }
        // DAA
        case 0x27: {
            uint16_t result = regs.A;
            bool carry = regs.F & 0x10;

            // If last op was ADD
            if (!(regs.F & 0x40)) {
                if ((regs.F & 0x20) || ((result & 0x0F) > 9))
                    result += 0x06;

                if ((regs.F & 0x10) || (result > 0x99)) {
                    result += 0x60;
                    carry = true;
                }
            // If last op was SUBTRACT
            } else {
                if (regs.F & 0x20)
                    result -= 0x06;

                if (regs.F & 0x10) {
                    result -= 0x60;
                    carry = true;
                }                
            }

            regs.A = result & 0xFF;

            // Z flag
            if (regs.A == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // H cleared
            regs.F &= ~0x20;

            // C
            if (carry)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 4;
        }
        // JR Z, e8
        case 0x28: {
            int8_t e8 = memory.read(regs.PC++);
            // if Z == 1, do jump
            if (regs.F & 0x80) {
                regs.PC += e8;
                return 12;
            }
            return 8;
        }
        // ADD HL, HL
        case 0x29: {
            uint16_t hl = regHL();
            uint32_t result = hl + hl;

            // N = 0
            regs.F &= ~0x40;

            // H carry from bit 11
            if ((hl & 0x0FFF) + (hl & 0x0FFF) > 0x0FFF)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C carry from bit 15
            if (result > 0xFFFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.H = (result >> 8) & 0xFF;
            regs.L = result & 0xFF;

            return 8;
        }
        // STOPPED MANUALLY WRITING THEM AT THIS POINT
        // LD A, [HL+]
        case 0x2A: {
            uint16_t HL = regHL();

            // Load value from memory at HL into A
            regs.A = memory.read(HL);

            // Increment HL
            HL++;
            regs.H = HL >> 8;
            regs.L = HL & 0xFF;

            return 8;
        }
        // DEC HL
        case 0x2B: {
            uint16_t HL = regHL();

            HL--;

            regs.H = HL >> 8;
            regs.L = HL & 0xFF;

            return 8;
        }
        // INC L
        case 0x2C: {
            uint8_t result = regs.L + 1;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag (carry from bit 3 to 4)
            if ((regs.L & 0x0F) + 1 > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            regs.L = result;

            return 4;
        }
        // DEC L
        case 0x2D: {
            uint8_t result = regs.L - 1;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag (borrow from bit 4)
            if ((regs.L & 0x0F) == 0)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            regs.L = result;

            return 4;
        }
        // LD L, n8
        case 0x2E: {
            regs.L = memory.read(regs.PC++);
            return 8;
        }
        // CPL
        case 0x2F: {
            regs.A = ~regs.A;

            // N flag set
            regs.F |= 0x40;

            // H flag set
            regs.F |= 0x20;

            return 4;
        }
        // JR NC, e8
        case 0x30: {
            int8_t e8 = memory.read(regs.PC++);

            // If C flag is set, do NOT jump
            if (regs.F & 0x10)
                return 8;

            // Otherwise perform relative jump
            regs.PC += e8;
            return 12;
        }
        // LD SP, n16
        case 0x31: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);

            regs.SP = unsigned_16(msb, lsb);

            return 12;
        }
        // LD [HL-], A
        case 0x32: {
            uint16_t HL = regHL();

            // Store A into memory at HL
            memory.write(HL, regs.A);

            // Decrement HL
            HL--;
            regs.H = HL >> 8;
            regs.L = HL & 0xFF;

            return 8;
        }
        // INC SP
        case 0x33: {
            regs.SP++;
            return 8;
        }
        // INC [HL]
        case 0x34: {
            uint16_t HL = regHL();
            uint8_t value = memory.read(HL);
            uint8_t result = value + 1;

            // Z
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N = 0
            regs.F &= ~0x40;

            // H
            if ((value & 0x0F) + 1 > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            memory.write(HL, result);

            return 12;
        }
        // DEC [HL]
        case 0x35: {
            uint16_t HL = regHL();
            uint8_t value = memory.read(HL);
            uint8_t result = value - 1;

            // Z
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N = 1
            regs.F |= 0x40;

            // H
            if ((value & 0x0F) == 0)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            memory.write(HL, result);

            return 12;
        }
        // LD [HL], n8
        case 0x36: {
            uint8_t n8 = memory.read(regs.PC++);
            memory.write(regHL(), n8);
            return 12;
        }
        // SCF
        case 0x37: {
            regs.F &= ~0x40; // N = 0
            regs.F &= ~0x20; // H = 0
            regs.F |= 0x10;  // C = 1
            return 4;
        }
        // JR C, e8
        case 0x38: {
            int8_t e8 = memory.read(regs.PC++);

            if (regs.F & 0x10) {
                regs.PC += e8;
                return 12;
            }

            return 8;
        }
        // ADD HL, SP
        case 0x39: {
            uint16_t HL = regHL();
            uint32_t result = HL + regs.SP;

            // N = 0
            regs.F &= ~0x40;

            // H
            if ((HL & 0x0FFF) + (regs.SP & 0x0FFF) > 0x0FFF)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C
            if (result > 0xFFFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.H = (result >> 8) & 0xFF;
            regs.L = result & 0xFF;

            return 8;
        }
        // LD A, [HL-]
        case 0x3A: {
            uint16_t HL = regHL();

            regs.A = memory.read(HL);

            HL--;
            regs.H = HL >> 8;
            regs.L = HL & 0xFF;

            return 8;
        }
        // DEC SP
        case 0x3B: {
            regs.SP--;
            return 8;
        }

        // INC A
        case 0x3C: {
            uint8_t result = regs.A + 1;

            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            regs.F &= ~0x40;

            if ((regs.A & 0x0F) + 1 > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            regs.A = result;

            return 4;
        }
        // DEC A
        case 0x3D: {
            uint8_t result = regs.A - 1;

            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            regs.F |= 0x40;

            if ((regs.A & 0x0F) == 0)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            regs.A = result;

            return 4;
        }
        // LD A, n8
        case 0x3E: {
            regs.A = memory.read(regs.PC++);
            return 8;
        }
        // CCF
        case 0x3F: {
            regs.F &= ~0x40; // N = 0
            regs.F &= ~0x20; // H = 0

            if (regs.F & 0x10)
                regs.F &= ~0x10;
            else
                regs.F |= 0x10;

            return 4;
        }
        // LD B, B
        case 0x40: {
            regs.B = regs.B;
            return 4;
        }
        // LD B, C
        case 0x41: {
            regs.B = regs.C;
            return 4;
        }
        // LD B, D
        case 0x42: {
            regs.B = regs.D;
            return 4;
        }
        // LD B, E
        case 0x43: {
            regs.B = regs.E;
            return 4;
        }
        // LD B, H
        case 0x44: {
            regs.B = regs.H;
            return 4;
        }
        // LD B, L
        case 0x45: {
            regs.B = regs.L;
            return 4;
        }
        // LD B, [HL]
        case 0x46: {
            regs.B = memory.read(regHL());
            return 8;
        }
        // LD B, A
        case 0x47: {
            regs.B = regs.A;
            return 4;
        }
        // LD C, B
        case 0x48: {
            regs.C = regs.B;
            return 4;
        }
        // LD C, C
        case 0x49: {
            regs.C = regs.C;
            return 4;
        }
        // LD C, D
        case 0x4A: {
            regs.C = regs.D;
            return 4;
        }
        // LD C, E
        case 0x4B: {
            regs.C = regs.E;
            return 4;
        }
        // LD C, H
        case 0x4C: {
            regs.C = regs.H;
            return 4;
        }
        // LD C, L
        case 0x4D: {
            regs.C = regs.L;
            return 4;
        }
        // LD C, [HL]
        case 0x4E: {
            regs.C = memory.read(regHL());
            return 8;
        }
        // LD C, A
        case 0x4F: {
            regs.C = regs.A;
            return 4;
        }
        // LD D, B
        case 0x50: {
            regs.D = regs.B;
            return 4;
        }
        // LD D, C
        case 0x51: {
            regs.D = regs.C;
            return 4;
        }
        // LD D, D
        case 0x52: {
            return 4;
        }
        // LD D, E
        case 0x53: {
            regs.D = regs.E;
            return 4;
        }
        // LD D, H
        case 0x54: {
            regs.D = regs.H;
            return 4;
        }
        // LD D, L
        case 0x55: {
            regs.D = regs.L;
            return 4;
        }
        // LD D, [HL]
        case 0x56: {
            regs.D = memory.read(regHL());
            return 8;
        }
        // LD D, A
        case 0x57: {
            regs.D = regs.A;
            return 4;
        }
        // LD E, B
        case 0x58: {
            regs.E = regs.B;
            return 4;
        }
        // LD E, C
        case 0x59: {
            regs.E = regs.C;
            return 4;
        }
        // LD E, D
        case 0x5A: {
            regs.E = regs.D;
            return 4;
        }
        // LD E, E
        case 0x5B: {
            return 4;
        }
        // LD E, H
        case 0x5C: {
            regs.E = regs.H;
            return 4;
        }
        // LD E, L
        case 0x5D: {
            regs.E = regs.L;
            return 4;
        }
        // LD E, [HL]
        case 0x5E: {
            regs.E = memory.read(regHL());
            return 8;
        }
        // LD E, A
        case 0x5F: {
            regs.E = regs.A;
            return 4;
        }
        // LD H, B
        case 0x60: {
            regs.H = regs.B;
            return 4;
        }
        // LD H, C
        case 0x61: {
            regs.H = regs.C;
            return 4;
        }
        // LD H, D
        case 0x62: {
            regs.H = regs.D;
            return 4;
        }
        // LD H, E
        case 0x63: {
            regs.H = regs.E;
            return 4;
        }
        // LD H, H
        case 0x64: {
            return 4;
        }
        // LD H, L
        case 0x65: {
            regs.H = regs.L;
            return 4;
        }
        // LD H, [HL]
        case 0x66: {
            regs.H = memory.read(regHL());
            return 8;
        }
        // LD H, A
        case 0x67: {
            regs.H = regs.A;
            return 4;
        }
        // LD L, B
        case 0x68: {
            regs.L = regs.B;
            return 4;
        }
        // LD L, C
        case 0x69: {
            regs.L = regs.C;
            return 4;
        }
        // LD L, D
        case 0x6A: {
            regs.L = regs.D;
            return 4;
        }
        // LD L, E
        case 0x6B: {
            regs.L = regs.E;
            return 4;
        }
        // LD L, H
        case 0x6C: {
            regs.L = regs.H;
            return 4;
        }
        // LD L, L
        case 0x6D: {
            return 4;
        }
        // LD L, [HL]
        case 0x6E: {
            regs.L = memory.read(regHL());
            return 8;
        }
        // LD L, A
        case 0x6F: {
            regs.L = regs.A;
            return 4;
        }
        // LD [HL], B
        case 0x70: {
            memory.write(regHL(), regs.B);
            return 8;
        }
        // LD [HL], C
        case 0x71: {
            memory.write(regHL(), regs.C);
            return 8;
        }
        // LD [HL], D
        case 0x72: {
            memory.write(regHL(), regs.D);
            return 8;
        }
        // LD [HL], E
        case 0x73: {
            memory.write(regHL(), regs.E);
            return 8;
        }
        // LD [HL], H
        case 0x74: {
            memory.write(regHL(), regs.H);
            return 8;
        }
        // LD [HL], L
        case 0x75: {
            memory.write(regHL(), regs.L);
            return 8;
        }
        // HALT
        case 0x76: {
            stopped = true;
            return 4;
        }
        // LD [HL], A
        case 0x77: {
            memory.write(regHL(), regs.A);
            return 8;
        }
        // LD A, B
        case 0x78: {
            regs.A = regs.B;
            return 4;
        }
        // LD A, C
        case 0x79: {
            regs.A = regs.C;
            return 4;
        }
        // LD A, D
        case 0x7A: {
            regs.A = regs.D;
            return 4;
        }
        // LD A, E
        case 0x7B: {
            regs.A = regs.E;
            return 4;
        }
        // LD A, H
        case 0x7C: {
            regs.A = regs.H;
            return 4;
        }
        // LD A, L
        case 0x7D: {
            regs.A = regs.L;
            return 4;
        }
        // LD A, [HL]
        case 0x7E: {
            regs.A = memory.read(regHL());
            return 8;
        }
        // LD A, A
        case 0x7F: {
            return 4;
        }
        // ADD A, B
        case 0x80: {
            uint16_t result = regs.A + regs.B;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.B & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADD A, C
        case 0x81: {
            uint16_t result = regs.A + regs.C;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.C & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADD A, D
        case 0x82: {
            uint16_t result = regs.A + regs.D;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.D & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADD A, E
        case 0x83: {
            uint16_t result = regs.A + regs.E;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.E & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADD A, H
        case 0x84: {
            uint16_t result = regs.A + regs.H;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.H & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADD A, L
        case 0x85: {
            uint16_t result = regs.A + regs.L;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.L & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADD A, [HL]
        case 0x86: {
            uint8_t value = memory.read(regHL());
            uint16_t result = regs.A + value;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (value & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // ADD A, A
        case 0x87: {
            uint16_t result = regs.A + regs.A;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.A & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADC A, B
        case 0x88: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + regs.B + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.B & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADC A, C
        case 0x89: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + regs.C + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.C & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADC A, D
        case 0x8A: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + regs.D + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.D & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADC A, E
        case 0x8B: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + regs.E + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.E & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADC A, H
        case 0x8C: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + regs.H + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.H & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADC A, L
        case 0x8D: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + regs.L + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.L & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // ADC A, [HL]
        case 0x8E: {
            uint8_t value = memory.read(regHL());
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + value + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (value & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // ADC A, A
        case 0x8F: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + regs.A + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 0
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (regs.A & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SUB A, B
        case 0x90: {
            uint16_t result = regs.A - regs.B;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.B & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.B)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SUB A, C
        case 0x91: {
            uint16_t result = regs.A - regs.C;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.C & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.C)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SUB A, D
        case 0x92: {
            uint16_t result = regs.A - regs.D;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.D & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.D)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SUB A, E
        case 0x93: {
            uint16_t result = regs.A - regs.E;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.E & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.E)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SUB A, H
        case 0x94: {
            uint16_t result = regs.A - regs.H;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.H & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.H)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SUB A, L
        case 0x95: {
            uint16_t result = regs.A - regs.L;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.L & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.L)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SUB A, [HL]
        case 0x96: {
            uint8_t value = memory.read(regHL());
            uint16_t result = regs.A - value;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (value & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < value)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // SUB A, A
        case 0x97: {
            uint16_t result = regs.A - regs.A;

            // Z flag
            regs.F |= 0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SBC A, B
        case 0x98: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - regs.B - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((regs.B & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (regs.B + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SBC A, C
        case 0x99: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - regs.C - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((regs.C & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (regs.C + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SBC A, D
        case 0x9A: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - regs.D - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((regs.D & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (regs.D + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SBC A, E
        case 0x9B: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - regs.E - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((regs.E & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (regs.E + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SBC A, H
        case 0x9C: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - regs.H - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((regs.H & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (regs.H + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SBC A, L
        case 0x9D: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - regs.L - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((regs.L & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (regs.L + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // SBC A, [HL]
        case 0x9E: {
            uint8_t value = memory.read(regHL());
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - value - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((value & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (value + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // SBC A, A
        case 0x9F: {
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - regs.A - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set to 1
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((regs.A & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (regs.A + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 4;
        }
        // AND A, B
        case 0xA0: {
            uint8_t result = regs.A & regs.B;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // AND A, C
        case 0xA1: {
            uint8_t result = regs.A & regs.C;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // AND A, D
        case 0xA2: {
            uint8_t result = regs.A & regs.D;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // AND A, E
        case 0xA3: {
            uint8_t result = regs.A & regs.E;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // AND A, H
        case 0xA4: {
            uint8_t result = regs.A & regs.H;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // AND A, L
        case 0xA5: {
            uint8_t result = regs.A & regs.L;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // AND A, [HL]
        case 0xA6: {
            uint8_t value = memory.read(regHL());
            uint8_t result = regs.A & value;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 8;
        }
        // AND A, A
        case 0xA7: {
            uint8_t result = regs.A & regs.A;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // XOR A, B
        case 0xA8: {
            uint8_t result = regs.A ^ regs.B;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // XOR A, C
        case 0xA9: {
            uint8_t result = regs.A ^ regs.C;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // XOR A, D
        case 0xAA: {
            uint8_t result = regs.A ^ regs.D;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // XOR A, E
        case 0xAB: {
            uint8_t result = regs.A ^ regs.E;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // XOR A, H
        case 0xAC: {
            uint8_t result = regs.A ^ regs.H;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // XOR A, L
        case 0xAD: {
            uint8_t result = regs.A ^ regs.L;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // XOR A, [HL]
        case 0xAE: {
            uint8_t value = memory.read(regHL());
            uint8_t result = regs.A ^ value;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 8;
        }
        // XOR A, A
        case 0xAF: {
            uint8_t result = regs.A ^ regs.A;

            // Z flag
            regs.F |= 0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // OR A, B
        case 0xB0: {
            uint8_t result = regs.A | regs.B;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // OR A, C
        case 0xB1: {
            uint8_t result = regs.A | regs.C;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // OR A, D
        case 0xB2: {
            uint8_t result = regs.A | regs.D;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // OR A, E
        case 0xB3: {
            uint8_t result = regs.A | regs.E;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // OR A, H
        case 0xB4: {
            uint8_t result = regs.A | regs.H;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // OR A, L
        case 0xB5: {
            uint8_t result = regs.A | regs.L;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // OR A, [HL]
        case 0xB6: {
            uint8_t value = memory.read(regHL());
            uint8_t result = regs.A | value;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 8;
        }
        // OR A, A
        case 0xB7: {
            uint8_t result = regs.A | regs.A;

            // Z flag
            if (result == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            regs.A = result;

            return 4;
        }
        // CP A, B
        case 0xB8: {
            uint16_t result = regs.A - regs.B;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.B & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.B)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 4;
        }
        // CP A, C
        case 0xB9: {
            uint16_t result = regs.A - regs.C;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.C & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.C)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 4;
        }
        // CP A, D
        case 0xBA: {
            uint16_t result = regs.A - regs.D;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.D & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.D)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 4;
        }
        // CP A, E
        case 0xBB: {
            uint16_t result = regs.A - regs.E;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.E & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.E)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 4;
        }
        // CP A, H
        case 0xBC: {
            uint16_t result = regs.A - regs.H;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.H & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.H)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 4;
        }
        // CP A, L
        case 0xBD: {
            uint16_t result = regs.A - regs.L;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (regs.L & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < regs.L)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 4;
        }
        // CP A, [HL]
        case 0xBE: {
            uint8_t value = memory.read(regHL());
            uint16_t result = regs.A - value;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (value & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < value)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 8;
        }
        // CP A, A
        case 0xBF: {
            uint16_t result = regs.A - regs.A;

            // Z flag
            regs.F |= 0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag cleared
            regs.F &= ~0x20;

            // C flag cleared
            regs.F &= ~0x10;

            return 4;
        }
        // RET NZ
        case 0xC0: {
            if (!(regs.F & 0x80)) {
                uint8_t lsb = memory.read(regs.SP++);
                uint8_t msb = memory.read(regs.SP++);
                regs.PC = unsigned_16(msb, lsb);
                return 20;
            }
            return 8;
        }
        // POP BC
        case 0xC1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.B = msb;
            regs.C = lsb;
            return 12;
        }
        // JP NZ, a16
        case 0xC2: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (!(regs.F & 0x80)) {
                regs.PC = addr;
                return 16;
            }

            return 12;
        }
        // JP a16
        case 0xC3: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            regs.PC = unsigned_16(msb, lsb);
            return 16;
        }
        // CALL NZ, a16
        case 0xC4: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (!(regs.F & 0x80)) {
                uint16_t ret = regs.PC;
                memory.write(--regs.SP, (ret >> 8));
                memory.write(--regs.SP, (ret & 0xFF));
                regs.PC = addr;
                return 24;
            }

            return 12;
        }
        // PUSH BC
        case 0xC5: {
            memory.write(--regs.SP, regs.B);
            memory.write(--regs.SP, regs.C);
            return 16;
        }
        // ADD A, n8
        case 0xC6: {
            uint8_t value = memory.read(regs.PC++);
            uint16_t result = regs.A + value;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (value & 0x0F) > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // RST 00H
        case 0xC7: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x00;
            return 16;
        }
        // RET Z
        case 0xC8: {
            if (regs.F & 0x80) {
                uint8_t lsb = memory.read(regs.SP++);
                uint8_t msb = memory.read(regs.SP++);
                regs.PC = unsigned_16(msb, lsb);
                return 20;
            }
            return 8;
        }
        // RET
        case 0xC9: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.PC = unsigned_16(msb, lsb);
            return 16;
        }
        // JP Z, a16
        case 0xCA: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (regs.F & 0x80) {
                regs.PC = addr;
                return 16;
            }

            return 12;
        }
        // PREFIX CB
        case 0xCB: {
            uint8_t opcode = memory.read(regs.PC++);
            return 4 + executePrefixed(opcode);
        }
        // CALL Z, a16
        case 0xCC: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (regs.F & 0x80) {
                uint16_t ret = regs.PC;
                memory.write(--regs.SP, (ret >> 8));
                memory.write(--regs.SP, (ret & 0xFF));
                regs.PC = addr;
                return 24;
            }

            return 12;
        }
        // CALL a16
        case 0xCD: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            uint16_t ret = regs.PC;
            memory.write(--regs.SP, (ret >> 8));
            memory.write(--regs.SP, (ret & 0xFF));

            regs.PC = addr;

            return 24;
        }
        // ADC A, n8
        case 0xCE: {
            uint8_t value = memory.read(regs.PC++);
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A + value + carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag cleared
            regs.F &= ~0x40;

            // H flag
            if ((regs.A & 0x0F) + (value & 0x0F) + carry > 0x0F)
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (result > 0xFF)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // RST 08H
        case 0xCF: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x08;
            return 16;
        }
        // RET NC
        case 0xD0: {
            if (!(regs.F & 0x10)) {
                uint8_t lsb = memory.read(regs.SP++);
                uint8_t msb = memory.read(regs.SP++);
                regs.PC = unsigned_16(msb, lsb);
                return 20;
            }
            return 8;
        }
        // POP DE
        case 0xD1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.D = msb;
            regs.E = lsb;
            return 12;
        }
        // JP NC, a16
        case 0xD2: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (!(regs.F & 0x10)) {
                regs.PC = addr;
                return 16;
            }

            return 12;
        }
        // INVALID / UNUSED
        case 0xD3:
            return 4;

        // CALL NC, a16
        case 0xD4: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (!(regs.F & 0x10)) {
                uint16_t ret = regs.PC;
                memory.write(--regs.SP, (ret >> 8));
                memory.write(--regs.SP, (ret & 0xFF));
                regs.PC = addr;
                return 24;
            }

            return 12;
        }
        // PUSH DE
        case 0xD5: {
            memory.write(--regs.SP, regs.D);
            memory.write(--regs.SP, regs.E);
            return 16;
        }
        // SUB A, n8
        case 0xD6: {
            uint8_t value = memory.read(regs.PC++);
            uint16_t result = regs.A - value;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < (value & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < value)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // RST 10H
        case 0xD7: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x10;
            return 16;
        }
        // RET C
        case 0xD8: {
            if (regs.F & 0x10) {
                uint8_t lsb = memory.read(regs.SP++);
                uint8_t msb = memory.read(regs.SP++);
                regs.PC = unsigned_16(msb, lsb);
                return 20;
            }
            return 8;
        }
        // RETI
        case 0xD9: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.PC = unsigned_16(msb, lsb);

            ime = true;

            return 16;
        }
        // JP C, a16
        case 0xDA: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (regs.F & 0x10) {
                regs.PC = addr;
                return 16;
            }

            return 12;
        }
        // INVALID / UNUSED
        case 0xDB:
            return 4;

        // CALL C, a16
        case 0xDC: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            if (regs.F & 0x10) {
                uint16_t ret = regs.PC;
                memory.write(--regs.SP, (ret >> 8));
                memory.write(--regs.SP, (ret & 0xFF));
                regs.PC = addr;
                return 24;
            }

            return 12;
        }
        // INVALID / UNUSED
        case 0xDD:
            return 4;

        // SBC A, n8
        case 0xDE: {
            uint8_t value = memory.read(regs.PC++);
            uint8_t carry = (regs.F & 0x10) >> 4;
            uint16_t result = regs.A - value - carry;

            // Z flag
            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag set
            regs.F |= 0x40;

            // H flag
            if ((regs.A & 0x0F) < ((value & 0x0F) + carry))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            // C flag
            if (regs.A < (value + carry))
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            regs.A = result & 0xFF;

            return 8;
        }
        // RST 18H
        case 0xDF: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x18;
            return 16;
        }
        // LDH (a8), A
        case 0xE0: {
            uint8_t offset = memory.read(regs.PC++);
            memory.write(0xFF00 + offset, regs.A);
            return 12;
        }
        // POP HL
        case 0xE1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.H = msb;
            regs.L = lsb;
            return 12;
        }
        // LD (C), A
        case 0xE2: {
            memory.write(0xFF00 + regs.C, regs.A);
            return 8;
        }
        // INVALID / UNUSED
        case 0xE3:
            return 4;

        // INVALID / UNUSED
        case 0xE4:
            return 4;

        // PUSH HL
        case 0xE5: {
            memory.write(--regs.SP, regs.H);
            memory.write(--regs.SP, regs.L);
            return 16;
        }
        // AND A, n8
        case 0xE6: {
            uint8_t value = memory.read(regs.PC++);
            regs.A &= value;

            // Z flag
            if (regs.A == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N flag reset
            regs.F &= ~0x40;

            // H flag set
            regs.F |= 0x20;

            // C flag reset
            regs.F &= ~0x10;

            return 8;
        }
        // RST 20H
        case 0xE7: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x20;
            return 16;
        }
        // ADD SP, r8
        case 0xE8: {
            int8_t value = (int8_t)memory.read(regs.PC++);
            uint16_t sp = regs.SP;
            uint16_t result = sp + value;

            regs.F = 0;

            if (((sp & 0xF) + (value & 0xF)) > 0xF)
                regs.F |= 0x20;

            if (((sp & 0xFF) + (value & 0xFF)) > 0xFF)
                regs.F |= 0x10;

            regs.SP = result;

            return 16;
        }
        // JP (HL)
        case 0xE9: {
            regs.PC = unsigned_16(regs.H, regs.L);
            return 4;
        }
        // LD (a16), A
        case 0xEA: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            memory.write(addr, regs.A);
            return 16;
        }
        // INVALID / UNUSED
        case 0xEB:
            return 4;

        // INVALID / UNUSED
        case 0xEC:
            return 4;

        // INVALID / UNUSED
        case 0xED:
            return 4;

        // XOR A, n8
        case 0xEE: {
            uint8_t value = memory.read(regs.PC++);
            regs.A ^= value;

            // Z flag
            if (regs.A == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            // N, H, C reset
            regs.F &= ~(0x40 | 0x20 | 0x10);

            return 8;
        }
        // RST 28H
        case 0xEF: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x28;
            return 16;
        }
        // LDH A, (a8)
        case 0xF0: {
            uint8_t offset = memory.read(regs.PC++);
            regs.A = memory.read(0xFF00 + offset);
            return 12;
        }
        // POP AF
        case 0xF1: {
            uint8_t lsb = memory.read(regs.SP++);
            uint8_t msb = memory.read(regs.SP++);
            regs.A = msb;
            regs.F = lsb & 0xF0; // lower 4 bits always zero
            return 12;
        }
        // LD A, (C)
        case 0xF2: {
            regs.A = memory.read(0xFF00 + regs.C);
            return 8;
        }
        // DI
        case 0xF3: {
            ime = false;
            return 4;
        }
        // INVALID / UNUSED
        case 0xF4:
            return 4;

        // PUSH AF
        case 0xF5: {
            memory.write(--regs.SP, regs.A);
            memory.write(--regs.SP, regs.F & 0xF0);
            return 16;
        }
        // OR A, n8
        case 0xF6: {
            uint8_t value = memory.read(regs.PC++);
            regs.A |= value;

            if (regs.A == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            regs.F &= ~(0x40 | 0x20 | 0x10);

            return 8;
        }
        // RST 30H
        case 0xF7: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x30;
            return 16;
        }
        // LD HL, SP + r8
        case 0xF8: {
            int8_t value = (int8_t)memory.read(regs.PC++);
            uint16_t sp = regs.SP;
            uint16_t result = sp + value;

            regs.F = 0;

            if (((sp & 0xF) + (value & 0xF)) > 0xF)
                regs.F |= 0x20;

            if (((sp & 0xFF) + (value & 0xFF)) > 0xFF)
                regs.F |= 0x10;

            uint16_t hl = result;
            regs.H = (hl >> 8);
            regs.L = (hl & 0xFF);

            return 12;
        }
        // LD SP, HL
        case 0xF9: {
            regs.SP = unsigned_16(regs.H, regs.L);
            return 8;
        }
        // LD A, (a16)
        case 0xFA: {
            uint8_t lsb = memory.read(regs.PC++);
            uint8_t msb = memory.read(regs.PC++);
            uint16_t addr = unsigned_16(msb, lsb);

            regs.A = memory.read(addr);
            return 16;
        }
        // EI
        case 0xFB: {
            ime = true;
            return 4;
        }
        // INVALID / UNUSED
        case 0xFC:
            return 4;

        // INVALID / UNUSED
        case 0xFD:
            return 4;

        // CP A, n8
        case 0xFE: {
            uint8_t value = memory.read(regs.PC++);
            uint16_t result = regs.A - value;

            if ((result & 0xFF) == 0)
                regs.F |= 0x80;
            else
                regs.F &= ~0x80;

            regs.F |= 0x40;

            if ((regs.A & 0x0F) < (value & 0x0F))
                regs.F |= 0x20;
            else
                regs.F &= ~0x20;

            if (regs.A < value)
                regs.F |= 0x10;
            else
                regs.F &= ~0x10;

            return 8;
        }
        // RST 38H
        case 0xFF: {
            memory.write(--regs.SP, (regs.PC >> 8));
            memory.write(--regs.SP, (regs.PC & 0xFF));
            regs.PC = 0x38;
            return 16;
        }
    }
}

int CPU::executePrefixed(uint8_t opcode) {
    // Prefixed opcodes, or opcodes that were immediately after a 0xCB prefix instruction
    switch (opcode) {
        // RLC B
        case 0x00: {
            uint8_t carry = (regs.B & 0x80) >> 7;
            regs.B = (regs.B << 1) | carry;

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RLC C
        case 0x01: {
            uint8_t carry = (regs.C & 0x80) >> 7;
            regs.C = (regs.C << 1) | carry;

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RLC D
        case 0x02: {
            uint8_t carry = (regs.D & 0x80) >> 7;
            regs.D = (regs.D << 1) | carry;

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RLC E
        case 0x03: {
            uint8_t carry = (regs.E & 0x80) >> 7;
            regs.E = (regs.E << 1) | carry;

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RLC H
        case 0x04: {
            uint8_t carry = (regs.H & 0x80) >> 7;
            regs.H = (regs.H << 1) | carry;

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RLC L
        case 0x05: {
            uint8_t carry = (regs.L & 0x80) >> 7;
            regs.L = (regs.L << 1) | carry;

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RLC (HL)
        case 0x06: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            uint8_t carry = (value & 0x80) >> 7;
            value = (value << 1) | carry;

            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 12;
        }

        // RLC A
        case 0x07: {
            uint8_t carry = (regs.A & 0x80) >> 7;
            regs.A = (regs.A << 1) | carry;

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RRC B
        case 0x08: {
            uint8_t carry = regs.B & 0x01;
            regs.B = (regs.B >> 1) | (carry << 7);

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RRC C
        case 0x09: {
            uint8_t carry = regs.C & 0x01;
            regs.C = (regs.C >> 1) | (carry << 7);

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RRC D
        case 0x0A: {
            uint8_t carry = regs.D & 0x01;
            regs.D = (regs.D >> 1) | (carry << 7);

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RRC E
        case 0x0B: {
            uint8_t carry = regs.E & 0x01;
            regs.E = (regs.E >> 1) | (carry << 7);

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RRC H
        case 0x0C: {
            uint8_t carry = regs.H & 0x01;
            regs.H = (regs.H >> 1) | (carry << 7);

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RRC L
        case 0x0D: {
            uint8_t carry = regs.L & 0x01;
            regs.L = (regs.L >> 1) | (carry << 7);

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RRC (HL)
        case 0x0E: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            uint8_t carry = value & 0x01;
            value = (value >> 1) | (carry << 7);

            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 12;
        }

        // RRC A
        case 0x0F: {
            uint8_t carry = regs.A & 0x01;
            regs.A = (regs.A >> 1) | (carry << 7);

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }
        // RL B
        case 0x10: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (regs.B & 0x80) >> 7;
            regs.B = (regs.B << 1) | oldCarry;

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RL C
        case 0x11: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (regs.C & 0x80) >> 7;
            regs.C = (regs.C << 1) | oldCarry;

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RL D
        case 0x12: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (regs.D & 0x80) >> 7;
            regs.D = (regs.D << 1) | oldCarry;

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RL E
        case 0x13: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (regs.E & 0x80) >> 7;
            regs.E = (regs.E << 1) | oldCarry;

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RL H
        case 0x14: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (regs.H & 0x80) >> 7;
            regs.H = (regs.H << 1) | oldCarry;

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RL L
        case 0x15: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (regs.L & 0x80) >> 7;
            regs.L = (regs.L << 1) | oldCarry;

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RL (HL)
        case 0x16: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (value & 0x80) >> 7;
            value = (value << 1) | oldCarry;

            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 12;
        }

        // RL A
        case 0x17: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = (regs.A & 0x80) >> 7;
            regs.A = (regs.A << 1) | oldCarry;

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RR B
        case 0x18: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = regs.B & 0x01;
            regs.B = (regs.B >> 1) | (oldCarry << 7);

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RR C
        case 0x19: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = regs.C & 0x01;
            regs.C = (regs.C >> 1) | (oldCarry << 7);

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RR D
        case 0x1A: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = regs.D & 0x01;
            regs.D = (regs.D >> 1) | (oldCarry << 7);

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RR E
        case 0x1B: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = regs.E & 0x01;
            regs.E = (regs.E >> 1) | (oldCarry << 7);

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RR H
        case 0x1C: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = regs.H & 0x01;
            regs.H = (regs.H >> 1) | (oldCarry << 7);

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RR L
        case 0x1D: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = regs.L & 0x01;
            regs.L = (regs.L >> 1) | (oldCarry << 7);

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // RR (HL)
        case 0x1E: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = value & 0x01;
            value = (value >> 1) | (oldCarry << 7);

            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 12;
        }

        // RR A
        case 0x1F: {
            uint8_t oldCarry = (regs.F & 0x10) >> 4;
            uint8_t carry = regs.A & 0x01;
            regs.A = (regs.A >> 1) | (oldCarry << 7);

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }
        // SLA B
        case 0x20: {
            uint8_t carry = (regs.B & 0x80) >> 7;
            regs.B <<= 1;

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SLA C
        case 0x21: {
            uint8_t carry = (regs.C & 0x80) >> 7;
            regs.C <<= 1;

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SLA D
        case 0x22: {
            uint8_t carry = (regs.D & 0x80) >> 7;
            regs.D <<= 1;

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SLA E
        case 0x23: {
            uint8_t carry = (regs.E & 0x80) >> 7;
            regs.E <<= 1;

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SLA H
        case 0x24: {
            uint8_t carry = (regs.H & 0x80) >> 7;
            regs.H <<= 1;

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SLA L
        case 0x25: {
            uint8_t carry = (regs.L & 0x80) >> 7;
            regs.L <<= 1;

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SLA (HL)
        case 0x26: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            uint8_t carry = (value & 0x80) >> 7;
            value <<= 1;

            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 12;
        }

        // SLA A
        case 0x27: {
            uint8_t carry = (regs.A & 0x80) >> 7;
            regs.A <<= 1;

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRA B
        case 0x28: {
            uint8_t carry = regs.B & 0x01;
            uint8_t msb = regs.B & 0x80;
            regs.B = (regs.B >> 1) | msb;

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRA C
        case 0x29: {
            uint8_t carry = regs.C & 0x01;
            uint8_t msb = regs.C & 0x80;
            regs.C = (regs.C >> 1) | msb;

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRA D
        case 0x2A: {
            uint8_t carry = regs.D & 0x01;
            uint8_t msb = regs.D & 0x80;
            regs.D = (regs.D >> 1) | msb;

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRA E
        case 0x2B: {
            uint8_t carry = regs.E & 0x01;
            uint8_t msb = regs.E & 0x80;
            regs.E = (regs.E >> 1) | msb;

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRA H
        case 0x2C: {
            uint8_t carry = regs.H & 0x01;
            uint8_t msb = regs.H & 0x80;
            regs.H = (regs.H >> 1) | msb;

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRA L
        case 0x2D: {
            uint8_t carry = regs.L & 0x01;
            uint8_t msb = regs.L & 0x80;
            regs.L = (regs.L >> 1) | msb;

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRA (HL)
        case 0x2E: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            uint8_t carry = value & 0x01;
            uint8_t msb = value & 0x80;
            value = (value >> 1) | msb;

            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 12;
        }

        // SRA A
        case 0x2F: {
            uint8_t carry = regs.A & 0x01;
            uint8_t msb = regs.A & 0x80;
            regs.A = (regs.A >> 1) | msb;

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }
        // SWAP B
        case 0x30: {
            regs.B = (regs.B << 4) | (regs.B >> 4);

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;

            return 4;
        }

        // SWAP C
        case 0x31: {
            regs.C = (regs.C << 4) | (regs.C >> 4);

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;

            return 4;
        }

        // SWAP D
        case 0x32: {
            regs.D = (regs.D << 4) | (regs.D >> 4);

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;

            return 4;
        }

        // SWAP E
        case 0x33: {
            regs.E = (regs.E << 4) | (regs.E >> 4);

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;

            return 4;
        }

        // SWAP H
        case 0x34: {
            regs.H = (regs.H << 4) | (regs.H >> 4);

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;

            return 4;
        }

        // SWAP L
        case 0x35: {
            regs.L = (regs.L << 4) | (regs.L >> 4);

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;

            return 4;
        }

        // SWAP (HL)
        case 0x36: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            value = (value << 4) | (value >> 4);
            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;

            return 12;
        }

        // SWAP A
        case 0x37: {
            regs.A = (regs.A << 4) | (regs.A >> 4);

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;

            return 4;
        }

        // SRL B
        case 0x38: {
            uint8_t carry = regs.B & 0x01;
            regs.B >>= 1;

            regs.F = 0;
            if (regs.B == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRL C
        case 0x39: {
            uint8_t carry = regs.C & 0x01;
            regs.C >>= 1;

            regs.F = 0;
            if (regs.C == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRL D
        case 0x3A: {
            uint8_t carry = regs.D & 0x01;
            regs.D >>= 1;

            regs.F = 0;
            if (regs.D == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRL E
        case 0x3B: {
            uint8_t carry = regs.E & 0x01;
            regs.E >>= 1;

            regs.F = 0;
            if (regs.E == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRL H
        case 0x3C: {
            uint8_t carry = regs.H & 0x01;
            regs.H >>= 1;

            regs.F = 0;
            if (regs.H == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRL L
        case 0x3D: {
            uint8_t carry = regs.L & 0x01;
            regs.L >>= 1;

            regs.F = 0;
            if (regs.L == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }

        // SRL (HL)
        case 0x3E: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);

            uint8_t carry = value & 0x01;
            value >>= 1;

            memory.write(addr, value);

            regs.F = 0;
            if (value == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 12;
        }

        // SRL A
        case 0x3F: {
            uint8_t carry = regs.A & 0x01;
            regs.A >>= 1;

            regs.F = 0;
            if (regs.A == 0) regs.F |= 0x80;
            if (carry) regs.F |= 0x10;

            return 4;
        }
        // BIT 0,B
        case 0x40: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 0))) regs.F |= 0x80;
            return 4;
        }

        // BIT 0,C
        case 0x41: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 0))) regs.F |= 0x80;
            return 4;
        }

        // BIT 0,D
        case 0x42: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 0))) regs.F |= 0x80;
            return 4;
        }

        // BIT 0,E
        case 0x43: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 0))) regs.F |= 0x80;
            return 4;
        }

        // BIT 0,H
        case 0x44: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 0))) regs.F |= 0x80;
            return 4;
        }

        // BIT 0,L
        case 0x45: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 0))) regs.F |= 0x80;
            return 4;
        }

        // BIT 0,(HL)
        case 0x46: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 0))) regs.F |= 0x80;

            return 8;
        }

        // BIT 0,A
        case 0x47: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 0))) regs.F |= 0x80;
            return 4;
        }

        // BIT 1,B
        case 0x48: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 1))) regs.F |= 0x80;
            return 4;
        }

        // BIT 1,C
        case 0x49: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 1))) regs.F |= 0x80;
            return 4;
        }

        // BIT 1,D
        case 0x4A: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 1))) regs.F |= 0x80;
            return 4;
        }

        // BIT 1,E
        case 0x4B: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 1))) regs.F |= 0x80;
            return 4;
        }

        // BIT 1,H
        case 0x4C: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 1))) regs.F |= 0x80;
            return 4;
        }

        // BIT 1,L
        case 0x4D: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 1))) regs.F |= 0x80;
            return 4;
        }

        // BIT 1,(HL)
        case 0x4E: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 1))) regs.F |= 0x80;

            return 8;
        }

        // BIT 1,A
        case 0x4F: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 1))) regs.F |= 0x80;
            return 4;
        }
        // BIT 2,B
        case 0x50: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 2))) regs.F |= 0x80;
            return 4;
        }

        // BIT 2,C
        case 0x51: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 2))) regs.F |= 0x80;
            return 4;
        }

        // BIT 2,D
        case 0x52: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 2))) regs.F |= 0x80;
            return 4;
        }

        // BIT 2,E
        case 0x53: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 2))) regs.F |= 0x80;
            return 4;
        }

        // BIT 2,H
        case 0x54: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 2))) regs.F |= 0x80;
            return 4;
        }

        // BIT 2,L
        case 0x55: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 2))) regs.F |= 0x80;
            return 4;
        }

        // BIT 2,(HL)
        case 0x56: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 2))) regs.F |= 0x80;

            return 8;
        }

        // BIT 2,A
        case 0x57: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 2))) regs.F |= 0x80;
            return 4;
        }

        // BIT 3,B
        case 0x58: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 3))) regs.F |= 0x80;
            return 4;
        }

        // BIT 3,C
        case 0x59: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 3))) regs.F |= 0x80;
            return 4;
        }

        // BIT 3,D
        case 0x5A: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 3))) regs.F |= 0x80;
            return 4;
        }

        // BIT 3,E
        case 0x5B: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 3))) regs.F |= 0x80;
            return 4;
        }

        // BIT 3,H
        case 0x5C: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 3))) regs.F |= 0x80;
            return 4;
        }

        // BIT 3,L
        case 0x5D: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 3))) regs.F |= 0x80;
            return 4;
        }

        // BIT 3,(HL)
        case 0x5E: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 3))) regs.F |= 0x80;

            return 8;
        }

        // BIT 3,A
        case 0x5F: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 3))) regs.F |= 0x80;
            return 4;
        }
        // BIT 4,B
        case 0x60: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 4))) regs.F |= 0x80;
            return 4;
        }

        // BIT 4,C
        case 0x61: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 4))) regs.F |= 0x80;
            return 4;
        }

        // BIT 4,D
        case 0x62: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 4))) regs.F |= 0x80;
            return 4;
        }

        // BIT 4,E
        case 0x63: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 4))) regs.F |= 0x80;
            return 4;
        }

        // BIT 4,H
        case 0x64: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 4))) regs.F |= 0x80;
            return 4;
        }

        // BIT 4,L
        case 0x65: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 4))) regs.F |= 0x80;
            return 4;
        }

        // BIT 4,(HL)
        case 0x66: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 4))) regs.F |= 0x80;

            return 8;
        }

        // BIT 4,A
        case 0x67: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 4))) regs.F |= 0x80;
            return 4;
        }

        // BIT 5,B
        case 0x68: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 5))) regs.F |= 0x80;
            return 4;
        }

        // BIT 5,C
        case 0x69: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 5))) regs.F |= 0x80;
            return 4;
        }

        // BIT 5,D
        case 0x6A: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 5))) regs.F |= 0x80;
            return 4;
        }

        // BIT 5,E
        case 0x6B: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 5))) regs.F |= 0x80;
            return 4;
        }

        // BIT 5,H
        case 0x6C: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 5))) regs.F |= 0x80;
            return 4;
        }

        // BIT 5,L
        case 0x6D: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 5))) regs.F |= 0x80;
            return 4;
        }

        // BIT 5,(HL)
        case 0x6E: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 5))) regs.F |= 0x80;

            return 8;
        }

        // BIT 5,A
        case 0x6F: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 5))) regs.F |= 0x80;
            return 4;
        }
        // BIT 6,B
        case 0x70: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 6))) regs.F |= 0x80;
            return 4;
        }

        // BIT 6,C
        case 0x71: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 6))) regs.F |= 0x80;
            return 4;
        }

        // BIT 6,D
        case 0x72: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 6))) regs.F |= 0x80;
            return 4;
        }

        // BIT 6,E
        case 0x73: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 6))) regs.F |= 0x80;
            return 4;
        }

        // BIT 6,H
        case 0x74: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 6))) regs.F |= 0x80;
            return 4;
        }

        // BIT 6,L
        case 0x75: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 6))) regs.F |= 0x80;
            return 4;
        }

        // BIT 6,(HL)
        case 0x76: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 6))) regs.F |= 0x80;

            return 8;
        }

        // BIT 6,A
        case 0x77: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 6))) regs.F |= 0x80;
            return 4;
        }

        // BIT 7,B
        case 0x78: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.B & (1 << 7))) regs.F |= 0x80;
            return 4;
        }

        // BIT 7,C
        case 0x79: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.C & (1 << 7))) regs.F |= 0x80;
            return 4;
        }

        // BIT 7,D
        case 0x7A: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.D & (1 << 7))) regs.F |= 0x80;
            return 4;
        }

        // BIT 7,E
        case 0x7B: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.E & (1 << 7))) regs.F |= 0x80;
            return 4;
        }

        // BIT 7,H
        case 0x7C: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.H & (1 << 7))) regs.F |= 0x80;
            return 4;
        }

        // BIT 7,L
        case 0x7D: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.L & (1 << 7))) regs.F |= 0x80;
            return 4;
        }

        // BIT 7,(HL)
        case 0x7E: {
            uint8_t value = memory.read(unsigned_16(regs.H, regs.L));

            regs.F = (regs.F & 0x10) | 0x20;
            if (!(value & (1 << 7))) regs.F |= 0x80;

            return 8;
        }

        // BIT 7,A
        case 0x7F: {
            regs.F = (regs.F & 0x10) | 0x20;
            if (!(regs.A & (1 << 7))) regs.F |= 0x80;
            return 4;
        }
        // RES 0,B
        case 0x80: {
            regs.B &= ~(1 << 0);
            return 4;
        }

        // RES 0,C
        case 0x81: {
            regs.C &= ~(1 << 0);
            return 4;
        }

        // RES 0,D
        case 0x82: {
            regs.D &= ~(1 << 0);
            return 4;
        }

        // RES 0,E
        case 0x83: {
            regs.E &= ~(1 << 0);
            return 4;
        }

        // RES 0,H
        case 0x84: {
            regs.H &= ~(1 << 0);
            return 4;
        }

        // RES 0,L
        case 0x85: {
            regs.L &= ~(1 << 0);
            return 4;
        }

        // RES 0,(HL)
        case 0x86: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 0);
            memory.write(addr, value);
            return 12;
        }

        // RES 0,A
        case 0x87: {
            regs.A &= ~(1 << 0);
            return 4;
        }

        // RES 1,B
        case 0x88: {
            regs.B &= ~(1 << 1);
            return 4;
        }

        // RES 1,C
        case 0x89: {
            regs.C &= ~(1 << 1);
            return 4;
        }

        // RES 1,D
        case 0x8A: {
            regs.D &= ~(1 << 1);
            return 4;
        }

        // RES 1,E
        case 0x8B: {
            regs.E &= ~(1 << 1);
            return 4;
        }

        // RES 1,H
        case 0x8C: {
            regs.H &= ~(1 << 1);
            return 4;
        }

        // RES 1,L
        case 0x8D: {
            regs.L &= ~(1 << 1);
            return 4;
        }

        // RES 1,(HL)
        case 0x8E: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 1);
            memory.write(addr, value);
            return 12;
        }

        // RES 1,A
        case 0x8F: {
            regs.A &= ~(1 << 1);
            return 4;
        }
        // RES 2,B
        case 0x90: {
            regs.B &= ~(1 << 2);
            return 4;
        }

        // RES 2,C
        case 0x91: {
            regs.C &= ~(1 << 2);
            return 4;
        }

        // RES 2,D
        case 0x92: {
            regs.D &= ~(1 << 2);
            return 4;
        }

        // RES 2,E
        case 0x93: {
            regs.E &= ~(1 << 2);
            return 4;
        }

        // RES 2,H
        case 0x94: {
            regs.H &= ~(1 << 2);
            return 4;
        }

        // RES 2,L
        case 0x95: {
            regs.L &= ~(1 << 2);
            return 4;
        }

        // RES 2,(HL)
        case 0x96: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 2);
            memory.write(addr, value);
            return 12;
        }

        // RES 2,A
        case 0x97: {
            regs.A &= ~(1 << 2);
            return 4;
        }

        // RES 3,B
        case 0x98: {
            regs.B &= ~(1 << 3);
            return 4;
        }

        // RES 3,C
        case 0x99: {
            regs.C &= ~(1 << 3);
            return 4;
        }

        // RES 3,D
        case 0x9A: {
            regs.D &= ~(1 << 3);
            return 4;
        }

        // RES 3,E
        case 0x9B: {
            regs.E &= ~(1 << 3);
            return 4;
        }

        // RES 3,H
        case 0x9C: {
            regs.H &= ~(1 << 3);
            return 4;
        }

        // RES 3,L
        case 0x9D: {
            regs.L &= ~(1 << 3);
            return 4;
        }

        // RES 3,(HL)
        case 0x9E: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 3);
            memory.write(addr, value);
            return 12;
        }

        // RES 3,A
        case 0x9F: {
            regs.A &= ~(1 << 3);
            return 4;
        }
        // RES 4,B
        case 0xA0: {
            regs.B &= ~(1 << 4);
            return 4;
        }

        // RES 4,C
        case 0xA1: {
            regs.C &= ~(1 << 4);
            return 4;
        }

        // RES 4,D
        case 0xA2: {
            regs.D &= ~(1 << 4);
            return 4;
        }

        // RES 4,E
        case 0xA3: {
            regs.E &= ~(1 << 4);
            return 4;
        }

        // RES 4,H
        case 0xA4: {
            regs.H &= ~(1 << 4);
            return 4;
        }

        // RES 4,L
        case 0xA5: {
            regs.L &= ~(1 << 4);
            return 4;
        }

        // RES 4,(HL)
        case 0xA6: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 4);
            memory.write(addr, value);
            return 12;
        }

        // RES 4,A
        case 0xA7: {
            regs.A &= ~(1 << 4);
            return 4;
        }

        // RES 5,B
        case 0xA8: {
            regs.B &= ~(1 << 5);
            return 4;
        }

        // RES 5,C
        case 0xA9: {
            regs.C &= ~(1 << 5);
            return 4;
        }

        // RES 5,D
        case 0xAA: {
            regs.D &= ~(1 << 5);
            return 4;
        }

        // RES 5,E
        case 0xAB: {
            regs.E &= ~(1 << 5);
            return 4;
        }

        // RES 5,H
        case 0xAC: {
            regs.H &= ~(1 << 5);
            return 4;
        }

        // RES 5,L
        case 0xAD: {
            regs.L &= ~(1 << 5);
            return 4;
        }

        // RES 5,(HL)
        case 0xAE: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 5);
            memory.write(addr, value);
            return 12;
        }

        // RES 5,A
        case 0xAF: {
            regs.A &= ~(1 << 5);
            return 4;
        }
        // RES 6,B
        case 0xB0: {
            regs.B &= ~(1 << 6);
            return 4;
        }

        // RES 6,C
        case 0xB1: {
            regs.C &= ~(1 << 6);
            return 4;
        }

        // RES 6,D
        case 0xB2: {
            regs.D &= ~(1 << 6);
            return 4;
        }

        // RES 6,E
        case 0xB3: {
            regs.E &= ~(1 << 6);
            return 4;
        }

        // RES 6,H
        case 0xB4: {
            regs.H &= ~(1 << 6);
            return 4;
        }

        // RES 6,L
        case 0xB5: {
            regs.L &= ~(1 << 6);
            return 4;
        }

        // RES 6,(HL)
        case 0xB6: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 6);
            memory.write(addr, value);
            return 12;
        }

        // RES 6,A
        case 0xB7: {
            regs.A &= ~(1 << 6);
            return 4;
        }

        // RES 7,B
        case 0xB8: {
            regs.B &= ~(1 << 7);
            return 4;
        }

        // RES 7,C
        case 0xB9: {
            regs.C &= ~(1 << 7);
            return 4;
        }

        // RES 7,D
        case 0xBA: {
            regs.D &= ~(1 << 7);
            return 4;
        }

        // RES 7,E
        case 0xBB: {
            regs.E &= ~(1 << 7);
            return 4;
        }

        // RES 7,H
        case 0xBC: {
            regs.H &= ~(1 << 7);
            return 4;
        }

        // RES 7,L
        case 0xBD: {
            regs.L &= ~(1 << 7);
            return 4;
        }

        // RES 7,(HL)
        case 0xBE: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value &= ~(1 << 7);
            memory.write(addr, value);
            return 12;
        }

        // RES 7,A
        case 0xBF: {
            regs.A &= ~(1 << 7);
            return 4;
        }
        // SET 0,B
        case 0xC0: {
            regs.B |= (1 << 0);
            return 4;
        }

        // SET 0,C
        case 0xC1: {
            regs.C |= (1 << 0);
            return 4;
        }

        // SET 0,D
        case 0xC2: {
            regs.D |= (1 << 0);
            return 4;
        }

        // SET 0,E
        case 0xC3: {
            regs.E |= (1 << 0);
            return 4;
        }

        // SET 0,H
        case 0xC4: {
            regs.H |= (1 << 0);
            return 4;
        }

        // SET 0,L
        case 0xC5: {
            regs.L |= (1 << 0);
            return 4;
        }

        // SET 0,(HL)
        case 0xC6: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 0);
            memory.write(addr, value);
            return 12;
        }

        // SET 0,A
        case 0xC7: {
            regs.A |= (1 << 0);
            return 4;
        }

        // SET 1,B
        case 0xC8: {
            regs.B |= (1 << 1);
            return 4;
        }

        // SET 1,C
        case 0xC9: {
            regs.C |= (1 << 1);
            return 4;
        }

        // SET 1,D
        case 0xCA: {
            regs.D |= (1 << 1);
            return 4;
        }

        // SET 1,E
        case 0xCB: {
            regs.E |= (1 << 1);
            return 4;
        }

        // SET 1,H
        case 0xCC: {
            regs.H |= (1 << 1);
            return 4;
        }

        // SET 1,L
        case 0xCD: {
            regs.L |= (1 << 1);
            return 4;
        }

        // SET 1,(HL)
        case 0xCE: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 1);
            memory.write(addr, value);
            return 12;
        }

        // SET 1,A
        case 0xCF: {
            regs.A |= (1 << 1);
            return 4;
        }
        // SET 2,B
        case 0xD0: {
            regs.B |= (1 << 2);
            return 4;
        }

        // SET 2,C
        case 0xD1: {
            regs.C |= (1 << 2);
            return 4;
        }

        // SET 2,D
        case 0xD2: {
            regs.D |= (1 << 2);
            return 4;
        }

        // SET 2,E
        case 0xD3: {
            regs.E |= (1 << 2);
            return 4;
        }

        // SET 2,H
        case 0xD4: {
            regs.H |= (1 << 2);
            return 4;
        }

        // SET 2,L
        case 0xD5: {
            regs.L |= (1 << 2);
            return 4;
        }

        // SET 2,(HL)
        case 0xD6: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 2);
            memory.write(addr, value);
            return 12;
        }

        // SET 2,A
        case 0xD7: {
            regs.A |= (1 << 2);
            return 4;
        }

        // SET 3,B
        case 0xD8: {
            regs.B |= (1 << 3);
            return 4;
        }

        // SET 3,C
        case 0xD9: {
            regs.C |= (1 << 3);
            return 4;
        }

        // SET 3,D
        case 0xDA: {
            regs.D |= (1 << 3);
            return 4;
        }

        // SET 3,E
        case 0xDB: {
            regs.E |= (1 << 3);
            return 4;
        }

        // SET 3,H
        case 0xDC: {
            regs.H |= (1 << 3);
            return 4;
        }

        // SET 3,L
        case 0xDD: {
            regs.L |= (1 << 3);
            return 4;
        }

        // SET 3,(HL)
        case 0xDE: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 3);
            memory.write(addr, value);
            return 12;
        }

        // SET 3,A
        case 0xDF: {
            regs.A |= (1 << 3);
            return 4;
        }
        // SET 4,B
        case 0xE0: {
            regs.B |= (1 << 4);
            return 4;
        }

        // SET 4,C
        case 0xE1: {
            regs.C |= (1 << 4);
            return 4;
        }

        // SET 4,D
        case 0xE2: {
            regs.D |= (1 << 4);
            return 4;
        }

        // SET 4,E
        case 0xE3: {
            regs.E |= (1 << 4);
            return 4;
        }

        // SET 4,H
        case 0xE4: {
            regs.H |= (1 << 4);
            return 4;
        }

        // SET 4,L
        case 0xE5: {
            regs.L |= (1 << 4);
            return 4;
        }

        // SET 4,(HL)
        case 0xE6: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 4);
            memory.write(addr, value);
            return 12;
        }

        // SET 4,A
        case 0xE7: {
            regs.A |= (1 << 4);
            return 4;
        }

        // SET 5,B
        case 0xE8: {
            regs.B |= (1 << 5);
            return 4;
        }

        // SET 5,C
        case 0xE9: {
            regs.C |= (1 << 5);
            return 4;
        }

        // SET 5,D
        case 0xEA: {
            regs.D |= (1 << 5);
            return 4;
        }

        // SET 5,E
        case 0xEB: {
            regs.E |= (1 << 5);
            return 4;
        }

        // SET 5,H
        case 0xEC: {
            regs.H |= (1 << 5);
            return 4;
        }

        // SET 5,L
        case 0xED: {
            regs.L |= (1 << 5);
            return 4;
        }

        // SET 5,(HL)
        case 0xEE: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 5);
            memory.write(addr, value);
            return 12;
        }

        // SET 5,A
        case 0xEF: {
            regs.A |= (1 << 5);
            return 4;
        }
        // SET 6,B
        case 0xF0: {
            regs.B |= (1 << 6);
            return 4;
        }

        // SET 6,C
        case 0xF1: {
            regs.C |= (1 << 6);
            return 4;
        }

        // SET 6,D
        case 0xF2: {
            regs.D |= (1 << 6);
            return 4;
        }

        // SET 6,E
        case 0xF3: {
            regs.E |= (1 << 6);
            return 4;
        }

        // SET 6,H
        case 0xF4: {
            regs.H |= (1 << 6);
            return 4;
        }

        // SET 6,L
        case 0xF5: {
            regs.L |= (1 << 6);
            return 4;
        }

        // SET 6,(HL)
        case 0xF6: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 6);
            memory.write(addr, value);
            return 12;
        }

        // SET 6,A
        case 0xF7: {
            regs.A |= (1 << 6);
            return 4;
        }

        // SET 7,B
        case 0xF8: {
            regs.B |= (1 << 7);
            return 4;
        }

        // SET 7,C
        case 0xF9: {
            regs.C |= (1 << 7);
            return 4;
        }

        // SET 7,D
        case 0xFA: {
            regs.D |= (1 << 7);
            return 4;
        }

        // SET 7,E
        case 0xFB: {
            regs.E |= (1 << 7);
            return 4;
        }

        // SET 7,H
        case 0xFC: {
            regs.H |= (1 << 7);
            return 4;
        }

        // SET 7,L
        case 0xFD: {
            regs.L |= (1 << 7);
            return 4;
        }

        // SET 7,(HL)
        case 0xFE: {
            uint16_t addr = unsigned_16(regs.H, regs.L);
            uint8_t value = memory.read(addr);
            value |= (1 << 7);
            memory.write(addr, value);
            return 12;
        }

        // SET 7,A
        case 0xFF: {
            regs.A |= (1 << 7);
            return 4;
        }
    }
}

uint16_t CPU::unsigned_16(uint8_t r1, uint8_t r2) {
    // Type cast to (uint16_t) since the << operator turns the value to a regular integer
    return ((uint16_t)r1 << 8 | r2);
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
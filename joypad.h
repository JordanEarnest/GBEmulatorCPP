#pragma once
#include <SDL2/SDL.h>

class Memory; // forward declaration

class Joypad {
private:

    // 1 == pressed, 0 == not pressed
    bool A, B, Start, Select;
    bool Up, Down, Left, Right;

    // Stores bits 4 and 5 of value at 0xFF00 (joypad register)
    // Bit 4 = D-Pad
    // Bit 5 = Buttons
    uint8_t select;

    Memory* memory;

public: 
    void connectToMemory(Memory* memory);

    uint8_t read();
    void write(uint8_t value);

    void setA(bool pressed);
    void setB(bool pressed);
    void setStart(bool pressed);
    void setSelect(bool pressed);

    void setUp(bool pressed);
    void setDown(bool pressed);
    void setLeft(bool pressed);
    void setRight(bool pressed);

    // Sets IF to correct bit for Joypad Interrupt, later to be handled by CPU
    void requestJoypadInterrupt();
    
    void update(const Uint8* state);
};

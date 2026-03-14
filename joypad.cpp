#include "memory.h"
#include "joypad.h"

void Joypad::connectToMemory(Memory* memory) {
    this->memory = memory;
}

void Joypad::write(uint8_t value) {
    select = value & 0x30; // Masks to only get bits 4 and 5
}

uint8_t Joypad::read() {
    uint8_t result = 0xFF;

    result &= 0xCF | select;

    // buttons selected
    if (!(select & 0x20)) {
        if (A)
            result &= ~0x01;
        if (B)
            result &= ~0x02;
        if (Select)
            result &= ~0x04;
        if (Start)
            result &= ~0x08;
    }

    // dpad selected
    if (!(select & 0x10)) {
        if (Right)
            result &= ~0x01;
        if (Left)
            result &= ~0x02;
        if (Up)
            result &= ~0x04;
        if (Down)
            result &= ~0x08;
    }
    return result;
}

void Joypad::update(const Uint8* state) {
   // get any updates or changes in buttons
   bool newA = state[SDL_SCANCODE_Z];
   bool newB = state[SDL_SCANCODE_X];
   bool newStart = state[SDL_SCANCODE_RETURN];
   bool newSelect = state[SDL_SCANCODE_RSHIFT];

   bool newUp = state[SDL_SCANCODE_UP];
   bool newDown = state[SDL_SCANCODE_DOWN];
   bool newLeft = state[SDL_SCANCODE_LEFT];
   bool newRight = state[SDL_SCANCODE_RIGHT];

   // compare previous and current buttons to see any change, if so interrupt = true
   bool interrupt = (!A && newA) || (!B && newB) || (!Start && newStart) || (!Select && newSelect) || (!Up && newUp) || (!Down && newDown) || (!Left && newLeft) || (!Right && newRight);

    if (interrupt)
        requestJoypadInterrupt(); // set IF to 0x10, CPU handles rest
                                  
    A = newA;
    B = newB;
    Start = newStart;
    Select = newSelect;
    Up = newUp;
    Down = newDown;
    Left = newLeft;
    Right = newRight;
}

void Joypad::requestJoypadInterrupt() {
    uint8_t IF = memory->read(0xFF0F);
    IF |= 0x10; 
    memory->write(0xFF0F, IF);
} 

void Joypad::setA(bool pressed) {
    A = pressed;
}
void Joypad::setB(bool pressed) {
    B = pressed;
}
void Joypad::setStart(bool pressed) {
    Start = pressed;
}
void Joypad::setSelect(bool pressed) {
    Select = pressed;
}

void Joypad::setUp(bool pressed) {
    Up = pressed;
}
void Joypad::setDown(bool pressed) {
    Down = pressed;
}
void Joypad::setLeft(bool pressed) {
    Left = pressed;
}
void Joypad::setRight(bool pressed) {
    Right = pressed;
}


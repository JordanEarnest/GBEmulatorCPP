#pragma once
#include "memory.h"
#include <SDL2/SDL.h>

class Memory;

struct PulseSweepChannel {
    bool enabled;

    uint8_t sweep;

    uint8_t timerAndDuty;

    uint8_t volumeAndEnvelope;

    uint8_t periodLow; 

    uint8_t periodHighAndControl;

    void clearChannelRegisters();
};

struct PulseChannel {
    bool enabled;

    int lengthCounter;
    bool lengthEnabled;

    int intVolume;
    int envelopeTimer;
    int envelopePeriod;
    bool envelopeIncrease;

    uint8_t volume;
    uint8_t dutyPosition;

    uint16_t timer;

    uint8_t timerAndDuty;

    uint8_t volumeAndEnvelope;

    uint8_t periodLow;

    uint8_t periodHighAndControl;

    void clearChannelRegisters();
    void step(int cycles);
    void clockLength();
    void clockEnvelope();

    uint8_t getWaveDuty();
    uint8_t getInitialVolume();
    uint8_t getEnvelopeDirection();
    uint8_t getSweepPace();
    uint8_t getPeriodHigh();
    uint8_t getLengthEnable();
};


class APU {
    private:
        Memory* memory;

        // Global Control Registers
        
        // Audio Master Control
        uint8_t audioMasterControl; // 0xFF26
                        
        // Sound Panning
        uint8_t soundPanning; // 0xFF25

        // Master Volume
        uint8_t masterVolume; // 0xFF24
        
        // 4 Sound Channels of APU
        PulseSweepChannel ch1;
        PulseChannel ch2;
        
        double sampleClock;
        int frameClock;
        uint8_t frameStep;
        const double CYCLES_PER_SAMPLE = 4194304.0 / 48000.0;


        static constexpr int dutyTable[4][8] = {
            {0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,1},
            {1,0,0,0,0,1,1,1},
            {0,1,1,1,1,1,1,0}
        };

        float audioBuffer[8192];
        int bufferIndex = 0;

        float generateSample();
        
        void updateChannelRegisters();

        SDL_AudioDeviceID device;


    public:
        APU();

        uint8_t read();
        
        void connectToMemory(Memory* memory);

        void write(uint16_t address, uint8_t value);

        void step(int cycles);
        
        void updateGlobalControlRegisters();
        void clearGlobalControlRegisters();

        int sampleChannel2();
        void triggerChannel2();

        // Audio Master Control: Setters and Getters
        bool audioEnabled();
        void setAudioEnabled(bool value);
        
        // Master Volume: Getters
        uint8_t getRightVolume();
        uint8_t getLeftVolume();

        float* getAudioBuffer();
};

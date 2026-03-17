#include "apu.h"
#include <SDL2/SDL.h>

APU::APU() {
    SDL_AudioSpec spec{};
    spec.freq = 48000;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = 2048;
    spec.callback = nullptr;   // IMPORTANT

    device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);

    SDL_PauseAudioDevice(device, 0);

    frameStep = 0;
    sampleClock = 0;
    frameClock = 0;
    audioMasterControl = 0x80;
}

void APU::connectToMemory(Memory* memory) {
    this->memory = memory;
}

void APU::step(int cycles) { 
    //updateChannelRegisters();
    // advance channel waveforms
    sampleClock += cycles;
    frameClock += cycles;
    
    while (frameClock >= 8192) {
        frameClock -= 8192;
        frameStep = (frameStep + 1) & 7;

        if (frameStep % 2 == 0)
            ch2.clockLength();

        if (frameStep == 7)
            ch2.clockEnvelope();

    }

    ch2.step(cycles);   

    while (sampleClock >= CYCLES_PER_SAMPLE) {
        sampleClock -= CYCLES_PER_SAMPLE;

        float sample = generateSample();

        audioBuffer[bufferIndex++] = sample;

        if (bufferIndex >= 8192) {

            if (SDL_GetQueuedAudioSize(device) < 16384) {

                SDL_QueueAudio(device, audioBuffer, sizeof(audioBuffer));

            }

            bufferIndex = 0;
        }
    }
}

float APU::generateSample() {
    int s2 = sampleChannel2();

    int mix = s2;

    return mix / 15.0f;
}

// Memory calls this, APU handles its own registers
void APU::write(uint16_t address, uint8_t value) {
    // TODO: put in memory.cpp, make sure to write the value to memory first! then call this
    switch (address) {
        // Channel 1
        case 0xFF10: ch1.sweep = value; break;
        case 0xFF11: ch1.timerAndDuty = value; break;
        case 0xFF12: ch1.volumeAndEnvelope = value; break;
        case 0xFF13: ch1.periodLow = value; break;
        case 0xFF14: 
            ch1.periodHighAndControl = value;
            break;
        
        
        // Channel 2
        case 0xFF16: ch2.timerAndDuty = value; break;
        case 0xFF17: 
            ch2.volumeAndEnvelope = value;

            ch2.volume = (value >> 4) & 0x0F;
            ch2.envelopeIncrease = (value & 0x08) != 0;
            ch2.envelopePeriod = value & 0x07;

            break;
        case 0xFF18: ch2.periodLow = value; break;
        case 0xFF19: 
            ch2.periodHighAndControl = value;
            ch2.lengthEnabled = value & 0x40; 
            if (value & 0x80) {
                triggerChannel2();
            }
            break;
        // TODO: Add rest of the channels 3 and 4
        
        // Master Volume / Panning / APU Power
        case 0xFF24: masterVolume = value; break;
        case 0xFF25: soundPanning = value; break;
        case 0xFF26: setAudioEnabled(value); break;
        default: break;
    }
}

uint8_t APU::read() {
    // Only handles the special read from 0xFF26
    uint8_t result = 0;

    result |= audioEnabled() ? 0x80 : 0;
    result |= 0x70; // bits 4-6 always read as 1
    
    //if (ch1.enabled) result |= 1;
    if (ch2.enabled) result |= 2;
    // TODO: add the rest of channels
    return result;
}

void APU::updateGlobalControlRegisters() {
     soundPanning = memory->read(0xFF25);
     masterVolume = memory->read(0xFF24);
}
void APU::clearGlobalControlRegisters() {
    soundPanning = 0;
    masterVolume = 0;
}

int APU::sampleChannel2() {
    if (!ch2.enabled)
        return 0;
    uint8_t duty = (ch2.timerAndDuty >> 6) & 0x03;
    int bit = dutyTable[duty][ch2.dutyPosition];
    return bit ? ch2.volume : 0;
}

void APU::triggerChannel2() {
    ch2.enabled = true;
    ch2.lengthCounter = 64 - (ch2.timerAndDuty & 0x3F);
    ch2.envelopeIncrease = (ch2.volumeAndEnvelope & 0x08) != 0;
    ch2.envelopePeriod = ch2.volumeAndEnvelope & 0x07;
    ch2.envelopeTimer = ch2.envelopePeriod;

    ch2.volume = (ch2.volumeAndEnvelope >> 4) & 0xF;
    ch2.dutyPosition = 0;
    uint16_t freq = ((ch2.periodHighAndControl & 0x07) << 8) | ch2.periodLow;
    ch2.timer = (2048 - freq) * 4;
}

float* APU::getAudioBuffer() {
    return audioBuffer;
}

// Audio Master Control: Setters and Getters
bool APU::audioEnabled() {
    return (audioMasterControl & 0x80) != 0;
}
void APU::setAudioEnabled(bool value) {
    // Disabling APU
    if (!value) {
        clearGlobalControlRegisters();
        ch1.clearChannelRegisters();
        ch2.clearChannelRegisters();
        audioMasterControl &= ~0x80;
    }
    // Enabling APU
    else {
        audioMasterControl |= 0x80;
    }
}

// Master Volume: Getters
uint8_t APU::getRightVolume() {
    return masterVolume & 0x07;
}
uint8_t APU::getLeftVolume() {
    return (masterVolume >> 4) & 0x07;
}

void APU::updateChannelRegisters() {
    ch1.sweep = memory->read(0xFF10);
    ch1.timerAndDuty = memory->read(0xFF11);
    ch1.volumeAndEnvelope = memory->read(0xFF12);
    ch1.periodLow = memory->read(0xFF13);
    ch1.periodHighAndControl = memory->read(0xFF14);


    ch2.timerAndDuty = memory->read(0xFF16);
    ch2.volumeAndEnvelope = memory->read(0xFF17);
    ch2.periodLow = memory->read(0xFF18);
    ch2.periodHighAndControl = memory->read(0xFF19);
}




// Pulse Channel with Sweep Functionality
void PulseSweepChannel::clearChannelRegisters() {
    sweep = 0;
    timerAndDuty = 0;
    volumeAndEnvelope = 0;
    periodLow = 0;
    periodHighAndControl = 0;
}

// Pulse Channel
void PulseChannel::clearChannelRegisters() {
    timerAndDuty = 0;
    volumeAndEnvelope = 0;
    periodLow = 0;
    periodHighAndControl = 0;
}
void PulseChannel::step(int cycles) {
    if (!enabled)
        return;
    timer -= cycles;

    while (timer <= 0) {
        uint16_t freq = ((periodHighAndControl & 0x07) << 8) | periodLow;

        timer += (2048 - freq) * 4;

        dutyPosition = (dutyPosition + 1) & 7;
    }
}
void PulseChannel::clockLength() {
    if (lengthEnabled && lengthCounter > 0) {
        lengthCounter--;

        if (lengthCounter == 0) {
            enabled = false;
        }
    }   
}
void PulseChannel::clockEnvelope() {
    if (!enabled)
        return;

    if (envelopePeriod == 0)
        return;

    envelopeTimer--;

    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod;

        if (envelopeIncrease) {
            if (volume < 15)
                volume++;
        } else {
            if (volume > 0)
                volume--;
        }
    } 
}

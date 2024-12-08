#pragma once

#include <satemu/core_types.hpp>

namespace satemu::scsp {

struct Slot {
    Slot() {
        Reset();
    }

    void Reset() {
        startAddress = 0;
        loopStartAddress = 0;
        loopEndAddress = 0;
        keyOnBit = false;

        keyOn = false;
    }

    // -------------------------------------------------------------------------
    // Registers

    // Loop Control Register
    uint32 startAddress;     // (R/W) SA
    uint32 loopStartAddress; // (R/W) LSA
    uint32 loopEndAddress;   // (R/W) LEA
    bool keyOnBit;           // (R/W) KYONB

    // Envelope Generator Register

    // FM Modulation Control Register

    // Sound Volume Register

    // Pitch Register

    // LFO Register

    // Mixer Register

    // Slot Status Register

    // Sound Memory Configuration Register

    // MIDI Register

    // Timer Register

    // Interrupt Control Register

    // DMA Transfer Register

    // -------------------------------------------------------------------------
    // State

    // KYONEX is a write-only register that applies the value of keyOnBit to this variable.
    // If changed, triggers the new state (e.g. begin ADSR attack phase on key ON or release phase on key OFF).
    bool keyOn; // current key on state
};

} // namespace satemu::scsp

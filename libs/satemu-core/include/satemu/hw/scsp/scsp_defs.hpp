#pragma once

#include <satemu/core_types.hpp>
#include <satemu/hw/hw_defs.hpp>

#include <array>

namespace satemu::scsp {

struct Slot {
    Slot() {
        Reset();
    }

    void Reset() {
        startAddress = 0;
        loopStartAddress = 0;
        loopEndAddress = 0;
        pcm8Bit = false;
        keyOnBit = false;
        loopControl = LoopControl::Off;
        sampleXOR = 0x0000;
        soundSource = SoundSource::SoundRAM;

        attackRate = 0;
        decay1Rate = 0;
        decay2Rate = 0;
        releaseRate = 0;
        decayLevel = 0;
        keyRateScaling = 0;
        loopStateLink = false;
        egHold = false;

        keyOn = false;
    }

    template <typename T, bool fromM68K>
    T ReadReg(uint32 address) {
        // TODO: implement similar to SCSP::ReadReg
        return 0;
    }

    template <typename T, bool fromM68K>
    void WriteReg(uint32 address, T value) {
        // TODO: implement similar to SCSP::WriteReg
    }

    // -------------------------------------------------------------------------
    // Registers

    // Loop Control Register
    uint32 startAddress;     // (R/W) SA
    uint32 loopStartAddress; // (R/W) LSA
    uint32 loopEndAddress;   // (R/W) LEA
    bool pcm8Bit;            // (R/W) PCM8B
    bool keyOnBit;           // (R/W) KYONB

    // Loop control specifies how the loop segment is played if the sound is held continuously.
    // All modes play the segment between SA and LSA forwards.
    //   Off disables sample looping. The sample stops at LEA.
    //   Normal loops the segment between LSA and LEA forwards.
    //   Reverse plays forwards from SA to LSA, then jumps to LEA and repeats the loop segment in reverse.
    //   Alternate plays the loop segment forwards, then backwards, then forwards, ...
    //
    //            SA     LSA         LEA
    //            |       |           |
    //       Off  +--->---+--->-------X   sample stops playing at LEA
    //            |       |           |
    //    Normal  +--->---+--->------->   sample repeats from LSA when it hits
    //            |       +--->------->   LEA and always plays forwards
    //            |       +--->------->
    //            |       |           |
    //   Reverse  +--->--->  >  >  >  |   sample skips LSA, plays backwards
    //            |       <-------<---+   from LEA and repeats from LEA upon
    //            |       <-------<---+   reaching LSA; always plays in reverse
    //            |       |           |
    // Alternate  +--->---+--->------->   sample plays forwards until LEA
    //            |       <-------<---+   then plays backwards until LSA,
    //            |       +--->------->   and keeps bouncing back and forth
    enum class LoopControl { Off, Normal, Reverse, Alternate };
    LoopControl loopControl; // (R/W) LPCTL

    // SBCTL enables XORing sample data.
    //   bit 0 flips every bit other than the sign bit
    //   bit 1 flips the sign bit
    // This is useful for supporting samples in different formats (e.g. unsigned)
    uint16 sampleXOR; // (R/W) SBCTL0/1
    // Implementation notes:
    //   SBCTL0: 0x7FFF
    //   SBCTL1: 0x8000
    // Make a simple LUT: 0x0000, 0x7FFF, 0x8000, 0xFFFF
    // To read back the value, simply look at bits 14 and 15.

    enum class SoundSource { SoundRAM, Noise, Silence, Unknown };
    SoundSource soundSource; // (R/W) SSCTL

    // Envelope Generator Register
    //
    // States: Attack, Decay 1, Decay 2, Release
    //
    // Starts from Attack on Key ON.
    // While Key ON is held, goes through Attack -> Decay 1 -> Decay 2 and stays at the minimum value of Decay 2.
    // On Key OFF, it will immediately skip to Release state, decrementing the envelope from whatever point it was.
    //
    // Values range from 0x3FF (minimum) to 0x000 (maximum) - 10 bits.

    // Value ranges are from minimum to maximum.
    uint8 attackRate;     // (R/W) AR  - 0x00 to 0x1F
    uint8 decay1Rate;     // (R/W) D1R - 0x00 to 0x1F
    uint8 decay2Rate;     // (R/W) D2R - 0x00 to 0x1F
    uint8 releaseRate;    // (R/W) RR  - 0x00 to 0x1F
    uint8 decayLevel;     // (R/W) DL  - 0x1F to 0x00
                          //   specifies the MSB 5 bits of the EG value where to switch from decay 1 to decay 2
    uint8 keyRateScaling; // (R/W) KRS - 0x00 to 0x0E; 0x0F turns off scaling
    bool loopStateLink;   // (R/W) LPSLNK
                          //   true:  switches to decay 1 state on LSA
                          //          attack state is interrupted if too slow or held if too fast
                          //          if the state change happens below DL, decay 2 state is never reached
                          //   false: state changes are dictated by rates only
    bool egHold;          // (R/W) EGHOLD
                          //   true:  volume raises during attack state
                          //   false: volume is set to maximum during attack phase while maintaining the same duration

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

    // KYONEX is a write-only register that applies the value of keyOnBit (KYONB) to this variable.
    // If changed, triggers the new state (e.g. begin ADSR attack phase on key ON or release phase on key OFF).
    // Writing 1 to KYONEX on any slot will apply KYONB on every slot.
    bool keyOn; // current key on state
};

} // namespace satemu::scsp

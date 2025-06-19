#pragma once

#include <ymir/core/types.hpp>

namespace ymir::scsp {

// Audio sampling rate in Hz
inline constexpr uint64 kAudioFreq = 44100;

// Number of SCSP cycles per sample
inline constexpr uint64 kCyclesPerSample = 512;

// Number of SCSP cycles per MC68EC000 cycle
inline constexpr uint64 kCyclesPerM68KCycle = 2;

// Number of M68K cycles per sample
inline constexpr uint64 kM68KCyclesPerSample = kCyclesPerSample / kCyclesPerM68KCycle;

// SCSP clock frequency: 22,579,200 Hz = 44,100 Hz * 512 cycles per sample
inline constexpr uint64 kClockFreq = kAudioFreq * kCyclesPerSample;

// Maximum length of time MIDI events can be scheduled ahead (0.25s)
inline constexpr uint64 kMaxMidiScheduleTime = kAudioFreq / 4;

// Constant value added to MIDI schedule time to keep events ahead of real time
inline constexpr uint64 kMidiAheadTime = 1024;

// Pending interrupt flags
inline constexpr uint16 kIntrINT0N = 0;          // External INT0N line
inline constexpr uint16 kIntrINT1N = 1;          // External INT1N line
inline constexpr uint16 kIntrINT2N = 2;          // External INT2N line
inline constexpr uint16 kIntrMIDIInput = 3;      // MIDI input non-empty
inline constexpr uint16 kIntrDMATransferEnd = 4; // DMA transfer end
inline constexpr uint16 kIntrCPUManual = 5;      // CPU manual interrupt request
inline constexpr uint16 kIntrTimerA = 6;         // Timer A
inline constexpr uint16 kIntrTimerB = 7;         // Timer B
inline constexpr uint16 kIntrTimerC = 8;         // Timer C
inline constexpr uint16 kIntrMIDIOutput = 9;     // MIDI output empty
inline constexpr uint16 kIntrSample = 10;        // Once every sample tick

} // namespace ymir::scsp

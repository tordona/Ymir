#pragma once

#include <satemu/core_types.hpp>

namespace satemu::cdblock {

// HIRQ flags
inline constexpr uint16 kHIRQ_CMOK = 0x0001; // Ready to receive command
inline constexpr uint16 kHIRQ_DRDY = 0x0002; // Ready to transfer data
inline constexpr uint16 kHIRQ_CSCT = 0x0004; // Sector read finished
inline constexpr uint16 kHIRQ_BFUL = 0x0008; // CD buffer full
inline constexpr uint16 kHIRQ_PEND = 0x0010; // CD playback stopped
inline constexpr uint16 kHIRQ_DCHG = 0x0020; // Disc changed
inline constexpr uint16 kHIRQ_ESEL = 0x0040; // Selector processing finished
inline constexpr uint16 kHIRQ_EHST = 0x0080; // Host I/O processing finished
inline constexpr uint16 kHIRQ_ECPY = 0x0100; // Copy or move finished
inline constexpr uint16 kHIRQ_EFLS = 0x0200; // Filesystem processing finished
inline constexpr uint16 kHIRQ_SCDQ = 0x0400; // Subcode Q updated
inline constexpr uint16 kHIRQ_MPED = 0x0800; // MPEG processing finished
inline constexpr uint16 kHIRQ_MPCM = 0x1000; // Long-running MPEG operation finished
inline constexpr uint16 kHIRQ_MPST = 0x2000; // MPEG interrupt raised

// Status codes
inline constexpr uint8 kStatusCodeBusy = 0x00;
inline constexpr uint8 kStatusCodePause = 0x01;
inline constexpr uint8 kStatusCodeStandby = 0x02;
inline constexpr uint8 kStatusCodePlay = 0x03;
inline constexpr uint8 kStatusCodeSeek = 0x04;
inline constexpr uint8 kStatusCodeScan = 0x05;
inline constexpr uint8 kStatusCodeOpen = 0x06;
inline constexpr uint8 kStatusCodeNoDisc = 0x07;
inline constexpr uint8 kStatusCodeRetry = 0x08;
inline constexpr uint8 kStatusCodeError = 0x09;
inline constexpr uint8 kStatusCodeFatal = 0x0A;

inline constexpr uint8 kStatusFlagPeriodic = 0x20;
inline constexpr uint8 kStatusFlagXferRequest = 0x40;
inline constexpr uint8 kStatusFlagWait = 0x80;

inline constexpr uint8 kStatusReject = 0xFF;

// Drive parameters
inline constexpr uint16 kMinStandbyTime = 60;
inline constexpr uint16 kMaxStandbyTime = 900;

inline constexpr uint32 kCyclesPerSecond = 20000000;

// Periodic response intervals:
// - Not playing:         16.667ms =  60 Hz = 1000000/3 (333333.333) cycles @ 20 MHz = once per video frame
// - Playing at 1x speed: 13.333ms =  75 Hz =  800000/3 (266666.667) cycles @ 20 MHz = once per CD frame
// - Playing at 2x speed:  6.667ms = 150 Hz =  400000/3 (133333.333) cycles @ 20 MHz = once per CD frame

// Constants for periodic report cycles are tripled to avoid rounding.
// Cycle counting must be tripled to account for that.
// 2x cycles can be easily derived from 1x cycles.

inline constexpr uint32 kDriveCyclesNotPlaying = 1000000;
inline constexpr uint32 kDriveCyclesPlaying1x = 800000;

} // namespace satemu::cdblock

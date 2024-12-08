#pragma once

#include <satemu/core_types.hpp>

namespace satemu::cdblock {

// HIRQ flags
constexpr uint16 kHIRQ_CMOK = 0x0001; // Ready to receive command
constexpr uint16 kHIRQ_DRDY = 0x0002; // Ready to transfer data
constexpr uint16 kHIRQ_CSCT = 0x0004; // Sector read finished
constexpr uint16 kHIRQ_BFUL = 0x0008; // CD buffer full
constexpr uint16 kHIRQ_PEND = 0x0010; // CD playback stopped
constexpr uint16 kHIRQ_DCHG = 0x0020; // Disc changed
constexpr uint16 kHIRQ_ESEL = 0x0040; // Selector processing finished
constexpr uint16 kHIRQ_EHST = 0x0080; // Host I/O processing finished
constexpr uint16 kHIRQ_ECPY = 0x0100; // Copy or move finished
constexpr uint16 kHIRQ_EFLS = 0x0200; // Filesystem processing finished
constexpr uint16 kHIRQ_SCDQ = 0x0400; // Subcode Q updated
constexpr uint16 kHIRQ_MPED = 0x0800; // MPEG processing finished
constexpr uint16 kHIRQ_MPCM = 0x1000; // Long-running MPEG operation finished
constexpr uint16 kHIRQ_MPST = 0x2000; // MPEG interrupt raised

// Status codes
constexpr uint8 kStatusBusy = 0x00;
constexpr uint8 kStatusPause = 0x01;
constexpr uint8 kStatusStandby = 0x02;
constexpr uint8 kStatusPlay = 0x03;
constexpr uint8 kStatusSeek = 0x04;
constexpr uint8 kStatusScan = 0x05;
constexpr uint8 kStatusOpen = 0x06;
constexpr uint8 kStatusNoDisc = 0x07;
constexpr uint8 kStatusRetry = 0x08;
constexpr uint8 kStatusError = 0x09;
constexpr uint8 kStatusFatal = 0x0A;
constexpr uint8 kStatusPeriodic = 0x20;
constexpr uint8 kStatusXferRequest = 0x40;
constexpr uint8 kStatusWait = 0x80;
constexpr uint8 kStatusReject = 0xFF;

} // namespace satemu::cdblock

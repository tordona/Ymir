#pragma once

#include <satemu/core_types.hpp>

namespace satemu::cdblock {

// HIRQ flags
constexpr uint16 kHIRQ_CMOK = 1u << 0u;  // Ready to receive command
constexpr uint16 kHIRQ_DRDY = 1u << 1u;  // Ready to transfer data
constexpr uint16 kHIRQ_CSCT = 1u << 2u;  // Sector read finished
constexpr uint16 kHIRQ_BFUL = 1u << 3u;  // CD buffer full
constexpr uint16 kHIRQ_PEND = 1u << 4u;  // CD playback stopped
constexpr uint16 kHIRQ_DCHG = 1u << 5u;  // Disc changed
constexpr uint16 kHIRQ_ESEL = 1u << 6u;  // Selector processing finished
constexpr uint16 kHIRQ_EHST = 1u << 7u;  // Host I/O processing finished
constexpr uint16 kHIRQ_ECPY = 1u << 8u;  // Copy or move finished
constexpr uint16 kHIRQ_EFLS = 1u << 9u;  // Filesystem processing finished
constexpr uint16 kHIRQ_SCDQ = 1u << 10u; // Subcode Q updated
constexpr uint16 kHIRQ_MPED = 1u << 11u; // MPEG processing finished
constexpr uint16 kHIRQ_MPCM = 1u << 12u; // Long-running MPEG operation finished
constexpr uint16 kHIRQ_MPST = 1u << 13u; // MPEG interrupt raised

} // namespace satemu::cdblock

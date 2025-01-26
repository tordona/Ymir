#pragma once

#include <satemu/core_types.hpp>

#include <array>

namespace satemu::scsp {

union DSPInstr {
    uint64 u64;
    uint16 u16[4];
    struct {
        uint64 NXADDR : 1; // 0
        uint64 ADRGB : 1;  // 1
        uint64 MASA : 5;   // 2-6
        uint64 : 1;        // 7
        uint64 NOFL : 1;   // 8
        uint64 CRA : 6;    // 9-14
        uint64 : 1;        // 15
        uint64 BSEL : 1;   // 16
        uint64 ZERO : 1;   // 17
        uint64 NEGB : 1;   // 18
        uint64 YRL : 1;    // 19
        uint64 SHFT0 : 1;  // 20
        uint64 SHFT1 : 1;  // 21
        uint64 FRCL : 1;   // 22
        uint64 ADRL : 1;   // 23
        uint64 EWA : 3;    // 24-27
        uint64 EWT : 1;    // 28
        uint64 MRT : 1;    // 29
        uint64 MWT : 1;    // 30
        uint64 TABLE : 1;  // 31
        uint64 IWA : 5;    // 32-36
        uint64 IWT : 1;    // 37
        uint64 IRA : 6;    // 38-43
        uint64 : 1;        // 44
        uint64 YSEL : 2;   // 45-46
        uint64 TWT : 1;    // 55
        uint64 TRA : 7;    // 56-62
        uint64 : 2;        // 63-64
    };
};

class DSP {
public:
    DSP();

    void Reset();

    void Run();

    alignas(16) std::array<DSPInstr, 128> program; // (60-bit) MPRO - DSP program RAM
    alignas(16) std::array<uint32, 256> temp;      // (24-bit) TEMP - DSP temporary (universal) RAM
    alignas(16) std::array<uint32, 64> soundMem;   // (24-bit) SMEM - DSP sound memory
    alignas(16) std::array<uint16, 64> coeffs;     // (13-bit) COEF - DSP coefficient data RAM
    alignas(16) std::array<uint16, 32> addrs;      // (16-bit) MADRS - DSP memory address registers
    alignas(16) std::array<sint32, 16> mixStack;   // (20-bit) MIXS - DSP mix sound slot data stack (4 frac bits)
    alignas(16) std::array<sint16, 16> effectOut;  // (16-bit) EFREG - DSP effected data output
    alignas(16) std::array<sint16, 2> audioIn;     // (16-bit) EXTS - DSP digital audio input

    uint32 ringBufferLeadAddress; // (W) RBP - DSP Ring Buffer Lead Address
    uint8 ringBufferLength;       // (W) RBL - DSP Ring Buffer Length
};

} // namespace satemu::scsp

#pragma once

#include <satemu/core_types.hpp>

#include <array>
#include <iosfwd>

namespace satemu::scsp {

union DSPInstr {
    uint64 u64;
    uint16 u16[4];
    struct {
        uint64 NXADDR : 1; //     0 - Increments the memory address by one
        uint64 ADREB : 1;  //     1 - 0=Gate the output of the address register (ADRS_REG), and make the output 0
        uint64 MASA : 5;   //   2-6 - MADRS read address
        uint64 : 1;        //     7 - (unused)
        uint64 NOFL : 1;   //     8 - 1=Do not perform a floating conversion for wave memory access
        uint64 CRA : 6;    //  9-14 - COEF read address
        uint64 : 1;        //    15 - (unused)
        uint64 BSEL : 1;   //    16 - 0=TEMP data select; 1=accumulator select
        uint64 ZERO : 1;   //    17 - 1=Assume the adder input as 0
        uint64 NEGB : 1;   //    18 - 0=addition; 1=subtraction
        uint64 YRL : 1;    //    19 - Latches INPUTS[23:4]
        uint64 SHFT0 : 1;  //    20 - Shifter control 0
        uint64 SHFT1 : 1;  //    21 - Shifter control 1
        uint64 FRCL : 1;   //    22 - Memory address decimal latch
        uint64 ADRL : 1;   //    23 - Memory address integer latch
        uint64 EWA : 4;    // 24-27 - Output EFREG address
        uint64 EWT : 1;    //    28 - Request to write output data to EFREG
        uint64 MRD : 1;    //    29 - Wave memory read request
        uint64 MWT : 1;    //    30 - Write request to wave memory
        uint64 TABLE : 1;  //    31 - 1=Gate the output of the decrement counter (MDEC_CT), and make the output 0
        uint64 IWA : 5;    // 32-36 - Write address for the input data (INPUTS)
        uint64 IWT : 1;    //    37 - DSP input data write request
        uint64 IRA : 6;    // 38-43 - Read address for the input data (INPUTS)
        uint64 : 1;        //    44 - (unused)
        uint64 YSEL : 2;   // 45-46 - Multiplier Y input select (0=FRC_REG, 1=COEF, 2=Y_REG[23:11], 3=0|Y_REG[15:4])
        uint64 XSEL : 1;   //    47 - Multiplier X input select (0=TEMP data select, 1=INPUTS data select)
        uint64 TWA : 7;    // 48-54 - TEMP write address
        uint64 TWT : 1;    //    55 - TEMP input data write request
        uint64 TRA : 7;    // 56-62 - TEMP read address
        uint64 : 1;        //    63 - (unused)
    };
};
static_assert(sizeof(DSPInstr) == sizeof(uint64));

class DSP {
public:
    DSP(uint8 *ram);

    void Reset();

    void Run();

    void DumpRegs(std::ostream &out) const;

    // -------------------------------------------------------------------------
    // Registers

    alignas(16) std::array<DSPInstr, 128> program; // (60-bit) MPRO - DSP program RAM
    alignas(16) std::array<uint32, 128> tempMem;   // (24-bit) TEMP - DSP temporary (universal) RAM
    alignas(16) std::array<uint32, 32> soundMem;   // (24-bit) MEMS - DSP sound memory
    alignas(16) std::array<uint16, 64> coeffs;     // (13-bit) COEF - DSP coefficient data RAM
    alignas(16) std::array<uint16, 32> addrs;      // (16-bit) MADRS - DSP memory address registers
    alignas(16) std::array<sint32, 16> mixStack;   // (20-bit) MIXS - DSP mix sound slot data stack (4 frac bits)
    alignas(16) std::array<sint16, 16> effectOut;  // (16-bit) EFREG - DSP effected data output
    alignas(16) std::array<sint16, 2> audioInOut;  // (16-bit) EXTS - DSP digital audio input

    uint8 ringBufferLeadAddress; // (W) RBP - DSP Ring Buffer Lead Address
    uint8 ringBufferLength;      // (W) RBL - DSP Ring Buffer Length

private:
    // -------------------------------------------------------------------------
    // State

    uint32 INPUTS; // (24-bit) INPUTS - input data

    uint32 SFT_REG;  // (26-bit)
    uint16 FRC_REG;  // (13-bit)
    uint32 Y_REG;    // (24-bit)
    uint16 ADRS_REG; // (12-bit)

    uint16 MDEC_CT;

    bool m_readPending;
    bool m_readNOFL;
    uint32 m_readValue;

    bool m_writePending;
    uint16 m_writeValue;

    uint32 m_readWriteAddr;

    uint8 *m_WRAM;

    uint16 ReadWRAM(uint32 address);
    void WriteWRAM(uint32 address, uint16 value);
};

} // namespace satemu::scsp

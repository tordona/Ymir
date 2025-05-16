#pragma once

#include <ymir/state/state_scsp_dsp.hpp>

#include "scsp_dsp_instr.hpp"

#include <ymir/core/types.hpp>

#include <ymir/util/bit_ops.hpp>
#include <ymir/util/inline.hpp>

#include <array>
#include <iosfwd>

namespace ymir::scsp {

class DSP {
public:
    DSP(uint8 *ram);

    void Reset();

    void Step();

    void DumpRegs(std::ostream &out) const;

    // -------------------------------------------------------------------------
    // Registers

    alignas(16) std::array<DSPInstr, 128> program;   // (60-bit) MPRO - DSP program RAM
    alignas(16) std::array<uint32, 128> tempMem;     // (24-bit) TEMP - DSP temporary (universal) RAM
    alignas(16) std::array<uint32, 32> soundMem;     // (24-bit) MEMS - DSP sound memory
    alignas(16) std::array<uint16, 64> coeffs;       // (13-bit) COEF - DSP coefficient data RAM
    alignas(16) std::array<uint16, 32> addrs;        // (16-bit) MADRS - DSP memory address registers
    alignas(16) std::array<sint32, 16 * 2> mixStack; // (20-bit) MIXS - DSP mix sound slot data stack (4 frac bits)
    alignas(16) std::array<sint16, 16> effectOut;    // (16-bit) EFREG - DSP effected data output
    alignas(16) std::array<sint16, 2> audioInOut;    // (16-bit) EXTS - DSP digital audio input

    [[nodiscard]] FORCE_INLINE uint32 GetMIXSIndex(uint8 offset) const noexcept {
        return offset | m_mixStackGen;
    }

    FORCE_INLINE void MIXSSlotWrite(uint8 offset, sint32 value) {
        assert(offset <= 0xF);
        value = bit::sign_extend<20>(value);
        if (m_mixStackNull & (1u << offset)) {
            m_mixStackNull &= ~(1u << offset);
            mixStack[GetMIXSIndex(offset)] = value;
        } else {
            mixStack[GetMIXSIndex(offset)] += value;
        }
    }

    uint8 ringBufferLeadAddress; // (W) RBP - DSP Ring Buffer Lead Address
    uint8 ringBufferLength;      // (W) RBL - DSP Ring Buffer Length

    // -------------------------------------------------------------------------
    // Save states

    void SaveState(state::SCSPDSP &state) const;
    bool ValidateState(const state::SCSPDSP &state) const;
    void LoadState(const state::SCSPDSP &state);

private:
    // -------------------------------------------------------------------------
    // State

    uint8 PC;

    uint32 INPUTS; // (24-bit) INPUTS - input data

    uint32 SFT_REG;  // (26-bit)
    uint16 FRC_REG;  // (13-bit)
    uint32 Y_REG;    // (24-bit)
    uint16 ADRS_REG; // (12-bit)

    uint16 MDEC_CT;

    // MIXS stack generation.
    // MIXS is double-buffered (2x16 entries).
    // The value is added as an offset to mixStack, so it must be either 0 or 16.
    uint8 m_mixStackGen;

    // Each bit indicates which MIXS entries should be cleared on slot writes.
    // Filled with 1s whenever the MIXS is swapped.
    // Entries are cleared when a write from a slot happens.
    uint16 m_mixStackNull;

    bool m_readPending;
    bool m_readNOFL;
    uint32 m_readValue;

    bool m_writePending;
    uint16 m_writeValue;

    uint32 m_readWriteAddr;

    uint8 *m_WRAM;

    uint16 ReadWRAM() const;
    void WriteWRAM();
};

} // namespace ymir::scsp

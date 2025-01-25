#pragma once

#include "scsp_defs.hpp"
#include "scsp_dsp.hpp"
#include "scsp_slot.hpp"
#include "scsp_timer.hpp"

#include <satemu/core/scheduler.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <satemu/hw/m68k/m68k.hpp>
#include <satemu/hw/m68k/m68k_defs.hpp>

#include <satemu/util/callback.hpp>
#include <satemu/util/data_ops.hpp>
#include <satemu/util/debug_print.hpp>
#include <satemu/util/inline.hpp>

#include <array>
#include <iosfwd>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

// -----------------------------------------------------------------------------

namespace satemu::scsp {

enum class SCSPAccessType { SCU, M68kCode, M68kData, DMA };

template <SCSPAccessType accessType>
struct SCSPAccessTypeInfo;

template <>
struct SCSPAccessTypeInfo<SCSPAccessType::SCU> {
    static constexpr const char *name = "SCU";
};

template <>
struct SCSPAccessTypeInfo<SCSPAccessType::M68kCode> {
    static constexpr const char *name = "M68K code";
};

template <>
struct SCSPAccessTypeInfo<SCSPAccessType::M68kData> {
    static constexpr const char *name = "M68K data";
};

template <>
struct SCSPAccessTypeInfo<SCSPAccessType::DMA> {
    static constexpr const char *name = "DMA";
};

template <SCSPAccessType accessType>
constexpr const char *accessTypeName = SCSPAccessTypeInfo<accessType>::name;

using CBOutputSample = util::Callback<void(sint16 left, sint16 right)>;

class SCSP {
    static constexpr dbg::Category rootLog{"SCSP"};
    static constexpr dbg::Category regsLog{rootLog, "Regs"};
    static constexpr dbg::Category dmaLog{rootLog, "DMA"};

public:
    SCSP(core::Scheduler &scheduler, scu::SCU &scu);

    void Reset(bool hard);

    FORCE_INLINE void SetCallback(CBOutputSample cbOutputSample) {
        m_cbOutputSample = cbOutputSample;
    }

    void Advance(uint64 cycles);

    void DumpWRAM(std::ostream &out);

    // -------------------------------------------------------------------------
    // SCU-facing bus
    // 16-bit reads, 8- or 16-bit writes

    template <mem_primitive T>
    FLATTEN T ReadWRAM(uint32 address) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP WRAM read size");
        // TODO: handle memory size bit
        return util::ReadBE<T>(&m_WRAM[address & 0x7FFFF]);
    }

    template <mem_primitive T>
    FLATTEN void WriteWRAM(uint32 address, T value) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP WRAM write size");
        // TODO: handle memory size bit
        util::WriteBE<T>(&m_WRAM[address & 0x7FFFF], value);
    }

    template <mem_primitive T>
    T ReadReg(uint32 address) {
        return ReadReg<T, SCSPAccessType::SCU>(address);
    }

    template <mem_primitive T>
    void WriteReg(uint32 address, T value) {
        WriteReg<T, SCSPAccessType::SCU>(address, value);
    }

    void SetCPUEnabled(bool enabled);

private:
    alignas(16) std::array<uint8, m68k::kM68KWRAMSize> m_WRAM;

    m68k::MC68EC000 m_m68k;
    bool m_m68kEnabled;

    scu::SCU &m_scu;

    core::Scheduler &m_scheduler;
    core::EventID m_sampleTickEvent;

    CBOutputSample m_cbOutputSample;

    // -------------------------------------------------------------------------
    // MC68EC000-facing bus
    // 8- or 16-bit reads and writes

    friend class m68k::MC68EC000;

    template <mem_primitive T, bool instrFetch>
    T Read(uint32 address) {
        static constexpr SCSPAccessType accessType = instrFetch ? SCSPAccessType::M68kCode : SCSPAccessType::M68kData;

        if (util::AddressInRange<0x000000, 0x0FFFFF>(address)) {
            // TODO: handle memory size bit
            return ReadWRAM<T>(address);
        } else if (util::AddressInRange<0x100000, 0x1FFFFF>(address)) {
            return ReadReg<T, accessType>(address & 0xFFF);
        } else {
            return 0;
        }
    }

    template <mem_primitive T>
    void Write(uint32 address, T value) {
        if (util::AddressInRange<0x000000, 0x0FFFFF>(address)) {
            WriteWRAM<T>(address, value);
        } else if (util::AddressInRange<0x100000, 0x1FFFFF>(address)) {
            WriteReg<T, SCSPAccessType::M68kData>(address & 0xFFF, value);
        }
    }

    // -------------------------------------------------------------------------
    // Generic accessors
    // T is either uint8 or uint16, never uint32

    // Register accesses are handled by individual templated methods that use flags for each half of the 16-bit value.
    // 16-bit accesses have both flags set, while 8-bit writes only have the flag for the corresponding half set
    // (upper for even addresses, lower for odd addresses).
    //
    // These methods receive a 16-bit value containing either the full 16-bit value or the 8-bit value shifted into the
    // appropriate place so that all three cases are handled consistently and efficiently, including values that span
    // both halves:
    //   Access                    Contents of 16-bit value sent to accessor methods
    //   16-bit                    The entire value, unmodified
    //   8-bit on even addresses   8-bit value in bits 15-8
    //   8-bit on odd addresses    8-bit value in bits 7-0

    template <mem_primitive T, SCSPAccessType accessType>
    T ReadReg(uint32 address) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP register read size");
        static constexpr bool is16 = std::is_same_v<T, uint16>;

        regsLog.trace("{}-bit SCSP register read via {} bus from {:03X}", sizeof(T) * 8, accessTypeName<accessType>,
                      address);

        using namespace util;

        auto shiftByte = [](uint16 value) {
            if constexpr (is16) {
                return value;
            } else {
                return value >> 8u;
            }
        };

        auto read16 = [=](uint16 value) {
            if constexpr (is16) {
                return value;
            } else if (address & 1) {
                return value >> 0u;
            } else {
                return value >> 8u;
            }
        };

        if constexpr (accessType == SCSPAccessType::M68kCode) {
            regsLog.debug("M68K attempted to fetch instruction from SCSP register area at {:03X}", address);
            return 0;
        }

        /**/ if (AddressInRange<0x000, 0x3FF>(address)) {
            const uint32 slotIndex = address >> 5;
            auto &slot = m_slots[slotIndex];
            return slot.ReadReg<T>(address & 0x1F);
        } else if (AddressInRange<0x600, 0x67F>(address)) {
            const uint32 idx = (address >> 1u) & 0x3F;
            const uint16 stack = m_soundStack[idx];
            return read16(stack);
        } else if (AddressInRange<0x700, 0x77F>(address)) {
            return read16(m_dspCoeffs[(address >> 1u) & 0x3F]);
        } else if (AddressInRange<0x780, 0x7BF>(address)) {
            return read16(m_dspAddrs[(address >> 1u) & 0x1F]);
        } else if (AddressInRange<0x800, 0xBFF>(address)) {
            const uint32 index = (address >> 3u) & 0x7F;
            const uint32 subindex = (address >> 1u) & 0x3;
            return read16(m_dspProgram[index].u16[subindex]);
        } else if (AddressInRange<0xC00, 0xDFF>(address)) {
            const uint32 offset = (address >> 1u) & 0x1;
            const uint32 index = (address >> 2u) & 0xFF;
            if (offset == 0) {
                return read16(bit::extract<0, 7>(m_dspTemp[index]));
            } else {
                return read16(bit::extract<8, 23>(m_dspTemp[index]));
            }
        } else if (AddressInRange<0xE00, 0xE7F>(address)) {
            const uint32 offset = (address >> 1u) & 0x1;
            const uint32 index = (address >> 2u) & 0x3F;
            if (offset == 0) {
                return read16(bit::extract<0, 7>(m_dspSoundMem[index]));
            } else {
                return read16(bit::extract<8, 23>(m_dspSoundMem[index]));
            }
        } else if (AddressInRange<0xE80, 0xEBF>(address)) {
            const uint32 offset = (address >> 1u) & 0x1;
            const uint32 index = (address >> 2u) & 0xF;
            if (offset == 0) {
                return read16(bit::extract<0, 3>(m_dspMixStack[index]));
            } else {
                return read16(bit::extract<4, 19>(m_dspMixStack[index]));
            }
        } else if (AddressInRange<0xEC0, 0xEDF>(address)) {
            return read16(m_dspEffectOut[(address >> 1u) & 0xF]);
        } else if (AddressInRange<0xEE0, 0xEE3>(address)) {
            return read16(m_dspAudioIn[(address >> 1u) & 0x1]);
        }

        switch (address) {
        case 0x400: return 0; // MVOL, DAC18B, MEM4MB are write-only
        case 0x401: return 0; // Only VER is readable, and it is 0
        case 0x402: return 0; // RBP and RBL are write-only
        case 0x403: return 0; // RBP and RBL are write-only

        case 0x404: return shiftByte(ReadMIDIIn<is16, true>());
        case 0x405: return ReadMIDIIn<true, is16>();
        case 0x406: return 0; // MOBUF is write-only
        case 0x407: return 0; // MOBUF is write-only

        case 0x408: return shiftByte(ReadSlotStatus<is16, true>());
        case 0x409: return ReadSlotStatus<true, is16>();

        case 0x412: return 0; // DMEA is write-only
        case 0x413: return 0; // DMEA is write-only
        case 0x414: return 0; // DMEA and DRGA are write-only
        case 0x415: return 0; // DMEA and DRGA are write-only
        case 0x416: return shiftByte(ReadDMAStatus<is16, true>());
        case 0x417: return ReadDMAStatus<true, is16>();

        case 0x418: return 0; // Timers are write-only
        case 0x419: return 0; // Timers are write-only
        case 0x41A: return 0; // Timers are write-only
        case 0x41B: return 0; // Timers are write-only
        case 0x41C: return 0; // Timers are write-only
        case 0x41D: return 0; // Timers are write-only

        case 0x41E: return shiftByte(ReadSCIEB());
        case 0x41F: return ReadSCIEB();
        case 0x420: return shiftByte(ReadSCIPD());
        case 0x421: return ReadSCIPD();
        case 0x422: return 0; // SCIRE is write-only
        case 0x423: return 0; // SCIRE is write-only

        case 0x424: return shiftByte(ReadSCILV(0));
        case 0x425: return ReadSCILV(0);
        case 0x426: return shiftByte(ReadSCILV(1));
        case 0x427: return ReadSCILV(1);
        case 0x428: return shiftByte(ReadSCILV(2));
        case 0x429: return ReadSCILV(2);

        case 0x42A: return shiftByte(ReadMCIEB());
        case 0x42B: return ReadMCIEB();
        case 0x42C: return shiftByte(ReadMCIPD());
        case 0x42D: return ReadMCIPD();
        case 0x42E: return 0; // MCIRE is write-only
        case 0x42F: return 0; // MCIRE is write-only

        default:
            regsLog.debug("unhandled {}-bit SCSP register read via {} bus from {:03X}", sizeof(T) * 8,
                          accessTypeName<accessType>, address);
            break;
        }

        return 0;
    }

    template <mem_primitive T, SCSPAccessType accessType>
    void WriteReg(uint32 address, T value) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP register write size");
        static constexpr bool is16 = std::is_same_v<T, uint16>;

        regsLog.trace("{}-bit SCSP register write via {} bus to {:03X} = {:X}", sizeof(T) * 8,
                      accessTypeName<accessType>, address, value);

        using namespace util;

        uint16 value16 = value;
        if constexpr (!is16) {
            if ((address & 1) == 0) {
                value16 <<= 8u;
            }
        }

        auto write16 = [=](auto &reg, uint16 value) {
            if constexpr (is16) {
                reg = value;
            } else if (address & 1) {
                reg = (reg & 0xFF00) | value;
            } else {
                reg = (reg & 0x00FF) | value;
            }
        };

        /**/ if (AddressInRange<0x000, 0x3FF>(address)) {
            const uint32 slotIndex = address >> 5;
            auto &slot = m_slots[slotIndex];
            slot.WriteReg<T>(address & 0x1F, value);
            m_keyOnEx |= bit::extract<12>(value16);
            return;
        } else if (AddressInRange<0x600, 0x67F>(address)) {
            const uint32 idx = (address >> 1u) & 0x3F;
            return write16(m_soundStack[idx], value16);
        } else if (AddressInRange<0x700, 0x77F>(address)) {
            return write16(m_dspCoeffs[(address >> 1u) & 0x3F], value16 >> 3);
        } else if (AddressInRange<0x780, 0x7BF>(address)) {
            return write16(m_dspAddrs[(address >> 1u) & 0x1F], value16);
        } else if (AddressInRange<0x7C0, 0x7FF>(address)) {
            // Some games and even the IPL ROM try to write to this empty area
            regsLog.trace("Unexpected write to {:03X} = {:X}", address, value);
            return;
        } else if (AddressInRange<0x800, 0xBFF>(address)) {
            const uint32 index = (address >> 3u) & 0x7F;
            const uint32 subindex = (address >> 1u) & 0x3;
            return write16(m_dspProgram[index].u16[subindex], value16);
        } else if (AddressInRange<0xC00, 0xDFF>(address)) {
            const uint32 offset = (address >> 1u) & 0x1;
            const uint32 index = (address >> 2u) & 0xFF;
            if (offset == 0) {
                uint16 tmpValue = bit::extract<0, 7>(m_dspTemp[index]);
                write16(tmpValue, value16);
                bit::deposit_into<0, 7>(m_dspTemp[index], tmpValue);
            } else {
                uint16 tmpValue = bit::extract<8, 23>(m_dspTemp[index]);
                write16(tmpValue, value16);
                bit::deposit_into<8, 23>(m_dspTemp[index], tmpValue);
            }
            return;
        } else if (AddressInRange<0xE00, 0xE7F>(address)) {
            const uint32 offset = (address >> 1u) & 0x1;
            const uint32 index = (address >> 2u) & 0x3F;
            if (offset == 0) {
                uint16 tmpValue = bit::extract<0, 7>(m_dspSoundMem[index]);
                write16(tmpValue, value16);
                bit::deposit_into<0, 7>(m_dspSoundMem[index], tmpValue);
            } else {
                uint16 tmpValue = bit::extract<8, 23>(m_dspSoundMem[index]);
                write16(tmpValue, value16);
                bit::deposit_into<8, 23>(m_dspSoundMem[index], tmpValue);
            }
            return;
        } else if (AddressInRange<0xE80, 0xEBF>(address)) {
            const uint32 offset = (address >> 1u) & 1;
            const uint32 index = (address >> 2u) & 0xF;
            if (offset == 0) {
                uint16 tmpValue = bit::extract<0, 3>(m_dspMixStack[index]);
                write16(tmpValue, value16);
                bit::deposit_into<0, 3>(m_dspMixStack[index], tmpValue);
            } else {
                uint16 tmpValue = bit::extract<4, 19>(m_dspMixStack[index]);
                write16(tmpValue, value16);
                bit::deposit_into<4, 19>(m_dspMixStack[index], tmpValue);
            }
            return;
        } else if (AddressInRange<0xEC0, 0xEDF>(address)) {
            const uint32 index = (address >> 1u) & 0xF;
            return write16(m_dspEffectOut[index], value16);
        } else if (AddressInRange<0xEE0, 0xEE3>(address)) {
            const uint32 index = (address >> 1u) & 0x1;
            return write16(m_dspAudioIn[index], value16);
        }

        switch (address) {
        case 0x400: WriteReg400<is16, true>(value16); break;
        case 0x401: WriteReg400<true, is16>(value16); break;
        case 0x402: WriteReg402<is16, true>(value16); break;
        case 0x403: WriteReg402<true, is16>(value16); break;

        case 0x404: // MIDI In registers are read-only
        case 0x405: // MIDI In registers are read-only
        case 0x406: WriteMIDIOut<is16, true>(value16); break;
        case 0x407: WriteMIDIOut<true, is16>(value16); break;

        case 0x408: WriteSlotStatus<is16, true>(value16); break;
        case 0x409: WriteSlotStatus<true, is16>(value16); break;

        case 0x412: WriteReg412<is16, true>(value16); break;
        case 0x413: WriteReg412<true, is16>(value16); break;
        case 0x414: WriteReg414<is16, true>(value16); break;
        case 0x415: WriteReg414<true, is16>(value16); break;
        case 0x416: WriteReg416<is16, true>(value16); break;
        case 0x417: WriteReg416<true, is16>(value16); break;

        case 0x418: WriteTimer<is16, true>(0, value16); break;
        case 0x419: WriteTimer<true, is16>(0, value16); break;
        case 0x41A: WriteTimer<is16, true>(1, value16); break;
        case 0x41B: WriteTimer<true, is16>(1, value16); break;
        case 0x41C: WriteTimer<is16, true>(2, value16); break;
        case 0x41D: WriteTimer<true, is16>(2, value16); break;

        case 0x41E: WriteSCIEB<is16, true>(value16); break;
        case 0x41F: WriteSCIEB<true, is16>(value16); break;
        case 0x420: WriteSCIPD<is16, true>(value16); break;
        case 0x421: WriteSCIPD<true, is16>(value16); break;
        case 0x422: WriteSCIRE<is16, true>(value16); break;
        case 0x423: WriteSCIRE<true, is16>(value16); break;

        case 0x424: WriteSCILV<is16, true>(0, value16); break;
        case 0x425: WriteSCILV<true, is16>(0, value16); break;
        case 0x426: WriteSCILV<is16, true>(1, value16); break;
        case 0x427: WriteSCILV<true, is16>(1, value16); break;
        case 0x428: WriteSCILV<is16, true>(2, value16); break;
        case 0x429: WriteSCILV<true, is16>(2, value16); break;

        case 0x42A: WriteMCIEB<is16, true>(value16); break;
        case 0x42B: WriteMCIEB<true, is16>(value16); break;
        case 0x42C: WriteMCIPD<is16, true>(value16); break;
        case 0x42D: WriteMCIPD<true, is16>(value16); break;
        case 0x42E: WriteMCIRE<is16, true>(value16); break;
        case 0x42F: WriteMCIRE<true, is16>(value16); break;

        default:
            regsLog.debug("unhandled {}-bit SCSP register write via {} bus to {:03X} = {:X}", sizeof(T) * 8,
                          accessTypeName<accessType>, address, value);
            break;
        }
    }

    // --- Mixer Register ---
    // --- Sound Memory Configuration Register ---

    template <bool lowerByte, bool upperByte>
    void WriteReg400(uint16 value) {
        if constexpr (lowerByte) {
            m_masterVolume = bit::extract<0, 3>(value);
        }
        if constexpr (upperByte) {
            m_mem4MB = bit::extract<8>(value);
            m_dac18Bits = bit::extract<9>(value);
        }
    }

    // --- Slot Status Register ---

    template <bool lowerByte, bool upperByte>
    uint16 ReadSlotStatus() {
        uint16 value = 0;
        bit::deposit_into<7, 10>(value, m_slots[m_monitorSlotCall].currAddress >> 12u);
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteSlotStatus(uint16 value) {
        if constexpr (upperByte) {
            m_monitorSlotCall = bit::extract<11, 15>(value);
        }
    }

    // --- MIDI Register ---

    template <bool lowerByte, bool upperByte>
    uint16 ReadMIDIIn() {
        regsLog.trace("Read from MIDI IN is unimplemented");
        return 0;
    }

    template <bool lowerByte, bool upperByte>
    void WriteMIDIOut(uint16 value) {
        if constexpr (lowerByte) {
            regsLog.trace("Write to MIDI OUT is unimplemented - {:02X}", value);
            // TODO: write bit::extract<0, 7>(value) to MIDI out buffer
        }
    }

    // --- Timer Register ---

    template <bool lowerByte, bool upperByte>
    void WriteTimer(uint32 index, uint16 value) {
        if constexpr (lowerByte) {
            m_timers[index].WriteTIMx(bit::extract<0, 7>(value));
        }
        if constexpr (upperByte) {
            m_timers[index].WriteTxCTL(bit::extract<8, 10>(value));
        }
    }

    // --- Interrupt Control Register ---

    uint16 ReadSCIEB() const {
        return m_m68kEnabledInterrupts;
    }

    template <bool lowerByte, bool upperByte>
    void WriteSCIEB(uint16 value) {
        util::SplitWriteWord<lowerByte, upperByte, 0, 10>(m_m68kEnabledInterrupts, value);
        UpdateM68KInterrupts();
    }

    uint16 ReadSCIPD() const {
        return m_m68kPendingInterrupts;
    }

    template <bool lowerByte, bool upperByte>
    void WriteSCIPD(uint16 value) {
        if constexpr (lowerByte) {
            bit::deposit_into<5>(m_m68kPendingInterrupts, bit::extract<5>(value));
            UpdateM68KInterrupts();
        }
    }

    template <bool lowerByte, bool upperByte>
    void WriteSCIRE(uint16 value) {
        m_m68kPendingInterrupts &= ~value;
        UpdateM68KInterrupts();
    }

    // ---

    uint16 ReadMCIEB() const {
        return m_scuEnabledInterrupts;
    }

    template <bool lowerByte, bool upperByte>
    void WriteMCIEB(uint16 value) {
        util::SplitWriteWord<lowerByte, upperByte, 0, 10>(m_scuEnabledInterrupts, value);
        UpdateSCUInterrupts();
    }

    uint16 ReadMCIPD() const {
        return m_scuPendingInterrupts;
    }

    template <bool lowerByte, bool upperByte>
    void WriteMCIPD(uint16 value) {
        if constexpr (lowerByte) {
            bit::deposit_into<5>(m_scuPendingInterrupts, bit::extract<5>(value));
            UpdateSCUInterrupts();
        }
    }

    template <bool lowerByte, bool upperByte>
    void WriteMCIRE(uint16 value) {
        m_scuPendingInterrupts &= ~value;
        UpdateSCUInterrupts();
    }

    // ---

    uint16 ReadSCILV(uint32 index) {
        return m_m68kInterruptLevels[index];
    }

    template <bool lowerByte, bool upperByte>
    void WriteSCILV(uint32 index, uint16 value) {
        if constexpr (lowerByte) {
            m_m68kInterruptLevels[index] = bit::extract<0, 7>(value);
            UpdateM68KInterrupts();
        }
    }

    // --- DMA Transfer Register ---

    template <bool lowerByte, bool upperByte>
    void WriteReg412(uint16 value) {
        if constexpr (lowerByte) {
            bit::deposit_into<1, 7>(m_dmaMemAddress, bit::extract<1, 7>(value));
        }
        if constexpr (upperByte) {
            bit::deposit_into<8, 15>(m_dmaMemAddress, bit::extract<8, 15>(value));
        }
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg414(uint16 value) {
        if constexpr (lowerByte) {
            bit::deposit_into<1, 7>(m_dmaRegAddress, bit::extract<1, 7>(value));
        }
        if constexpr (upperByte) {
            bit::deposit_into<8, 11>(m_dmaRegAddress, bit::extract<8, 11>(value));
            bit::deposit_into<16, 19>(m_dmaMemAddress, bit::extract<12, 15>(value));
        }
    }

    template <bool lowerByte, bool upperByte>
    uint16 ReadDMAStatus() {
        uint16 value = 0;
        if constexpr (upperByte) {
            bit::deposit_into<12>(value, m_dmaExec);
            bit::deposit_into<13>(value, m_dmaXferToMem);
            bit::deposit_into<14>(value, m_dmaGate);
        }
        return value;
    }

    template <bool lowerByte, bool upperByte>
    void WriteReg416(uint16 value) {
        if constexpr (lowerByte) {
            bit::deposit_into<1, 7>(m_dmaXferLength, bit::extract<1, 7>(value));
        }
        if constexpr (upperByte) {
            bit::deposit_into<8, 11>(m_dmaXferLength, bit::extract<8, 11>(value));
            m_dmaExec |= bit::extract<12>(value);
            m_dmaXferToMem = bit::extract<13>(value);
            m_dmaGate = bit::extract<14>(value);
            ExecuteDMA();
        }
    }

    // --- DSP Registers ---

    template <bool lowerByte, bool upperByte>
    void WriteReg402(uint16 value) {
        if constexpr (lowerByte) {
            m_dspRingBufferLeadAddress = bit::extract<0, 6>(value);
        }
        util::SplitWriteWord<lowerByte, upperByte, 7, 8>(m_dspRingBufferLength, value);
    }

    // -------------------------------------------------------------------------
    // Registers

    // --- Sound slots ---

    alignas(16) std::array<Slot, 32> m_slots;

    // --- Mixer Register ---

    uint32 m_masterVolume; // (W) MVOL - master volume adjustment after all audio processing
    bool m_dac18Bits;      // (W) DAC18B - outputs 18-bit instead of 16-bit data to DAC

    // --- Sound Memory Configuration Register ---

    bool m_mem4MB; // (W) MEM4MB - enables full 4 Mbit RAM access instead of 1 Mbit

    // --- Slot Status Register ---

    uint8 m_monitorSlotCall; // (W) MSLC - selects a slot to monitor the current sample offset from SA
                             // (R) CA - Call Address - the offset from SA of the current sample (in 4 KiB units?)

    bool m_keyOnEx; // (W) KYONEX - executes key on/off on all slots on next sample

    // --- MIDI Register ---

    // TODO

    // --- Timer Register ---

    alignas(16) std::array<Timer, 3> m_timers;

    // --- Interrupt Control Register ---

    uint16 m_scuEnabledInterrupts;  // (W) MCIEB
    uint16 m_scuPendingInterrupts;  // (W) MCIPD
    uint16 m_m68kEnabledInterrupts; // (W) SCIEB
    uint16 m_m68kPendingInterrupts; // (W) SCIPD

    std::array<uint8, 3> m_m68kInterruptLevels; // (W) SCILV0-2

    void SetInterrupt(uint16 intr, bool level);

    void UpdateM68KInterrupts();
    void UpdateSCUInterrupts();

    // --- DMA Transfer Register ---

    bool m_dmaExec;         // (R/W) DEXE - DMA Execution
    bool m_dmaXferToMem;    // (R/W) DDIR - DMA Transfer Direction (0=mem->reg, 1=reg->mem)
    bool m_dmaGate;         // (R/W) DGATE - DMA Gate (0=mem/reg->dst, 1=zero->dest)
    uint32 m_dmaMemAddress; // (W) DMEA - DMA Memory Start Address
    uint16 m_dmaRegAddress; // (W) DRGA - DMA Register Start Address
    uint16 m_dmaXferLength; // (W) DTLG - DMA Transfer Length

    void ExecuteDMA();

    // --- Direct Sound Data Stack ---

    alignas(16) std::array<uint16, 64> m_soundStack;
    uint32 m_soundStackIndex;

    // --- DSP Registers ---

    alignas(16) std::array<DSPInstr, 128> m_dspProgram; // (60-bit) MPRO - DSP program RAM
    alignas(16) std::array<uint32, 256> m_dspTemp;      // (24-bit) TEMP - DSP temporary (universal) RAM
    alignas(16) std::array<uint32, 64> m_dspSoundMem;   // (24-bit) SMEM - DSP sound memory
    alignas(16) std::array<uint16, 64> m_dspCoeffs;     // (13-bit) COEF - DSP coefficient data RAM
    alignas(16) std::array<uint16, 32> m_dspAddrs;      // (16-bit) MADRS - DSP memory address registers
    alignas(16) std::array<sint32, 16> m_dspMixStack;   // (20-bit) MIXS - DSP mix sound slot data stack (4 frac bits)
    alignas(16) std::array<sint16, 16> m_dspEffectOut;  // (16-bit) EFREG - DSP effected data output
    alignas(16) std::array<sint16, 2> m_dspAudioIn;     // (16-bit) EXTS - DSP digital audio input

    uint32 m_dspRingBufferLeadAddress; // (W) RBP - DSP Ring Buffer Lead Address
    uint8 m_dspRingBufferLength;       // (W) RBL - DSP Ring Buffer Length

    // -------------------------------------------------------------------------
    // Audio processing

    void ProcessSample();

    void SlotProcessStep1(Slot &slot); // Phase generation and pitch LFO
    void SlotProcessStep2(Slot &slot); // Address pointer calculation and X/Y modulation data read
    void SlotProcessStep3(Slot &slot); // Waveform read
    void SlotProcessStep4(Slot &slot); // Interpolation, envelope generator update and amplitude LFO calculation
    void SlotProcessStep5(Slot &slot); // Level calculation part 1
    void SlotProcessStep6(Slot &slot); // Level calculation part 2
    void SlotProcessStep7(Slot &slot); // Sound stack write

    uint64 m_m68kCycles;    // MC68EC000 cycle counter
    uint64 m_sampleCounter; // total number of samples

    // -------------------------------------------------------------------------
    // Interrupt handling

    m68k::ExceptionVector AcknowledgeInterrupt(uint8 level);
};

} // namespace satemu::scsp

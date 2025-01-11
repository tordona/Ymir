#pragma once

#include "scsp_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/hw/m68k/m68k.hpp>
#include <satemu/hw/m68k/m68k_defs.hpp>

#include <satemu/util/data_ops.hpp>
#include <satemu/util/debug_print.hpp>
#include <satemu/util/inline.hpp>

#include <array>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

// -----------------------------------------------------------------------------

namespace satemu::scsp {

class SCSP {
    static constexpr dbg::Category rootLog{"SCSP"};
    static constexpr dbg::Category regsLog{rootLog, "Regs"};

public:
    SCSP(scu::SCU &scu);

    void Reset(bool hard);

    void Advance(uint64 cycles);

    // -------------------------------------------------------------------------
    // SCU-facing bus
    // 16-bit reads, 8- or 16-bit writes

    template <mem_primitive T>
    FLATTEN T ReadWRAM(uint32 address) {
        return ReadWRAM<T, false, false>(address);
    }

    template <mem_primitive T>
    FLATTEN void WriteWRAM(uint32 address, T value) {
        WriteWRAM<T, false>(address, value);
    }

    template <mem_primitive T>
    T ReadReg(uint32 address) {
        return ReadReg<T, false, false>(address);
    }

    template <mem_primitive T>
    void WriteReg(uint32 address, T value) {
        WriteReg<T, false>(address, value);
    }

    void SetCPUEnabled(bool enabled);

private:
    m68k::MC68EC000 m_m68k;
    bool m_m68kEnabled;

    alignas(16) std::array<uint8, m68k::kM68KWRAMSize> m_WRAM;

    scu::SCU &m_scu;

    // -------------------------------------------------------------------------
    // MC68EC000-facing bus
    // 8- or 16-bit reads and writes

    friend class m68k::MC68EC000;

    template <mem_primitive T, bool instrFetch>
    T Read(uint32 address) {
        if (util::AddressInRange<0x000000, 0x0FFFFF>(address)) {
            // TODO: handle memory size bit
            return ReadWRAM<T, true, instrFetch>(address);
        } else if (util::AddressInRange<0x100000, 0x1FFFFF>(address)) {
            return ReadReg<T, true, instrFetch>(address & 0xFFF);
        } else {
            return 0;
        }
    }

    template <mem_primitive T>
    void Write(uint32 address, T value) {
        if (util::AddressInRange<0x000000, 0x0FFFFF>(address)) {
            WriteWRAM<T, true>(address, value);
        } else if (util::AddressInRange<0x100000, 0x1FFFFF>(address)) {
            WriteReg<T, true>(address & 0xFFF, value);
        }
    }

    // -------------------------------------------------------------------------
    // Generic accessors
    // fromM68K: false=SCU, true=MC68EC000
    // T is either uint8 or uint16, never uint32

    template <mem_primitive T, bool fromM68K, bool instrFetch>
    T ReadWRAM(uint32 address) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP WRAM read size");
        // TODO: handle memory size bit
        return util::ReadBE<T>(&m_WRAM[address & 0x7FFFF]);
    }

    template <mem_primitive T, bool fromM68K>
    void WriteWRAM(uint32 address, T value) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP WRAM write size");
        // TODO: handle memory size bit
        util::WriteBE<T>(&m_WRAM[address & 0x7FFFF], value);
    }

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

    template <mem_primitive T, bool fromM68K, bool instrFetch>
    T ReadReg(uint32 address) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP register read size");
        static constexpr bool is16 = std::is_same_v<T, uint16>;

        auto shiftByte = [](uint16 value) {
            if constexpr (is16) {
                return value >> 16u;
            } else {
                return value;
            }
        };

        if constexpr (!instrFetch) {
            if (address < 0x400) {
                const uint32 slotIndex = address >> 5;
                auto &slot = m_slots[slotIndex];
                return slot.ReadReg<T, fromM68K>(address & 0x1F);
            }
            if (address >= 0x700) {
                // TODO: DSP
            }

            switch (address) {
            case 0x400: return 0; // Write-only
            case 0x401: return 0; // Only VER is readable, and it is 0

            case 0x408: return shiftByte(ReadReg408<is16, true>());
            case 0x409: return ReadReg408<true, is16>();

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
                regsLog.debug("unhandled {}-bit SCSP register read via {} from {:03X}", sizeof(T) * 8,
                              (fromM68K ? "M68K" : "SCU"), address);
                break;
            }
        }
        return 0;
    }

    template <mem_primitive T, bool fromM68K>
    void WriteReg(uint32 address, T value) {
        static_assert(!std::is_same_v<T, uint32>, "Invalid SCSP register write size");

        static constexpr bool is16 = std::is_same_v<T, uint16>;
        uint16 value16 = value;
        if constexpr (!is16) {
            if ((address & 1) == 0) {
                value16 <<= 8u;
            }
        }

        if (address < 0x400) {
            const uint32 slotIndex = address >> 5;
            auto &slot = m_slots[slotIndex];
            slot.WriteReg<T, fromM68K>(address & 0x1F, value);
            
            // Handle KYONEX
            if ((address & 0x1F) == 0 && bit::extract<12>(value16)) {
                for (auto &slot : m_slots) {
                    slot.TriggerKeyOn();
                }
            }
            return;
        }
        if (address >= 0x700) {
            // TODO: DSP
        }

        switch (address) {
        case 0x400: WriteReg400<is16, true>(value16); break;
        case 0x401: WriteReg400<true, is16>(value16); break;

        case 0x408: WriteReg408<is16, true>(value16); break;
        case 0x409: WriteReg408<true, is16>(value16); break;

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
            regsLog.debug("unhandled {}-bit SCSP register write via {} to {:03X} = {:X}", sizeof(T) * 8,
                          (fromM68K ? "M68K" : "SCU"), address, value);
            break;
        }
    }

    // --- Mixer Register ---
    // --- Sound Memory Configuration Register ---

    template <bool lowerHalf, bool upperHalf>
    void WriteReg400(uint16 value) {
        if constexpr (lowerHalf) {
            m_masterVolume = bit::extract<0, 3>(value);
        }
        if constexpr (upperHalf) {
            m_mem4MB = bit::extract<8>(value);
            m_dac18Bits = bit::extract<9>(value);
        }
    }

    template <bool lowerHalf, bool upperHalf>
    uint16 ReadReg408() {
        uint16 value = 0;
        // TODO: implement
        // bit::deposit_into<7, 10>(m_slots[m_monitorSlotCall].samplePosition >> 12u);
        return value;
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteReg408(uint16 value) {
        if constexpr (upperHalf) {
            m_monitorSlotCall = bit::extract<11, 15>(value);
        }
    }

    // --- Timer Register ---

    template <bool lowerHalf, bool upperHalf>
    void WriteTimer(uint32 index, uint16 value) {
        if constexpr (lowerHalf) {
            m_timers[index].WriteTIMx(bit::extract<0, 7>(value));
        }
        if constexpr (upperHalf) {
            m_timers[index].WriteTxCTL(bit::extract<8, 10>(value));
        }
    }

    // --- Interrupt Control Register ---

    uint16 ReadSCIEB() const {
        return m_m68kEnabledInterrupts;
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteSCIEB(uint16 value) {
        static constexpr uint32 lb = lowerHalf ? 0 : 8;
        static constexpr uint32 ub = upperHalf ? 10 : 7;
        bit::deposit_into<lb, ub>(m_m68kEnabledInterrupts, bit::extract<lb, ub>(value));
        UpdateM68KInterrupts();
    }

    uint16 ReadSCIPD() const {
        return m_m68kPendingInterrupts;
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteSCIPD(uint16 value) {
        if constexpr (lowerHalf) {
            bit::deposit_into<5>(m_m68kPendingInterrupts, bit::extract<5>(value));
            UpdateM68KInterrupts();
        }
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteSCIRE(uint16 value) {
        m_m68kEnabledInterrupts &= ~value;
        UpdateM68KInterrupts();
    }

    // ---

    uint16 ReadMCIEB() const {
        return m_scuEnabledInterrupts;
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteMCIEB(uint16 value) {
        static constexpr uint32 lb = lowerHalf ? 0 : 8;
        static constexpr uint32 ub = upperHalf ? 10 : 7;
        bit::deposit_into<lb, ub>(m_scuEnabledInterrupts, bit::extract<lb, ub>(value));
        UpdateSCUInterrupts();
    }

    uint16 ReadMCIPD() const {
        return m_scuPendingInterrupts;
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteMCIPD(uint16 value) {
        if constexpr (lowerHalf) {
            bit::deposit_into<5>(m_scuPendingInterrupts, bit::extract<5>(value));
            UpdateSCUInterrupts();
        }
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteMCIRE(uint16 value) {
        m_scuEnabledInterrupts &= ~value;
        UpdateSCUInterrupts();
    }

    // ---

    uint16 ReadSCILV(uint32 index) {
        return m_m68kInterruptLevels[index];
    }

    template <bool lowerHalf, bool upperHalf>
    void WriteSCILV(uint32 index, uint16 value) {
        if constexpr (lowerHalf) {
            m_m68kInterruptLevels[index] = bit::extract<0, 7>(value);
            UpdateM68KInterrupts();
        }
    }

    // -------------------------------------------------------------------------
    // Registers

    // --- Mixer Register ---

    uint32 m_masterVolume; // (W) MVOL - master volume adjustment after all audio processing
    bool m_dac18Bits;      // (W) DAC18B - outputs 18-bit instead of 16-bit data to DAC

    // --- Sound Memory Configuration Register ---

    bool m_mem4MB; // (W) MEM4MB - enables full 4 Mbit RAM access instead of 1 Mbit

    // --- Slot Status Register ---

    uint8 m_monitorSlotCall; // (W) MSLC - selects a slot to monitor the current sample offset from SA
                             // (R) CA - Call Address - the offset from SA of the current sample (in 4 KiB units?)

    // --- MIDI Register ---

    // TODO

    // --- Timer Register ---

    std::array<Timer, 3> m_timers;

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

    // --- Sound slots ---

    std::array<Slot, 32> m_slots;

    // -------------------------------------------------------------------------
    // Audio processing

    void ProcessSample();

    uint64 m_accumSampleCycles; // number of SCSP cycles toward the next sample tick
    uint64 m_sampleCounter;     // total number of samples

    // -------------------------------------------------------------------------
    // Interrupt handling

    m68k::ExceptionVector AcknowledgeInterrupt(uint8 level);
};

} // namespace satemu::scsp

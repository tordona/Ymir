#pragma once

#include "scsp_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/hw/m68k/m68k.hpp>
#include <satemu/hw/m68k/m68k_defs.hpp>

#include <satemu/util/data_ops.hpp>
#include <satemu/util/inline.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu::scsp {

class SCSP {
public:
    SCSP();

    void Reset(bool hard);

    void Advance(uint64 cycles);

    // -------------------------------------------------------------------------
    // SCU-facing bus
    // 16-bit reads, 8- or 16-bit writes

    template <mem_access_type T>
    FLATTEN T ReadWRAM(uint32 address) {
        return ReadWRAM<T, false, false>(address);
    }

    template <mem_access_type T>
    FLATTEN void WriteWRAM(uint32 address, T value) {
        WriteWRAM<T, false>(address, value);
    }

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        return ReadReg<T, false, false>(address);
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        WriteReg<T, false>(address, value);
    }

    void SetCPUEnabled(bool enabled);

private:
    m68k::MC68EC000 m_m68k;
    bool m_cpuEnabled;

    alignas(16) std::array<uint8, m68k::kM68KWRAMSize> m_WRAM;

    // -------------------------------------------------------------------------
    // MC68EC000-facing bus
    // 8- or 16-bit reads and writes

    friend class m68k::MC68EC000;

    template <mem_access_type T, bool instrFetch>
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

    template <mem_access_type T>
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

    template <mem_access_type T, bool fromM68K, bool instrFetch>
    T ReadWRAM(uint32 address) {
        // TODO: handle memory size bit
        return util::ReadBE<T>(&m_WRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T, bool fromM68K>
    void WriteWRAM(uint32 address, T value) {
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

    template <mem_access_type T, bool fromM68K, bool instrFetch>
    T ReadReg(uint32 address) {
        // static constexpr bool is16 = std::is_same_v<T, uint16>;

        /*auto shiftByte = [](uint16 value) {
            if constexpr (is16) {
                return value >> 16u;
            } else {
                return value;
            }
        };*/

        if constexpr (!instrFetch) {
            // case 0x400: return ReadReg400<is16, true>();
            // case 0x401: return shiftByte(ReadReg400<true, is16>());

            switch (address) {
            case 0x400: return 0;
            case 0x401: return 0;

            case 0x420:
                fmt::println("faked {}-bit SCSP register read via {} from {:03X}", sizeof(T) * 8,
                             (fromM68K ? "M68K" : "SCU"), address);
                return 0x40; // TODO: implement CCR.SCIPD

            default:
                fmt::println("unhandled {}-bit SCSP register read via {} from {:03X}", sizeof(T) * 8,
                             (fromM68K ? "M68K" : "SCU"), address);
                break;
            }
        }
        return 0;
    }

    template <mem_access_type T, bool fromM68K>
    void WriteReg(uint32 address, T value) {
        static constexpr bool is16 = std::is_same_v<T, uint16>;

        uint16 value16 = value;
        if constexpr (!is16) {
            if ((address & 1) == 0) {
                value16 <<= 8u;
            }
        }

        switch (address) {
        case 0x400: WriteReg400<is16, true>(value16); break;
        case 0x401: WriteReg400<true, is16>(value16); break;

        default:
            fmt::println("unhandled {}-bit SCSP register write via {} to {:03X} = {:X}", sizeof(T) * 8,
                         (fromM68K ? "M68K" : "SCU"), address, value);
            break;
        }
    }

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

    // -------------------------------------------------------------------------
    // Registers

    uint32 m_masterVolume;
    bool m_mem4MB;
    bool m_dac18Bits;

    std::array<Slot, 32> m_slots;

    // -------------------------------------------------------------------------
    // Interrupt handling

    m68k::ExceptionVector AcknowledgeInterrupt(uint8 level);
};

} // namespace satemu::scsp

#pragma once

#include "scsp_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/hw/m68k/m68k.hpp>
#include <satemu/hw/m68k/m68k_defs.hpp>

#include <satemu/util/data_ops.hpp>
#include <satemu/util/inline.hpp>

#include <fmt/format.h>

namespace satemu::scsp {

class SCSP {
public:
    SCSP();

    void Reset(bool hard);

    void Advance(uint64 cycles);

    // -------------------------------------------------------------------------
    // SCU-facing bus

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

    template <mem_access_type T, bool fromM68K, bool instrFetch>
    T ReadReg(uint32 address) {
        if constexpr (!instrFetch) {
            switch (address) {
            case 0x420: return 0x40; // TODO: implement CCR.SCIPD

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
        fmt::println("unhandled {}-bit SCSP register write via {} to {:03X} = {:X}", sizeof(T) * 8,
                     (fromM68K ? "M68K" : "SCU"), address, value);
    }

    // -------------------------------------------------------------------------
    // Interrupt handling

    m68k::ExceptionVector AcknowledgeInterrupt(uint8 level);
};

} // namespace satemu::scsp

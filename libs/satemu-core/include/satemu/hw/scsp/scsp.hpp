#pragma once

#include "scsp_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/hw/m68k/m68k.hpp>
#include <satemu/hw/m68k/m68k_defs.hpp>

#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

namespace satemu::scsp {

class SCSP {
public:
    SCSP();

    void Reset(bool hard);

    void Advance(uint64 cycles);

    template <mem_access_type T>
    T ReadWRAM(uint32 address) {
        // TODO: handle memory size bit
        return util::ReadBE<T>(&m_WRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T>
    void WriteWRAM(uint32 address, T value) {
        // TODO: handle memory size bit
        util::WriteBE<T>(&m_WRAM[address & 0x7FFFF], value);
    }

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        fmt::println("unhandled {}-bit SCSP register read from {:03X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        fmt::println("unhandled {}-bit SCSP register write to {:03X} = {:X}", sizeof(T) * 8, address, value);
    }

    void SetCPUEnabled(bool enabled);

private:
    m68k::MC68EC000 m_m68k;
    bool m_cpuEnabled;

    std::array<uint8, m68k::kM68KWRAMSize> m_WRAM;

    // -------------------------------------------------------------------------
    // MC68EC000 bus

    friend class m68k::MC68EC000;

    template <mem_access_type T>
    T Read(uint32 address) {
        if (util::AddressInRange<0x000000, 0x0FFFFF>(address)) {
            // TODO: handle memory size bit
            return ReadWRAM<T>(address);
        } else if (util::AddressInRange<0x100000, 0x1FFFFF>(address)) {
            return ReadReg<T>(address & 0xFFF);
        } else {
            return 0;
        }
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        if (util::AddressInRange<0x000000, 0x0FFFFF>(address)) {
            WriteWRAM<T>(address, value);
        } else if (util::AddressInRange<0x100000, 0x1FFFFF>(address)) {
            WriteReg<T>(address & 0xFFF, value);
        }
    }
};

} // namespace satemu::scsp

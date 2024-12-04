#pragma once

#include "scsp_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/hw/m68k/m68k.hpp>
#include <satemu/hw/m68k/m68k_bus.hpp>

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
        return m_bus.Read<T>(address);
    }

    template <mem_access_type T>
    void WriteWRAM(uint32 address, T value) {
        // TODO: handle memory size bit
        // TODO: delay writes?
        m_bus.Write<T>(address, value);
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

private:
    m68k::M68EC000 m_m68k;
    m68k::M68kBus m_bus;
};

} // namespace satemu::scsp

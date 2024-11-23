#pragma once

#include "scsp_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu {

class SCSP {
public:
    SCSP();

    void Reset(bool hard);

    template <mem_access_type T>
    T ReadWRAM(uint32 address) {
        // TODO: handle memory size bit
        return util::ReadBE<T>(&m_m68kWRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T>
    void WriteWRAM(uint32 address, T value) {
        // TODO: handle memory size bit
        // TODO: delay writes?
        util::WriteBE<T>(&m_m68kWRAM[address & 0x7FFFF], value);
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
    std::array<uint8, kM68KWRAMSize> m_m68kWRAM;
};

} // namespace satemu

#pragma once

#include "m68k_defs.hpp"

#include <satemu/core_types.hpp>
#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/data_ops.hpp>

#include <array>

namespace satemu::m68k {

class M68kBus {
public:
    M68kBus();

    void Reset();

    template <mem_access_type T>
    T Read(uint32 address) {
        // TODO: handle memory size bit
        return util::ReadBE<T>(&m_WRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        // TODO: handle memory size bit
        // TODO: delay writes?
        util::WriteBE<T>(&m_WRAM[address & 0x7FFFF], value);
    }

private:
    std::array<uint8, kM68KWRAMSize> m_WRAM;
};

} // namespace satemu::m68k

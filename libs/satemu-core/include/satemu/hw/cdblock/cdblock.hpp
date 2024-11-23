#pragma once

#include "cdblock_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu {

class CDBlock {
public:
    CDBlock();

    void Reset(bool hard);

    template <mem_access_type T>
    T Read(uint32 address) {
        fmt::println("CD Block read from {:02X}", address);
        // TODO: implement properly; we're just stubbing the CDBLOCK init sequence here
        switch (address) {
        case 0x08:
            // MEGA HACK to get past the boot sequence
            return std::is_same_v<T, uint8> ? (0x400 >> (((address & 1) ^ 1) * 8)) : 0x400;
        case 0x18: return m_CR[0];
        case 0x1C: return m_CR[1];
        case 0x20: return m_CR[2];
        case 0x24: {
            uint16 result = m_CR[3];

            // MEGA HACK! replace with a blank periodic report to get past the boot sequence
            // TODO: implement periodic CD status reporting *properly*
            m_CR[0] = 0x20FF;
            m_CR[1] = 0xFFFF;
            m_CR[2] = 0xFFFF;
            m_CR[3] = 0xFFFF;

            return result;
        }
        default: fmt::println("unhandled {}-bit CD Block read from {:02X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        fmt::println("unhandled {}-bit CD Block write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }

private:
    std::array<uint16, 4> m_CR;
};

} // namespace satemu

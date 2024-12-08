#pragma once

#include "cdblock_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <fmt/format.h>

#include <array>
#include <type_traits>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

// -----------------------------------------------------------------------------

namespace satemu::cdblock {

class CDBlock {
public:
    CDBlock(scu::SCU &scu);

    void Reset(bool hard);

    void Advance(uint64 cycles);

    // TODO: handle 8-bit and 32-bit accesses properly

    template <mem_access_type T>
    T Read(uint32 address) {
        fmt::println("{}-bit CD Block read from {:02X}", sizeof(T) * 8, address);
        // TODO: implement properly; we're just stubbing the CDBLOCK init sequence here
        switch (address) {
        case 0x08: // return m_HIRQ;
            // MEGA HACK to get past the boot sequence
            return std::is_same_v<T, uint8> ? (0x601 >> (((address & 1) ^ 1) * 8)) : 0x601;
        case 0x0C: return m_HIRQMASK;
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
        switch (address) {
        case 0x08:
            m_HIRQ &= value;
            UpdateInterrupts();
            break;
        case 0x0C: m_HIRQMASK = value; break;

        default: fmt::println("unhandled {}-bit CD Block write to {:02X} = {:X}", sizeof(T) * 8, address, value); break;
        }
    }

private:
    scu::SCU &m_scu;

    std::array<uint16, 4> m_CR;

    // -------------------------------------------------------------------------
    // Interrupts

    uint16 m_HIRQ;
    uint16 m_HIRQMASK;

    void UpdateInterrupts();
};

} // namespace satemu::cdblock

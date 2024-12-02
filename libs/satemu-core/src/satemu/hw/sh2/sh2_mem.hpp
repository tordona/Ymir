#pragma once

#include <satemu/core_types.hpp>

#include <satemu/hw/hw_defs.hpp>
#include <satemu/hw/sh2/sh2_bus.hpp>
#include <satemu/hw/sh2/sh2_state.hpp>

#include <satemu/util/inline.hpp>
#include <satemu/util/unreachable.hpp>

#include <fmt/format.h>

#include <type_traits>

namespace satemu::sh2 {

// -----------------------------------------------------------------------------
// Memory accessors

// According to the SH7604 manual, the address space is divided into these areas:
//
// Address range            Space                           Memory
// 0x00000000..0x01FFFFFF   CS0 space, cache area           Ordinary space or burst ROM
// 0x02000000..0x03FFFFFF   CS1 space, cache area           Ordinary space
// 0x04000000..0x05FFFFFF   CS2 space, cache area           Ordinary space or synchronous DRAM
// 0x06000000..0x07FFFFFF   CS3 space, cache area           Ordinary space, synchronous SDRAM, DRAM or pseudo-DRAM
// 0x08000000..0x1FFFFFFF   Reserved
// 0x20000000..0x21FFFFFF   CS0 space, cache-through area   Ordinary space or burst ROM
// 0x22000000..0x23FFFFFF   CS1 space, cache-through area   Ordinary space
// 0x24000000..0x25FFFFFF   CS2 space, cache-through area   Ordinary space or synchronous DRAM
// 0x26000000..0x27FFFFFF   CS3 space, cache-through area   Ordinary space, synchronous SDRAM, DRAM or pseudo-DRAM
// 0x28000000..0x3FFFFFFF   Reserved
// 0x40000000..0x47FFFFFF   Associative purge space
// 0x48000000..0x5FFFFFFF   Reserved
// 0x60000000..0x7FFFFFFF   Address array, read/write space
// 0x80000000..0x9FFFFFFF   Reserved  [undocumented mirror of 0xC0000000..0xDFFFFFFF]
// 0xA0000000..0xBFFFFFFF   Reserved  [undocumented mirror of 0x20000000..0x3FFFFFFF]
// 0xC0000000..0xC0000FFF   Data array, read/write space
// 0xC0001000..0xDFFFFFFF   Reserved
// 0xE0000000..0xFFFF7FFF   Reserved
// 0xFFFF8000..0xFFFFBFFF   For setting synchronous DRAM mode
// 0xFFFFC000..0xFFFFFDFF   Reserved
// 0xFFFFFE00..0xFFFFFFFF   On-chip peripheral modules
//
// The cache uses address bits 31..29 to specify its behavior:
//    Bits  Partition                       Cache operation
//    000   Cache area                      Cache used when CCR.CE=1
//    001   Cache-through area              Cache bypassed
//    010   Associative purge area          Purge accessed cache lines (reads return 0x2312)
//    011   Address array read/write area   Cache addresses acessed directly (1 KiB, mirrored)
//    100   [undocumented, same as 110]
//    101   [undocumented, same as 001]
//    110   Data array read/write area      Cache data acessed directly (4 KiB, mirrored)
//    111   I/O area (on-chip registers)    Cache bypassed

// Returns 00 00 00 01 00 02 00 03 00 04 00 05 00 06 00 07 ... repeating
template <mem_access_type T>
inline T OpenBusSeqRead(uint32 address) {
    if constexpr (std::is_same_v<T, uint8>) {
        return (address & 1u) * ((address >> 1u) & 0x7);
        // return OpenBusSeqRead<uint16>(address) >> (((address & 1) ^ 1) * 8);
    } else if constexpr (std::is_same_v<T, uint16>) {
        return (address >> 1u) & 0x7;
    } else if constexpr (std::is_same_v<T, uint32>) {
        return (OpenBusSeqRead<uint16>(address + 1) << 16u) | OpenBusSeqRead<uint16>(address);
    }
}

template <mem_access_type T>
inline T MemRead(SH2State &state, SH2Bus &bus, uint32 address) {
    const uint32 partition = (address >> 29u) & 0b111;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        fmt::println("WARNING: misaligned {}-bit read from {:08X}", sizeof(T) * 8, address);
        // TODO: address error (misaligned access)
        // - might have to store data in a class member instead of returning
    }

    switch (partition) {
    case 0b000: // cache
        if (state.CCR.CE) {
            // TODO: use cache
        }
        // fallthrough
    case 0b001:
    case 0b101: // cache-through
        return bus.Read<T>(address & 0x7FFFFFF);
    case 0b010: // associative purge

        // TODO: implement
        fmt::println("unhandled {}-bit SH-2 associative purge read from {:08X}", sizeof(T) * 8, address);
        return (address & 1) ? static_cast<T>(0x12231223) : static_cast<T>(0x23122312);
    case 0b011: { // cache address array
        uint32 entry = (address >> 4u) & 0x3F;
        return state.cacheEntries[entry].tag[state.CCR.Wn]; // TODO: include LRU data
    }
    case 0b100:
    case 0b110: // cache data array

        // TODO: implement
        fmt::println("unhandled {}-bit SH-2 cache data array read from {:08X}", sizeof(T) * 8, address);
        return 0;
    case 0b111: // I/O area
        if ((address & 0xE0004000) == 0xE0004000) {
            // bits 31-29 and 14 must be set
            // bits 8-0 index the register
            // bits 28 and 12 must be both set to access the lower half of the registers
            if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                return state.OnChipRegRead<T>(address & 0x1FF);
            } else {
                return OpenBusSeqRead<T>(address);
            }
        } else {
            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    util::unreachable();
}

template <mem_access_type T>
inline void MemWrite(SH2State &state, SH2Bus &bus, uint32 address, T value) {
    const uint32 partition = address >> 29u;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        fmt::println("WARNING: misaligned {}-bit write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        // TODO: address error (misaligned access)
    }

    switch (partition) {
    case 0b000: // cache
        if (state.CCR.CE) {
            // TODO: use cache
        }
        // fallthrough
    case 0b001:
    case 0b101: // cache-through
        bus.Write<T>(address & 0x7FFFFFF, value);
        break;
    case 0b010: // associative purge
        // TODO: implement
        fmt::println("unhandled {}-bit SH-2 associative purge write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    case 0b011: { // cache address array
        uint32 entry = (address >> 4u) & 0x3F;
        state.cacheEntries[entry].tag[state.CCR.Wn] = address & 0x1FFFFC04;
        // TODO: update LRU data
        break;
    }
    case 0b100:
    case 0b110: // cache data array
        // TODO: implement
        fmt::println("unhandled {}-bit SH-2 cache data array write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    case 0b111: // I/O area
        if ((address & 0xE0004000) == 0xE0004000) {
            // bits 31-29 and 14 must be set
            // bits 8-0 index the register
            // bits 28 and 12 must be both set to access the lower half of the registers
            if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                state.OnChipRegWrite<T>(address & 0x1FF, value);
            }
        } else if ((address >> 12u) == 0xFFFF8) {
            // DRAM setup stuff
            switch (address) {
            case 0xFFFF8426: fmt::println("16-bit CAS latency 1"); break;
            case 0xFFFF8446: fmt::println("16-bit CAS latency 2"); break;
            case 0xFFFF8466: fmt::println("16-bit CAS latency 3"); break;
            case 0xFFFF8848: fmt::println("32-bit CAS latency 1"); break;
            case 0xFFFF8888: fmt::println("32-bit CAS latency 2"); break;
            case 0xFFFF88C8: fmt::println("32-bit CAS latency 3"); break;
            default: fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address); break;
            }
        } else {
            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 I/O area write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        }
        break;
    }
}

FLATTEN FORCE_INLINE uint8 MemReadByte(SH2State &state, SH2Bus &bus, uint32 address) {
    return MemRead<uint8>(state, bus, address);
}

FLATTEN FORCE_INLINE uint16 MemReadWord(SH2State &state, SH2Bus &bus, uint32 address) {
    return MemRead<uint16>(state, bus, address);
}

FLATTEN FORCE_INLINE uint32 MemReadLong(SH2State &state, SH2Bus &bus, uint32 address) {
    return MemRead<uint32>(state, bus, address);
}

FLATTEN FORCE_INLINE void MemWriteByte(SH2State &state, SH2Bus &bus, uint32 address, uint8 value) {
    MemWrite<uint8>(state, bus, address, value);
}

FLATTEN FORCE_INLINE void MemWriteWord(SH2State &state, SH2Bus &bus, uint32 address, uint16 value) {
    MemWrite<uint16>(state, bus, address, value);
}

FLATTEN FORCE_INLINE void MemWriteLong(SH2State &state, SH2Bus &bus, uint32 address, uint32 value) {
    MemWrite<uint32>(state, bus, address, value);
}

} // namespace satemu::sh2

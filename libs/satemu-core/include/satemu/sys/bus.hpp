#pragma once

#include <satemu/core/types.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>
#include <satemu/util/debug_print.hpp>
#include <satemu/util/inline.hpp>
#include <satemu/util/unreachable.hpp>

namespace satemu::sys {

// Represents a memory bus interconnecting various components in the system.
//
// MapMemory assigns read/write functions to a range of addresses.
// UnmapMemory clears the assignments.
//
// Read and Write perform reads and writes with all side-effects and restrictions imposed by the hardware.
// Peek and Poke bypass restrictions and don't cause any side-effects. These are meant to be used by debuggers.
class Bus {
    static constexpr dbg::Category rootLog{"Bus"};

    static constexpr uint32 kAddressBits = 27;
    static constexpr uint32 kAddressMask = (1u << kAddressBits) - 1;
    static constexpr uint32 kPageGranularityBits = 16;
    static constexpr uint32 kPageMask = (1u << kPageGranularityBits) - 1;
    static constexpr uint32 kPageCount = (1u << (kAddressBits - kPageGranularityBits));

public:
    using FnRead8 = uint8 (*)(uint32 address, void *ctx);
    using FnRead16 = uint16 (*)(uint32 address, void *ctx);
    using FnRead32 = uint32 (*)(uint32 address, void *ctx);

    using FnWrite8 = void (*)(uint32 address, uint8 value, void *ctx);
    using FnWrite16 = void (*)(uint32 address, uint16 value, void *ctx);
    using FnWrite32 = void (*)(uint32 address, uint32 value, void *ctx);

    struct MemoryPage {
        void *ctx = nullptr;

        FnRead8 read8 = [](uint32 address, void *) -> uint8 {
            rootLog.debug("Unhandled 8-bit read from {:07X}", address);
            return 0;
        };
        FnRead16 read16 = [](uint32 address, void *) -> uint16 {
            rootLog.debug("Unhandled 16-bit read from {:07X}", address);
            return 0;
        };
        FnRead32 read32 = [](uint32 address, void *) -> uint32 {
            rootLog.debug("Unhandled 32-bit read from {:07X}", address);
            return 0;
        };

        FnWrite8 write8 = [](uint32 address, uint8 value, void *) {
            rootLog.debug("Unhandled 8-bit write to {:07X} = {:02X}", address, value);
        };
        FnWrite16 write16 = [](uint32 address, uint16 value, void *) {
            rootLog.debug("Unhandled 16-bit write to {:07X} = {:04X}", address, value);
        };
        FnWrite32 write32 = [](uint32 address, uint32 value, void *) {
            rootLog.debug("Unhandled 32-bit write to {:07X} = {:07X}", address, value);
        };

        FnRead8 peek8 = [](uint32 address, void *) -> uint8 { return 0; };
        FnRead16 peek16 = [](uint32 address, void *) -> uint16 { return 0; };
        FnRead32 peek32 = [](uint32 address, void *) -> uint32 { return 0; };

        FnWrite8 poke8 = [](uint32 address, uint8 value, void *) {};
        FnWrite16 poke16 = [](uint32 address, uint16 value, void *) {};
        FnWrite32 poke32 = [](uint32 address, uint32 value, void *) {};
    };

    template <size_t N>
        requires(bit::is_power_of_two(N))
    void MapMemory(uint32 start, uint32 end, std::array<uint8, N> &array, bool writable) {
        static constexpr uint32 kAddrMask = N - 1;

        const uint32 startIndex = start >> kPageGranularityBits;
        const uint32 endIndex = end >> kPageGranularityBits;
        for (uint32 i = startIndex; i <= endIndex; i++) {
            m_pages[i].ctx = array.data();

            m_pages[i].read8 = m_pages[i].peek8 = [](uint32 address, void *ctx) -> uint8 {
                return static_cast<uint8 *>(ctx)[address & kAddrMask];
            };
            m_pages[i].read16 = m_pages[i].peek16 = [](uint32 address, void *ctx) -> uint16 {
                return util::ReadBE<uint16>(&static_cast<uint8 *>(ctx)[address & kAddrMask]);
            };
            m_pages[i].read32 = m_pages[i].peek32 = [](uint32 address, void *ctx) -> uint32 {
                return util::ReadBE<uint32>(&static_cast<uint8 *>(ctx)[address & kAddrMask]);
            };

            if (writable) {
                m_pages[i].write8 = m_pages[i].poke8 = [](uint32 address, uint8 value, void *ctx) {
                    static_cast<uint8 *>(ctx)[address & kAddrMask] = value;
                };
                m_pages[i].write16 = m_pages[i].poke16 = [](uint32 address, uint16 value, void *ctx) {
                    util::WriteBE<uint16>(&static_cast<uint8 *>(ctx)[address & kAddrMask], value);
                };
                m_pages[i].write32 = m_pages[i].poke32 = [](uint32 address, uint32 value, void *ctx) {
                    return util::WriteBE<uint32>(&static_cast<uint8 *>(ctx)[address & kAddrMask], value);
                };
            } else {
                m_pages[i].write8 = m_pages[i].poke8 = [](uint32, uint8, void *) {};
                m_pages[i].write16 = m_pages[i].poke16 = [](uint32, uint16, void *) {};
                m_pages[i].write32 = m_pages[i].poke32 = [](uint32, uint32, void *) {};
            }
        }
    }

    void MapMemory(uint32 start, uint32 end, MemoryPage entry);
    void UnmapMemory(uint32 start, uint32 end);

    template <mem_primitive T>
    FLATTEN FORCE_INLINE T Read(uint32 address) const {
        address &= kAddressMask & ~(sizeof(T) - 1);

        const MemoryPage &entry = m_pages[address >> kPageGranularityBits];

        if constexpr (std::is_same_v<T, uint8>) {
            return entry.read8(address, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint16>) {
            return entry.read16(address, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint32>) {
            return entry.read32(address, entry.ctx);
        } else {
            // should never happen
            util::unreachable();
        }
    }

    template <mem_primitive T>
    FLATTEN FORCE_INLINE void Write(uint32 address, T value) {
        address &= kAddressMask & ~(sizeof(T) - 1);

        const MemoryPage &entry = m_pages[address >> kPageGranularityBits];

        if constexpr (std::is_same_v<T, uint8>) {
            entry.write8(address, value, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint16>) {
            entry.write16(address, value, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint32>) {
            entry.write32(address, value, entry.ctx);
        } else {
            // should never happen
            util::unreachable();
        }
    }

    template <mem_primitive T>
    FLATTEN FORCE_INLINE T Peek(uint32 address) const {
        address &= kAddressMask & ~(sizeof(T) - 1);

        const MemoryPage &entry = m_pages[address >> kPageGranularityBits];

        if constexpr (std::is_same_v<T, uint8>) {
            return entry.peek8(address, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint16>) {
            return entry.peek16(address, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint32>) {
            return entry.peek32(address, entry.ctx);
        } else {
            // should never happen
            util::unreachable();
        }
    }

    template <mem_primitive T>
    FLATTEN FORCE_INLINE void Poke(uint32 address, T value) {
        address &= kAddressMask & ~(sizeof(T) - 1);

        const MemoryPage &entry = m_pages[address >> kPageGranularityBits];

        if constexpr (std::is_same_v<T, uint8>) {
            entry.poke8(address, value, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint16>) {
            entry.poke16(address, value, entry.ctx);
        } else if constexpr (std::is_same_v<T, uint32>) {
            entry.poke32(address, value, entry.ctx);
        } else {
            // should never happen
            util::unreachable();
        }
    }

private:
    std::array<MemoryPage, kPageCount> m_pages;
};

} // namespace satemu::sys

#pragma once

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/debug_print.hpp>
#include <satemu/util/inline.hpp>
#include <satemu/util/unreachable.hpp>

namespace satemu::sys {

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
    };

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

private:
    std::array<MemoryPage, kPageCount> m_pages;
};

} // namespace satemu::sys

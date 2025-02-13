#pragma once

#include "sh2_bus_defs.hpp"

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/hw_defs.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/smpc/smpc.hpp>

#include <satemu/util/data_ops.hpp>
#include <satemu/util/unreachable.hpp>

#include <mio/mmap.hpp> // HACK: should be used in a binary reader/writer object

#include <iostream>
#include <span>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::sh2 {

class SH2;

} // namespace satemu::sh2

// -----------------------------------------------------------------------------

namespace satemu::sh2 {

// SH-2 memory map
// https://wiki.yabause.org/index.php5?title=SH-2CPU
//
// TODO? Address range            Mirror size   Description
//       0x000'0000..0x00F'FFFF   0x80000       Boot ROM / IPL
//       0x010'0000..0x017'FFFF   0x80          SMPC registers
// TODO  0x018'0000..0x01F'FFFF   0x10000       Backup RAM
//       0x020'0000..0x02F'FFFF   0x100000      Work RAM Low
// TODO  0x030'0000..0x03F'FFFF   -             Open bus? (reads random data, mostly 0x00)
// TODO  0x040'0000..0x07F'FFFF   -             Reads 0x0000
// TODO  0x080'0000..0x0FF'FFFF   -             Reads 0x0000 0x0001 0x0002 0x0003 0x0004 0x0005 0x0006 0x0007
// TODO  0x100'0000..0x17F'FFFF   -             Reads 0xFFFF; writes go to slave SH-2 FRT  (MINIT area)
// TODO  0x180'0000..0x1FF'FFFF   -             Reads 0xFFFF; writes go to master SH-2 FRT (SINIT area)
//       0x200'0000..0x58F'FFFF   -             SCU A-Bus (cartridge interface, CD block)
//       0x590'0000..0x59F'FFFF   -             Lock-up when read
//       0x5A0'0000..0x5FB'FFFF   -             SCU B-Bus (SCSP, VDP1, VDP2)
//       0x600'0000..0x7FF'FFFF   0x100000      Work RAM High
//
// Notes
// - Unless otherwise specified, all regions are mirrored across the designated area
// - Backup RAM
//   - Only odd bytes mapped
//   - Reads from even bytes return 0xFF
//   - Writes to even bytes map to correspoding odd byte
//
// SH-2 has access to SCU, SMPC, 2 MiB WRAM and 512 KiB IPL ROM
// SCU has access to VDP1, VDP2, SCSP, CD Block and the cartridge interface
// SMPC has access to peripherals (gamepads)
// VDP1 has 1 MiB RAM (2x 256 KiB framebuffers + 512 KiB VRAM)
// VDP2 has 516 KiB RAM (4x 128 KiB VRAM banks A0 A1 B0 B1 + 4 KiB color RAM)
// SCSP contains the MC68EC000 and 512 KiB of RAM
class SH2Bus {
    static constexpr dbg::Category rootLog{"SH2Bus"};

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

    struct MemoryRegionEntry {
        void *ctx = nullptr;

        FnRead8 read8 = [](uint32, void *) -> uint8 { return 0; };
        FnRead16 read16 = [](uint32, void *) -> uint16 { return 0; };
        FnRead32 read32 = [](uint32, void *) -> uint32 { return 0; };

        FnWrite8 write8 = [](uint32, uint8, void *) {};
        FnWrite16 write16 = [](uint32, uint16, void *) {};
        FnWrite32 write32 = [](uint32, uint32, void *) {};
    };

    SH2Bus(SH2 &masterSH2, SH2 &slaveSH2, scu::SCU &scu, smpc::SMPC &smpc);

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    void DumpWRAMLow(std::ostream &out) const;
    void DumpWRAMHigh(std::ostream &out) const;

    void MapMemory(uint32 start, uint32 end, MemoryRegionEntry entry);
    void UnmapMemory(uint32 start, uint32 end);

    template <mem_primitive T>
    T Read(uint32 address) {
        address &= kAddressMask & ~(sizeof(T) - 1);

        const MemoryRegionEntry &entry = m_pages[address >> kPageGranularityBits];

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
    void Write(uint32 address, T value) {
        address &= kAddressMask & ~(sizeof(T) - 1);

        const MemoryRegionEntry &entry = m_pages[address >> kPageGranularityBits];

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

    void AcknowledgeExternalInterrupt();

    alignas(16) std::array<uint8, kIPLSize> IPL; // aka BIOS ROM
    alignas(16) std::array<uint8, kWRAMLowSize> WRAMLow;
    alignas(16) std::array<uint8, kWRAMHighSize> WRAMHigh;

    // TODO: don't hardcode this
    // TODO: use an abstraction
    // TODO: move to its own class
    // std::array<uint8, kInternalBackupRAMSize> internalBackupRAM;
    mio::mmap_sink internalBackupRAM;

private:
    SH2 &m_masterSH2;
    SH2 &m_slaveSH2;
    scu::SCU &m_SCU;
    smpc::SMPC &m_SMPC;

    std::array<MemoryRegionEntry, kPageCount> m_pages;

    void WriteMINIT(uint16 value);
    void WriteSINIT(uint16 value);
};

} // namespace satemu::sh2

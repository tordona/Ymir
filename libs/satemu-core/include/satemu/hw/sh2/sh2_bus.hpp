#pragma once

#include "sh2_bus_defs.hpp"

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/hw_defs.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/smpc/smpc.hpp>

#include <satemu/util/data_ops.hpp>

#include <span>

namespace satemu {

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
//       0x200'0000..0x3FF'FFFF   -             SCU A-Bus (cartridge interface)
//       0x400'0000..0x5FF'FFFF   -             SCU B-Bus (VDP1, VDP2)
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
public:
    SH2Bus(SCU &scu, SMPC &smpc);

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    template <mem_access_type T>
    T Read(uint32 address) {
        address &= ~(sizeof(T) - 1);

        // TODO: consider using a LUT

        using namespace util;

        /****/ if (AddressInRange<0x000'0000, 0x00F'FFFF>(address)) {
            return ReadBE<T>(&m_IPL[address & 0x7FFFF]);
        } else if (AddressInRange<0x010'0000, 0x017'FFFF>(address)) {
            return m_SMPC.Read((address & 0x7F) | 1);
        } else if (AddressInRange<0x020'0000, 0x02F'FFFF>(address)) {
            return ReadBE<T>(&m_WRAMLow[address & 0xFFFFF]);
        } else if (AddressInRange<0x200'0000, 0x5FF'FFFF>(address)) {
            return m_SCU.Read<T>(address);
        } else if (AddressInRange<0x600'0000, 0x7FF'FFFF>(address)) {
            return ReadBE<T>(&m_WRAMHigh[address & 0xFFFFF]);
        } else {
            fmt::println("unhandled {}-bit SH2 bus read from {:08X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        address &= ~(sizeof(T) - 1);

        // TODO: consider using a LUT

        using namespace util;

        /****/ if (AddressInRange<0x010'0000, 0x017'FFFF>(address)) {
            m_SMPC.Write((address & 0x7F) | 1, value);
        } else if (AddressInRange<0x020'0000, 0x02F'FFFF>(address)) {
            WriteBE<T>(&m_WRAMLow[address & 0xFFFFF], value);
        } else if (AddressInRange<0x200'0000, 0x5FF'FFFF>(address)) {
            m_SCU.Write<T>(address, value);
        } else if (AddressInRange<0x600'0000, 0x7FF'FFFF>(address)) {
            WriteBE<T>(&m_WRAMHigh[address & 0xFFFFF], value);
        } else {
            fmt::println("unhandled {}-bit SH2 bus write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        }
    }

private:
    std::array<uint8, kIPLSize> m_IPL; // aka BIOS ROM
    std::array<uint8, kWRAMLowSize> m_WRAMLow;
    std::array<uint8, kWRAMHighSize> m_WRAMHigh;

    SCU &m_SCU;
    SMPC &m_SMPC;
};

} // namespace satemu

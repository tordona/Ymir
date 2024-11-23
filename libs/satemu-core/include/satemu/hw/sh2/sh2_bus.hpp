#pragma once

#include "sh2_bus_defs.hpp"

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/hw_defs.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/smpc/smpc.hpp>

#include <satemu/util/data_ops.hpp>

#include <span>

namespace satemu {

// SH-2 memory map
// https://wiki.yabause.org/index.php5?title=SH-2CPU
//
// TODO? Address range            Mirror size       Description
//       0x00000000..0x000FFFFF   0x80000           Boot ROM / IPL
//       0x00100000..0x0017FFFF   0x80              SMPC registers
// TODO  0x00180000..0x001FFFFF   0x10000           Backup RAM
//       0x00200000..0x002FFFFF   0x100000          Work RAM Low
// TODO  0x00300000..0x003FFFFF   -                 Open bus? (reads random data, mostly 0x00)
// TODO  0x00400000..0x007FFFFF   -                 Reads 0x0000
// TODO  0x00800000..0x00FFFFFF   -                 Reads 0x0000 0x0001 0x0002 0x0003 0x0004 0x0005 0x0006 0x0007
// TODO  0x01000000..0x017FFFFF   -                 Reads 0xFFFF; writes go to slave SH-2 FRT  (MINIT area)
// TODO  0x01800000..0x01FFFFFF   -                 Reads 0xFFFF; writes go to master SH-2 FRT (SINIT area)
// TODO  0x02000000..0x03FFFFFF   -                 A-Bus CS0
// TODO  0x04000000..0x04FFFFFF   -                 A-Bus CS1
// TODO  0x05000000..0x057FFFFF   -                 A-Bus Dummy
//       0x05800000..0x058FFFFF   -                 A-Bus CS2 (includes CD-ROM registers)
// TODO  0x05900000..0x059FFFFF   -                 Lockup when read
//       0x05A00000..0x05AFFFFF   0x40000/0x80000   68000 Work RAM
//       0x05B00000..0x05BFFFFF   0x1000            SCSP registers
// TODO  0x05C00000..0x05C7FFFF   0x80000           VDP1 VRAM
// TODO  0x05C80000..0x05CFFFFF   0x40000           VDP1 Framebuffer (backbuffer only)
// TODO  0x05D00000..0x05D7FFFF   0x18 (no mirror)  VDP1 Registers
// TODO  0x05D80000..0x05DFFFFF   -                 Lockup when read
// TODO  0x05E00000..0x05EFFFFF   0x80000           VDP2 VRAM
// TODO  0x05F00000..0x05F7FFFF   0x1000            VDP2 CRAM
// TODO  0x05F80000..0x05FBFFFF   0x200             VDP2 registers
// TODO  0x05FC0000..0x05FDFFFF   -                 Reads 0x000E0000
//       0x05FE0000..0x05FEFFFF   0x100             SCU registers
// TODO  0x05FF0000..0x05FFFFFF   0x100             Unknown registers
//       0x06000000..0x07FFFFFF   0x100000          Work RAM High
//
// Notes
// - Unless otherwise specified, all regions are mirrored across the designated area
// - Backup RAM
//   - Only odd bytes mapped
//   - Reads from even bytes return 0xFF
//   - Writes to even bytes map to correspoding odd byte
// - 68000 Work RAM
//   - Area size depends on MEM4MB bit setting:
//       0=only first 256 KiB are used/mirrored
//       1=all 512 KiB are used/mirrored
// - VDP2 CRAM
//   - Byte writes write garbage to the odd/even byte counterpart
//   - Byte reads work normally
class SH2Bus {
public:
    SH2Bus(SCU &scu, SMPC &smpc, SCSP &scsp, CDBlock &cdblock);

    void Reset(bool hard);

    void LoadIPL(std::span<uint8, kIPLSize> ipl);

    template <mem_access_type T>
    T Read(uint32 address) {
        address &= ~(sizeof(T) - 1);

        // TODO: consider using a LUT

        using namespace util;

        if (AddressInRange<0x0000000, 0x100000>(address)) {
            return ReadBE<T>(&m_IPL[address & 0x7FFFF]);
        } else if (AddressInRange<0x0100000, 0x80000>(address)) {
            return m_SMPC.Read((address & 0x7F) | 1);
        } else if (AddressInRange<0x0200000, 0x100000>(address)) {
            return ReadBE<T>(&m_WRAMLow[address & 0xFFFFF]);
        } else if (AddressInRange<0x5800000, 0x100000>(address)) {
            return CS2Read<T>(address & 0xFFFFF);
        } else if (AddressInRange<0x5A00000, 0x100000>(address)) {
            return m_SCSP.ReadWRAM<T>(address & 0x7FFFF);
        } else if (AddressInRange<0x5B00000, 0x100000>(address)) {
            return m_SCSP.ReadReg<T>(address & 0xFFF);
        } else if (AddressInRange<0x5FE0000, 0x10000>(address)) {
            return m_SCU.Read<T>(address & 0xFF);
        } else if (AddressInRange<0x6000000, 0x2000000>(address)) {
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

        if (AddressInRange<0x100000, 0x80000>(address)) {
            m_SMPC.Write((address & 0x7F) | 1, value);
        } else if (AddressInRange<0x200000, 0x100000>(address)) {
            WriteBE<T>(&m_WRAMLow[address & 0xFFFFF], value);
        } else if (AddressInRange<0x5800000, 0x100000>(address)) {
            CS2Write<T>(address & 0xFFFFF, value);
        } else if (AddressInRange<0x5A00000, 0x100000>(address)) {
            m_SCSP.WriteWRAM<T>(address & 0x7FFFF, value);
        } else if (AddressInRange<0x5B00000, 0x100000>(address)) {
            m_SCSP.WriteReg<T>(address & 0xFFF, value);
        } else if (AddressInRange<0x5FE0000, 0x10000>(address)) {
            m_SCU.Write<T>(address & 0xFF, value);
        } else if (AddressInRange<0x6000000, 0x2000000>(address)) {
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
    SCSP &m_SCSP;
    CDBlock &m_CDBlock;

    template <mem_access_type T>
    T CS2Read(uint32 address) {
        // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
        // These 4 KiB blocks are mapped every 32 KiB, up to 0x25891000.
        if ((address & 0x7FFF) < 0x1000 && address < 0x91000) {
            return m_CDBlock.Read<T>(address & 0x3F);
        }

        fmt::println("unhandled {}-bit A-Bus CS2 read from {:05X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void CS2Write(uint32 address, T value) {
        // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
        // These 4 KiB blocks are mapped every 32 KiB, up to 0x25891000.
        if ((address & 0x7FFF) < 0x1000 && address < 0x91000) {
            m_CDBlock.Write<T>(address & 0x3F, value);
        } else {
            fmt::println("unhandled {}-bit A-Bus CS2 write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        }
    }
};

} // namespace satemu

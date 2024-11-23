#pragma once

#include "scu_defs.hpp"

#include <satemu/hw/cdblock/cdblock.hpp>
#include <satemu/hw/hw_defs.hpp>
#include <satemu/hw/scsp/scsp.hpp>
#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

namespace satemu {

// SCU memory map
//
// TODO? Address range            Mirror size       Description
// TODO  0x200'0000..0x3FF'FFFF   -                 A-Bus CS0
// TODO  0x400'0000..0x4FF'FFFF   -                 A-Bus CS1
// TODO  0x500'0000..0x57F'FFFF   -                 A-Bus Dummy
//       0x580'0000..0x58F'FFFF   -                 A-Bus CS2 (includes CD-ROM registers)
// TODO  0x590'0000..0x59F'FFFF   -                 Lockup when read
//       0x5A0'0000..0x5AF'FFFF   0x40000/0x80000   68000 Work RAM
//       0x5B0'0000..0x5BF'FFFF   0x1000            SCSP registers
//       0x5C0'0000..0x5C7'FFFF   0x80000           VDP1 VRAM
//       0x5C8'0000..0x5CF'FFFF   0x40000           VDP1 Framebuffer (backbuffer only)
//       0x5D0'0000..0x5D7'FFFF   0x18 (no mirror)  VDP1 Registers
// TODO  0x5D8'0000..0x5DF'FFFF   -                 Lockup when read
//       0x5E0'0000..0x5EF'FFFF   0x80000           VDP2 VRAM
//       0x5F0'0000..0x5F7'FFFF   0x1000            VDP2 CRAM
//       0x5F8'0000..0x5FB'FFFF   0x200             VDP2 registers
// TODO  0x5FC'0000..0x5FD'FFFF   -                 Reads 0x000E0000
//       0x5FE'0000..0x5FE'FFFF   0x100             SCU registers
// TODO  0x5FF'0000..0x5FF'FFFF   0x100             Unknown registers
//
// Notes
// - Unless otherwise specified, all regions are mirrored across the designated area
// - Addresses 0x200'0000..0x58F'FFFF comprise the SCU A-Bus
// - Addresses 0x5A0'0000..0x5FB'FFFF comprise the SCU B-Bus
// - [TODO] A-Bus and B-Bus reads are always 32-bit (split into two 16-bit reads internally)
// - [TODO] A-Bus and B-Bus 32-bit writes are split into two 16-bit writes internally
// - 68000 Work RAM
//   - [TODO] Area size depends on MEM4MB bit setting:
//       0=only first 256 KiB are used/mirrored
//       1=all 512 KiB are used/mirrored
// - VDP2 CRAM
//   - [TODO] Byte writes write garbage to the odd/even byte counterpart
//   - Byte reads work normally
class SCU {
public:
    SCU(VDP1 &vdp1, vdp2::VDP2 &vdp2, SCSP &scsp, CDBlock &cdblock);

    void Reset(bool hard);

    template <mem_access_type T>
    T Read(uint32 address) {
        using namespace util;

        /****/ if (AddressInRange<0x580'0000, 0x58F'FFFF>(address)) {
            if ((address & 0x7FFF) < 0x1000 && address < 0x5891000) {
                // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
                // These 4 KiB blocks are mapped every 32 KiB, up to 0x25891000.
                return m_CDBlock.Read<T>(address & 0x3F);
            }

        } else if (AddressInRange<0x5A0'0000, 0x5AF'FFFF>(address)) {
            return m_SCSP.ReadWRAM<T>(address & 0x7FFFF);
        } else if (AddressInRange<0x5B0'0000, 0x5BF'FFFF>(address)) {
            return m_SCSP.ReadReg<T>(address & 0xFFF);

        } else if (AddressInRange<0x5C0'0000, 0x5C7'FFFF>(address)) {
            return m_VDP1.ReadVRAM<T>(address & 0x7FFFF);
        } else if (AddressInRange<0x5C8'0000, 0x5CF'FFFF>(address)) {
            return m_VDP1.ReadFB<T>(address & 0x3FFFF);
        } else if (AddressInRange<0x5D0'0000, 0x5D7'FFFF>(address)) {
            return m_VDP1.ReadReg<T>(address & 0x7FFFF);

        } else if (AddressInRange<0x5E0'0000, 0x5EF'FFFF>(address)) {
            return m_VDP2.ReadVRAM<T>(address & 0x7FFFF);
        } else if (AddressInRange<0x5F0'0000, 0x5F7'FFFF>(address)) {
            return m_VDP2.ReadCRAM<T>(address & 0xFFF);
        } else if (AddressInRange<0x5F8'0000, 0x5FB'FFFF>(address)) {
            return m_VDP2.ReadReg<T>(address & 0x1FF);

        } else if (AddressInRange<0x5FE'0000, 0x5FE'FFFF>(address)) {
            return ReadReg<T>(address & 0xFF);
        }

        fmt::println("unhandled {}-bit SCU read from {:05X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void Write(uint32 address, T value) {
        using namespace util;

        /****/ if (AddressInRange<0x580'0000, 0x58F'FFFF>(address)) {
            if ((address & 0x7FFF) < 0x1000 && address < 0x5891000) {
                // CD Block registers are mirrored every 64 bytes in a 4 KiB block.
                // These 4 KiB blocks are mapped every 32 KiB, up to 0x25891000.
                m_CDBlock.Write<T>(address & 0x3F, value);
            } else {
                fmt::println("unhandled {}-bit SCU write to {:05X} = {:X}", sizeof(T) * 8, address, value);
            }

        } else if (AddressInRange<0x5A0'0000, 0x5AF'FFFF>(address)) {
            m_SCSP.WriteWRAM<T>(address & 0x7FFFF, value);
        } else if (AddressInRange<0x5B0'0000, 0x5BF'FFFF>(address)) {
            m_SCSP.WriteReg<T>(address & 0xFFF, value);

        } else if (AddressInRange<0x5C0'0000, 0x5C7'FFFF>(address)) {
            m_VDP1.WriteVRAM<T>(address & 0x7FFFF, value);
        } else if (AddressInRange<0x5C8'0000, 0x5CF'FFFF>(address)) {
            m_VDP1.WriteFB<T>(address & 0x3FFFF, value);
        } else if (AddressInRange<0x5D0'0000, 0x5D7'FFFF>(address)) {
            m_VDP1.WriteReg<T>(address & 0x7FFFF, value);

        } else if (AddressInRange<0x5E0'0000, 0x5EF'FFFF>(address)) {
            m_VDP2.WriteVRAM<T>(address & 0x7FFFF, value);
        } else if (AddressInRange<0x5F0'0000, 0x5F7'FFFF>(address)) {
            m_VDP2.WriteCRAM<T>(address & 0xFFF, value);
        } else if (AddressInRange<0x5F8'0000, 0x5FB'FFFF>(address)) {
            m_VDP2.WriteReg<T>(address & 0x1FF, value);

        } else if (AddressInRange<0x5FE'0000, 0x5FE'FFFF>(address)) {
            WriteReg<T>(address & 0xFF, value);
        } else {
            fmt::println("unhandled {}-bit SCU write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        }
    }

private:
    VDP1 &m_VDP1;
    vdp2::VDP2 &m_VDP2;
    SCSP &m_SCSP;
    CDBlock &m_CDBlock;

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        fmt::println("unhandled {}-bit SCU register read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        fmt::println("unhandled {}-bit SCU register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
    }
};

} // namespace satemu

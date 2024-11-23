#pragma once

#include "vdp2_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu {

class VDP2 {
public:
    VDP2();

    void Reset(bool hard);

    template <mem_access_type T>
    T ReadVRAM(uint32 address) {
        return util::ReadBE<T>(&m_VRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T>
    void WriteVRAM(uint32 address, T value) {
        util::WriteBE<T>(&m_VRAM[address & 0x7FFFF], value);
    }

    template <mem_access_type T>
    T ReadCRAM(uint32 address) {
        return util::ReadBE<T>(&m_CRAM[MapCRAMAddress(address)]);
    }

    template <mem_access_type T>
    void WriteCRAM(uint32 address, T value) {
        address = MapCRAMAddress(address);
        util::WriteBE<T>(&m_CRAM[address], value);
        if (RAMCTL.CRMDn == 0) {
            util::WriteBE<T>(&m_CRAM[address ^ 0x800], value);
        }
    }

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        switch (address) {
        case 0x00E: return RAMCTL.u16;
        default: fmt::println("unhandled {}-bit VDP2 register read from {:03X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x00E: RAMCTL.u16 = value & 0xB3FF; break;
        default:
            fmt::println("unhandled {}-bit VDP2 register write to {:03X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

private:
    std::array<uint8, kVDP2VRAMSize> m_VRAM; // 4x 128 KiB banks: A0, A1, B0, B1
    std::array<uint8, kCRAMSize> m_CRAM;

    // 18000E   RAMCTL  RAM Control Register
    //
    //   bits   r/w  code          description
    //     15   R/W  CRKTE         Color RAM Coefficient Table Enable Bit
    //     14   R    -             Reserved, must be zero
    //  13-12   R/W  CRMD(1-0)     Color RAM Mode
    //                               00 (0) = RGB 5:5:5, 1024 words
    //                               01 (1) = RGB 5:5:5, 2048 words
    //                               10 (2) = RGB 8:8:8, 1024 words
    //                               11 (3) = RGB 8:8:8, 1024 words  (same as mode 2, undocumented)
    //  11-10   R    -             Reserved, must be zero
    //      9   R/W  VRBMD         VRAM-B Mode Bit (0=single partition, 1=two partitions)
    //      8   R/W  VRAMD         VRAM-A Mode Bit (0=single partition, 1=two partitions)
    //    7-6   R/W  RDBSB1(1-0)   Rotation Data Bank Select Bit for VRAM-B1
    //    5-4   R/W  RDBSB0(1-0)   Rotation Data Bank Select Bit for VRAM-B0 (or VRAM-B)
    //    3-2   R/W  RDBSA1(1-0)   Rotation Data Bank Select Bit for VRAM-A1
    //    1-0   R/W  RDBSA0(1-0)   Rotation Data Bank Select Bit for VRAM-A0 (or VRAM-B)
    union RAMCTL_t {
        uint16 u16;
        struct {
            uint16 RDBSA0n : 2;
            uint16 RDBSA1n : 2;
            uint16 RDBSB0n : 2;
            uint16 RDBSB1n : 2;
            uint16 VRAMD : 1;
            uint16 VRBMD : 1;
            uint16 _rsvd10_11 : 2;
            uint16 CRMDn : 2;
            uint16 _rsvd14 : 1;
            uint16 CRKTE : 1;
        };
    } RAMCTL;

    // CRAM has four modes specified by RAMCTL.CRMD. Each mode has some addressing quirks:
    //   0 - writes to the lower half are mirrored to the upper half and vice-versa
    //   1 - no mirroring
    //   2 and 3 - address bits are shuffled as follows:
    //      10 09 08 07 06 05 04 03 02 01 11 00
    //      in short, bits 10-01 are shifted left and bit 11 takes place of bit 01
    uint32 MapCRAMAddress(uint32 address) const {
        if (RAMCTL.CRMDn == 2 || RAMCTL.CRMDn == 3) {
            address =
                bit::extract<0>(address) | (bit::extract<11>(address) << 1u) | (bit::extract<1, 10>(address) << 2u);
        }
        return address;
    }
};

} // namespace satemu

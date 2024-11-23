#pragma once

#include "vdp2_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu::vdp2 {

class VDP2 {
public:
    VDP2();

    void Reset(bool hard);

    // TODO: handle VRSIZE.VRAMSZ in Read/WriteVRAM maybe?

    template <mem_access_type T>
    T ReadVRAM(uint32 address) {
        /*address &= 0x7FFFF;
        T value = util::ReadBE<T>(&m_VRAM[address]);
        fmt::println("{}-bit VDP2 VRAM read from {:05X} = {:X}", sizeof(T) * 8, address, value);
        return value;*/
        return util::ReadBE<T>(&m_VRAM[address & 0x7FFFF]);
    }

    template <mem_access_type T>
    void WriteVRAM(uint32 address, T value) {
        // fmt::println("{}-bit VDP2 VRAM write to {:05X} = {:X}", sizeof(T) * 8, address & 0x7FFFF, value);
        util::WriteBE<T>(&m_VRAM[address & 0x7FFFF], value);
    }

    template <mem_access_type T>
    T ReadCRAM(uint32 address) {
        /*address &= MapCRAMAddress(address);
        T value = util::ReadBE<T>(&m_CRAM[address]);
        fmt::println("{}-bit VDP2 CRAM read from {:03X} = {:X}", sizeof(T) * 8, address, value);
        return value;*/
        return util::ReadBE<T>(&m_CRAM[MapCRAMAddress(address)]);
    }

    template <mem_access_type T>
    void WriteCRAM(uint32 address, T value) {
        address = MapCRAMAddress(address);
        // fmt::println("{}-bit VDP2 CRAM write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        util::WriteBE<T>(&m_CRAM[address], value);
        if (RAMCTL.CRMDn == 0) {
            // fmt::println("   replicated to {:05X}", address ^ 0x800);
            util::WriteBE<T>(&m_CRAM[address ^ 0x800], value);
        }
    }

    template <mem_access_type T>
    T ReadReg(uint32 address) {
        switch (address) {
        case 0x000: return TVMD.u16;
        case 0x002: return EXTEN.u16;
        // case 0x004: return 0; // TODO: TVSTAT
        case 0x006: return VRSIZE.u16;
        // case 0x008: return 0; // TODO: HCNT
        // case 0x00A: return 0; // TODO: VCNT
        case 0x00E: return RAMCTL.u16;
        case 0x010: return 0; // CYCA0 is write-only
        case 0x012: return 0; // CYCA0 is write-only
        case 0x014: return 0; // CYCA1 is write-only
        case 0x016: return 0; // CYCA1 is write-only
        case 0x018: return 0; // CYCB0 is write-only
        case 0x01A: return 0; // CYCB0 is write-only
        case 0x01E: return 0; // CYCB1 is write-only
        case 0x01C: return 0; // CYCB1 is write-only
        case 0x020: return 0; // BGON is write-only
        case 0x028: return 0; // CHCTLA is write-only
        case 0x02A: return 0; // CHCTLB is write-only
        case 0x030: return 0; // PNCN0 is write-only
        case 0x032: return 0; // PNCN1 is write-only
        case 0x034: return 0; // PNCN2 is write-only
        case 0x036: return 0; // PNCN3 is write-only
        case 0x038: return 0; // PNCR is write-only
        case 0x03A: return 0; // PLSZ is write-only
        case 0x03C: return 0; // MPOFN is write-only
        case 0x03E: return 0; // MPOFR is write-only
        case 0x040: return 0; // MPN0 is write-only
        case 0x042: return 0; // MPN0 is write-only
        case 0x044: return 0; // MPN1 is write-only
        case 0x046: return 0; // MPN1 is write-only
        case 0x048: return 0; // MPN2 is write-only
        case 0x04A: return 0; // MPN2 is write-only
        case 0x04C: return 0; // MPN3 is write-only
        case 0x04E: return 0; // MPN3 is write-only
        case 0x050: return 0; // MPRA is write-only
        case 0x052: return 0; // MPRA is write-only
        case 0x054: return 0; // MPRA is write-only
        case 0x056: return 0; // MPRA is write-only
        case 0x058: return 0; // MPRA is write-only
        case 0x05A: return 0; // MPRA is write-only
        case 0x05C: return 0; // MPRA is write-only
        case 0x05E: return 0; // MPRA is write-only
        case 0x060: return 0; // MPRB is write-only
        case 0x062: return 0; // MPRB is write-only
        case 0x064: return 0; // MPRB is write-only
        case 0x066: return 0; // MPRB is write-only
        case 0x068: return 0; // MPRB is write-only
        case 0x06A: return 0; // MPRB is write-only
        case 0x06C: return 0; // MPRB is write-only
        case 0x06E: return 0; // MPRB is write-only
        default: fmt::println("unhandled {}-bit VDP2 register read from {:03X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x000: TVMD.u16 = value & 0x81F7; break;
        case 0x002: EXTEN.u16 = value & 0x0303; break;
        case 0x004: /* TVSTAT is read-only */ break;
        case 0x006: VRSIZE.u16 = value & 0x8000; break;
        case 0x008: /* HCNT is read-only */ break;
        case 0x00A: /* VCNT is read-only */ break;
        case 0x00E: RAMCTL.u16 = value & 0xB3FF; break;
        case 0x010: CYCA0.L.u16 = value; break;
        case 0x012: CYCA0.U.u16 = value; break;
        case 0x014: CYCA1.L.u16 = value; break;
        case 0x016: CYCA1.U.u16 = value; break;
        case 0x018: CYCB0.L.u16 = value; break;
        case 0x01A: CYCB0.U.u16 = value; break;
        case 0x01E: CYCB1.U.u16 = value; break;
        case 0x01C: CYCB1.L.u16 = value; break;
        case 0x020: BGON.u16 = value & 0x1F3F; break;
        case 0x028: CHCTLA.u16 = value & 0x3F7F; break;
        case 0x02A: CHCTLB.u16 = value & 0x7733; break;
        case 0x030: PNCN0.u16 = value & 0xC3FF; break;
        case 0x032: PNCN1.u16 = value & 0xC3FF; break;
        case 0x034: PNCN2.u16 = value & 0xC3FF; break;
        case 0x036: PNCN3.u16 = value & 0xC3FF; break;
        case 0x038: PNCR.u16 = value & 0xC3FF; break;
        case 0x03A: PLSZ.u16 = value; break;
        case 0x03C: MPOFN.u16 = value & 0x7777; break;
        case 0x03E: MPOFR.u16 = value & 0x0077; break;
        case 0x040: MPN0.AB.u16 = value & 0x3F3F; break;
        case 0x042: MPN0.CD.u16 = value & 0x3F3F; break;
        case 0x044: MPN1.AB.u16 = value & 0x3F3F; break;
        case 0x046: MPN1.CD.u16 = value & 0x3F3F; break;
        case 0x048: MPN2.AB.u16 = value & 0x3F3F; break;
        case 0x04A: MPN2.CD.u16 = value & 0x3F3F; break;
        case 0x04C: MPN3.AB.u16 = value & 0x3F3F; break;
        case 0x04E: MPN3.CD.u16 = value & 0x3F3F; break;
        case 0x050: MPRA.AB.u16 = value & 0x3F3F; break;
        case 0x052: MPRA.CD.u16 = value & 0x3F3F; break;
        case 0x054: MPRA.EF.u16 = value & 0x3F3F; break;
        case 0x056: MPRA.GH.u16 = value & 0x3F3F; break;
        case 0x058: MPRA.IJ.u16 = value & 0x3F3F; break;
        case 0x05A: MPRA.KL.u16 = value & 0x3F3F; break;
        case 0x05C: MPRA.MN.u16 = value & 0x3F3F; break;
        case 0x05E: MPRA.OP.u16 = value & 0x3F3F; break;
        case 0x060: MPRB.AB.u16 = value & 0x3F3F; break;
        case 0x062: MPRB.CD.u16 = value & 0x3F3F; break;
        case 0x064: MPRB.EF.u16 = value & 0x3F3F; break;
        case 0x066: MPRB.GH.u16 = value & 0x3F3F; break;
        case 0x068: MPRB.IJ.u16 = value & 0x3F3F; break;
        case 0x06A: MPRB.KL.u16 = value & 0x3F3F; break;
        case 0x06C: MPRB.MN.u16 = value & 0x3F3F; break;
        case 0x06E: MPRB.OP.u16 = value & 0x3F3F; break;
        default:
            fmt::println("unhandled {}-bit VDP2 register write to {:03X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

private:
    std::array<uint8, kVDP2VRAMSize> m_VRAM; // 4x 128 KiB banks: A0, A1, B0, B1
    std::array<uint8, kCRAMSize> m_CRAM;

    TVMD_t TVMD;     // 180000   TVMD    TV Screen Mode
    EXTEN_t EXTEN;   // 180002   EXTEN   External Signal Enable
    VRSIZE_t VRSIZE; // 180006   VRSIZE  VRAM Size
    RAMCTL_t RAMCTL; // 18000E   RAMCTL  RAM Control
                     // 180010   CYCA0L  VRAM Cycle Pattern A0 Lower
    CYC_t CYCA0;     // 180012   CYCA0U  VRAM Cycle Pattern A0 Upper
                     // 180014   CYCA1L  VRAM Cycle Pattern A1 Lower
    CYC_t CYCA1;     // 180016   CYCA1U  VRAM Cycle Pattern A1 Upper
                     // 180018   CYCB0L  VRAM Cycle Pattern B0 Lower
    CYC_t CYCB0;     // 18001A   CYCB0U  VRAM Cycle Pattern B0 Upper
                     // 18001C   CYCB1L  VRAM Cycle Pattern B1 Lower
    CYC_t CYCB1;     // 18001E   CYCB1U  VRAM Cycle Pattern B1 Upper
    BGON_t BGON;     // 180020   BGON    Screen Display Enable
    CHCTLA_t CHCTLA; // 180028   CHCTLA  Character Control Register A
    CHCTLB_t CHCTLB; // 18002A   CHCTLB  Character Control Register A

    PNC_t PNCN0;   // 180030   PNCN0   NBG0/RBG1 Pattern Name Control
    PNC_t PNCN1;   // 180032   PNCN1   NBG1 Pattern Name Control
    PNC_t PNCN2;   // 180034   PNCN2   NBG2 Pattern Name Control
    PNC_t PNCN3;   // 180036   PNCN3   NBG3 Pattern Name Control
    PNC_t PNCR;    // 180038   PNCR    RBG0 Pattern Name Control
    PLSZ_t PLSZ;   // 18003A   PLSZ    Plane Size
    MPOFN_t MPOFN; // 18003C   MPOFN   NBG0-3 Map Offset
    MPOFR_t MPOFR; // 18003E   MPOFR   Rotation Parameter A/B Map Offset
                   // 180040   MPABN0  NBG0 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN0;   // 180042   MPCDN0  NBG0 Normal Scroll Screen Map for Planes C,D
                   // 180044   MPABN1  NBG1 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN1;   // 180046   MPCDN1  NBG1 Normal Scroll Screen Map for Planes C,D
                   // 180048   MPABN2  NBG2 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN2;   // 18004A   MPCDN2  NBG2 Normal Scroll Screen Map for Planes C,D
                   // 18004C   MPABN3  NBG3 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN3;   // 18004E   MPCDN3  NBG3 Normal Scroll Screen Map for Planes C,D
                   // 180050   MPABRA  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
                   // 180052   MPCDRA  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
                   // 180054   MPEFRA  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
                   // 180056   MPGHRA  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
                   // 180058   MPIJRA  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
                   // 18005A   MPKLRA  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
                   // 18005C   MPMNRA  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    MPRP_t MPRA;   // 18005E   MPOPRA  Rotation Parameter A Scroll Surface Map for Screen Planes O,P
                   // 180060   MPABRB  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
                   // 180062   MPCDRB  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
                   // 180064   MPEFRB  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
                   // 180066   MPGHRB  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
                   // 180068   MPIJRB  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
                   // 18006A   MPKLRB  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
                   // 18006C   MPMNRB  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    MPRP_t MPRB;   // 18006E   MPOPRB  Rotation Parameter A Scroll Surface Map for Screen Planes O,P

    // -------------------------------------------------------------------------

    // RAMCTL.CRMD modes 2 and 3 shuffle address bits as follows:
    //   10 09 08 07 06 05 04 03 02 01 11 00
    //   in short, bits 10-01 are shifted left and bit 11 takes place of bit 01
    uint32 MapCRAMAddress(uint32 address) const {
        if (RAMCTL.CRMDn == 2 || RAMCTL.CRMDn == 3) {
            address =
                bit::extract<0>(address) | (bit::extract<11>(address) << 1u) | (bit::extract<1, 10>(address) << 2u);
        }
        return address;
    }
};

} // namespace satemu::vdp2

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
        case 0x022: return 0; // MZCTL is write-only
        case 0x024: return 0; // SFSEL is write-only
        case 0x026: return 0; // SFCODE is write-only
        case 0x028: return 0; // CHCTLA is write-only
        case 0x02A: return 0; // CHCTLB is write-only
        case 0x02C: return 0; // BMPNA is write-only
        case 0x02E: return 0; // BMPNB is write-only
        case 0x030: return 0; // PNCN0 is write-only
        case 0x032: return 0; // PNCN1 is write-only
        case 0x034: return 0; // PNCN2 is write-only
        case 0x036: return 0; // PNCN3 is write-only
        case 0x038: return 0; // PNCR is write-only
        case 0x03A: return 0; // PLSZ is write-only
        case 0x03C: return 0; // MPOFN is write-only
        case 0x03E: return 0; // MPOFR is write-only
        case 0x040: return 0; // MPABN0 is write-only
        case 0x042: return 0; // MPCDN0 is write-only
        case 0x044: return 0; // MPABN1 is write-only
        case 0x046: return 0; // MPCDN1 is write-only
        case 0x048: return 0; // MPABN2 is write-only
        case 0x04A: return 0; // MPCDN2 is write-only
        case 0x04C: return 0; // MPABN3 is write-only
        case 0x04E: return 0; // MPCDN3 is write-only
        case 0x050: return 0; // MPABRA is write-only
        case 0x052: return 0; // MPCDRA is write-only
        case 0x054: return 0; // MPEFRA is write-only
        case 0x056: return 0; // MPGHRA is write-only
        case 0x058: return 0; // MPIJRA is write-only
        case 0x05A: return 0; // MPKLRA is write-only
        case 0x05C: return 0; // MPMNRA is write-only
        case 0x05E: return 0; // MPOPRA is write-only
        case 0x060: return 0; // MPABRB is write-only
        case 0x062: return 0; // MPCDRB is write-only
        case 0x064: return 0; // MPEFRB is write-only
        case 0x066: return 0; // MPGHRB is write-only
        case 0x068: return 0; // MPIJRB is write-only
        case 0x06A: return 0; // MPKLRB is write-only
        case 0x06C: return 0; // MPMNRB is write-only
        case 0x06E: return 0; // MPOPRB is write-only
        case 0x070: return 0; // SCXIN0 is write-only
        case 0x072: return 0; // SCXDN0 is write-only
        case 0x074: return 0; // SCYIN0 is write-only
        case 0x076: return 0; // SCYDN0 is write-only
        case 0x078: return 0; // ZMXIN0 is write-only
        case 0x07A: return 0; // ZMXDN0 is write-only
        case 0x07C: return 0; // ZMYIN0 is write-only
        case 0x07E: return 0; // ZMYDN0 is write-only
        case 0x080: return 0; // SCXIN1 is write-only
        case 0x082: return 0; // SCXDN1 is write-only
        case 0x084: return 0; // SCYIN1 is write-only
        case 0x086: return 0; // SCYDN1 is write-only
        case 0x088: return 0; // ZMXIN1 is write-only
        case 0x08A: return 0; // ZMXDN1 is write-only
        case 0x08C: return 0; // ZMYIN1 is write-only
        case 0x08E: return 0; // ZMYDN1 is write-only
        case 0x090: return 0; // SCXN2 is write-only
        case 0x092: return 0; // SCYN2 is write-only
        case 0x094: return 0; // SCXN3 is write-only
        case 0x096: return 0; // SCYN3 is write-only
        case 0x098: return 0; // ZMCTL is write-only
        case 0x09A: return 0; // SCRCTL is write-only
        case 0x09C: return 0; // VCSTAU is write-only
        case 0x09E: return 0; // VCSTAL is write-only
        case 0x0A0: return 0; // LSTA0U is write-only
        case 0x0A2: return 0; // LSTA0L is write-only
        case 0x0A4: return 0; // LSTA1U is write-only
        case 0x0A6: return 0; // LSTA1L is write-only
        case 0x0A8: return 0; // LCTAU is write-only
        case 0x0AA: return 0; // LCTAL is write-only
        case 0x0AC: return 0; // BKTAU is write-only
        case 0x0AE: return 0; // BKTAL is write-only
        case 0x0B0: return 0; // RPMD is write-only
        case 0x0B2: return 0; // RPRCTL is write-only
        case 0x0B4: return 0; // KTCTL is write-only
        case 0x0B6: return 0; // KTAOF is write-only
        case 0x0B8: return 0; // OVPNRA is write-only
        case 0x0BA: return 0; // OVPNRB is write-only
        case 0x0BC: return 0; // RPTAU is write-only
        case 0x0BE: return 0; // RPTAL is write-only
        case 0x0C0: return 0; // WPSX0 is write-only
        case 0x0C2: return 0; // WPEX0 is write-only
        case 0x0C4: return 0; // WPSY0 is write-only
        case 0x0C6: return 0; // WPEY0 is write-only
        case 0x0C8: return 0; // WPSX1 is write-only
        case 0x0CA: return 0; // WPEX1 is write-only
        case 0x0CC: return 0; // WPSY1 is write-only
        case 0x0CE: return 0; // WPEY1 is write-only
        case 0x0D0: return 0; // WCTLA is write-only
        case 0x0D2: return 0; // WCTLB is write-only
        case 0x0D4: return 0; // WCTLC is write-only
        case 0x0D6: return 0; // WCTLD is write-only
        case 0x0D8: return 0; // LWTA0U is write-only
        case 0x0DA: return 0; // LWTA0L is write-only
        case 0x0DC: return 0; // LWTA1U is write-only
        case 0x0DE: return 0; // LWTA1L is write-only
        case 0x0E0: return 0; // SPCTL is write-only
        case 0x0E2: return 0; // SDCTL is write-only
        case 0x0E4: return 0; // CRAOFA is write-only
        case 0x0E6: return 0; // CRAOFB is write-only
        case 0x0E8: return 0; // LNCLEN is write-only
        case 0x0F0: return 0; // PRISA is write-only
        case 0x0F2: return 0; // PRISB is write-only
        case 0x0F4: return 0; // PRISC is write-only
        case 0x0F6: return 0; // PRISD is write-only
        case 0x0F8: return 0; // PRINA is write-only
        case 0x0FA: return 0; // PRINB is write-only
        case 0x0FC: return 0; // PRIR is write-only
        case 0x100: return 0; // CCRSA is write-only
        case 0x102: return 0; // CCRSB is write-only
        case 0x104: return 0; // CCRSC is write-only
        case 0x106: return 0; // CCRSD is write-only
        case 0x108: return 0; // CCRNA is write-only
        case 0x10A: return 0; // CCRNB is write-only
        case 0x10C: return 0; // CCRR is write-only
        case 0x10E: return 0; // CCRLB is write-only
        case 0x110: return 0; // CLOFEN is write-only
        case 0x112: return 0; // CLOFSL is write-only
        case 0x114: return 0; // COAR is write-only
        case 0x116: return 0; // COAG is write-only
        case 0x118: return 0; // COAB is write-only
        case 0x11A: return 0; // COBR is write-only
        case 0x11C: return 0; // COBG is write-only
        case 0x11E: return 0; // COBB is write-only
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
        case 0x022: MZCTL.u16 = value & 0xFF1F; break;
        case 0x024: SFSEL.u16 = value & 0x001F; break;
        case 0x026: SFCODE.u16 = value; break;
        case 0x028: CHCTLA.u16 = value & 0x3F7F; break;
        case 0x02A: CHCTLB.u16 = value & 0x7733; break;
        case 0x02C: BMPNA.u16 = value & 0x3737; break;
        case 0x02E: BMPNB.u16 = value & 0x0037; break;
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
        case 0x070: SCN0.X.I.u16 = value & 0x07FF; break;
        case 0x072: SCN0.X.D.u16 = value & 0xFF00; break;
        case 0x074: SCN0.Y.I.u16 = value & 0x07FF; break;
        case 0x076: SCN0.Y.D.u16 = value & 0xFF00; break;
        case 0x078: ZMN0.X.I.u16 = value & 0x0007; break;
        case 0x07A: ZMN0.X.D.u16 = value & 0xFF00; break;
        case 0x07C: ZMN0.Y.I.u16 = value & 0x0007; break;
        case 0x07E: ZMN0.Y.D.u16 = value & 0xFF00; break;
        case 0x080: SCN1.X.I.u16 = value & 0x07FF; break;
        case 0x082: SCN1.X.D.u16 = value & 0xFF00; break;
        case 0x084: SCN1.Y.I.u16 = value & 0x07FF; break;
        case 0x086: SCN1.Y.D.u16 = value & 0xFF00; break;
        case 0x088: ZMN1.X.I.u16 = value & 0x0007; break;
        case 0x08A: ZMN1.X.D.u16 = value & 0xFF00; break;
        case 0x08C: ZMN1.Y.I.u16 = value & 0x0007; break;
        case 0x08E: ZMN1.Y.D.u16 = value & 0xFF00; break;
        case 0x090: SCN2.X.u16 = value & 0x07FF; break;
        case 0x092: SCN2.Y.u16 = value & 0x07FF; break;
        case 0x094: SCN3.X.u16 = value & 0x07FF; break;
        case 0x096: SCN3.Y.u16 = value & 0x07FF; break;
        case 0x098: ZMCTL.u16 = value & 0x0303; break;
        case 0x09A: SCRCTL.u16 = value & 0x3F3F; break;
        case 0x09C: VCSTA.U.u16 = value & 0x0007; break;
        case 0x09E: VCSTA.L.u16 = value & 0xFFFE; break;
        case 0x0A0: LSTA0.U.u16 = value & 0x0007; break;
        case 0x0A2: LSTA0.L.u16 = value & 0xFFFE; break;
        case 0x0A4: LSTA1.U.u16 = value & 0x0007; break;
        case 0x0A6: LSTA1.L.u16 = value & 0xFFFE; break;
        case 0x0A8: LCTA.U.u16 = value & 0x8007; break;
        case 0x0AA: LCTA.L.u16 = value; break;
        case 0x0AC: BKTA.U.u16 = value & 0x8007; break;
        case 0x0AE: BKTA.L.u16 = value; break;
        case 0x0B0: RPMD.u16 = value & 0x0003; break;
        case 0x0B2: RPRCTL.u16 = value & 0x0707; break;
        case 0x0B4: KTCTL.u16 = value & 0x1F1F; break;
        case 0x0B6: KTAOF.u16 = value & 0x0707; break;
        case 0x0B8: OVPNRA = value; break;
        case 0x0BA: OVPNRB = value; break;
        case 0x0BC: RPTA.U.u16 = value & 0x0007; break;
        case 0x0BE: RPTA.L.u16 = value & 0xFFFE; break;
        case 0x0C0: WPXY0.X.S.u16 = value & 0x03FF; break;
        case 0x0C2: WPXY0.X.E.u16 = value & 0x03FF; break;
        case 0x0C4: WPXY0.Y.S.u16 = value & 0x01FF; break;
        case 0x0C6: WPXY0.Y.E.u16 = value & 0x01FF; break;
        case 0x0C8: WPXY1.X.S.u16 = value & 0x03FF; break;
        case 0x0CA: WPXY1.X.E.u16 = value & 0x03FF; break;
        case 0x0CC: WPXY1.Y.S.u16 = value & 0x01FF; break;
        case 0x0CE: WPXY1.Y.E.u16 = value & 0x01FF; break;
        case 0x0D0: WCTL.A.u16 = value & 0xBFBF; break;
        case 0x0D2: WCTL.B.u16 = value & 0xBFBF; break;
        case 0x0D4: WCTL.C.u16 = value & 0xBFBF; break;
        case 0x0D6: WCTL.D.u16 = value & 0xBF8F; break;
        case 0x0D8: LWTA0.U.u16 = value & 0x8007; break;
        case 0x0DA: LWTA0.L.u16 = value & 0xFFFE; break;
        case 0x0DC: LWTA1.U.u16 = value & 0x8007; break;
        case 0x0DE: LWTA1.L.u16 = value & 0xFFFE; break;
        case 0x0E0: SPCTL.u16 = value & 0x373F; break;
        case 0x0E2: SDCTL.u16 = value & 0x013F; break;
        case 0x0E4: CRAOFA.u16 = value & 0x7777; break;
        case 0x0E6: CRAOFA.u16 = value & 0x0077; break;
        case 0x0E8: LNCLEN.u16 = value & 0x003F; break;
        case 0x0F0: PRISA.u16 = value & 0x0707; break;
        case 0x0F2: PRISB.u16 = value & 0x0707; break;
        case 0x0F4: PRISC.u16 = value & 0x0707; break;
        case 0x0F6: PRISD.u16 = value & 0x0707; break;
        case 0x0F8: PRINA.u16 = value & 0x0707; break;
        case 0x0FA: PRINB.u16 = value & 0x0707; break;
        case 0x0FC: PRIR.u16 = value & 0x0007; break;
        case 0x100: CCRSA.u16 = value & 0x1F1F; break;
        case 0x102: CCRSB.u16 = value & 0x1F1F; break;
        case 0x104: CCRSC.u16 = value & 0x1F1F; break;
        case 0x106: CCRSD.u16 = value & 0x1F1F; break;
        case 0x108: CCRNA.u16 = value & 0x1F1F; break;
        case 0x10A: CCRNB.u16 = value & 0x1F1F; break;
        case 0x10C: CCRR.u16 = value & 0x001F; break;
        case 0x10E: CCRLB.u16 = value & 0x1F1F; break;
        case 0x110: CLOFEN.u16 = value & 0x007F; break;
        case 0x112: CLOFSL.u16 = value & 0x007F; break;
        case 0x114: COAR.u16 = value & 0x01FF; break;
        case 0x116: COAG.u16 = value & 0x01FF; break;
        case 0x118: COAB.u16 = value & 0x01FF; break;
        case 0x11A: COBR.u16 = value & 0x01FF; break;
        case 0x11C: COBG.u16 = value & 0x01FF; break;
        case 0x11E: COBB.u16 = value & 0x01FF; break;
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
                     // 180004   TVSTAT  Screen Status (read-only)
    VRSIZE_t VRSIZE; // 180006   VRSIZE  VRAM Size
                     // 180008   HCNT    H Counter (read-only)
                     // 18000A   VCNT    V Counter (read-only)
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
    MZCTL_t MZCTL;   // 180022   MZCTL   Mosaic Control
    SFSEL_t SFSEL;   // 180024   SFSEL   Special Function Code Select
    SFCODE_t SFCODE; // 180026   SFCODE  Special Function Code
    CHCTLA_t CHCTLA; // 180028   CHCTLA  Character Control Register A
    CHCTLB_t CHCTLB; // 18002A   CHCTLB  Character Control Register A
    BMPNA_t BMPNA;   // 18002C   BMPNA   NBG0/NBG1 Bitmap Palette Number
    BMPNB_t BMPNB;   // 18002E   BMPNB   RBG0 Bitmap Palette Number
    PNC_t PNCN0;     // 180030   PNCN0   NBG0/RBG1 Pattern Name Control
    PNC_t PNCN1;     // 180032   PNCN1   NBG1 Pattern Name Control
    PNC_t PNCN2;     // 180034   PNCN2   NBG2 Pattern Name Control
    PNC_t PNCN3;     // 180036   PNCN3   NBG3 Pattern Name Control
    PNC_t PNCR;      // 180038   PNCR    RBG0 Pattern Name Control
    PLSZ_t PLSZ;     // 18003A   PLSZ    Plane Size
    MPOFN_t MPOFN;   // 18003C   MPOFN   NBG0-3 Map Offset
    MPOFR_t MPOFR;   // 18003E   MPOFR   Rotation Parameter A/B Map Offset
                     // 180040   MPABN0  NBG0 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN0;     // 180042   MPCDN0  NBG0 Normal Scroll Screen Map for Planes C,D
                     // 180044   MPABN1  NBG1 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN1;     // 180046   MPCDN1  NBG1 Normal Scroll Screen Map for Planes C,D
                     // 180048   MPABN2  NBG2 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN2;     // 18004A   MPCDN2  NBG2 Normal Scroll Screen Map for Planes C,D
                     // 18004C   MPABN3  NBG3 Normal Scroll Screen Map for Planes A,B
    MPBG_t MPN3;     // 18004E   MPCDN3  NBG3 Normal Scroll Screen Map for Planes C,D
                     // 180050   MPABRA  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
                     // 180052   MPCDRA  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
                     // 180054   MPEFRA  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
                     // 180056   MPGHRA  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
                     // 180058   MPIJRA  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
                     // 18005A   MPKLRA  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
                     // 18005C   MPMNRA  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    MPRP_t MPRA;     // 18005E   MPOPRA  Rotation Parameter A Scroll Surface Map for Screen Planes O,P
                     // 180060   MPABRB  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
                     // 180062   MPCDRB  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
                     // 180064   MPEFRB  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
                     // 180066   MPGHRB  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
                     // 180068   MPIJRB  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
                     // 18006A   MPKLRB  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
                     // 18006C   MPMNRB  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    MPRP_t MPRB;     // 18006E   MPOPRB  Rotation Parameter A Scroll Surface Map for Screen Planes O,P
                     // 180070   SCXIN0  NBG0 Horizontal Screen Scroll Value (integer part)
                     // 180072   SCXDN0  NBG0 Horizontal Screen Scroll Value (fractional part)
                     // 180074   SCYIN0  NBG0 Vertical Screen Scroll Value (integer part)
    SCXYID_t SCN0;   // 180076   SCYDN0  NBG0 Vertical Screen Scroll Value (fractional part)
                     // 180078   ZMXIN0  NBG0 Horizontal Coordinate Increment (integer part)
                     // 18007A   ZMXDN0  NBG0 Horizontal Coordinate Increment (fractional part)
                     // 18007C   ZMYIN0  NBG0 Vertical Coordinate Increment (integer part)
    ZMXYID_t ZMN0;   // 18007E   ZMYDN0  NBG0 Vertical Coordinate Increment (fractional part)
                     // 180080   SCXIN1  NBG1 Horizontal Screen Scroll Value (integer part)
                     // 180082   SCXDN1  NBG1 Horizontal Screen Scroll Value (fractional part)
                     // 180084   SCYIN1  NBG1 Vertical Screen Scroll Value (integer part)
    SCXYID_t SCN1;   // 180086   SCYDN1  NBG1 Vertical Screen Scroll Value (fractional part)
                     // 180088   ZMXIN1  NBG1 Horizontal Coordinate Increment (integer part)
                     // 18008A   ZMXDN1  NBG1 Horizontal Coordinate Increment (fractional part)
                     // 18008C   ZMYIN1  NBG1 Vertical Coordinate Increment (integer part)
    ZMXYID_t ZMN1;   // 18008E   ZMYDN1  NBG1 Vertical Coordinate Increment (fractional part)
                     // 180090   SCXN2   NBG2 Horizontal Screen Scroll Value
    SCXY_t SCN2;     // 180092   SCYN2   NBG2 Vertical Screen Scroll Value
                     // 180094   SCXN3   NBG3 Horizontal Screen Scroll Value
    SCXY_t SCN3;     // 180096   SCYN3   NBG3 Vertical Screen Scroll Value
    ZMCTL_t ZMCTL;   // 180098   ZMCTL   Reduction Enable
    SCRCTL_t SCRCTL; // 18009A   SCRCTL  Line and Vertical Cell Scroll Control
                     // 18009C   VCSTAU  Vertical Cell Scroll Table Address (upper)
    VCSTA_t VCSTA;   // 18009E   VCSTAL  Vertical Cell Scroll Table Address (lower)
                     // 1800A0   LSTA0U  NBG0 Line Cell Scroll Table Address (upper)
    LSTA_t LSTA0;    // 1800A2   LSTA0L  NBG0 Line Cell Scroll Table Address (lower)
                     // 1800A4   LSTA1U  NBG1 Line Cell Scroll Table Address (upper)
    LSTA_t LSTA1;    // 1800A6   LSTA1L  NBG1 Line Cell Scroll Table Address (lower)
                     // 1800A8   LCTAU   Line Color Screen Table Address (upper)
    LCTA_t LCTA;     // 1800AA   LCTAL   Line Color Screen Table Address (lower)
                     // 1800AC   BKTAU   Back Screen Table Address (upper)
    BKTA_t BKTA;     // 1800AE   BKTAL   Back Screen Table Address (lower)
    RPMD_t RPMD;     // 1800B0   RPMD    Rotation Parameter Mode
    RPRCTL_t RPRCTL; // 1800B2   RPRCTL  Rotation Parameter Read Control
    KTCTL_t KTCTL;   // 1800B4   KTCTL   Coefficient Table Control
    KTAOF_t KTAOF;   // 1800B6   KTAOF   Coefficient Table Address Offset
    OVPNR_t OVPNRA;  // 1800B8   OVPNRA  Rotation Parameter A Screen-Over Pattern Name
    OVPNR_t OVPNRB;  // 1800BA   OVPNRB  Rotation Parameter B Screen-Over Pattern Name
                     // 1800BC   RPTAU   Rotation Parameters Table Address (upper)
    RPTA_t RPTA;     // 1800BE   RPTAL   Rotation Parameters Table Address (lower)
                     // 1800C0   WPSX0   Window 0 Horizontal Start Point
                     // 1800C2   WPSY0   Window 0 Vertical Start Point
                     // 1800C4   WPEX0   Window 0 Horizontal End Point
    WPXY_t WPXY0;    // 1800C6   WPEY0   Window 0 Vertical End Point
                     // 1800C8   WPSX1   Window 1 Horizontal Start Point
                     // 1800CA   WPSY1   Window 1 Vertical Start Point
                     // 1800CC   WPEX1   Window 1 Horizontal End Point
    WPXY_t WPXY1;    // 1800CE   WPEY1   Window 1 Vertical End Point
                     // 1800D0   WCTLA   NBG0 and NBG1 Window Control
                     // 1800D2   WCTLB   NBG2 and NBG3 Window Control
                     // 1800D4   WCTLC   RBG0 and Sprite Window Control
    WCTL_t WCTL;     // 1800D6   WCTLD   Rotation Window and Color Calculation Window Control
                     // 1800D8   LWTA0U  Window 0 Line Window Address Table (upper)
    LWTA_t LWTA0;    // 1800DA   LWTA0L  Window 0 Line Window Address Table (lower)
                     // 1800DC   LWTA1U  Window 1 Line Window Address Table (upper)
    LWTA_t LWTA1;    // 1800DE   LWTA1L  Window 1 Line Window Address Table (lower)
    SPCTL_t SPCTL;   // 1800E0   SPCTL   Sprite Control
    SDCTL_t SDCTL;   // 1800E2   SDCTL   Shadow Control
    CRAOFA_t CRAOFA; // 1800E4   CRAOFA  NBG0-NBG3 Color RAM Address Offset
    CRAOFB_t CRAOFB; // 1800E6   CRAOFB  RBG0 and Sprite Color RAM Address Offset
    LNCLEN_t LNCLEN; // 1800E8   LNCLEN  Line Color Screen Enable
                     // 1800EA
                     // 1800EC
                     // 1800EE
    PRI_t PRISA;     // 1800F0   PRISA   Sprite 0 and 1 Priority Number
    PRI_t PRISB;     // 1800F2   PRISB   Sprite 2 and 3 Priority Number
    PRI_t PRISC;     // 1800F4   PRISC   Sprite 4 and 5 Priority Number
    PRI_t PRISD;     // 1800F6   PRISD   Sprite 6 and 7 Priority Number
    PRI_t PRINA;     // 1800F8   PRINA   NBG0 and NBG1 Priority Number
    PRI_t PRINB;     // 1800FA   PRINB   NBG2 and NBG3 Priority Number
    PRI_t PRIR;      // 1800FC   PRIR    RBG0 Priority Number
                     // 1800FE   -       Reserved
    CCRS_t CCRSA;    // 180100   CCRSA   Sprite 0 and 1 Color Calculation Ratio
    CCRS_t CCRSB;    // 180102   CCRSB   Sprite 2 and 3 Color Calculation Ratio
    CCRS_t CCRSC;    // 180104   CCRSC   Sprite 4 and 5 Color Calculation Ratio
    CCRS_t CCRSD;    // 180106   CCRSD   Sprite 6 and 7 Color Calculation Ratio
    CCR_t CCRNA;     // 180108   CCRNA   NBG0 and NBG1 Color Calculation Ratio
    CCR_t CCRNB;     // 18010A   CCRNB   NBG2 and NBG3 Color Calculation Ratio
    CCR_t CCRR;      // 18010C   CCRR    RBG0 Color Calculation Ratio
    CCR_t CCRLB;     // 18010E   CCRLB   Line Color Screen and Back Screen Color Calculation Ratio
    CLOFEN_t CLOFEN; // 180110   CLOFEN  Color Offset Enable
    CLOFSL_t CLOFSL; // 180112   CLOFSL  Color Offset Select
    CO_t COAR;       // 180114   COAR    Color Offset A - Red
    CO_t COAG;       // 180116   COAG    Color Offset A - Green
    CO_t COAB;       // 180118   COAB    Color Offset A - Blue
    CO_t COBR;       // 18011A   COBR    Color Offset B - Red
    CO_t COBG;       // 18011C   COBG    Color Offset B - Green
    CO_t COBB;       // 18011E   COBB    Color Offset B - Blue

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

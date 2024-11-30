#pragma once

#include "vdp2_defs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/hw/vdp/vdp_common_defs.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>
#include <satemu/util/inline.hpp>

#include <fmt/format.h>

#include <array>

namespace satemu::vdp2 {

class VDP2 {
public:
    VDP2();

    void Reset(bool hard);

    // TODO: use scheduler
    void Advance(uint64 cycles);

    // TODO: handle VRSIZE.VRAMSZ in Read/WriteVRAM maybe?
    // TODO: CRAM and registers only accept 16-bit and 32-bit accesses

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
        case 0x004: return TVSTAT.u16;
        case 0x006: return VRSIZE.u16;
        case 0x008: return HCNT;
        case 0x00A: return VCNT;
        case 0x00E: return RAMCTL.u16;
        case 0x010: return CYCA0.L.u16;   // write-only?
        case 0x012: return CYCA0.U.u16;   // write-only?
        case 0x014: return CYCA1.L.u16;   // write-only?
        case 0x016: return CYCA1.U.u16;   // write-only?
        case 0x018: return CYCB0.L.u16;   // write-only?
        case 0x01A: return CYCB0.U.u16;   // write-only?
        case 0x01E: return CYCB1.L.u16;   // write-only?
        case 0x01C: return CYCB1.U.u16;   // write-only?
        case 0x020: return ReadBGON();    // write-only?
        case 0x022: return MZCTL.u16;     // write-only?
        case 0x024: return ReadSFSEL();   // write-only?
        case 0x026: return SFCODE.u16;    // write-only?
        case 0x028: return ReadCHCTLA();  // write-only?
        case 0x02A: return ReadCHCTLB();  // write-only?
        case 0x02C: return ReadBMPNA();   // write-only?
        case 0x02E: return ReadBMPNB();   // write-only?
        case 0x030: return ReadPNCN(0);   // write-only?
        case 0x032: return ReadPNCN(1);   // write-only?
        case 0x034: return ReadPNCN(2);   // write-only?
        case 0x036: return ReadPNCN(3);   // write-only?
        case 0x038: return ReadPNCR();    // write-only?
        case 0x03A: return ReadPLSZ();    // write-only?
        case 0x03C: return ReadMPOFN();   // write-only?
        case 0x03E: return ReadMPOFR();   // write-only?
        case 0x040: return ReadMPN(0, 0); // write-only?
        case 0x042: return ReadMPN(0, 1); // write-only?
        case 0x044: return ReadMPN(1, 0); // write-only?
        case 0x046: return ReadMPN(1, 1); // write-only?
        case 0x048: return ReadMPN(2, 0); // write-only?
        case 0x04A: return ReadMPN(2, 1); // write-only?
        case 0x04C: return ReadMPN(3, 0); // write-only?
        case 0x04E: return ReadMPN(3, 1); // write-only?
        case 0x050: return ReadMPR(0, 0); // write-only?
        case 0x052: return ReadMPR(0, 1); // write-only?
        case 0x054: return ReadMPR(0, 2); // write-only?
        case 0x056: return ReadMPR(0, 3); // write-only?
        case 0x058: return ReadMPR(0, 4); // write-only?
        case 0x05A: return ReadMPR(0, 5); // write-only?
        case 0x05C: return ReadMPR(0, 6); // write-only?
        case 0x05E: return ReadMPR(0, 7); // write-only?
        case 0x060: return ReadMPR(1, 0); // write-only?
        case 0x062: return ReadMPR(1, 1); // write-only?
        case 0x064: return ReadMPR(1, 2); // write-only?
        case 0x066: return ReadMPR(1, 3); // write-only?
        case 0x068: return ReadMPR(1, 4); // write-only?
        case 0x06A: return ReadMPR(1, 5); // write-only?
        case 0x06C: return ReadMPR(1, 6); // write-only?
        case 0x06E: return ReadMPR(1, 7); // write-only?
        case 0x070: return ReadSCXIN(0);  // write-only?
        case 0x072: return ReadSCXDN(0);  // write-only?
        case 0x074: return ReadSCYIN(0);  // write-only?
        case 0x076: return ReadSCYDN(0);  // write-only?
        case 0x078: return ReadZMXIN(0);  // write-only?
        case 0x07A: return ReadZMXDN(0);  // write-only?
        case 0x07C: return ReadZMYIN(0);  // write-only?
        case 0x07E: return ReadZMYDN(0);  // write-only?
        case 0x080: return ReadSCXIN(1);  // write-only?
        case 0x082: return ReadSCXDN(1);  // write-only?
        case 0x084: return ReadSCYIN(1);  // write-only?
        case 0x086: return ReadSCYDN(1);  // write-only?
        case 0x088: return ReadZMXIN(1);  // write-only?
        case 0x08A: return ReadZMXDN(1);  // write-only?
        case 0x08C: return ReadZMYIN(1);  // write-only?
        case 0x08E: return ReadZMYDN(1);  // write-only?
        case 0x090: return ReadSCXIN(2);  // write-only?
        case 0x092: return ReadSCYIN(2);  // write-only?
        case 0x094: return ReadSCXIN(3);  // write-only?
        case 0x096: return ReadSCYIN(3);  // write-only?
        case 0x098: return ZMCTL.u16;     // write-only?
        case 0x09A: return SCRCTL.u16;    // write-only?
        case 0x09C: return VCSTA.U.u16;   // write-only?
        case 0x09E: return VCSTA.L.u16;   // write-only?
        case 0x0A0: return LSTA0.U.u16;   // write-only?
        case 0x0A2: return LSTA0.L.u16;   // write-only?
        case 0x0A4: return LSTA1.U.u16;   // write-only?
        case 0x0A6: return LSTA1.L.u16;   // write-only?
        case 0x0A8: return LCTA.U.u16;    // write-only?
        case 0x0AA: return LCTA.L.u16;    // write-only?
        case 0x0AC: return BKTA.U.u16;    // write-only?
        case 0x0AE: return BKTA.L.u16;    // write-only?
        case 0x0B0: return RPMD.u16;      // write-only?
        case 0x0B2: return RPRCTL.u16;    // write-only?
        case 0x0B4: return KTCTL.u16;     // write-only?
        case 0x0B6: return KTAOF.u16;     // write-only?
        case 0x0B8: return OVPNRA;        // write-only?
        case 0x0BA: return OVPNRB;        // write-only?
        case 0x0BC: return RPTA.U.u16;    // write-only?
        case 0x0BE: return RPTA.L.u16;    // write-only?
        case 0x0C0: return WPXY0.X.S.u16; // write-only?
        case 0x0C2: return WPXY0.X.E.u16; // write-only?
        case 0x0C4: return WPXY0.Y.S.u16; // write-only?
        case 0x0C6: return WPXY0.Y.E.u16; // write-only?
        case 0x0C8: return WPXY1.X.S.u16; // write-only?
        case 0x0CA: return WPXY1.X.E.u16; // write-only?
        case 0x0CC: return WPXY1.Y.S.u16; // write-only?
        case 0x0CE: return WPXY1.Y.E.u16; // write-only?
        case 0x0D0: return WCTL.A.u16;    // write-only?
        case 0x0D2: return WCTL.B.u16;    // write-only?
        case 0x0D4: return WCTL.C.u16;    // write-only?
        case 0x0D6: return WCTL.D.u16;    // write-only?
        case 0x0D8: return LWTA0.U.u16;   // write-only?
        case 0x0DA: return LWTA0.L.u16;   // write-only?
        case 0x0DC: return LWTA1.U.u16;   // write-only?
        case 0x0DE: return LWTA1.L.u16;   // write-only?
        case 0x0E0: return SPCTL.u16;     // write-only?
        case 0x0E2: return SDCTL.u16;     // write-only?
        case 0x0E4: return ReadCRAOFA();  // write-only?
        case 0x0E6: return ReadCRAOFB();  // write-only?
        case 0x0E8: return ReadLNCLEN();  // write-only?
        case 0x0EA: return ReadSFPRMD();  // write-only?
        case 0x0EC: return CCCTL.u16;     // write-only?
        case 0x0EE: return SFCCMD.u16;    // write-only?
        case 0x0F0: return PRISA.u16;     // write-only?
        case 0x0F2: return PRISB.u16;     // write-only?
        case 0x0F4: return PRISC.u16;     // write-only?
        case 0x0F6: return PRISD.u16;     // write-only?
        case 0x0F8: return ReadPRINA();   // write-only?
        case 0x0FA: return ReadPRINB();   // write-only?
        case 0x0FC: return ReadPRIR();    // write-only?
        case 0x100: return CCRSA.u16;     // write-only?
        case 0x102: return CCRSB.u16;     // write-only?
        case 0x104: return CCRSC.u16;     // write-only?
        case 0x106: return CCRSD.u16;     // write-only?
        case 0x108: return CCRNA.u16;     // write-only?
        case 0x10A: return CCRNB.u16;     // write-only?
        case 0x10C: return CCRR.u16;      // write-only?
        case 0x10E: return CCRLB.u16;     // write-only?
        case 0x110: return CLOFEN.u16;    // write-only?
        case 0x112: return CLOFSL.u16;    // write-only?
        case 0x114: return COAR.u16;      // write-only?
        case 0x116: return COAG.u16;      // write-only?
        case 0x118: return COAB.u16;      // write-only?
        case 0x11A: return COBR.u16;      // write-only?
        case 0x11C: return COBG.u16;      // write-only?
        case 0x11E: return COBB.u16;      // write-only?
        default: fmt::println("unhandled {}-bit VDP2 register read from {:03X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_access_type T>
    void WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x000:
            TVMD.u16 = value & 0x81F7;
            UpdateResolution();
            break;
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
        case 0x020: WriteBGON(value); break;
        case 0x022: MZCTL.u16 = value & 0xFF1F; break;
        case 0x024: WriteSFSEL(value); break;
        case 0x026: SFCODE.u16 = value; break;
        case 0x028: WriteCHCTLA(value); break;
        case 0x02A: WriteCHCTLB(value); break;
        case 0x02C: WriteBMPNA(value); break;
        case 0x02E: WriteBMPNB(value); break;
        case 0x030: WritePNCN(0, value); break;
        case 0x032: WritePNCN(1, value); break;
        case 0x034: WritePNCN(2, value); break;
        case 0x036: WritePNCN(3, value); break;
        case 0x038: WritePNCR(value); break;
        case 0x03A: WritePLSZ(value); break;
        case 0x03C: WriteMPOFN(value); break;
        case 0x03E: WriteMPOFR(value); break;
        case 0x040: WriteMPN(value, 0, 0); break;
        case 0x042: WriteMPN(value, 0, 1); break;
        case 0x044: WriteMPN(value, 1, 0); break;
        case 0x046: WriteMPN(value, 1, 1); break;
        case 0x048: WriteMPN(value, 2, 0); break;
        case 0x04A: WriteMPN(value, 2, 1); break;
        case 0x04C: WriteMPN(value, 3, 0); break;
        case 0x04E: WriteMPN(value, 3, 1); break;
        case 0x050: WriteMPR(value, 0, 0); break;
        case 0x052: WriteMPR(value, 0, 1); break;
        case 0x054: WriteMPR(value, 0, 2); break;
        case 0x056: WriteMPR(value, 0, 3); break;
        case 0x058: WriteMPR(value, 0, 4); break;
        case 0x05A: WriteMPR(value, 0, 5); break;
        case 0x05C: WriteMPR(value, 0, 6); break;
        case 0x05E: WriteMPR(value, 0, 7); break;
        case 0x060: WriteMPR(value, 1, 0); break;
        case 0x062: WriteMPR(value, 1, 1); break;
        case 0x064: WriteMPR(value, 1, 2); break;
        case 0x066: WriteMPR(value, 1, 3); break;
        case 0x068: WriteMPR(value, 1, 4); break;
        case 0x06A: WriteMPR(value, 1, 5); break;
        case 0x06C: WriteMPR(value, 1, 6); break;
        case 0x06E: WriteMPR(value, 1, 7); break;
        case 0x070: WriteSCXIN(value, 0); break;
        case 0x072: WriteSCXDN(value, 0); break;
        case 0x074: WriteSCYIN(value, 0); break;
        case 0x076: WriteSCYDN(value, 0); break;
        case 0x078: WriteZMXIN(value, 0); break;
        case 0x07A: WriteZMXDN(value, 0); break;
        case 0x07C: WriteZMYIN(value, 0); break;
        case 0x07E: WriteZMYDN(value, 0); break;
        case 0x080: WriteSCXIN(value, 1); break;
        case 0x082: WriteSCXDN(value, 1); break;
        case 0x084: WriteSCYIN(value, 1); break;
        case 0x086: WriteSCYDN(value, 1); break;
        case 0x088: WriteZMXIN(value, 1); break;
        case 0x08A: WriteZMXDN(value, 1); break;
        case 0x08C: WriteZMYIN(value, 1); break;
        case 0x08E: WriteZMYDN(value, 1); break;
        case 0x090: WriteSCXIN(value, 2); break;
        case 0x092: WriteSCYIN(value, 2); break;
        case 0x094: WriteSCXIN(value, 3); break;
        case 0x096: WriteSCYIN(value, 3); break;
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
        case 0x0E4: WriteCRAOFA(value); break;
        case 0x0E6: WriteCRAOFB(value); break;
        case 0x0E8: WriteLNCLEN(value); break;
        case 0x0EA: WriteSFPRMD(value); break;
        case 0x0EC: CCCTL.u16 = value & 0xF77F; break;
        case 0x0EE: SFCCMD.u16 = value & 0x03FF; break;
        case 0x0F0: PRISA.u16 = value & 0x0707; break;
        case 0x0F2: PRISB.u16 = value & 0x0707; break;
        case 0x0F4: PRISC.u16 = value & 0x0707; break;
        case 0x0F6: PRISD.u16 = value & 0x0707; break;
        case 0x0F8: WritePRINA(value); break;
        case 0x0FA: WritePRINB(value); break;
        case 0x0FC: WritePRIR(value); break;
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

    // -------------------------------------------------------------------------

    TVMD_t TVMD;     // 180000   TVMD    TV Screen Mode
    EXTEN_t EXTEN;   // 180002   EXTEN   External Signal Enable
    TVSTAT_t TVSTAT; // 180004   TVSTAT  Screen Status (read-only)
    VRSIZE_t VRSIZE; // 180006   VRSIZE  VRAM Size
    uint16 HCNT;     // 180008   HCNT    H Counter (read-only)
    uint16 VCNT;     // 18000A   VCNT    V Counter (read-only)
                     // 18000C   -       Reserved (but not really)
    RAMCTL_t RAMCTL; // 18000E   RAMCTL  RAM Control
                     // 180010   CYCA0L  VRAM Cycle Pattern A0 Lower
    CYC_t CYCA0;     // 180012   CYCA0U  VRAM Cycle Pattern A0 Upper
                     // 180014   CYCA1L  VRAM Cycle Pattern A1 Lower
    CYC_t CYCA1;     // 180016   CYCA1U  VRAM Cycle Pattern A1 Upper
                     // 180018   CYCB0L  VRAM Cycle Pattern B0 Lower
    CYC_t CYCB0;     // 18001A   CYCB0U  VRAM Cycle Pattern B0 Upper
                     // 18001C   CYCB1L  VRAM Cycle Pattern B1 Lower
    CYC_t CYCB1;     // 18001E   CYCB1U  VRAM Cycle Pattern B1 Upper

    // 180020   BGON    Screen Display Enable
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //     12     W  R0TPON        RBG0 Transparent Display (0=enable, 1=disable)
    //     11     W  N3TPON        NBG3 Transparent Display (0=enable, 1=disable)
    //     10     W  N2TPON        NBG2 Transparent Display (0=enable, 1=disable)
    //      9     W  N1TPON        NBG1/EXBG Transparent Display (0=enable, 1=disable)
    //      8     W  N0TPON        NBG0/RBG1 Transparent Display (0=enable, 1=disable)
    //    7-6        -             Reserved, must be zero
    //      5     W  R1ON          RBG1 Display (0=disable, 1=enable)
    //      4     W  R0ON          RBG0 Display (0=disable, 1=enable)
    //      3     W  N3ON          NBG3 Display (0=disable, 1=enable)
    //      2     W  N2ON          NBG2 Display (0=disable, 1=enable)
    //      1     W  N1ON          NBG1 Display (0=disable, 1=enable)
    //      0     W  N0ON          NBG0 Display (0=disable, 1=enable)

    FORCE_INLINE uint16 ReadBGON() {
        uint16 value = 0;
        bit::deposit_into<0>(value, m_NormBGParams[0].enabled);
        bit::deposit_into<1>(value, m_NormBGParams[1].enabled);
        bit::deposit_into<2>(value, m_NormBGParams[2].enabled);
        bit::deposit_into<3>(value, m_NormBGParams[3].enabled);
        bit::deposit_into<4>(value, m_RotBGParams[0].enabled);
        bit::deposit_into<5>(value, m_RotBGParams[1].enabled);

        bit::deposit_into<8>(value, m_NormBGParams[0].transparent);
        bit::deposit_into<9>(value, m_NormBGParams[1].transparent);
        bit::deposit_into<10>(value, m_NormBGParams[2].transparent);
        bit::deposit_into<11>(value, m_NormBGParams[3].transparent);
        bit::deposit_into<12>(value, m_RotBGParams[0].transparent);
        return value;
    }

    FORCE_INLINE void WriteBGON(uint16 value) {
        m_NormBGParams[0].enabled = bit::extract<0>(value);
        m_NormBGParams[1].enabled = bit::extract<1>(value);
        m_NormBGParams[2].enabled = bit::extract<2>(value);
        m_NormBGParams[3].enabled = bit::extract<3>(value);
        m_RotBGParams[0].enabled = bit::extract<4>(value);
        m_RotBGParams[1].enabled = bit::extract<5>(value);

        m_NormBGParams[0].transparent = bit::extract<8>(value);
        m_NormBGParams[1].transparent = bit::extract<9>(value);
        m_NormBGParams[2].transparent = bit::extract<10>(value);
        m_NormBGParams[3].transparent = bit::extract<11>(value);
        m_RotBGParams[0].transparent = bit::extract<12>(value);
        m_RotBGParams[1].transparent = m_NormBGParams[0].transparent;
    }

    MZCTL_t MZCTL; // 180022   MZCTL   Mosaic Control

    // 180024   SFSEL   Special Function Code Select
    //
    //   bits   r/w  code          description
    //   15-5        -             Reserved, must be zero
    //      4     W  R0SFCS        RBG0 Special Function Code Select
    //      3     W  N3SFCS        NBG3 Special Function Code Select
    //      2     W  N2SFCS        NBG2 Special Function Code Select
    //      1     W  N1SFCS        NBG1 Special Function Code Select
    //      0     W  N0SFCS        NBG0/RBG1 Special Function Code Select

    FORCE_INLINE uint16 ReadSFSEL() {
        uint16 value = 0;
        bit::deposit_into<0>(value, m_NormBGParams[0].specialFunctionSelect);
        bit::deposit_into<1>(value, m_NormBGParams[1].specialFunctionSelect);
        bit::deposit_into<2>(value, m_NormBGParams[3].specialFunctionSelect);
        bit::deposit_into<3>(value, m_NormBGParams[4].specialFunctionSelect);
        bit::deposit_into<4>(value, m_RotBGParams[0].specialFunctionSelect);
        return value;
    }

    FORCE_INLINE void WriteSFSEL(uint16 value) {
        m_NormBGParams[0].specialFunctionSelect = bit::extract<0>(value);
        m_NormBGParams[1].specialFunctionSelect = bit::extract<1>(value);
        m_NormBGParams[2].specialFunctionSelect = bit::extract<2>(value);
        m_NormBGParams[3].specialFunctionSelect = bit::extract<3>(value);
        m_RotBGParams[0].specialFunctionSelect = bit::extract<4>(value);
        m_RotBGParams[1].specialFunctionSelect = m_NormBGParams[0].specialFunctionSelect;
    }

    SFCODE_t SFCODE; // 180026   SFCODE  Special Function Code

    // 180028   CHCTLA  Character Control Register A
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //  13-12     W  N1CHCN1-0     NBG1/EXBG Character Color Number
    //                               00 (0) =       16 colors - palette
    //                               01 (1) =      256 colors - palette
    //                               10 (2) =     2048 colors - palette
    //                               11 (3) =    32768 colors - RGB (NBG1)
    //                                        16777216 colors - RGB (EXBG)
    //  11-10     W  N1BMSZ1-0     NBG1 Bitmap Size
    //                               00 (0) = 512x256
    //                               01 (1) = 512x512
    //                               10 (2) = 1024x256
    //                               11 (3) = 1024x512
    //      9     W  N1BMEN        NBG1 Bitmap Enable (0=cells, 1=bitmap)
    //      8     W  N1CHSZ        NBG1 Character Size (0=1x1, 1=2x2)
    //      7        -             Reserved, must be zero
    //    6-4     W  N0CHCN2-0     NBG0/RBG1 Character Color Number
    //                               000 (0) =       16 colors - palette
    //                               001 (1) =      256 colors - palette
    //                               010 (2) =     2048 colors - palette
    //                               011 (3) =    32768 colors - RGB
    //                               100 (4) = 16777216 colors - RGB (Normal mode only)
    //                                           forbidden for Hi-Res or Exclusive Monitor
    //                               101 (5) = forbidden
    //                               110 (6) = forbidden
    //                               111 (7) = forbidden
    //    3-2     W  N0BMSZ1-0     NBG0 Bitmap Size
    //                               00 (0) = 512x256
    //                               01 (1) = 512x512
    //                               10 (2) = 1024x256
    //                               11 (3) = 1024x512
    //      1     W  N0BMEN        NBG0 Bitmap Enable (0=cells, 1=bitmap)
    //      0     W  N0CHSZ        NBG0 Character Size (0=1x1, 1=2x2)

    FORCE_INLINE uint16 ReadCHCTLA() {
        uint16 value = 0;
        bit::deposit_into<0>(value, m_NormBGParams[0].cellSizeShift);
        bit::deposit_into<1>(value, m_NormBGParams[0].bitmap);
        bit::deposit_into<2, 3>(value, m_NormBGParams[0].bmsz);
        bit::deposit_into<4, 6>(value, static_cast<uint32>(m_NormBGParams[0].colorFormat));

        bit::deposit_into<8>(value, m_NormBGParams[1].cellSizeShift);
        bit::deposit_into<9>(value, m_NormBGParams[1].bitmap);
        bit::deposit_into<10, 11>(value, m_NormBGParams[1].bmsz);
        bit::deposit_into<12, 13>(value, static_cast<uint32>(m_NormBGParams[1].colorFormat));
        return value;
    }

    FORCE_INLINE void WriteCHCTLA(uint16 value) {
        m_NormBGParams[0].cellSizeShift = bit::extract<0>(value);
        m_NormBGParams[0].bitmap = bit::extract<1>(value);
        m_NormBGParams[0].bmsz = bit::extract<2, 3>(value);
        m_NormBGParams[0].colorFormat = static_cast<ColorFormat>(bit::extract<4, 6>(value));
        m_NormBGParams[0].UpdateCHCTL();

        m_RotBGParams[1].colorFormat = m_NormBGParams[0].colorFormat;
        m_RotBGParams[1].UpdateCHCTL();

        m_NormBGParams[1].cellSizeShift = bit::extract<8>(value);
        m_NormBGParams[1].bitmap = bit::extract<9>(value);
        m_NormBGParams[1].bmsz = bit::extract<10, 11>(value);
        m_NormBGParams[1].colorFormat = static_cast<ColorFormat>(bit::extract<12, 13>(value));
        m_NormBGParams[1].UpdateCHCTL();
    }

    // 18002A   CHCTLB  Character Control Register B
    //
    //   bits   r/w  code          description
    //     15        -             Reserved, must be zero
    //  14-12     W  R0CHCN2-0     RBG0 Character Color Number
    //                               NOTE: Exclusive Monitor cannot display this BG plane
    //                               000 (0) =       16 colors - palette
    //                               001 (1) =      256 colors - palette
    //                               010 (2) =     2048 colors - palette
    //                               011 (3) =    32768 colors - RGB
    //                               100 (4) = 16777216 colors - RGB (Normal mode only)
    //                                           forbidden for Hi-Res
    //                               101 (5) = forbidden
    //                               110 (6) = forbidden
    //                               111 (7) = forbidden
    //     11        -             Reserved, must be zero
    //     10     W  R0BMSZ        RBG0 Bitmap Size (0=512x256, 1=512x512)
    //      9     W  R0BMEN        RBG0 Bitmap Enable (0=cells, 1=bitmap)
    //      8     W  R0CHSZ        RBG0 Character Size (0=1x1, 1=2x2)
    //    7-6        -             Reserved, must be zero
    //      5     W  N3CHCN        NBG3 Character Color Number (0=16 colors, 1=256 colors; both palette)
    //      4     W  N3CHSZ        NBG3 Character Size (0=1x1, 1=2x2)
    //    3-2        -             Reserved, must be zero
    //      1     W  N2CHCN        NBG2 Character Color Number (0=16 colors, 1=256 colors; both palette)
    //      0     W  N2CHSZ        NBG2 Character Size (0=1x1, 1=2x2)

    FORCE_INLINE uint16 ReadCHCTLB() {
        uint16 value = 0;
        bit::deposit_into<0>(value, m_NormBGParams[2].cellSizeShift);
        bit::deposit_into<1>(value, static_cast<uint32>(m_NormBGParams[2].colorFormat));

        bit::deposit_into<4>(value, m_NormBGParams[3].cellSizeShift);
        bit::deposit_into<5>(value, static_cast<uint32>(m_NormBGParams[3].colorFormat));

        bit::deposit_into<8>(value, m_RotBGParams[0].cellSizeShift);
        bit::deposit_into<9>(value, m_RotBGParams[0].bitmap);
        bit::deposit_into<10>(value, m_RotBGParams[0].bmsz);
        bit::deposit_into<12, 14>(value, static_cast<uint32>(m_RotBGParams[0].colorFormat));
        return value;
    }

    FORCE_INLINE void WriteCHCTLB(uint16 value) {
        m_NormBGParams[2].cellSizeShift = bit::extract<0>(value);
        m_NormBGParams[2].colorFormat = static_cast<ColorFormat>(bit::extract<1>(value));
        m_NormBGParams[2].UpdateCHCTL();

        m_NormBGParams[3].cellSizeShift = bit::extract<4>(value);
        m_NormBGParams[3].colorFormat = static_cast<ColorFormat>(bit::extract<5>(value));
        m_NormBGParams[3].UpdateCHCTL();

        m_RotBGParams[0].cellSizeShift = bit::extract<8>(value);
        m_RotBGParams[0].bitmap = bit::extract<9>(value);
        m_RotBGParams[0].bmsz = bit::extract<10>(value);
        m_RotBGParams[0].colorFormat = static_cast<ColorFormat>(bit::extract<12, 14>(value));
        m_RotBGParams[0].UpdateCHCTL();
    }

    // 18002C   BMPNA   NBG0/NBG1 Bitmap Palette Number
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //     13     W  N1BMPR        NBG1 Special Priority
    //     12     W  N1BMCC        NBG1 Special Color Calculation
    //     11        -             Reserved, must be zero
    //   10-8     W  N1BMP6-4      NBG1 Palette Number
    //    7-6        -             Reserved, must be zero
    //      5     W  N0BMPR        NBG0 Special Priority
    //      4     W  N0BMCC        NBG0 Special Color Calculation
    //      3        -             Reserved, must be zero
    //    2-0     W  N0BMP6-4      NBG0 Palette Number

    FORCE_INLINE uint16 ReadBMPNA() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, m_NormBGParams[0].supplBitmapPalNum >> 4u);
        bit::deposit_into<4>(value, m_NormBGParams[0].supplBitmapSpecialColorCalc);
        bit::deposit_into<5>(value, m_NormBGParams[0].supplBitmapSpecialPriority);

        bit::deposit_into<8, 10>(value, m_NormBGParams[1].supplBitmapPalNum >> 4u);
        bit::deposit_into<12>(value, m_NormBGParams[1].supplBitmapSpecialColorCalc);
        bit::deposit_into<13>(value, m_NormBGParams[1].supplBitmapSpecialPriority);
        return value;
    }

    FORCE_INLINE void WriteBMPNA(uint16 value) {
        m_NormBGParams[0].supplBitmapPalNum = bit::extract<0, 2>(value) << 4u;
        m_NormBGParams[0].supplBitmapSpecialColorCalc = bit::extract<4>(value);
        m_NormBGParams[0].supplBitmapSpecialPriority = bit::extract<5>(value);

        m_NormBGParams[1].supplBitmapPalNum = bit::extract<8, 10>(value) << 4u;
        m_NormBGParams[1].supplBitmapSpecialColorCalc = bit::extract<12>(value);
        m_NormBGParams[1].supplBitmapSpecialPriority = bit::extract<13>(value);
    }

    // 18002E   BMPNB   RBG0 Bitmap Palette Number
    //
    //   bits   r/w  code          description
    //   15-6        -             Reserved, must be zero
    //      5     W  R0BMPR        RBG0 Special Priority
    //      4     W  R0BMCC        RBG0 Special Color Calculation
    //      3        -             Reserved, must be zero
    //    2-0     W  R0BMP6-4      RBG0 Palette Number

    FORCE_INLINE uint16 ReadBMPNB() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, m_RotBGParams[0].supplBitmapPalNum >> 4u);
        bit::deposit_into<4>(value, m_RotBGParams[0].supplBitmapSpecialColorCalc);
        bit::deposit_into<5>(value, m_RotBGParams[0].supplBitmapSpecialPriority);
        return value;
    }

    FORCE_INLINE void WriteBMPNB(uint16 value) {
        m_RotBGParams[0].supplBitmapPalNum = bit::extract<0, 2>(value) << 4u;
        m_RotBGParams[0].supplBitmapSpecialColorCalc = bit::extract<4>(value);
        m_RotBGParams[0].supplBitmapSpecialPriority = bit::extract<5>(value);
    }

    // 180030   PNCN0   NBG0/RBG1 Pattern Name Control
    // 180032   PNCN1   NBG1 Pattern Name Control
    // 180034   PNCN2   NBG2 Pattern Name Control
    // 180036   PNCN3   NBG3 Pattern Name Control
    // 180038   PNCR    RBG0 Pattern Name Control
    //
    //   bits   r/w  code          description
    //     15     W  xxPNB         Pattern Name Data Size (0=2 words, 1=1 word)
    //     14     W  xxCNSM        Character Number Supplement
    //                               0 = char number is 10 bits; H/V flip available
    //                               1 = char number is 12 bits; H/V flip unavailable
    //  13-10        -             Reserved, must be zero
    //      9     W  xxSPR         Special Priority bit
    //      8     W  xxSCC         Special Color Calculation bit
    //    7-5     W  xxSPLT6-4     Supplementary Palette bits 6-4
    //    4-0     W  xxSCN4-0      Supplementary Character Number bits 4-0

    FORCE_INLINE uint16 ReadPNCN(uint32 bgIndex) {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, m_NormBGParams[bgIndex].supplScrollCharNum);
        bit::deposit_into<5, 7>(value, m_NormBGParams[bgIndex].supplScrollPalNum >> 4u);
        bit::deposit_into<8>(value, m_NormBGParams[bgIndex].supplScrollSpecialColorCalc);
        bit::deposit_into<9>(value, m_NormBGParams[bgIndex].supplScrollSpecialPriority);
        bit::deposit_into<14>(value, m_NormBGParams[bgIndex].wideChar);
        bit::deposit_into<15>(value, !m_NormBGParams[bgIndex].twoWordChar);
        return value;
    }

    FORCE_INLINE void WritePNCN(uint32 bgIndex, uint16 value) {
        m_NormBGParams[bgIndex].supplScrollCharNum = bit::extract<0, 4>(value);
        m_NormBGParams[bgIndex].supplScrollPalNum = bit::extract<5, 7>(value) << 4u;
        m_NormBGParams[bgIndex].supplScrollSpecialColorCalc = bit::extract<8>(value);
        m_NormBGParams[bgIndex].supplScrollSpecialPriority = bit::extract<9>(value);
        m_NormBGParams[bgIndex].wideChar = bit::extract<14>(value);
        m_NormBGParams[bgIndex].twoWordChar = !bit::extract<15>(value);
        m_NormBGParams[bgIndex].UpdatePageBaseAddresses();

        if (bgIndex == 0) {
            m_RotBGParams[1].supplScrollCharNum = m_NormBGParams[0].supplScrollCharNum;
            m_RotBGParams[1].supplScrollPalNum = m_NormBGParams[0].supplScrollPalNum;
            m_RotBGParams[1].supplScrollSpecialColorCalc = m_NormBGParams[0].supplScrollSpecialColorCalc;
            m_RotBGParams[1].supplScrollSpecialPriority = m_NormBGParams[0].supplScrollSpecialPriority;
            m_RotBGParams[1].wideChar = m_NormBGParams[0].wideChar;
            m_RotBGParams[1].twoWordChar = m_NormBGParams[0].twoWordChar;
            m_RotBGParams[1].UpdatePageBaseAddresses();
        }
    }

    FORCE_INLINE uint16 ReadPNCR() {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, m_RotBGParams[0].supplScrollCharNum);
        bit::deposit_into<5, 7>(value, m_RotBGParams[0].supplScrollPalNum >> 4u);
        bit::deposit_into<8>(value, m_RotBGParams[0].supplScrollSpecialColorCalc);
        bit::deposit_into<9>(value, m_RotBGParams[0].supplScrollSpecialPriority);
        bit::deposit_into<14>(value, m_RotBGParams[0].wideChar);
        bit::deposit_into<15>(value, !m_RotBGParams[0].twoWordChar);
        return value;
    }

    FORCE_INLINE void WritePNCR(uint16 value) {
        m_RotBGParams[0].supplScrollCharNum = bit::extract<0, 4>(value);
        m_RotBGParams[0].supplScrollPalNum = bit::extract<5, 7>(value) << 4u;
        m_RotBGParams[0].supplScrollSpecialColorCalc = bit::extract<8>(value);
        m_RotBGParams[0].supplScrollSpecialPriority = bit::extract<9>(value);
        m_RotBGParams[0].wideChar = bit::extract<14>(value);
        m_RotBGParams[0].twoWordChar = !bit::extract<15>(value);
        m_RotBGParams[0].UpdatePageBaseAddresses();
    }

    // 18003A   PLSZ    Plane Size
    //
    //   bits   r/w  code          description
    //  15-14     W  RBOVR1-0      Rotation Parameter B Screen-over Process
    //  13-12     W  RBPLSZ1-0     Rotation Parameter B Plane Size
    //  11-10     W  RAOVR1-0      Rotation Parameter A Screen-over Process
    //    9-8     W  RAPLSZ1-0     Rotation Parameter A Plane Size
    //    7-6     W  N3PLSZ1-0     NBG3 Plane Size
    //    5-4     W  N2PLSZ1-0     NBG2 Plane Size
    //    3-2     W  N1PLSZ1-0     NBG1 Plane Size
    //    1-0     W  N0PLSZ1-0     NBG0 Plane Size
    //
    //  xxOVR1-0:
    //    00 (0) = Repeat plane infinitely
    //    01 (1) = Use character pattern in screen-over pattern name register
    //    10 (2) = Transparent
    //    11 (3) = Force 512x512 with transparent outsides (256 line bitmaps draw twice)
    //
    //  xxPLSZ1-0:
    //    00 (0) = 1x1
    //    01 (1) = 2x1
    //    10 (2) = forbidden (but probably 1x2)
    //    11 (3) = 2x2

    FORCE_INLINE uint16 ReadPLSZ() {
        uint16 value = 0;
        bit::deposit_into<0, 1>(value, m_NormBGParams[0].plsz);
        bit::deposit_into<2, 3>(value, m_NormBGParams[1].plsz);
        bit::deposit_into<4, 5>(value, m_NormBGParams[2].plsz);
        bit::deposit_into<6, 7>(value, m_NormBGParams[3].plsz);
        bit::deposit_into<8, 9>(value, m_RotBGParams[0].plsz);
        bit::deposit_into<10, 11>(value, static_cast<uint32>(m_RotBGParams[0].screenOverProcess));
        bit::deposit_into<12, 13>(value, m_RotBGParams[1].plsz);
        bit::deposit_into<14, 15>(value, static_cast<uint32>(m_RotBGParams[1].screenOverProcess));
        return value;
    }

    FORCE_INLINE void WritePLSZ(uint16 value) {
        m_NormBGParams[0].plsz = bit::extract<0, 1>(value);
        m_NormBGParams[1].plsz = bit::extract<2, 3>(value);
        m_NormBGParams[2].plsz = bit::extract<4, 5>(value);
        m_NormBGParams[3].plsz = bit::extract<6, 7>(value);
        m_RotBGParams[0].plsz = bit::extract<8, 9>(value);
        m_RotBGParams[0].screenOverProcess = static_cast<ScreenOverProcess>(bit::extract<10, 11>(value));
        m_RotBGParams[1].plsz = bit::extract<12, 13>(value);
        m_RotBGParams[1].screenOverProcess = static_cast<ScreenOverProcess>(bit::extract<14, 15>(value));
        for (auto &bg : m_NormBGParams) {
            bg.UpdatePLSZ();
        }
        for (auto &bg : m_RotBGParams) {
            bg.UpdatePLSZ();
        }
    }

    // 18003C   MPOFN   NBG0-3 Map Offset
    //
    //   bits   r/w  code          description
    //     15        -             Reserved, must be zero
    //  14-12     W  M3MP8-6       NBG3 Map Offset
    //     11        -             Reserved, must be zero
    //   10-8     W  M2MP8-6       NBG2 Map Offset
    //      7        -             Reserved, must be zero
    //    6-4     W  M1MP8-6       NBG1 Map Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  M0MP8-6       NBG0 Map Offset

    FORCE_INLINE uint16 ReadMPOFN() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<6, 8>(m_NormBGParams[0].mapIndices[0]));
        bit::deposit_into<4, 6>(value, bit::extract<6, 8>(m_NormBGParams[1].mapIndices[0]));
        bit::deposit_into<8, 10>(value, bit::extract<6, 8>(m_NormBGParams[2].mapIndices[0]));
        bit::deposit_into<12, 14>(value, bit::extract<6, 8>(m_NormBGParams[3].mapIndices[0]));
        return value;
    }

    FORCE_INLINE void WriteMPOFN(uint16 value) {
        for (int i = 0; i < 4; i++) {
            bit::deposit_into<6, 8>(m_NormBGParams[0].mapIndices[i], bit::extract<0, 2>(value));
            bit::deposit_into<6, 8>(m_NormBGParams[1].mapIndices[i], bit::extract<4, 6>(value));
            bit::deposit_into<6, 8>(m_NormBGParams[2].mapIndices[i], bit::extract<8, 10>(value));
            bit::deposit_into<6, 8>(m_NormBGParams[3].mapIndices[i], bit::extract<12, 14>(value));
        }
        // shift by 17 is the same as multiply by 0x20000, which is the boundary for bitmap data
        m_NormBGParams[0].bitmapBaseAddress = bit::extract<0, 2>(value) << 17u;
        m_NormBGParams[1].bitmapBaseAddress = bit::extract<4, 6>(value) << 17u;
        m_NormBGParams[2].bitmapBaseAddress = bit::extract<8, 10>(value) << 17u;
        m_NormBGParams[3].bitmapBaseAddress = bit::extract<12, 14>(value) << 17u;

        for (auto &bg : m_NormBGParams) {
            bg.UpdatePageBaseAddresses();
        }
    }

    // 18003E   MPOFR   Rotation Parameter A/B Map Offset
    //
    //   bits   r/w  code          description
    //   15-7        -             Reserved, must be zero
    //    6-4     W  RBMP8-6       Rotation Parameter B Map Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  RAMP8-6       Rotation Parameter A Map Offset

    FORCE_INLINE uint16 ReadMPOFR() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<6, 8>(m_RotBGParams[0].mapIndices[0]));
        bit::deposit_into<4, 6>(value, bit::extract<6, 8>(m_RotBGParams[1].mapIndices[0]));
        return value;
    }

    FORCE_INLINE void WriteMPOFR(uint16 value) {
        for (int i = 0; i < 4; i++) {
            bit::deposit_into<6, 8>(m_RotBGParams[0].mapIndices[i], bit::extract<0, 2>(value));
            bit::deposit_into<6, 8>(m_RotBGParams[1].mapIndices[i], bit::extract<4, 6>(value));
        }
        // shift by 17 is the same as multiply by 0x20000, which is the boundary for bitmap data
        m_RotBGParams[0].bitmapBaseAddress = bit::extract<0, 2>(value) << 17u;
        m_RotBGParams[1].bitmapBaseAddress = bit::extract<4, 6>(value) << 17u;

        for (auto &bg : m_RotBGParams) {
            bg.UpdatePageBaseAddresses();
        }
    }

    // 180040   MPABN0  NBG0 Normal Scroll Screen Map for Planes A,B
    // 180042   MPCDN0  NBG0 Normal Scroll Screen Map for Planes C,D
    // 180044   MPABN1  NBG1 Normal Scroll Screen Map for Planes A,B
    // 180046   MPCDN1  NBG1 Normal Scroll Screen Map for Planes C,D
    // 180048   MPABN2  NBG2 Normal Scroll Screen Map for Planes A,B
    // 18004A   MPCDN2  NBG2 Normal Scroll Screen Map for Planes C,D
    // 18004C   MPABN3  NBG3 Normal Scroll Screen Map for Planes A,B
    // 18004E   MPCDN3  NBG3 Normal Scroll Screen Map for Planes C,D
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //   13-8     W  xxMPy5-0      BG xx Plane y Map
    //    7-6        -             Reserved, must be zero
    //    5-0     W  xxMPy5-0      BG xx Plane y Map
    //
    // xx:
    //   N0 = NBG0 (MPyyN0)
    //   N1 = NBG1 (MPyyN1)
    //   N2 = NBG2 (MPyyN2)
    //   N3 = NBG3 (MPyyN3)
    // y:
    //   A = Plane A (bits  5-0 of MPABxx)
    //   B = Plane B (bits 13-8 of MPABxx)
    //   C = Plane C (bits  5-0 of MPCDxx)
    //   D = Plane D (bits 13-8 of MPCDxx)

    FORCE_INLINE uint16 ReadMPN(uint32 bgIndex, uint32 planeIndex) {
        uint16 value = 0;
        auto &bg = m_NormBGParams[bgIndex];
        bit::deposit_into<0, 5>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 0]));
        bit::deposit_into<8, 13>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 1]));
        return value;
    }

    FORCE_INLINE void WriteMPN(uint16 value, uint32 bgIndex, uint32 planeIndex) {
        auto &bg = m_NormBGParams[bgIndex];
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 0], bit::extract<0, 5>(value));
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 1], bit::extract<8, 13>(value));
        bg.UpdatePageBaseAddresses();
    }

    // 180050   MPABRA  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
    // 180052   MPCDRA  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
    // 180054   MPEFRA  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
    // 180056   MPGHRA  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
    // 180058   MPIJRA  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
    // 18005A   MPKLRA  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
    // 18005C   MPMNRA  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    // 18005E   MPOPRA  Rotation Parameter A Scroll Surface Map for Screen Planes O,P
    // 180060   MPABRB  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
    // 180062   MPCDRB  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
    // 180064   MPEFRB  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
    // 180066   MPGHRB  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
    // 180068   MPIJRB  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
    // 18006A   MPKLRB  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
    // 18006C   MPMNRB  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    // 18006E   MPOPRB  Rotation Parameter A Scroll Surface Map for Screen Planes O,P
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //   13-8     W  RxMPy5-0      Rotation Parameter x Screen Plane y Map
    //    7-6        -             Reserved, must be zero
    //    5-0     W  RxMPy5-0      Rotation Parameter x Screen Plane y Map
    //
    // x:
    //   A = Rotation Parameter A (MPyyRA)
    //   B = Rotation Parameter A (MPyyRB)
    // y:
    //   A = Screen Plane A (bits  5-0 of MPABxx)
    //   B = Screen Plane B (bits 13-8 of MPABxx)
    //   C = Screen Plane C (bits  5-0 of MPCDxx)
    //   D = Screen Plane D (bits 13-8 of MPCDxx)
    //   ...
    //   M = Screen Plane M (bits  5-0 of MPMNxx)
    //   N = Screen Plane N (bits 13-8 of MPMNxx)
    //   O = Screen Plane O (bits  5-0 of MPOPxx)
    //   P = Screen Plane P (bits 13-8 of MPOPxx)

    FORCE_INLINE uint16 ReadMPR(uint32 bgIndex, uint32 planeIndex) {
        uint16 value = 0;
        auto &bg = m_RotBGParams[bgIndex];
        bit::deposit_into<0, 5>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 0]));
        bit::deposit_into<8, 13>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 1]));
        return value;
    }

    FORCE_INLINE void WriteMPR(uint16 value, uint32 bgIndex, uint32 planeIndex) {
        auto &bg = m_RotBGParams[bgIndex];
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 0], bit::extract<0, 5>(value));
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 1], bit::extract<8, 13>(value));
        bg.UpdatePageBaseAddresses();
    }

    // 180070   SCXIN0  NBG0 Horizontal Screen Scroll Value (integer part)
    // 180072   SCXDN0  NBG0 Horizontal Screen Scroll Value (fractional part)
    // 180074   SCYIN0  NBG0 Vertical Screen Scroll Value (integer part)
    // 180076   SCYDN0  NBG0 Vertical Screen Scroll Value (fractional part)
    // 180080   SCXIN1  NBG1 Horizontal Screen Scroll Value (integer part)
    // 180082   SCXDN1  NBG1 Horizontal Screen Scroll Value (fractional part)
    // 180084   SCYIN1  NBG1 Vertical Screen Scroll Value (integer part)
    // 180086   SCYDN1  NBG1 Vertical Screen Scroll Value (fractional part)
    //
    // SCdINx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-0     W  NxSCdI10-0    Horizontal/Vertical Screen Scroll Value (integer part)
    //
    // SCdDNx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //   15-8     W  NxSCdD1-8     Horizontal/Vertical Screen Scroll Value (fractional part)
    //    7-0        -             Reserved, must be zero
    //
    // 180090   SCXN2   NBG2 Horizontal Screen Scroll Value
    // 180092   SCYN2   NBG2 Vertical Screen Scroll Value
    // 180094   SCXN3   NBG3 Horizontal Screen Scroll Value
    // 180096   SCYN3   NBG3 Vertical Screen Scroll Value
    //
    // SCdNx:  (d=X,Y; x=2,3)
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-0     W  NxSCd10-0     Horizontal/Vertical Screen Scroll Value (integer)

    FORCE_INLINE uint16 ReadSCXIN(uint32 bgIndex) {
        return bit::extract<8, 18>(m_NormBGParams[bgIndex].scrollAmountH);
    }

    FORCE_INLINE void WriteSCXIN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<8, 18>(m_NormBGParams[bgIndex].scrollAmountH, bit::extract<0, 10>(value));
    }

    FORCE_INLINE uint16 ReadSCXDN(uint32 bgIndex) {
        return bit::extract<0, 7>(m_NormBGParams[bgIndex].scrollAmountH);
    }

    FORCE_INLINE void WriteSCXDN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<0, 7>(m_NormBGParams[bgIndex].scrollAmountH, bit::extract<8, 15>(value));
    }

    FORCE_INLINE uint16 ReadSCYIN(uint32 bgIndex) {
        return bit::extract<8, 18>(m_NormBGParams[bgIndex].scrollAmountV);
    }

    FORCE_INLINE void WriteSCYIN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<8, 18>(m_NormBGParams[bgIndex].scrollAmountV, bit::extract<0, 10>(value));
    }

    FORCE_INLINE uint16 ReadSCYDN(uint32 bgIndex) {
        return bit::extract<0, 7>(m_NormBGParams[bgIndex].scrollAmountV);
    }

    FORCE_INLINE void WriteSCYDN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<0, 7>(m_NormBGParams[bgIndex].scrollAmountV, bit::extract<8, 15>(value));
    }

    // 180078   ZMXIN0  NBG0 Horizontal Coordinate Increment (integer part)
    // 18007A   ZMXDN0  NBG0 Horizontal Coordinate Increment (fractional part)
    // 18007C   ZMYIN0  NBG0 Vertical Coordinate Increment (integer part)
    // 18007E   ZMYDN0  NBG0 Vertical Coordinate Increment (fractional part)
    // 180088   ZMXIN1  NBG1 Horizontal Coordinate Increment (integer part)
    // 18008A   ZMXDN1  NBG1 Horizontal Coordinate Increment (fractional part)
    // 18008C   ZMYIN1  NBG1 Vertical Coordinate Increment (integer part)
    // 18008E   ZMYDN1  NBG1 Vertical Coordinate Increment (fractional part)
    //
    // ZMdINx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //   15-3        -             Reserved, must be zero
    //    2-0     W  NxZMdI2-0     Horizontal/Vertical Coordinate Increment (integer part)
    //
    // ZMdDNx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //   15-8     W  NxZMdD1-8     Horizontal/Vertical Coordinate Increment (fractional part)
    //    7-0        -             Reserved, must be zero

    FORCE_INLINE uint16 ReadZMXIN(uint32 bgIndex) {
        return bit::extract<8, 10>(m_NormBGParams[bgIndex].scrollIncH);
    }

    FORCE_INLINE void WriteZMXIN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<8, 10>(m_NormBGParams[bgIndex].scrollIncH, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadZMXDN(uint32 bgIndex) {
        return bit::extract<0, 7>(m_NormBGParams[bgIndex].scrollIncH);
    }

    FORCE_INLINE void WriteZMXDN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<0, 7>(m_NormBGParams[bgIndex].scrollIncH, bit::extract<8, 15>(value));
    }

    FORCE_INLINE uint16 ReadZMYIN(uint32 bgIndex) {
        return bit::extract<8, 10>(m_NormBGParams[bgIndex].scrollIncV);
    }

    FORCE_INLINE void WriteZMYIN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<8, 10>(m_NormBGParams[bgIndex].scrollIncV, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadZMYDN(uint32 bgIndex) {
        return bit::extract<0, 7>(m_NormBGParams[bgIndex].scrollIncV);
    }

    FORCE_INLINE void WriteZMYDN(uint16 value, uint32 bgIndex) {
        bit::deposit_into<0, 7>(m_NormBGParams[bgIndex].scrollIncV, bit::extract<8, 15>(value));
    }

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

    // 1800E4   CRAOFA  NBG0-NBG3 Color RAM Address Offset
    //
    //   bits   r/w  code          description
    //     15        -             Reserved, must be zero
    //  14-12     W  N3CAOS2-0     NBG3 Color RAM Adress Offset
    //     11        -             Reserved, must be zero
    //   10-8     W  N2CAOS2-0     NBG2 Color RAM Adress Offset
    //      7        -             Reserved, must be zero
    //    6-4     W  N1CAOS2-0     NBG1/EXBG Color RAM Adress Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  N0CAOS2-0     NBG0/RBG1 Color RAM Adress Offset

    FORCE_INLINE uint16 ReadCRAOFA() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, m_NormBGParams[0].caos);
        bit::deposit_into<4, 6>(value, m_NormBGParams[1].caos);
        bit::deposit_into<8, 10>(value, m_NormBGParams[2].caos);
        bit::deposit_into<12, 14>(value, m_NormBGParams[3].caos);
        return value;
    }

    FORCE_INLINE void WriteCRAOFA(uint16 value) {
        m_NormBGParams[0].caos = bit::extract<0, 2>(value);
        m_NormBGParams[1].caos = bit::extract<4, 6>(value);
        m_NormBGParams[2].caos = bit::extract<8, 10>(value);
        m_NormBGParams[3].caos = bit::extract<12, 14>(value);
        m_RotBGParams[0].caos = m_NormBGParams[0].caos;
    }

    // 1800E6   CRAOFB  RBG0 and Sprite Color RAM Address Offset
    //
    //   bits   r/w  code          description
    //   15-7        -             Reserved, must be zero
    //    6-4     W  SPCAOS2-0     Sprite Color RAM Adress Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  R0CAOS2-0     RBG0 Color RAM Adress Offset

    FORCE_INLINE uint16 ReadCRAOFB() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, m_RotBGParams[0].caos);
        // TODO: SPCAOSn
        // bit::deposit_into<4, 6>(value, ?m_SpriteParams?.caos);
        return value;
    }

    FORCE_INLINE void WriteCRAOFB(uint16 value) {
        m_RotBGParams[0].caos = bit::extract<0, 2>(value);
        // TODO: SPCAOSn
        // ?m_SpriteParams?.caos = bit::extract<4, 6>(value);
    }

    // 1800E8   LNCLEN  Line Color Screen Enable
    //
    //   bits   r/w  code          description
    //   15-6        -             Reserved, must be zero
    //      5     W  SPLCEN        Sprite Line Color Screen Enable
    //      4     W  R0LCEN        RBG0 Line Color Screen Enable
    //      3     W  N3LCEN        NBG3 Line Color Screen Enable
    //      2     W  N2LCEN        NBG2 Line Color Screen Enable
    //      1     W  N1LCEN        NBG1 Line Color Screen Enable
    //      0     W  N0LCEN        NBG0 Line Color Screen Enable

    FORCE_INLINE uint16 ReadLNCLEN() {
        uint16 value = 0;
        bit::deposit_into<0>(value, m_NormBGParams[0].lineColorScreenEnable);
        bit::deposit_into<1>(value, m_NormBGParams[1].lineColorScreenEnable);
        bit::deposit_into<2>(value, m_NormBGParams[2].lineColorScreenEnable);
        bit::deposit_into<3>(value, m_NormBGParams[3].lineColorScreenEnable);
        bit::deposit_into<4>(value, m_RotBGParams[0].lineColorScreenEnable);
        // TODO: bit::deposit_into<5>(value, ?m_SpriteParams?.lineColorScreenEnable);
        return value;
    }

    void WriteLNCLEN(uint16 value) {
        m_NormBGParams[0].lineColorScreenEnable = bit::extract<0>(value);
        m_NormBGParams[1].lineColorScreenEnable = bit::extract<1>(value);
        m_NormBGParams[2].lineColorScreenEnable = bit::extract<2>(value);
        m_NormBGParams[3].lineColorScreenEnable = bit::extract<3>(value);
        m_RotBGParams[0].lineColorScreenEnable = bit::extract<4>(value);
        m_RotBGParams[1].lineColorScreenEnable = m_NormBGParams[0].lineColorScreenEnable;
        // TODO: ?m_SpriteParams?.lineColorScreenEnable = bit::extract<5>(value);
    }

    // 1800EA   SFPRMD  Special Priority Mode
    //
    //   bits   r/w  code          description
    //  15-10        -             Reserved, must be zero
    //    9-8     W  R0SPRM1-0     RBG0 Special Priority Mode
    //    7-6     W  N3SPRM1-0     NBG3 Special Priority Mode
    //    5-4     W  N2SPRM1-0     NBG2 Special Priority Mode
    //    3-2     W  N1SPRM1-0     NBG1/EXBG Special Priority Mode
    //    1-0     W  N0SPRM1-0     NBG0/RBG1 Special Priority Mode
    //
    // For all parameters, use LSB of priority number:
    //   00 (0) = per screen
    //   01 (1) = per character
    //   10 (2) = per pixel
    //   11 (3) = (forbidden)

    FORCE_INLINE uint16 ReadSFPRMD() {
        uint16 value = 0;
        bit::deposit_into<0, 1>(value, static_cast<uint32>(m_NormBGParams[0].priorityMode));
        bit::deposit_into<2, 3>(value, static_cast<uint32>(m_NormBGParams[1].priorityMode));
        bit::deposit_into<4, 5>(value, static_cast<uint32>(m_NormBGParams[2].priorityMode));
        bit::deposit_into<6, 7>(value, static_cast<uint32>(m_NormBGParams[3].priorityMode));
        bit::deposit_into<8, 9>(value, static_cast<uint32>(m_RotBGParams[0].priorityMode));
        return value;
    }

    FORCE_INLINE void WriteSFPRMD(uint16 value) {
        m_NormBGParams[0].priorityMode = static_cast<PriorityMode>(bit::extract<0, 1>(value));
        m_NormBGParams[1].priorityMode = static_cast<PriorityMode>(bit::extract<2, 3>(value));
        m_NormBGParams[2].priorityMode = static_cast<PriorityMode>(bit::extract<4, 5>(value));
        m_NormBGParams[3].priorityMode = static_cast<PriorityMode>(bit::extract<6, 7>(value));
        m_RotBGParams[0].priorityMode = static_cast<PriorityMode>(bit::extract<8, 9>(value));
        m_RotBGParams[1].priorityMode = m_NormBGParams[0].priorityMode;
    }

    CCCTL_t CCCTL;   // 1800EC   CCCTL   Color Calculation Control
    SFCCMD_t SFCCMD; // 1800EE   SFCCMD  Special Color Calculation Mode
    PRI_t PRISA;     // 1800F0   PRISA   Sprite 0 and 1 Priority Number
    PRI_t PRISB;     // 1800F2   PRISB   Sprite 2 and 3 Priority Number
    PRI_t PRISC;     // 1800F4   PRISC   Sprite 4 and 5 Priority Number
    PRI_t PRISD;     // 1800F6   PRISD   Sprite 6 and 7 Priority Number

    // 1800F8   PRINA   NBG0 and NBG1 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  N1PRIN2-0     NBG1 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  N0PRIN2-0     NBG0/RBG1 Priority Number

    FORCE_INLINE uint16 ReadPRINA() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, m_NormBGParams[0].priorityNumber);
        bit::deposit_into<8, 10>(value, m_NormBGParams[1].priorityNumber);
        return value;
    }

    void WritePRINA(uint16 value) {
        m_NormBGParams[0].priorityNumber = bit::extract<0, 2>(value);
        m_NormBGParams[1].priorityNumber = bit::extract<8, 10>(value);
        m_RotBGParams[1].priorityNumber = m_NormBGParams[0].priorityNumber;
    }

    // 1800FA   PRINB   NBG2 and NBG3 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  N3PRIN2-0     NBG3 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  N2PRIN2-0     NBG2 Priority Number

    FORCE_INLINE uint16 ReadPRINB() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, m_NormBGParams[2].priorityNumber);
        bit::deposit_into<8, 10>(value, m_NormBGParams[3].priorityNumber);
        return value;
    }

    void WritePRINB(uint16 value) {
        m_NormBGParams[2].priorityNumber = bit::extract<0, 2>(value);
        m_NormBGParams[3].priorityNumber = bit::extract<8, 10>(value);
    }

    // 1800FC   PRIR    RBG0 Priority Number
    //
    //   bits   r/w  code          description
    //   15-3        -             Reserved, must be zero
    //    2-0     W  R0PRIN2-0     RBG0 Priority Number

    FORCE_INLINE uint16 ReadPRIR() {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, m_RotBGParams[0].priorityNumber);
        return value;
    }

    void WritePRIR(uint16 value) {
        m_RotBGParams[0].priorityNumber = bit::extract<0, 2>(value);
    }

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

    std::array<NormBGParams, 4> m_NormBGParams;
    std::array<RotBGParams, 2> m_RotBGParams;

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

    // -------------------------------------------------------------------------

    // Horizontal display phases:
    // NOTE: dots listed are for NTSC/PAL modes
    // NOTE: each dot takes 4 system (SH-2) cycles
    //
    // 0             320/352        347/375     400/432    427/455 dots
    // +----------------+--------------+-----------+-------------+
    // | Active display | Right border | Horz sync | Left border | (no blanking intervals?)
    // +-+--------------+-+------------+-----------+-------------+
    //   |                |
    //   |                +-- Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    //   |                    The right border is optional.
    //   |
    //   +-- Graphics data is shown here
    enum class HorizontalPhase { Active, RightBorder, HorizontalSync, LeftBorder };
    HorizontalPhase m_HPhase; // Current horizontal display phase

    // Vertical display phases:
    // (from https://wiki.yabause.org/index.php5?title=VDP2, with extra notes by StrikerX3)
    // NOTE: scanlines listed are for NTSC/PAL modes
    //
    // +----------------+ Scanline 0
    // |                |
    // | Active display |   Graphics data is shown here.
    // |                |
    // +----------------+ Scanline 224, 240 or 256
    // |                |   Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    // | Bottom border  |   The bottom border is optional.
    // |                |
    // +----------------+ Scanline 232, 240, 256, 264 or 272
    // |                |
    // | Bottom blanking|   Appears as light black.
    // |                |
    // +----------------+ Scanline 237, 245, 259, 267 or 275
    // |                |
    // | Vertical sync  |   Appears as pure black.
    // |                |
    // +----------------+ Scanline 240, 248, 262, 270 or 278
    // |                |
    // | Top blanking   |   Appears as light black.
    // |                |
    // +----------------+ Scanline 255, 263, 281, 289 or 297
    // |                |   Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    // | Top border     |   The top border is optional.
    // |                |
    // +----------------+ Scanline 262 or 313
    enum class VerticalPhase { Active, BottomBorder, BottomBlanking, VerticalSync, TopBlanking, TopBorder };
    VerticalPhase m_VPhase; // Current vertical display phase

    // Current cycles (for phase timing) measured in system cycles.
    // HCNT is derived from this.
    // TODO: replace with scheduler
    uint64 m_currCycles;
    uint32 m_dotClockMult;
    uint16 m_VCounter;

    // Display resolution (derived from TVMODE)
    uint32 m_HRes; // Horizontal display resolution
    uint32 m_VRes; // Vertical display resolution

    // Display timings
    std::array<uint32, 4> m_HTimings;
    std::array<uint32, 6> m_VTimings;

    // Updates the display resolution and timings based on TVMODE
    void UpdateResolution();

    void IncrementVCounter();

    // Phase handlers
    void BeginHPhaseActiveDisplay();
    void BeginHPhaseRightBorder();
    void BeginHPhaseHorizontalSync();
    void BeginHPhaseLeftBorder();

    void BeginVPhaseActiveDisplay();
    void BeginVPhaseBottomBorder();
    void BeginVPhaseBottomBlanking();
    void BeginVPhaseVerticalSync();
    void BeginVPhaseTopBlanking();
    void BeginVPhaseTopBorder();

    // -------------------------------------------------------------------------
    // Rendering
    // TODO: move to a Renderer class

    // Pattern Name Data, contains parameters for a character
    struct Character {
        uint16 charNum;     // Character number, 15 bits
        uint8 palNum;       // Palette number, 7 bits
        bool specColorCalc; // Special color calculation
        bool specPriority;  // Special priority
        bool flipH;         // Horizontal flip
        bool flipV;         // Vertical flip
    };

    struct BGRenderContext {
        // CRAM base offset for color fetching.
        // Derived from RAMCTL.CRMDn and CRAOFA/CRAOFB.xxCAOSn
        uint32 cramOffset;

        // Pointer to framebuffer matching the current screen resolution.
        vdp::Color888 *framebuffer;
    };

    // Draws the scanline at m_VCounter.
    void DrawLine();

    // Draws a normal scroll BG scanline.
    // bgParams contains the parameters for the BG to draw.
    // rctx contains additional context for the renderer.
    // twoWordChar indicates if character patterns use one word (false) or two words (true).
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <bool twoWordChar, bool fourCellChar, bool wideChar, ColorFormat colorFormat, uint32 colorMode>
    void DrawNormalScrollBG(const NormBGParams &bgParams, BGRenderContext &rctx);

    // Draws a normal bitmap BG scanline.
    // bgParams contains the parameters for the BG to draw.
    // rctx contains additional context for the renderer.
    // colorFormat is the color format for bitmap data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    void DrawNormalBitmapBG(const NormBGParams &bgParams, BGRenderContext &rctx);

    // Fetches a two-word character from VRAM.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    Character FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a one-word character from VRAM.
    // bgParams contains the parameters for the BG to draw.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // largePalette indicates if the color format uses 16 colors (false) or more (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    template <bool fourCellChar, bool largePalette, bool wideChar>
    Character FetchOneWordCharacter(const NormBGParams &bgParams, uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a color from a pixel in the specified cell in a 2x2 character pattern.
    // ch contains character parameters.
    // dotX and dotY specify the coordinates of the pixel within the cell, both ranging from 0 to 7.
    // cellIndex is the index of the cell in the character pattern, ranging from 0 to 3.
    // colorFormat is the value of CHCTLA/CHCTLB.xxCHCNn.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 FetchCharacterColor(uint32 cramOffset, Character ch, uint8 dotX, uint8 dotY, uint32 cellIndex);

    // Fetches a color from a bitmap pixel.
    // bgParams contains the bitmap parameters.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // dotX and dotY specify the coordinates of the pixel within the bitmap.
    // colorFormat is the color format for pixel data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 FetchBitmapColor(const NormBGParams &bgParams, uint32 cramOffset, uint8 dotX, uint8 dotY);

    // Fetches a color from CRAM using the current color mode specified by RAMCTL.CRMDn.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorIndex specifies the color index.
    // colorMode is the CRAM color mode.
    template <uint32 colorMode>
    vdp::Color888 FetchCRAMColor(uint32 cramOffset, uint32 colorIndex);

    // DEBUG: to be removed
    uint64 m_frameNum;
};

} // namespace satemu::vdp2

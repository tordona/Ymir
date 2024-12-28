#pragma once

#include "vdp_defs.hpp"

#include "vdp1_regs.hpp"
#include "vdp2_regs.hpp"

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>
#include <satemu/util/inline.hpp>

#include <fmt/format.h>

#include <array>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

// -----------------------------------------------------------------------------

namespace satemu::vdp {

// Contains both VDP1 and VDP2
class VDP {
public:
    VDP(scu::SCU &scu);

    void Reset(bool hard);

    FORCE_INLINE void SetCallbacks(CBRequestFramebuffer cbRequestFramebuffer, CBFrameComplete cbFrameComplete) {
        m_cbRequestFramebuffer = cbRequestFramebuffer;
        m_cbFrameComplete = cbFrameComplete;
    }

    // TODO: replace with scheduler events
    void Advance(uint64 cycles);

    // -------------------------------------------------------------------------
    // VDP1 memory/register access

    // TODO: should only accept 16-bit accesses

    template <mem_primitive T>
    T VDP1ReadVRAM(uint32 address) {
        return util::ReadBE<T>(&m_VRAM1[address & 0x7FFFF]);
    }

    template <mem_primitive T>
    void VDP1WriteVRAM(uint32 address, T value) {
        util::WriteBE<T>(&m_VRAM1[address & 0x7FFFF], value);
    }

    template <mem_primitive T>
    T VDP1ReadFB(uint32 address) {
        return util::ReadBE<T>(&m_spriteFB[m_drawFB][address & 0x3FFFF]);
    }

    template <mem_primitive T>
    void VDP1WriteFB(uint32 address, T value) {
        util::WriteBE<T>(&m_spriteFB[m_drawFB][address & 0x3FFFF], value);
    }

    template <mem_primitive T>
    T VDP1ReadReg(uint32 address) {
        switch (address) {
        case 0x00: return 0; // TVMR is write-only
        case 0x02: return 0; // FBCR is write-only
        case 0x04: return 0; // PTMR is write-only
        case 0x06: return 0; // EWDR is write-only
        case 0x08: return 0; // EWLR is write-only
        case 0x0A: return 0; // EWRR is write-only
        case 0x0C: return 0; // ENDR is write-only

        case 0x10: return m_VDP1.ReadEDSR();
        case 0x12: return m_VDP1.ReadLOPR();
        case 0x14: return m_VDP1.ReadCOPR();
        case 0x16: return m_VDP1.ReadMODR();

        default: fmt::println("unhandled {}-bit VDP1 register read from {:02X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_primitive T>
    void VDP1WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x00: m_VDP1.WriteTVMR(value); break;
        case 0x02: m_VDP1.WriteFBCR(value); break;
        case 0x04:
            m_VDP1.WritePTMR(value);
            if (m_VDP1.plotTrigger == 0b01) {
                VDP1BeginFrame();
            }
            break;
        case 0x06: m_VDP1.WriteEWDR(value); break;
        case 0x08: m_VDP1.WriteEWLR(value); break;
        case 0x0A: m_VDP1.WriteEWRR(value); break;
        case 0x0C: // ENDR
            // TODO: schedule drawing termination after 30 cycles
            break;

        case 0x10: break; // EDSR is read-only
        case 0x12: break; // LOPR is read-only
        case 0x14: break; // COPR is read-only
        case 0x16: break; // MODR is read-only

        default:
            fmt::println("unhandled {}-bit VDP1 register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }
    // TODO: handle VRSIZE.VRAMSZ in Read/WriteVRAM maybe?
    // TODO: CRAM and registers only accept 16-bit and 32-bit accesses

    // -------------------------------------------------------------------------
    // VDP2 memory/register access

    template <mem_primitive T>
    T VDP2ReadVRAM(uint32 address) {
        /*address &= 0x7FFFF;
        T value = util::ReadBE<T>(&m_VRAM[address]);
        fmt::println("{}-bit VDP2 VRAM read from {:05X} = {:X}", sizeof(T) * 8, address, value);
        return value;*/
        return util::ReadBE<T>(&m_VRAM2[address & 0x7FFFF]);
    }

    template <mem_primitive T>
    void VDP2WriteVRAM(uint32 address, T value) {
        // fmt::println("{}-bit VDP2 VRAM write to {:05X} = {:X}", sizeof(T) * 8, address & 0x7FFFF, value);
        util::WriteBE<T>(&m_VRAM2[address & 0x7FFFF], value);
    }

    template <mem_primitive T>
    T VDP2ReadCRAM(uint32 address) {
        /*address &= MapCRAMAddress(address);
        T value = util::ReadBE<T>(&m_CRAM[address]);
        fmt::println("{}-bit VDP2 CRAM read from {:03X} = {:X}", sizeof(T) * 8, address, value);
        return value;*/
        return util::ReadBE<T>(&m_CRAM[MapCRAMAddress(address)]);
    }

    template <mem_primitive T>
    void VDP2WriteCRAM(uint32 address, T value) {
        address = MapCRAMAddress(address);
        // fmt::println("{}-bit VDP2 CRAM write to {:05X} = {:X}", sizeof(T) * 8, address, value);
        util::WriteBE<T>(&m_CRAM[address], value);
        if (m_VDP2.RAMCTL.CRMDn == 0) {
            // fmt::println("   replicated to {:05X}", address ^ 0x800);
            util::WriteBE<T>(&m_CRAM[address ^ 0x800], value);
        }
    }

    template <mem_primitive T>
    T VDP2ReadReg(uint32 address) {
        switch (address) {
        case 0x000: return m_VDP2.TVMD.u16;
        case 0x002: return m_VDP2.EXTEN.u16;
        case 0x004: return m_VDP2.TVSTAT.u16;
        case 0x006: return m_VDP2.VRSIZE.u16;
        case 0x008: return m_VDP2.HCNT;
        case 0x00A: return m_VDP2.VCNT;
        case 0x00C: return 0; // unknown/hidden register
        case 0x00E: return m_VDP2.RAMCTL.u16;
        case 0x010: return m_VDP2.CYCA0.L.u16;   // write-only?
        case 0x012: return m_VDP2.CYCA0.U.u16;   // write-only?
        case 0x014: return m_VDP2.CYCA1.L.u16;   // write-only?
        case 0x016: return m_VDP2.CYCA1.U.u16;   // write-only?
        case 0x018: return m_VDP2.CYCB0.L.u16;   // write-only?
        case 0x01A: return m_VDP2.CYCB0.U.u16;   // write-only?
        case 0x01E: return m_VDP2.CYCB1.L.u16;   // write-only?
        case 0x01C: return m_VDP2.CYCB1.U.u16;   // write-only?
        case 0x020: return m_VDP2.ReadBGON();    // write-only?
        case 0x022: return m_VDP2.ReadMZCTL();   // write-only?
        case 0x024: return m_VDP2.ReadSFSEL();   // write-only?
        case 0x026: return m_VDP2.ReadSFCODE();  // write-only?
        case 0x028: return m_VDP2.ReadCHCTLA();  // write-only?
        case 0x02A: return m_VDP2.ReadCHCTLB();  // write-only?
        case 0x02C: return m_VDP2.ReadBMPNA();   // write-only?
        case 0x02E: return m_VDP2.ReadBMPNB();   // write-only?
        case 0x030: return m_VDP2.ReadPNCN(1);   // write-only?
        case 0x032: return m_VDP2.ReadPNCN(2);   // write-only?
        case 0x034: return m_VDP2.ReadPNCN(3);   // write-only?
        case 0x036: return m_VDP2.ReadPNCN(4);   // write-only?
        case 0x038: return m_VDP2.ReadPNCR();    // write-only?
        case 0x03A: return m_VDP2.ReadPLSZ();    // write-only?
        case 0x03C: return m_VDP2.ReadMPOFN();   // write-only?
        case 0x03E: return m_VDP2.ReadMPOFR();   // write-only?
        case 0x040: return m_VDP2.ReadMPN(1, 0); // write-only?
        case 0x042: return m_VDP2.ReadMPN(1, 1); // write-only?
        case 0x044: return m_VDP2.ReadMPN(2, 0); // write-only?
        case 0x046: return m_VDP2.ReadMPN(2, 1); // write-only?
        case 0x048: return m_VDP2.ReadMPN(3, 0); // write-only?
        case 0x04A: return m_VDP2.ReadMPN(3, 1); // write-only?
        case 0x04C: return m_VDP2.ReadMPN(4, 0); // write-only?
        case 0x04E: return m_VDP2.ReadMPN(4, 1); // write-only?
        case 0x050: return m_VDP2.ReadMPR(0, 0); // write-only?
        case 0x052: return m_VDP2.ReadMPR(0, 1); // write-only?
        case 0x054: return m_VDP2.ReadMPR(0, 2); // write-only?
        case 0x056: return m_VDP2.ReadMPR(0, 3); // write-only?
        case 0x058: return m_VDP2.ReadMPR(0, 4); // write-only?
        case 0x05A: return m_VDP2.ReadMPR(0, 5); // write-only?
        case 0x05C: return m_VDP2.ReadMPR(0, 6); // write-only?
        case 0x05E: return m_VDP2.ReadMPR(0, 7); // write-only?
        case 0x060: return m_VDP2.ReadMPR(1, 0); // write-only?
        case 0x062: return m_VDP2.ReadMPR(1, 1); // write-only?
        case 0x064: return m_VDP2.ReadMPR(1, 2); // write-only?
        case 0x066: return m_VDP2.ReadMPR(1, 3); // write-only?
        case 0x068: return m_VDP2.ReadMPR(1, 4); // write-only?
        case 0x06A: return m_VDP2.ReadMPR(1, 5); // write-only?
        case 0x06C: return m_VDP2.ReadMPR(1, 6); // write-only?
        case 0x06E: return m_VDP2.ReadMPR(1, 7); // write-only?
        case 0x070: return m_VDP2.ReadSCXIN(1);  // write-only?
        case 0x072: return m_VDP2.ReadSCXDN(1);  // write-only?
        case 0x074: return m_VDP2.ReadSCYIN(1);  // write-only?
        case 0x076: return m_VDP2.ReadSCYDN(1);  // write-only?
        case 0x078: return m_VDP2.ReadZMXIN(1);  // write-only?
        case 0x07A: return m_VDP2.ReadZMXDN(1);  // write-only?
        case 0x07C: return m_VDP2.ReadZMYIN(1);  // write-only?
        case 0x07E: return m_VDP2.ReadZMYDN(1);  // write-only?
        case 0x080: return m_VDP2.ReadSCXIN(2);  // write-only?
        case 0x082: return m_VDP2.ReadSCXDN(2);  // write-only?
        case 0x084: return m_VDP2.ReadSCYIN(2);  // write-only?
        case 0x086: return m_VDP2.ReadSCYDN(2);  // write-only?
        case 0x088: return m_VDP2.ReadZMXIN(2);  // write-only?
        case 0x08A: return m_VDP2.ReadZMXDN(2);  // write-only?
        case 0x08C: return m_VDP2.ReadZMYIN(2);  // write-only?
        case 0x08E: return m_VDP2.ReadZMYDN(2);  // write-only?
        case 0x090: return m_VDP2.ReadSCXIN(3);  // write-only?
        case 0x092: return m_VDP2.ReadSCYIN(3);  // write-only?
        case 0x094: return m_VDP2.ReadSCXIN(4);  // write-only?
        case 0x096: return m_VDP2.ReadSCYIN(4);  // write-only?
        case 0x098: return m_VDP2.ZMCTL.u16;     // write-only?
        case 0x09A: return m_VDP2.ReadSCRCTL();  // write-only?
        case 0x09C: return m_VDP2.ReadVCSTAU();  // write-only?
        case 0x09E: return m_VDP2.ReadVCSTAL();  // write-only?
        case 0x0A0: return m_VDP2.ReadLSTAnU(1); // write-only?
        case 0x0A2: return m_VDP2.ReadLSTAnL(1); // write-only?
        case 0x0A4: return m_VDP2.ReadLSTAnU(2); // write-only?
        case 0x0A6: return m_VDP2.ReadLSTAnL(2); // write-only?
        case 0x0A8: return m_VDP2.ReadLCTAU();   // write-only?
        case 0x0AA: return m_VDP2.ReadLCTAL();   // write-only?
        case 0x0AC: return m_VDP2.ReadBKTAU();   // write-only?
        case 0x0AE: return m_VDP2.ReadBKTAL();   // write-only?
        case 0x0B0: return m_VDP2.ReadRPMD();    // write-only?
        case 0x0B2: return m_VDP2.ReadRPRCTL();  // write-only?
        case 0x0B4: return m_VDP2.ReadKTCTL();   // write-only?
        case 0x0B6: return m_VDP2.ReadKTAOF();   // write-only?
        case 0x0B8: return m_VDP2.ReadOVPNRn(0); // write-only?
        case 0x0BA: return m_VDP2.ReadOVPNRn(1); // write-only?
        case 0x0BC: return m_VDP2.ReadRPTAU();   // write-only?
        case 0x0BE: return m_VDP2.ReadRPTAL();   // write-only?
        case 0x0C0: return m_VDP2.WPXY0.X.S.u16; // write-only?
        case 0x0C2: return m_VDP2.WPXY0.X.E.u16; // write-only?
        case 0x0C4: return m_VDP2.WPXY0.Y.S.u16; // write-only?
        case 0x0C6: return m_VDP2.WPXY0.Y.E.u16; // write-only?
        case 0x0C8: return m_VDP2.WPXY1.X.S.u16; // write-only?
        case 0x0CA: return m_VDP2.WPXY1.X.E.u16; // write-only?
        case 0x0CC: return m_VDP2.WPXY1.Y.S.u16; // write-only?
        case 0x0CE: return m_VDP2.WPXY1.Y.E.u16; // write-only?
        case 0x0D0: return m_VDP2.WCTL.A.u16;    // write-only?
        case 0x0D2: return m_VDP2.WCTL.B.u16;    // write-only?
        case 0x0D4: return m_VDP2.WCTL.C.u16;    // write-only?
        case 0x0D6: return m_VDP2.WCTL.D.u16;    // write-only?
        case 0x0D8: return m_VDP2.LWTA0.U.u16;   // write-only?
        case 0x0DA: return m_VDP2.LWTA0.L.u16;   // write-only?
        case 0x0DC: return m_VDP2.LWTA1.U.u16;   // write-only?
        case 0x0DE: return m_VDP2.LWTA1.L.u16;   // write-only?
        case 0x0E0: return m_VDP2.ReadSPCTL();   // write-only?
        case 0x0E2: return m_VDP2.SDCTL.u16;     // write-only?
        case 0x0E4: return m_VDP2.ReadCRAOFA();  // write-only?
        case 0x0E6: return m_VDP2.ReadCRAOFB();  // write-only?
        case 0x0E8: return m_VDP2.ReadLNCLEN();  // write-only?
        case 0x0EA: return m_VDP2.ReadSFPRMD();  // write-only?
        case 0x0EC: return m_VDP2.ReadCCCTL();   // write-only?
        case 0x0EE: return m_VDP2.ReadSFCCMD();  // write-only?
        case 0x0F0: return m_VDP2.ReadPRISn(0);  // write-only?
        case 0x0F2: return m_VDP2.ReadPRISn(1);  // write-only?
        case 0x0F4: return m_VDP2.ReadPRISn(2);  // write-only?
        case 0x0F6: return m_VDP2.ReadPRISn(3);  // write-only?
        case 0x0F8: return m_VDP2.ReadPRINA();   // write-only?
        case 0x0FA: return m_VDP2.ReadPRINB();   // write-only?
        case 0x0FC: return m_VDP2.ReadPRIR();    // write-only?
        case 0x0FE: return 0;                    // supposedly reserved
        case 0x100: return m_VDP2.ReadCCRSn(0);  // write-only?
        case 0x102: return m_VDP2.ReadCCRSn(1);  // write-only?
        case 0x104: return m_VDP2.ReadCCRSn(2);  // write-only?
        case 0x106: return m_VDP2.ReadCCRSn(3);  // write-only?
        case 0x108: return m_VDP2.ReadCCRNA();   // write-only?
        case 0x10A: return m_VDP2.ReadCCRNB();   // write-only?
        case 0x10C: return m_VDP2.ReadCCRR();    // write-only?
        case 0x10E: return m_VDP2.ReadCCRLB();   // write-only?
        case 0x110: return m_VDP2.ReadCLOFEN();  // write-only?
        case 0x112: return m_VDP2.ReadCLOFSL();  // write-only?
        case 0x114: return m_VDP2.ReadCOxR(0);   // write-only?
        case 0x116: return m_VDP2.ReadCOxG(0);   // write-only?
        case 0x118: return m_VDP2.ReadCOxB(0);   // write-only?
        case 0x11A: return m_VDP2.ReadCOxR(1);   // write-only?
        case 0x11C: return m_VDP2.ReadCOxG(1);   // write-only?
        case 0x11E: return m_VDP2.ReadCOxB(1);   // write-only?
        default: fmt::println("unhandled {}-bit VDP2 register read from {:03X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_primitive T>
    void VDP2WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x000: {
            const uint16 oldTVMD = m_VDP2.TVMD.u16;
            m_VDP2.TVMD.u16 = value & 0x81F7;
            m_VDP2.TVMDDirty = m_VDP2.TVMD.u16 != oldTVMD;
            break;
        }
        case 0x002: m_VDP2.EXTEN.u16 = value & 0x0303; break;
        case 0x004: /* TVSTAT is read-only */ break;
        case 0x006: m_VDP2.VRSIZE.u16 = value & 0x8000; break;
        case 0x008: /* HCNT is read-only */ break;
        case 0x00A: /* VCNT is read-only */ break;
        case 0x00C: /* unknown/hidden register */ break;
        case 0x00E: m_VDP2.RAMCTL.u16 = value & 0xB3FF; break;
        case 0x010: m_VDP2.CYCA0.L.u16 = value; break;
        case 0x012: m_VDP2.CYCA0.U.u16 = value; break;
        case 0x014: m_VDP2.CYCA1.L.u16 = value; break;
        case 0x016: m_VDP2.CYCA1.U.u16 = value; break;
        case 0x018: m_VDP2.CYCB0.L.u16 = value; break;
        case 0x01A: m_VDP2.CYCB0.U.u16 = value; break;
        case 0x01E: m_VDP2.CYCB1.U.u16 = value; break;
        case 0x01C: m_VDP2.CYCB1.L.u16 = value; break;
        case 0x020: m_VDP2.WriteBGON(value), VDP2UpdateEnabledBGs(); break;
        case 0x022: m_VDP2.WriteMZCTL(value); break;
        case 0x024: m_VDP2.WriteSFSEL(value); break;
        case 0x026: m_VDP2.WriteSFCODE(value); break;
        case 0x028: m_VDP2.WriteCHCTLA(value); break;
        case 0x02A: m_VDP2.WriteCHCTLB(value); break;
        case 0x02C: m_VDP2.WriteBMPNA(value); break;
        case 0x02E: m_VDP2.WriteBMPNB(value); break;
        case 0x030: m_VDP2.WritePNCN(1, value); break;
        case 0x032: m_VDP2.WritePNCN(2, value); break;
        case 0x034: m_VDP2.WritePNCN(3, value); break;
        case 0x036: m_VDP2.WritePNCN(4, value); break;
        case 0x038: m_VDP2.WritePNCR(value); break;
        case 0x03A: m_VDP2.WritePLSZ(value); break;
        case 0x03C: m_VDP2.WriteMPOFN(value); break;
        case 0x03E: m_VDP2.WriteMPOFR(value); break;
        case 0x040: m_VDP2.WriteMPN(1, 0, value); break;
        case 0x042: m_VDP2.WriteMPN(1, 1, value); break;
        case 0x044: m_VDP2.WriteMPN(2, 0, value); break;
        case 0x046: m_VDP2.WriteMPN(2, 1, value); break;
        case 0x048: m_VDP2.WriteMPN(3, 0, value); break;
        case 0x04A: m_VDP2.WriteMPN(3, 1, value); break;
        case 0x04C: m_VDP2.WriteMPN(4, 0, value); break;
        case 0x04E: m_VDP2.WriteMPN(4, 1, value); break;
        case 0x050: m_VDP2.WriteMPR(0, 0, value); break;
        case 0x052: m_VDP2.WriteMPR(0, 1, value); break;
        case 0x054: m_VDP2.WriteMPR(0, 2, value); break;
        case 0x056: m_VDP2.WriteMPR(0, 3, value); break;
        case 0x058: m_VDP2.WriteMPR(0, 4, value); break;
        case 0x05A: m_VDP2.WriteMPR(0, 5, value); break;
        case 0x05C: m_VDP2.WriteMPR(0, 6, value); break;
        case 0x05E: m_VDP2.WriteMPR(0, 7, value); break;
        case 0x060: m_VDP2.WriteMPR(1, 0, value); break;
        case 0x062: m_VDP2.WriteMPR(1, 1, value); break;
        case 0x064: m_VDP2.WriteMPR(1, 2, value); break;
        case 0x066: m_VDP2.WriteMPR(1, 3, value); break;
        case 0x068: m_VDP2.WriteMPR(1, 4, value); break;
        case 0x06A: m_VDP2.WriteMPR(1, 5, value); break;
        case 0x06C: m_VDP2.WriteMPR(1, 6, value); break;
        case 0x06E: m_VDP2.WriteMPR(1, 7, value); break;
        case 0x070: m_VDP2.WriteSCXIN(1, value); break;
        case 0x072: m_VDP2.WriteSCXDN(1, value); break;
        case 0x074: m_VDP2.WriteSCYIN(1, value); break;
        case 0x076: m_VDP2.WriteSCYDN(1, value); break;
        case 0x078: m_VDP2.WriteZMXIN(1, value); break;
        case 0x07A: m_VDP2.WriteZMXDN(1, value); break;
        case 0x07C: m_VDP2.WriteZMYIN(1, value); break;
        case 0x07E: m_VDP2.WriteZMYDN(1, value); break;
        case 0x080: m_VDP2.WriteSCXIN(2, value); break;
        case 0x082: m_VDP2.WriteSCXDN(2, value); break;
        case 0x084: m_VDP2.WriteSCYIN(2, value); break;
        case 0x086: m_VDP2.WriteSCYDN(2, value); break;
        case 0x088: m_VDP2.WriteZMXIN(2, value); break;
        case 0x08A: m_VDP2.WriteZMXDN(2, value); break;
        case 0x08C: m_VDP2.WriteZMYIN(2, value); break;
        case 0x08E: m_VDP2.WriteZMYDN(2, value); break;
        case 0x090: m_VDP2.WriteSCXIN(3, value); break;
        case 0x092: m_VDP2.WriteSCYIN(3, value); break;
        case 0x094: m_VDP2.WriteSCXIN(4, value); break;
        case 0x096: m_VDP2.WriteSCYIN(4, value); break;
        case 0x098: m_VDP2.ZMCTL.u16 = value & 0x0303; break;
        case 0x09A: m_VDP2.WriteSCRCTL(value); break;
        case 0x09C: m_VDP2.WriteVCSTAU(value); break;
        case 0x09E: m_VDP2.WriteVCSTAL(value); break;
        case 0x0A0: m_VDP2.WriteLSTAnU(1, value); break;
        case 0x0A2: m_VDP2.WriteLSTAnL(1, value); break;
        case 0x0A4: m_VDP2.WriteLSTAnU(2, value); break;
        case 0x0A6: m_VDP2.WriteLSTAnL(2, value); break;
        case 0x0A8: m_VDP2.WriteLCTAU(value); break;
        case 0x0AA: m_VDP2.WriteLCTAL(value); break;
        case 0x0AC: m_VDP2.WriteBKTAU(value); break;
        case 0x0AE: m_VDP2.WriteBKTAL(value); break;
        case 0x0B0: m_VDP2.WriteRPMD(value); break;
        case 0x0B2: m_VDP2.WriteRPRCTL(value); break;
        case 0x0B4: m_VDP2.WriteKTCTL(value); break;
        case 0x0B6: m_VDP2.WriteKTAOF(value); break;
        case 0x0B8: m_VDP2.WriteOVPNRn(0, value); break;
        case 0x0BA: m_VDP2.WriteOVPNRn(1, value); break;
        case 0x0BC: m_VDP2.WriteRPTAU(value); break;
        case 0x0BE: m_VDP2.WriteRPTAL(value); break;
        case 0x0C0: m_VDP2.WPXY0.X.S.u16 = value & 0x03FF; break;
        case 0x0C2: m_VDP2.WPXY0.X.E.u16 = value & 0x03FF; break;
        case 0x0C4: m_VDP2.WPXY0.Y.S.u16 = value & 0x01FF; break;
        case 0x0C6: m_VDP2.WPXY0.Y.E.u16 = value & 0x01FF; break;
        case 0x0C8: m_VDP2.WPXY1.X.S.u16 = value & 0x03FF; break;
        case 0x0CA: m_VDP2.WPXY1.X.E.u16 = value & 0x03FF; break;
        case 0x0CC: m_VDP2.WPXY1.Y.S.u16 = value & 0x01FF; break;
        case 0x0CE: m_VDP2.WPXY1.Y.E.u16 = value & 0x01FF; break;
        case 0x0D0: m_VDP2.WCTL.A.u16 = value & 0xBFBF; break;
        case 0x0D2: m_VDP2.WCTL.B.u16 = value & 0xBFBF; break;
        case 0x0D4: m_VDP2.WCTL.C.u16 = value & 0xBFBF; break;
        case 0x0D6: m_VDP2.WCTL.D.u16 = value & 0xBF8F; break;
        case 0x0D8: m_VDP2.LWTA0.U.u16 = value & 0x8007; break;
        case 0x0DA: m_VDP2.LWTA0.L.u16 = value & 0xFFFE; break;
        case 0x0DC: m_VDP2.LWTA1.U.u16 = value & 0x8007; break;
        case 0x0DE: m_VDP2.LWTA1.L.u16 = value & 0xFFFE; break;
        case 0x0E0: m_VDP2.WriteSPCTL(value); break;
        case 0x0E2: m_VDP2.SDCTL.u16 = value & 0x013F; break;
        case 0x0E4: m_VDP2.WriteCRAOFA(value); break;
        case 0x0E6: m_VDP2.WriteCRAOFB(value); break;
        case 0x0E8: m_VDP2.WriteLNCLEN(value); break;
        case 0x0EA: m_VDP2.WriteSFPRMD(value); break;
        case 0x0EC: m_VDP2.WriteCCCTL(value); break;
        case 0x0EE: m_VDP2.WriteSFCCMD(value); break;
        case 0x0F0: m_VDP2.WritePRISn(0, value); break;
        case 0x0F2: m_VDP2.WritePRISn(1, value); break;
        case 0x0F4: m_VDP2.WritePRISn(2, value); break;
        case 0x0F6: m_VDP2.WritePRISn(3, value); break;
        case 0x0F8: m_VDP2.WritePRINA(value); break;
        case 0x0FA: m_VDP2.WritePRINB(value); break;
        case 0x0FC: m_VDP2.WritePRIR(value); break;
        case 0x0FE: break; // supposedly reserved
        case 0x100: m_VDP2.WriteCCRSn(0, value); break;
        case 0x102: m_VDP2.WriteCCRSn(1, value); break;
        case 0x104: m_VDP2.WriteCCRSn(2, value); break;
        case 0x106: m_VDP2.WriteCCRSn(3, value); break;
        case 0x108: m_VDP2.WriteCCRNA(value); break;
        case 0x10A: m_VDP2.WriteCCRNB(value); break;
        case 0x10C: m_VDP2.WriteCCRR(value); break;
        case 0x10E: m_VDP2.WriteCCRLB(value); break;
        case 0x110: m_VDP2.WriteCLOFEN(value); break;
        case 0x112: m_VDP2.WriteCLOFSL(value); break;
        case 0x114: m_VDP2.WriteCOxR(0, value); break;
        case 0x116: m_VDP2.WriteCOxG(0, value); break;
        case 0x118: m_VDP2.WriteCOxB(0, value); break;
        case 0x11A: m_VDP2.WriteCOxR(1, value); break;
        case 0x11C: m_VDP2.WriteCOxG(1, value); break;
        case 0x11E: m_VDP2.WriteCOxB(1, value); break;
        default:
            fmt::println("unhandled {}-bit VDP2 register write to {:03X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
    }

    bool InTopBlankingPhase() const {
        return m_VPhase == VerticalPhase::TopBlanking;
    }

private:
    std::array<uint8, kVDP1VRAMSize> m_VRAM1;
    std::array<uint8, kVDP2VRAMSize> m_VRAM2; // 4x 128 KiB banks: A0, A1, B0, B1
    std::array<uint8, kVDP2CRAMSize> m_CRAM;
    std::array<std::array<uint8, kVDP1FramebufferRAMSize>, 2> m_spriteFB;
    size_t m_drawFB; // index of current sprite draw buffer; opposite buffer is CPU-accessible

    scu::SCU &m_SCU;

    // -------------------------------------------------------------------------
    // Frontend callbacks

    // Invoked when the renderer is about to start a new frame, to retrieve a buffer from the frontend in which to
    // render the screen. The frame will contain <width> x <height> pixels in XBGR8888 little-endian format.
    CBRequestFramebuffer m_cbRequestFramebuffer;

    // Invoked whne the renderer finishes drawing a frame.
    CBFrameComplete m_cbFrameComplete;

    // -------------------------------------------------------------------------
    // Registers

    VDP1Regs m_VDP1;
    VDP2Regs m_VDP2;

    // -------------------------------------------------------------------------

    // RAMCTL.CRMD modes 2 and 3 shuffle address bits as follows:
    //   10 09 08 07 06 05 04 03 02 01 11 00
    //   in short, bits 10-01 are shifted left and bit 11 takes place of bit 01
    FORCE_INLINE uint32 MapCRAMAddress(uint32 address) const {
        if (m_VDP2.RAMCTL.CRMDn == 2 || m_VDP2.RAMCTL.CRMDn == 3) {
            address =
                bit::extract<0>(address) | (bit::extract<11>(address) << 1u) | (bit::extract<1, 10>(address) << 2u);
        }
        return address;
    }

    // -------------------------------------------------------------------------
    // Timings and signals

    // Horizontal display phases:
    // NOTE: dots listed are for NTSC/PAL modes
    // NOTE: each dot takes 4 system (SH-2) cycles
    //
    // 0             320/352        347/375     400/432       427/455 dots
    // +----------------+--------------+-----------+-------------+
    // | Active display | Right border | Horz sync | Left border | (no blanking intervals?)
    // +-+--------------+-+------------+-----------+-+-----------+
    //   |                |                          |
    //   |                +------------+-------------+
    //   |                             |
    //   |      Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    //   |      The border is optional.
    //   |
    //   +-- Graphics data is shown here
    //
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

    enum class HorizontalPhase { Active, RightBorder, HorizontalSync, LeftBorder };
    HorizontalPhase m_HPhase; // Current horizontal display phase

    enum class VerticalPhase { Active, BottomBorder, BottomBlanking, VerticalSync, TopBlanking, TopBorder, LastLine };
    VerticalPhase m_VPhase; // Current vertical display phase

    // Current cycles (for phase timing) measured in system cycles.
    // HCNT is derived from this.
    // TODO: replace with scheduler
    uint64 m_currCycles;
    uint32 m_dotClockMult;

    // 180008   HCNT    H Counter
    //
    //   bits   r/w  code          description
    //  15-10        -             Reserved, must be zero
    //    9-0   R    HCT9-0        H Counter Value
    //
    // Notes
    // - Counter layout depends on screen mode:
    //     Normal: bits 8-0 shifted left by 1; HCT0 is invalid
    //     Hi-Res: bits 9-0
    //     Excl. Normal: bits 8-0 (no shift); HCT9 is invalid
    //     Excl. Hi-Res: bits 9-1 shifted right by 1; HCT9 is invalid
    //
    // 18000A   VCNT    V Counter
    //
    //   bits   r/w  code          description
    //  15-10        -             Reserved, must be zero
    //    9-0   R    VCT9-0        V Counter Value
    //
    // Notes
    // - Counter layout depends on screen mode:
    //     Exclusive Monitor: bits 9-0
    //     Normal Hi-Res double-density interlace:
    //       bits 8-0 shifted left by 1
    //       bit 0 contains interlaced field (0=odd, 1=even)
    //     All other modes: bits 8-0 shifted left by 1; VCT0 is invalid

    // TODO: store latched HCounter
    uint16 m_VCounter;

    // Display resolution (derived from TVMODE)
    uint32 m_HRes; // Horizontal display resolution
    uint32 m_VRes; // Vertical display resolution

    // Display timings
    std::array<uint32, 4> m_HTimings;
    std::array<uint32, 7> m_VTimings;

    // Updates the display resolution and timings based on TVMODE if it is dirty
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
    void BeginVPhaseLastLine();

    // -------------------------------------------------------------------------
    // VDP1

    // TODO: split out rendering code

    // VDP1 renderer parameters and state
    struct VDP1RenderContext {
        VDP1RenderContext() {
            Reset();
        }

        void Reset() {
            sysClipH = 512;
            sysClipV = 256;

            userClipX0 = 0;
            userClipY0 = 0;

            userClipX1 = 512;
            userClipY1 = 256;

            localCoordX = 0;
            localCoordY = 0;
        }

        // System clipping dimensions
        uint16 sysClipH;
        uint16 sysClipV;

        // User clipping area
        // Top-left
        uint16 userClipX0;
        uint16 userClipY0;
        // Bottom-right
        uint16 userClipX1;
        uint16 userClipY1;

        // Local coordinates offset
        sint32 localCoordX;
        sint32 localCoordY;
    } m_VDP1RenderContext;

    // Pattern Name Data, contains parameters for a character
    struct Character {
        uint16 charNum;     // Character number, 15 bits
        uint8 palNum;       // Palette number, 7 bits
        bool specColorCalc; // Special color calculation
        bool specPriority;  // Special priority
        bool flipH;         // Horizontal flip
        bool flipV;         // Vertical flip
    };

    struct Pixel {
        Color888 color = {.u32 = 0};
        bool transparent = true;
        uint8 priority = 0;
    };

    struct LayerState {
        LayerState() {
            Reset();
        }

        void Reset() {
            pixels.fill({});
            enabled = false;
        }

        alignas(16) std::array<Pixel, 704> pixels;

        bool enabled;
    };

    struct SpriteLayerState {
        SpriteLayerState() {
            Reset();
        }

        void Reset() {
            attrs.fill({});
        }

        struct Attributes {
            uint8 colorCalcRatio = 0;
            bool shadowOrWindow = false;
            bool msbSet = false;
        };

        alignas(16) std::array<Attributes, 704> attrs;
    };

    struct NormBGLayerState {
        NormBGLayerState() {
            Reset();
        }

        void Reset() {
            cramOffset = 0;
            fracScrollX = 0;
            fracScrollY = 0;
            scrollIncH = 0x100;
            lineScrollTableAddress = 0;
            mosaicCounterY = 0;
        }

        // CRAM base offset for color fetching.
        // Derived from RAMCTL.CRMDn and CRAOFA.NxCAOSn
        uint32 cramOffset;

        // Initial fractional X scroll coordinate.
        uint32 fracScrollX;

        // Fractional Y scroll coordinate.
        // Reset at the start of every frame and updated every scanline.
        uint32 fracScrollY;

        // Fractional X scroll coordinate increment.
        // Applied every scanline.
        uint32 scrollIncH;

        // Current line scroll table address.
        // Reset at the start of every frame and incremented every 1/2/4/8/16 lines.
        uint32 lineScrollTableAddress;

        // Vertical mosaic counter.
        // Reset at the start of every frame and incremented every line.
        // The value is mod mosaicV.
        uint8 mosaicCounterY;
    };

    struct RotBGLayerState {
        RotBGLayerState() {
            Reset();
        }

        void Reset() {
            cramOffset = 0;
        }

        // CRAM base offset for color fetching.
        // Derived from RAMCTL.CRMDn and CRAOFB.R0CAOSn
        uint32 cramOffset;
    };

    // State for Rotation Parameters A and B
    struct RotParamState {
        RotParamState() {
            Reset();
        }

        void Reset() {
            pageBaseAddresses.fill(0);
            scrXIncV = scrYIncV = 0;
            scrXIncH = scrYIncH = 0;
            KA = 0;
            KAst = 0;
            dKAst = 0;
            dKAx = 0;
        }

        // Calculates counters and increments using the given rotation parameter table.
        void Calculate(const RotationParamTable &t, bool readKAst) {
            // 16*(16-16) + 16*(16-16) + 16*(16-16) = 32 frac bits
            // reduce to 16 frac bits
            Xsp = (t.A * (t.Xst - t.Px) + t.B * (t.Yst - t.Py) + t.C * (t.Zst - t.Pz)) >> 16ll;
            Ysp = (t.D * (t.Xst - t.Px) + t.E * (t.Yst - t.Py) + t.F * (t.Zst - t.Pz)) >> 16ll;

            // 16*(16-16) + 16*(16-16) + 16*(16-16) + 16 + 16 = 32+32+32 + 16+16
            // reduce 32 to 16 frac bits, result is 16 frac bits
            Xp = ((t.A * (t.Px - t.Cx) + t.B * (t.Py - t.Cy) + t.C * (t.Pz - t.Cz)) >> 16ll) + t.Cx + t.Mx;
            Yp = ((t.D * (t.Px - t.Cx) + t.E * (t.Py - t.Cy) + t.F * (t.Pz - t.Cz)) >> 16ll) + t.Cy + t.My;

            kx = t.kx;
            ky = t.ky;

            // Increment per Vcnt
            // 16*16 + 16*16 = 32
            // reduce to 16 frac bits
            scrXIncV = (t.A * t.deltaXst + t.B * t.deltaYst) >> 16ll;
            scrYIncV = (t.D * t.deltaXst + t.E * t.deltaYst) >> 16ll;

            // Increment per Hcnt
            // 16*16 + 16*16 = 32 frac bits
            // reduce to 16 frac bits
            scrXIncH = (t.A * t.deltaX + t.B * t.deltaY) >> 16ll;
            scrYIncH = (t.D * t.deltaX + t.E * t.deltaY) >> 16ll;

            // ---

            // All of these have 10 fractional bits
            // No maths involved, so store them directly

            KAst = t.KAst;
            dKAst = t.dKAst;
            dKAx = t.dKAx;

            // Reload coefficient address if requested
            if (readKAst) {
                KA = KAst;
            }
        }

        // Increments counters by one Vcnt step
        void IncrementV() {
            KA += dKAst;
        }

        // Page base addresses for RBG planes A-P using Rotation Parameters A and B.
        // Indexing: [Plane A-P]
        // Derived from mapIndices, CHCTLA/CHCTLB.xxCHSZ, PNCR.xxPNB and PLSZ.xxPLSZn
        std::array<uint32, 16> pageBaseAddresses;

        // Transformed initial screen coordinates, calculated from parameter table.
        sint32 Xsp, Ysp;

        // Transformed view coordinates, calculated from parameter table.
        sint32 Xp, Yp;

        // Scaling coefficients retrieved from the table (with 16 fractional bits).
        sint32 kx, ky;

        // Current screen coordinates (with 16 fractional bits).
        // Initialized to (scrX0, scrY0) when the rotation parameter table is read.
        // Incremented by (kx*scrXIncV, ky*scrYIncV) every scanline and by (kx*scrXIncH, ky*scrYIncH) every pixel.
        sint32 scrXIncV, scrYIncV; // Screen coordinate increments per vertical counter increment
        sint32 scrXIncH, scrYIncH; // Screen coordinate increments per horizontal counter increment

        // Current coefficient table address with 16 fractional bits.
        // Initialized to KAst when the rotation parameter table is read.
        // Incremented by dKAst every scanline and (in a copy) by dKAx every pixel.
        uint32 KA;
        uint32 KAst;  // Initial coefficient table address; updated when the rotation parameter table is read
        sint32 dKAst; // Coefficient table address increment per vertical counter increment
        sint32 dKAx;  // Coefficient table address increment per horizontal counter increment
    };

    // Layer state indices
    enum Layer { LYR_Sprite, LYR_RBG0, LYR_NBG0_RBG1, LYR_NBG1_EXBG, LYR_NBG2, LYR_NBG3, LYR_Back };

    // Common layer states
    //     RBG0+RBG1   RBG0        no RBGs
    // [0] Sprite      Sprite      Sprite
    // [1] RBG0        RBG0        -
    // [2] RBG1        NBG0        NBG0
    // [3] EXBG        NBG1/EXBG   NBG1/EXBG
    // [4] -           NBG2        NBG2
    // [5] -           NBG3        NBG3
    std::array<LayerState, 6> m_layerStates;

    // Sprite layer state
    SpriteLayerState m_spriteLayerState;

    // Layer state for NBGs 0-3
    std::array<NormBGLayerState, 4> m_normBGLayerStates;

    // Layer state for RBGs 0-1
    std::array<RotBGLayerState, 2> m_rotBGLayerStates;

    // States for Rotation Parameters A and B.
    std::array<RotParamState, 2> m_rotParamStates;

    // Framebuffer provided by the frontend to render the current frame into
    FramebufferColor *m_framebuffer;

    // Gets the current VDP1 draw framebuffer.
    std::array<uint8, kVDP1FramebufferRAMSize> &VDP1GetDrawFB();

    // Gets the current VDP1 display framebuffer.
    std::array<uint8, kVDP1FramebufferRAMSize> &VDP1GetDisplayFB();

    // Erases the current VDP1 display framebuffer.
    void VDP1EraseFramebuffer();

    // Swaps VDP1 framebuffers.
    void VDP1SwapFramebuffer();

    // Begins the next VDP1 frame.
    void VDP1BeginFrame();

    // Ends the current VDP1 frame.
    void VDP1EndFrame();

    // Processes the VDP1 command table.
    void VDP1ProcessCommands();

    bool VDP1IsPixelUserClipped(sint32 x, sint32 y) const;
    bool VDP1IsPixelSystemClipped(sint32 x, sint32 y) const;
    bool VDP1IsLineSystemClipped(sint32 x1, sint32 y1, sint32 x2, sint32 y2) const;
    bool VDP1IsQuadSystemClipped(sint32 x1, sint32 y1, sint32 x2, sint32 y2, sint32 x3, sint32 y3, sint32 x4,
                                 sint32 y4) const;

    void VDP1PlotPixel(sint32 x, sint32 y, uint16 color, VDP1Command::DrawMode mode, uint32 gouraudTable);
    void VDP1PlotLine(sint32 x1, sint32 y1, sint32 x2, sint32 y2, uint16 color, VDP1Command::DrawMode mode,
                      uint32 gouraudTable);
    void VDP1PlotTexturedLine(sint32 x1, sint32 y1, sint32 x2, sint32 y2, uint32 colorBank,
                              VDP1Command::Control control, VDP1Command::DrawMode mode, uint32 gouraudTable,
                              uint32 charAddr, uint32 charSizeH, uint32 charSizeV, uint32 v, bool swapped);

    // Individual VDP1 command processors

    void VDP1Cmd_DrawNormalSprite(uint32 cmdAddress, VDP1Command::Control control);
    void VDP1Cmd_DrawScaledSprite(uint32 cmdAddress, VDP1Command::Control control);
    void VDP1Cmd_DrawDistortedSprite(uint32 cmdAddress, VDP1Command::Control control);

    void VDP1Cmd_DrawPolygon(uint32 cmdAddress);
    void VDP1Cmd_DrawPolylines(uint32 cmdAddress);
    void VDP1Cmd_DrawLine(uint32 cmdAddress);

    void VDP1Cmd_SetSystemClipping(uint32 cmdAddress);
    void VDP1Cmd_SetUserClipping(uint32 cmdAddress);
    void VDP1Cmd_SetLocalCoordinates(uint32 cmdAddress);

    // -------------------------------------------------------------------------
    // VDP2

    // Initializes renderer state for a new frame.
    void VDP2InitFrame();

    // Initializes the specified NBG.
    template <uint32 index>
    void VDP2InitNormalBG();

    // Initializes the specified RBG.
    template <uint32 index>
    void VDP2InitRotationBG();

    // Updates the enabled backgrounds.
    void VDP2UpdateEnabledBGs();

    // Updates the line screen scroll parameters for the given background.
    // Only valid for NBG0 and NBG1.
    //
    // bgParams contains the parameters for the BG to draw.
    // bgState is a reference to the background layer state for the background.
    void VDP2UpdateLineScreenScroll(const BGParams &bgParams, NormBGLayerState &bgState);

    // Loads rotation parameter tables and calculates coefficients and increments.
    void VDP2LoadRotationParameterTables();

    // Draws the current VDP2 scanline.
    void VDP2DrawLine();

    // Draws the current VDP2 scanline of the sprite layer.
    //
    // colorMode is the CRAM color mode.
    template <uint32 colorMode>
    void VDP2DrawSpriteLayer();

    // Draws the current VDP2 scanline of the specified normal background layer.
    //
    // bgIndex specifies the normal background index, from 0 to 3.
    // colorMode is the CRAM color mode.
    void VDP2DrawNormalBG(uint32 bgIndex, uint32 colorMode);

    // Draws the current VDP2 scanline of the specified rotation background layer.
    //
    // bgIndex specifies the rotation background index, from 0 to 1.
    // colorMode is the CRAM color mode.
    void VDP2DrawRotationBG(uint32 bgIndex, uint32 colorMode);

    // Composes the current VDP2 scanline out of the rendered lines.
    void VDP2ComposeLine();

    // Draws a normal scroll BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    // bgState is a reference to the background layer state for the background.
    //
    // twoWordChar indicates if character patterns use one word (false) or two words (true).
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <bool twoWordChar, bool fourCellChar, bool wideChar, ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawNormalScrollBG(const BGParams &bgParams, LayerState &layerState, NormBGLayerState &bgState);

    // Draws a normal bitmap BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    // bgState is a reference to the background layer state for the background.
    //
    // colorFormat is the color format for bitmap data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawNormalBitmapBG(const BGParams &bgParams, LayerState &layerState, NormBGLayerState &bgState);

    // Draws a rotation scroll BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    // bgState is a reference to the background layer state for the background.
    //
    // twoWordChar indicates if character patterns use one word (false) or two words (true).
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <bool twoWordChar, bool fourCellChar, bool wideChar, ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawRotationScrollBG(const BGParams &bgParams, LayerState &layerState, RotBGLayerState &bgState);

    // Draws a rotation bitmap BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    // bgState is a reference to the background layer state for the background.
    //
    // colorFormat is the color format for bitmap data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawRotationBitmapBG(const BGParams &bgParams, LayerState &layerState, RotBGLayerState &bgState);

    // Fetches a rotation coefficient entry from VRAM or CRAM (depending on RAMCTL.CRKTE) using the specified rotation
    // parameters.
    //
    // params is the rotation parameter from which to retrieve the base address and coefficient data size.
    // coeffAddress is the calculated coefficient address (KA).
    Coefficient VDP2FetchRotationCoefficient(const RotationParams &params, uint32 coeffAddress);

    // Fetches a two-word character from VRAM.
    //
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    Character VDP2FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a one-word character from VRAM.
    //
    // bgParams contains the parameters for the BG to draw.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // largePalette indicates if the color format uses 16 colors (false) or more (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    template <bool fourCellChar, bool largePalette, bool wideChar>
    Character VDP2FetchOneWordCharacter(const BGParams &bgParams, uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a color from a pixel in the specified cell in a 2x2 character pattern.
    //
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorData is an output variable where bits 3-1 of the palette color data from VRAM is stored.
    // transparent is an output variable where the transparency of a pixel is set.
    // ch contains character parameters.
    // dotX and dotY specify the coordinates of the pixel within the cell, both ranging from 0 to 7.
    // cellIndex is the index of the cell in the character pattern, ranging from 0 to 3.
    // colorFormat is the value of CHCTLA/CHCTLB.xxCHCNn.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 VDP2FetchCharacterColor(uint32 cramOffset, uint8 &colorData, bool &transparent, Character ch,
                                          uint8 dotX, uint8 dotY, uint32 cellIndex);

    // Fetches a color from a bitmap pixel.
    //
    // bgParams contains the bitmap parameters.
    // transparent is an output variable where the transparency of a pixel is set.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // dotX and dotY specify the coordinates of the pixel within the bitmap.
    // colorFormat is the color format for pixel data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 VDP2FetchBitmapColor(const BGParams &bgParams, bool &transparent, uint32 cramOffset, uint32 dotX,
                                       uint32 dotY);

    // Fetches a color from CRAM using the current color mode specified by RAMCTL.CRMDn.
    //
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorIndex specifies the color index.
    // colorMode is the CRAM color mode.
    template <uint32 colorMode>
    vdp::Color888 VDP2FetchCRAMColor(uint32 cramOffset, uint32 colorIndex);

    // Fetches sprite data based on the current sprite mode.
    //
    // fbOffset is the offset into the framebuffer (in bytes) where the sprite data is located.
    SpriteData VDP2FetchSpriteData(uint32 fbOffset);

    // Fetches 8-bit sprite data based on the current sprite mode.
    //
    // fbOffset is the offset into the framebuffer (in bytes) where the sprite data is located.
    // type is the sprite type (between 8 and 15).
    SpriteData VDP2FetchByteSpriteData(uint32 fbOffset, uint8 type);

    // Fetches 16-bit sprite data based on the current sprite mode.
    //
    // fbOffset is the offset into the framebuffer (in bytes) where the sprite data is located.
    // type is the sprite type (between 0 and 7).
    SpriteData VDP2FetchWordSpriteData(uint32 fbOffset, uint8 type);
};

} // namespace satemu::vdp

#pragma once

#include "vdp_defs.hpp"

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

        case 0x10: return m_VDP1regs.ReadEDSR();
        case 0x12: return m_VDP1regs.ReadLOPR();
        case 0x14: return m_VDP1regs.ReadCOPR();
        case 0x16: return m_VDP1regs.ReadMODR();

        default: fmt::println("unhandled {}-bit VDP1 register read from {:02X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_primitive T>
    void VDP1WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x00: m_VDP1regs.WriteTVMR(value); break;
        case 0x02: m_VDP1regs.WriteFBCR(value); break;
        case 0x04:
            m_VDP1regs.WritePTMR(value);
            if (m_VDP1regs.params.plotTrigger == 0b01) {
                VDP1BeginFrame();
            }
            break;
        case 0x06: m_VDP1regs.WriteEWDR(value); break;
        case 0x08: m_VDP1regs.WriteEWLR(value); break;
        case 0x0A: m_VDP1regs.WriteEWRR(value); break;
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
        if (m_VDP2regs.RAMCTL.CRMDn == 0) {
            // fmt::println("   replicated to {:05X}", address ^ 0x800);
            util::WriteBE<T>(&m_CRAM[address ^ 0x800], value);
        }
    }

    template <mem_primitive T>
    T VDP2ReadReg(uint32 address) {
        switch (address) {
        case 0x000: return m_VDP2regs.TVMD.u16;
        case 0x002: return m_VDP2regs.EXTEN.u16;
        case 0x004: return m_VDP2regs.TVSTAT.u16;
        case 0x006: return m_VDP2regs.VRSIZE.u16;
        case 0x008: return m_VDP2regs.HCNT;
        case 0x00A: return m_VDP2regs.VCNT;
        case 0x00E: return m_VDP2regs.RAMCTL.u16;
        case 0x010: return m_VDP2regs.CYCA0.L.u16;   // write-only?
        case 0x012: return m_VDP2regs.CYCA0.U.u16;   // write-only?
        case 0x014: return m_VDP2regs.CYCA1.L.u16;   // write-only?
        case 0x016: return m_VDP2regs.CYCA1.U.u16;   // write-only?
        case 0x018: return m_VDP2regs.CYCB0.L.u16;   // write-only?
        case 0x01A: return m_VDP2regs.CYCB0.U.u16;   // write-only?
        case 0x01E: return m_VDP2regs.CYCB1.L.u16;   // write-only?
        case 0x01C: return m_VDP2regs.CYCB1.U.u16;   // write-only?
        case 0x020: return m_VDP2regs.ReadBGON();    // write-only?
        case 0x022: return m_VDP2regs.MZCTL.u16;     // write-only?
        case 0x024: return m_VDP2regs.ReadSFSEL();   // write-only?
        case 0x026: return m_VDP2regs.ReadSFCODE();  // write-only?
        case 0x028: return m_VDP2regs.ReadCHCTLA();  // write-only?
        case 0x02A: return m_VDP2regs.ReadCHCTLB();  // write-only?
        case 0x02C: return m_VDP2regs.ReadBMPNA();   // write-only?
        case 0x02E: return m_VDP2regs.ReadBMPNB();   // write-only?
        case 0x030: return m_VDP2regs.ReadPNCN(0);   // write-only?
        case 0x032: return m_VDP2regs.ReadPNCN(1);   // write-only?
        case 0x034: return m_VDP2regs.ReadPNCN(2);   // write-only?
        case 0x036: return m_VDP2regs.ReadPNCN(3);   // write-only?
        case 0x038: return m_VDP2regs.ReadPNCR();    // write-only?
        case 0x03A: return m_VDP2regs.ReadPLSZ();    // write-only?
        case 0x03C: return m_VDP2regs.ReadMPOFN();   // write-only?
        case 0x03E: return m_VDP2regs.ReadMPOFR();   // write-only?
        case 0x040: return m_VDP2regs.ReadMPN(0, 0); // write-only?
        case 0x042: return m_VDP2regs.ReadMPN(0, 1); // write-only?
        case 0x044: return m_VDP2regs.ReadMPN(1, 0); // write-only?
        case 0x046: return m_VDP2regs.ReadMPN(1, 1); // write-only?
        case 0x048: return m_VDP2regs.ReadMPN(2, 0); // write-only?
        case 0x04A: return m_VDP2regs.ReadMPN(2, 1); // write-only?
        case 0x04C: return m_VDP2regs.ReadMPN(3, 0); // write-only?
        case 0x04E: return m_VDP2regs.ReadMPN(3, 1); // write-only?
        case 0x050: return m_VDP2regs.ReadMPR(0, 0); // write-only?
        case 0x052: return m_VDP2regs.ReadMPR(0, 1); // write-only?
        case 0x054: return m_VDP2regs.ReadMPR(0, 2); // write-only?
        case 0x056: return m_VDP2regs.ReadMPR(0, 3); // write-only?
        case 0x058: return m_VDP2regs.ReadMPR(0, 4); // write-only?
        case 0x05A: return m_VDP2regs.ReadMPR(0, 5); // write-only?
        case 0x05C: return m_VDP2regs.ReadMPR(0, 6); // write-only?
        case 0x05E: return m_VDP2regs.ReadMPR(0, 7); // write-only?
        case 0x060: return m_VDP2regs.ReadMPR(1, 0); // write-only?
        case 0x062: return m_VDP2regs.ReadMPR(1, 1); // write-only?
        case 0x064: return m_VDP2regs.ReadMPR(1, 2); // write-only?
        case 0x066: return m_VDP2regs.ReadMPR(1, 3); // write-only?
        case 0x068: return m_VDP2regs.ReadMPR(1, 4); // write-only?
        case 0x06A: return m_VDP2regs.ReadMPR(1, 5); // write-only?
        case 0x06C: return m_VDP2regs.ReadMPR(1, 6); // write-only?
        case 0x06E: return m_VDP2regs.ReadMPR(1, 7); // write-only?
        case 0x070: return m_VDP2regs.ReadSCXIN(0);  // write-only?
        case 0x072: return m_VDP2regs.ReadSCXDN(0);  // write-only?
        case 0x074: return m_VDP2regs.ReadSCYIN(0);  // write-only?
        case 0x076: return m_VDP2regs.ReadSCYDN(0);  // write-only?
        case 0x078: return m_VDP2regs.ReadZMXIN(0);  // write-only?
        case 0x07A: return m_VDP2regs.ReadZMXDN(0);  // write-only?
        case 0x07C: return m_VDP2regs.ReadZMYIN(0);  // write-only?
        case 0x07E: return m_VDP2regs.ReadZMYDN(0);  // write-only?
        case 0x080: return m_VDP2regs.ReadSCXIN(1);  // write-only?
        case 0x082: return m_VDP2regs.ReadSCXDN(1);  // write-only?
        case 0x084: return m_VDP2regs.ReadSCYIN(1);  // write-only?
        case 0x086: return m_VDP2regs.ReadSCYDN(1);  // write-only?
        case 0x088: return m_VDP2regs.ReadZMXIN(1);  // write-only?
        case 0x08A: return m_VDP2regs.ReadZMXDN(1);  // write-only?
        case 0x08C: return m_VDP2regs.ReadZMYIN(1);  // write-only?
        case 0x08E: return m_VDP2regs.ReadZMYDN(1);  // write-only?
        case 0x090: return m_VDP2regs.ReadSCXIN(2);  // write-only?
        case 0x092: return m_VDP2regs.ReadSCYIN(2);  // write-only?
        case 0x094: return m_VDP2regs.ReadSCXIN(3);  // write-only?
        case 0x096: return m_VDP2regs.ReadSCYIN(3);  // write-only?
        case 0x098: return m_VDP2regs.ZMCTL.u16;     // write-only?
        case 0x09A: return m_VDP2regs.SCRCTL.u16;    // write-only?
        case 0x09C: return m_VDP2regs.VCSTA.U.u16;   // write-only?
        case 0x09E: return m_VDP2regs.VCSTA.L.u16;   // write-only?
        case 0x0A0: return m_VDP2regs.LSTA0.U.u16;   // write-only?
        case 0x0A2: return m_VDP2regs.LSTA0.L.u16;   // write-only?
        case 0x0A4: return m_VDP2regs.LSTA1.U.u16;   // write-only?
        case 0x0A6: return m_VDP2regs.LSTA1.L.u16;   // write-only?
        case 0x0A8: return m_VDP2regs.LCTA.U.u16;    // write-only?
        case 0x0AA: return m_VDP2regs.LCTA.L.u16;    // write-only?
        case 0x0AC: return m_VDP2regs.BKTA.U.u16;    // write-only?
        case 0x0AE: return m_VDP2regs.BKTA.L.u16;    // write-only?
        case 0x0B0: return m_VDP2regs.RPMD.u16;      // write-only?
        case 0x0B2: return m_VDP2regs.RPRCTL.u16;    // write-only?
        case 0x0B4: return m_VDP2regs.KTCTL.u16;     // write-only?
        case 0x0B6: return m_VDP2regs.KTAOF.u16;     // write-only?
        case 0x0B8: return m_VDP2regs.OVPNRA;        // write-only?
        case 0x0BA: return m_VDP2regs.OVPNRB;        // write-only?
        case 0x0BC: return m_VDP2regs.RPTA.U.u16;    // write-only?
        case 0x0BE: return m_VDP2regs.RPTA.L.u16;    // write-only?
        case 0x0C0: return m_VDP2regs.WPXY0.X.S.u16; // write-only?
        case 0x0C2: return m_VDP2regs.WPXY0.X.E.u16; // write-only?
        case 0x0C4: return m_VDP2regs.WPXY0.Y.S.u16; // write-only?
        case 0x0C6: return m_VDP2regs.WPXY0.Y.E.u16; // write-only?
        case 0x0C8: return m_VDP2regs.WPXY1.X.S.u16; // write-only?
        case 0x0CA: return m_VDP2regs.WPXY1.X.E.u16; // write-only?
        case 0x0CC: return m_VDP2regs.WPXY1.Y.S.u16; // write-only?
        case 0x0CE: return m_VDP2regs.WPXY1.Y.E.u16; // write-only?
        case 0x0D0: return m_VDP2regs.WCTL.A.u16;    // write-only?
        case 0x0D2: return m_VDP2regs.WCTL.B.u16;    // write-only?
        case 0x0D4: return m_VDP2regs.WCTL.C.u16;    // write-only?
        case 0x0D6: return m_VDP2regs.WCTL.D.u16;    // write-only?
        case 0x0D8: return m_VDP2regs.LWTA0.U.u16;   // write-only?
        case 0x0DA: return m_VDP2regs.LWTA0.L.u16;   // write-only?
        case 0x0DC: return m_VDP2regs.LWTA1.U.u16;   // write-only?
        case 0x0DE: return m_VDP2regs.LWTA1.L.u16;   // write-only?
        case 0x0E0: return m_VDP2regs.SPCTL.u16;     // write-only?
        case 0x0E2: return m_VDP2regs.SDCTL.u16;     // write-only?
        case 0x0E4: return m_VDP2regs.ReadCRAOFA();  // write-only?
        case 0x0E6: return m_VDP2regs.ReadCRAOFB();  // write-only?
        case 0x0E8: return m_VDP2regs.ReadLNCLEN();  // write-only?
        case 0x0EA: return m_VDP2regs.ReadSFPRMD();  // write-only?
        case 0x0EC: return m_VDP2regs.CCCTL.u16;     // write-only?
        case 0x0EE: return m_VDP2regs.SFCCMD.u16;    // write-only?
        case 0x0F0: return m_VDP2regs.PRISA.u16;     // write-only?
        case 0x0F2: return m_VDP2regs.PRISB.u16;     // write-only?
        case 0x0F4: return m_VDP2regs.PRISC.u16;     // write-only?
        case 0x0F6: return m_VDP2regs.PRISD.u16;     // write-only?
        case 0x0F8: return m_VDP2regs.ReadPRINA();   // write-only?
        case 0x0FA: return m_VDP2regs.ReadPRINB();   // write-only?
        case 0x0FC: return m_VDP2regs.ReadPRIR();    // write-only?
        case 0x100: return m_VDP2regs.CCRSA.u16;     // write-only?
        case 0x102: return m_VDP2regs.CCRSB.u16;     // write-only?
        case 0x104: return m_VDP2regs.CCRSC.u16;     // write-only?
        case 0x106: return m_VDP2regs.CCRSD.u16;     // write-only?
        case 0x108: return m_VDP2regs.CCRNA.u16;     // write-only?
        case 0x10A: return m_VDP2regs.CCRNB.u16;     // write-only?
        case 0x10C: return m_VDP2regs.CCRR.u16;      // write-only?
        case 0x10E: return m_VDP2regs.CCRLB.u16;     // write-only?
        case 0x110: return m_VDP2regs.CLOFEN.u16;    // write-only?
        case 0x112: return m_VDP2regs.CLOFSL.u16;    // write-only?
        case 0x114: return m_VDP2regs.COAR.u16;      // write-only?
        case 0x116: return m_VDP2regs.COAG.u16;      // write-only?
        case 0x118: return m_VDP2regs.COAB.u16;      // write-only?
        case 0x11A: return m_VDP2regs.COBR.u16;      // write-only?
        case 0x11C: return m_VDP2regs.COBG.u16;      // write-only?
        case 0x11E: return m_VDP2regs.COBB.u16;      // write-only?
        default: fmt::println("unhandled {}-bit VDP2 register read from {:03X}", sizeof(T) * 8, address); return 0;
        }
    }

    template <mem_primitive T>
    void VDP2WriteReg(uint32 address, T value) {
        switch (address) {
        case 0x000:
            m_VDP2regs.TVMD.u16 = value & 0x81F7;
            m_VDP2regs.TVMDDirty = true;
            break;
        case 0x002: m_VDP2regs.EXTEN.u16 = value & 0x0303; break;
        case 0x004: /* TVSTAT is read-only */ break;
        case 0x006: m_VDP2regs.VRSIZE.u16 = value & 0x8000; break;
        case 0x008: /* HCNT is read-only */ break;
        case 0x00A: /* VCNT is read-only */ break;
        case 0x00E: m_VDP2regs.RAMCTL.u16 = value & 0xB3FF; break;
        case 0x010: m_VDP2regs.CYCA0.L.u16 = value; break;
        case 0x012: m_VDP2regs.CYCA0.U.u16 = value; break;
        case 0x014: m_VDP2regs.CYCA1.L.u16 = value; break;
        case 0x016: m_VDP2regs.CYCA1.U.u16 = value; break;
        case 0x018: m_VDP2regs.CYCB0.L.u16 = value; break;
        case 0x01A: m_VDP2regs.CYCB0.U.u16 = value; break;
        case 0x01E: m_VDP2regs.CYCB1.U.u16 = value; break;
        case 0x01C: m_VDP2regs.CYCB1.L.u16 = value; break;
        case 0x020: m_VDP2regs.WriteBGON(value); break;
        case 0x022: m_VDP2regs.MZCTL.u16 = value & 0xFF1F; break;
        case 0x024: m_VDP2regs.WriteSFSEL(value); break;
        case 0x026: m_VDP2regs.WriteSFCODE(value); break;
        case 0x028: m_VDP2regs.WriteCHCTLA(value); break;
        case 0x02A: m_VDP2regs.WriteCHCTLB(value); break;
        case 0x02C: m_VDP2regs.WriteBMPNA(value); break;
        case 0x02E: m_VDP2regs.WriteBMPNB(value); break;
        case 0x030: m_VDP2regs.WritePNCN(0, value); break;
        case 0x032: m_VDP2regs.WritePNCN(1, value); break;
        case 0x034: m_VDP2regs.WritePNCN(2, value); break;
        case 0x036: m_VDP2regs.WritePNCN(3, value); break;
        case 0x038: m_VDP2regs.WritePNCR(value); break;
        case 0x03A: m_VDP2regs.WritePLSZ(value); break;
        case 0x03C: m_VDP2regs.WriteMPOFN(value); break;
        case 0x03E: m_VDP2regs.WriteMPOFR(value); break;
        case 0x040: m_VDP2regs.WriteMPN(value, 0, 0); break;
        case 0x042: m_VDP2regs.WriteMPN(value, 0, 1); break;
        case 0x044: m_VDP2regs.WriteMPN(value, 1, 0); break;
        case 0x046: m_VDP2regs.WriteMPN(value, 1, 1); break;
        case 0x048: m_VDP2regs.WriteMPN(value, 2, 0); break;
        case 0x04A: m_VDP2regs.WriteMPN(value, 2, 1); break;
        case 0x04C: m_VDP2regs.WriteMPN(value, 3, 0); break;
        case 0x04E: m_VDP2regs.WriteMPN(value, 3, 1); break;
        case 0x050: m_VDP2regs.WriteMPR(value, 0, 0); break;
        case 0x052: m_VDP2regs.WriteMPR(value, 0, 1); break;
        case 0x054: m_VDP2regs.WriteMPR(value, 0, 2); break;
        case 0x056: m_VDP2regs.WriteMPR(value, 0, 3); break;
        case 0x058: m_VDP2regs.WriteMPR(value, 0, 4); break;
        case 0x05A: m_VDP2regs.WriteMPR(value, 0, 5); break;
        case 0x05C: m_VDP2regs.WriteMPR(value, 0, 6); break;
        case 0x05E: m_VDP2regs.WriteMPR(value, 0, 7); break;
        case 0x060: m_VDP2regs.WriteMPR(value, 1, 0); break;
        case 0x062: m_VDP2regs.WriteMPR(value, 1, 1); break;
        case 0x064: m_VDP2regs.WriteMPR(value, 1, 2); break;
        case 0x066: m_VDP2regs.WriteMPR(value, 1, 3); break;
        case 0x068: m_VDP2regs.WriteMPR(value, 1, 4); break;
        case 0x06A: m_VDP2regs.WriteMPR(value, 1, 5); break;
        case 0x06C: m_VDP2regs.WriteMPR(value, 1, 6); break;
        case 0x06E: m_VDP2regs.WriteMPR(value, 1, 7); break;
        case 0x070: m_VDP2regs.WriteSCXIN(value, 0); break;
        case 0x072: m_VDP2regs.WriteSCXDN(value, 0); break;
        case 0x074: m_VDP2regs.WriteSCYIN(value, 0); break;
        case 0x076: m_VDP2regs.WriteSCYDN(value, 0); break;
        case 0x078: m_VDP2regs.WriteZMXIN(value, 0); break;
        case 0x07A: m_VDP2regs.WriteZMXDN(value, 0); break;
        case 0x07C: m_VDP2regs.WriteZMYIN(value, 0); break;
        case 0x07E: m_VDP2regs.WriteZMYDN(value, 0); break;
        case 0x080: m_VDP2regs.WriteSCXIN(value, 1); break;
        case 0x082: m_VDP2regs.WriteSCXDN(value, 1); break;
        case 0x084: m_VDP2regs.WriteSCYIN(value, 1); break;
        case 0x086: m_VDP2regs.WriteSCYDN(value, 1); break;
        case 0x088: m_VDP2regs.WriteZMXIN(value, 1); break;
        case 0x08A: m_VDP2regs.WriteZMXDN(value, 1); break;
        case 0x08C: m_VDP2regs.WriteZMYIN(value, 1); break;
        case 0x08E: m_VDP2regs.WriteZMYDN(value, 1); break;
        case 0x090: m_VDP2regs.WriteSCXIN(value, 2); break;
        case 0x092: m_VDP2regs.WriteSCYIN(value, 2); break;
        case 0x094: m_VDP2regs.WriteSCXIN(value, 3); break;
        case 0x096: m_VDP2regs.WriteSCYIN(value, 3); break;
        case 0x098: m_VDP2regs.ZMCTL.u16 = value & 0x0303; break;
        case 0x09A: m_VDP2regs.SCRCTL.u16 = value & 0x3F3F; break;
        case 0x09C: m_VDP2regs.VCSTA.U.u16 = value & 0x0007; break;
        case 0x09E: m_VDP2regs.VCSTA.L.u16 = value & 0xFFFE; break;
        case 0x0A0: m_VDP2regs.LSTA0.U.u16 = value & 0x0007; break;
        case 0x0A2: m_VDP2regs.LSTA0.L.u16 = value & 0xFFFE; break;
        case 0x0A4: m_VDP2regs.LSTA1.U.u16 = value & 0x0007; break;
        case 0x0A6: m_VDP2regs.LSTA1.L.u16 = value & 0xFFFE; break;
        case 0x0A8: m_VDP2regs.LCTA.U.u16 = value & 0x8007; break;
        case 0x0AA: m_VDP2regs.LCTA.L.u16 = value; break;
        case 0x0AC: m_VDP2regs.BKTA.U.u16 = value & 0x8007; break;
        case 0x0AE: m_VDP2regs.BKTA.L.u16 = value; break;
        case 0x0B0: m_VDP2regs.RPMD.u16 = value & 0x0003; break;
        case 0x0B2: m_VDP2regs.RPRCTL.u16 = value & 0x0707; break;
        case 0x0B4: m_VDP2regs.KTCTL.u16 = value & 0x1F1F; break;
        case 0x0B6: m_VDP2regs.KTAOF.u16 = value & 0x0707; break;
        case 0x0B8: m_VDP2regs.OVPNRA = value; break;
        case 0x0BA: m_VDP2regs.OVPNRB = value; break;
        case 0x0BC: m_VDP2regs.RPTA.U.u16 = value & 0x0007; break;
        case 0x0BE: m_VDP2regs.RPTA.L.u16 = value & 0xFFFE; break;
        case 0x0C0: m_VDP2regs.WPXY0.X.S.u16 = value & 0x03FF; break;
        case 0x0C2: m_VDP2regs.WPXY0.X.E.u16 = value & 0x03FF; break;
        case 0x0C4: m_VDP2regs.WPXY0.Y.S.u16 = value & 0x01FF; break;
        case 0x0C6: m_VDP2regs.WPXY0.Y.E.u16 = value & 0x01FF; break;
        case 0x0C8: m_VDP2regs.WPXY1.X.S.u16 = value & 0x03FF; break;
        case 0x0CA: m_VDP2regs.WPXY1.X.E.u16 = value & 0x03FF; break;
        case 0x0CC: m_VDP2regs.WPXY1.Y.S.u16 = value & 0x01FF; break;
        case 0x0CE: m_VDP2regs.WPXY1.Y.E.u16 = value & 0x01FF; break;
        case 0x0D0: m_VDP2regs.WCTL.A.u16 = value & 0xBFBF; break;
        case 0x0D2: m_VDP2regs.WCTL.B.u16 = value & 0xBFBF; break;
        case 0x0D4: m_VDP2regs.WCTL.C.u16 = value & 0xBFBF; break;
        case 0x0D6: m_VDP2regs.WCTL.D.u16 = value & 0xBF8F; break;
        case 0x0D8: m_VDP2regs.LWTA0.U.u16 = value & 0x8007; break;
        case 0x0DA: m_VDP2regs.LWTA0.L.u16 = value & 0xFFFE; break;
        case 0x0DC: m_VDP2regs.LWTA1.U.u16 = value & 0x8007; break;
        case 0x0DE: m_VDP2regs.LWTA1.L.u16 = value & 0xFFFE; break;
        case 0x0E0: m_VDP2regs.SPCTL.u16 = value & 0x373F; break;
        case 0x0E2: m_VDP2regs.SDCTL.u16 = value & 0x013F; break;
        case 0x0E4: m_VDP2regs.WriteCRAOFA(value); break;
        case 0x0E6: m_VDP2regs.WriteCRAOFB(value); break;
        case 0x0E8: m_VDP2regs.WriteLNCLEN(value); break;
        case 0x0EA: m_VDP2regs.WriteSFPRMD(value); break;
        case 0x0EC: m_VDP2regs.CCCTL.u16 = value & 0xF77F; break;
        case 0x0EE: m_VDP2regs.SFCCMD.u16 = value & 0x03FF; break;
        case 0x0F0: m_VDP2regs.PRISA.u16 = value & 0x0707; break;
        case 0x0F2: m_VDP2regs.PRISB.u16 = value & 0x0707; break;
        case 0x0F4: m_VDP2regs.PRISC.u16 = value & 0x0707; break;
        case 0x0F6: m_VDP2regs.PRISD.u16 = value & 0x0707; break;
        case 0x0F8: m_VDP2regs.WritePRINA(value); break;
        case 0x0FA: m_VDP2regs.WritePRINB(value); break;
        case 0x0FC: m_VDP2regs.WritePRIR(value); break;
        case 0x100: m_VDP2regs.CCRSA.u16 = value & 0x1F1F; break;
        case 0x102: m_VDP2regs.CCRSB.u16 = value & 0x1F1F; break;
        case 0x104: m_VDP2regs.CCRSC.u16 = value & 0x1F1F; break;
        case 0x106: m_VDP2regs.CCRSD.u16 = value & 0x1F1F; break;
        case 0x108: m_VDP2regs.CCRNA.u16 = value & 0x1F1F; break;
        case 0x10A: m_VDP2regs.CCRNB.u16 = value & 0x1F1F; break;
        case 0x10C: m_VDP2regs.CCRR.u16 = value & 0x001F; break;
        case 0x10E: m_VDP2regs.CCRLB.u16 = value & 0x1F1F; break;
        case 0x110: m_VDP2regs.CLOFEN.u16 = value & 0x007F; break;
        case 0x112: m_VDP2regs.CLOFSL.u16 = value & 0x007F; break;
        case 0x114: m_VDP2regs.COAR.u16 = value & 0x01FF; break;
        case 0x116: m_VDP2regs.COAG.u16 = value & 0x01FF; break;
        case 0x118: m_VDP2regs.COAB.u16 = value & 0x01FF; break;
        case 0x11A: m_VDP2regs.COBR.u16 = value & 0x01FF; break;
        case 0x11C: m_VDP2regs.COBG.u16 = value & 0x01FF; break;
        case 0x11E: m_VDP2regs.COBB.u16 = value & 0x01FF; break;
        default:
            fmt::println("unhandled {}-bit VDP2 register write to {:03X} = {:X}", sizeof(T) * 8, address, value);
            break;
        }
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

    VDP1Regs m_VDP1regs;
    VDP2Regs m_VDP2regs;

    // -------------------------------------------------------------------------

    // RAMCTL.CRMD modes 2 and 3 shuffle address bits as follows:
    //   10 09 08 07 06 05 04 03 02 01 11 00
    //   in short, bits 10-01 are shifted left and bit 11 takes place of bit 01
    FORCE_INLINE uint32 MapCRAMAddress(uint32 address) const {
        if (m_VDP2regs.RAMCTL.CRMDn == 2 || m_VDP2regs.RAMCTL.CRMDn == 3) {
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
    // 0             320/352        347/375     400/432    427/455 dots
    // +----------------+--------------+-----------+-------------+
    // | Active display | Right border | Horz sync | Left border | (no blanking intervals?)
    // +-+--------------+-+------------+-----------+-------------+
    //   |                |
    //   |                +-- Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    //   |                    The border is optional.
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
    // Rendering
    // TODO: move to a class or namespace

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

    struct BGRenderContext {
        // CRAM base offset for color fetching.
        // Derived from RAMCTL.CRMDn and CRAOFA/CRAOFB.xxCAOSn
        uint32 cramOffset;

        // Bits 3-1 of the color data retrieved from VRAM per pixel.
        // Used by special priority function.
        std::array<uint8, 704> colorData;

        // Colors per pixel
        std::array<vdp::Color888, 704> colors;

        // Priorities per pixel
        std::array<uint8, 704> priorities;
    };

    // Render contexts for NBGs 0-3 then RBGs 0-1
    std::array<BGRenderContext, 4 + 2> m_renderContexts;

    // Framebuffer provided by the frontend to render the current frame into
    FramebufferColor *m_framebuffer;

    // Erases the current VDP1 display framebuffer.
    void VDP1EraseFramebuffer();

    // Switches VDP1 framebuffers.
    void VDP1SwitchFramebuffer();

    // Begins the next VDP1 frame.
    void VDP1BeginFrame();

    // Ends the current VDP1 frame.
    void VDP1EndFrame();

    // Processes the VDP1 command table.
    void VDP1ProcessCommands();

    // Individual VDP1 command processors

    void VDP1SetSystemClipping(uint16 cmdAddress);
    void VDP1SetUserClipping(uint16 cmdAddress);
    void VDP1SetLocalCoordinates(uint16 cmdAddress);

    // Draws the VDP2 scanline at m_VCounter.
    void VDP2DrawLine();

    // Draws a normal scroll BG scanline.
    // bgParams contains the parameters for the BG to draw.
    // rctx contains additional context for the renderer.
    // twoWordChar indicates if character patterns use one word (false) or two words (true).
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <bool twoWordChar, bool fourCellChar, bool wideChar, ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawNormalScrollBG(const NormBGParams &bgParams, BGRenderContext &rctx);

    // Draws a normal bitmap BG scanline.
    // bgParams contains the parameters for the BG to draw.
    // rctx contains additional context for the renderer.
    // colorFormat is the color format for bitmap data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawNormalBitmapBG(const NormBGParams &bgParams, BGRenderContext &rctx);

    // Fetches a two-word character from VRAM.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    Character VDP2FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a one-word character from VRAM.
    // bgParams contains the parameters for the BG to draw.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // largePalette indicates if the color format uses 16 colors (false) or more (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    template <bool fourCellChar, bool largePalette, bool wideChar>
    Character VDP2FetchOneWordCharacter(const NormBGParams &bgParams, uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a color from a pixel in the specified cell in a 2x2 character pattern.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorData is an output variable where bits 3-1 of the palette color data from VRAM is stored.
    // ch contains character parameters.
    // dotX and dotY specify the coordinates of the pixel within the cell, both ranging from 0 to 7.
    // cellIndex is the index of the cell in the character pattern, ranging from 0 to 3.
    // colorFormat is the value of CHCTLA/CHCTLB.xxCHCNn.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 VDP2FetchCharacterColor(uint32 cramOffset, uint8 &colorData, Character ch, uint8 dotX, uint8 dotY,
                                          uint32 cellIndex);

    // Fetches a color from a bitmap pixel.
    // bgParams contains the bitmap parameters.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // dotX and dotY specify the coordinates of the pixel within the bitmap.
    // colorFormat is the color format for pixel data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 VDP2FetchBitmapColor(const NormBGParams &bgParams, uint32 cramOffset, uint8 dotX, uint8 dotY);

    // Fetches a color from CRAM using the current color mode specified by RAMCTL.CRMDn.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorIndex specifies the color index.
    // colorMode is the CRAM color mode.
    template <uint32 colorMode>
    vdp::Color888 VDP2FetchCRAMColor(uint32 cramOffset, uint32 colorIndex);
};

} // namespace satemu::vdp

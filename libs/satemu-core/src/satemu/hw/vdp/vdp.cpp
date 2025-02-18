#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/hw/scu/scu.hpp>
#include <satemu/hw/sh2/sh2_bus.hpp>

#include "slope.hpp"

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/constexpr_for.hpp>
#include <satemu/util/scope_guard.hpp>
#include <satemu/util/unreachable.hpp>

#include <cassert>

namespace satemu::vdp {

VDP::VDP(core::Scheduler &scheduler, scu::SCU &scu, smpc::SMPC &smpc)
    : m_SCU(scu)
    , m_SMPC(smpc)
    , m_scheduler(scheduler) {

    m_phaseUpdateEvent =
        scheduler.RegisterEvent(core::events::VDPPhase, this, OnPhaseUpdateEvent<false>, OnPhaseUpdateEvent<true>);

    m_framebuffer = nullptr;

    // TODO: set PAL flag
    Reset(true);
}

void VDP::Reset(bool hard) {
    if (hard) {
        for (uint32 addr = 0; addr < m_VRAM1.size(); addr++) {
            if ((addr & 0x1F) == 0) {
                m_VRAM1[addr] = 0x80;
            } else if ((addr & 0x1F) == 1) {
                m_VRAM1[addr] = 0x00;
            } else if ((addr & 2) == 2) {
                m_VRAM1[addr] = 0x55;
            } else {
                m_VRAM1[addr] = 0xAA;
            }
        }

        m_VRAM2.fill(0);
        m_CRAM.fill(0);
        for (auto &fb : m_spriteFB) {
            fb.fill(0);
        }
        m_drawFB = 0;
    }

    m_VDP1.Reset();
    m_VDP2.Reset();

    m_HPhase = HorizontalPhase::Active;
    m_VPhase = VerticalPhase::Active;
    m_VCounter = 0;
    m_HRes = 320;
    m_VRes = 224;

    m_VDP1RenderContext.Reset();

    for (auto &state : m_layerStates) {
        state.Reset();
    }
    m_spriteLayerState.Reset();
    for (auto &state : m_normBGLayerStates) {
        state.Reset();
    }
    for (auto &state : m_rotParamStates) {
        state.Reset();
    }
    m_lineBackLayerState.Reset();

    BeginHPhaseActiveDisplay();
    BeginVPhaseActiveDisplay();

    UpdateResolution();

    m_scheduler.ScheduleFromNow(m_phaseUpdateEvent, GetPhaseCycles());
}

template <bool debug>
void VDP::Advance(uint64 cycles) {
    for (uint64 cy = 0; cy < cycles; cy++) {
        VDP1ProcessCommands();
    }
}

template void VDP::Advance<false>(uint64 cycles);
template void VDP::Advance<true>(uint64 cycles);

void VDP::DumpVDP1VRAM(std::ostream &out) const {
    out.write((const char *)m_VRAM1.data(), m_VRAM1.size());
}

void VDP::DumpVDP2VRAM(std::ostream &out) const {
    out.write((const char *)m_VRAM2.data(), m_VRAM2.size());
}

void VDP::DumpVDP2CRAM(std::ostream &out) const {
    out.write((const char *)m_CRAM.data(), m_CRAM.size());
}

void VDP::DumpVDP1Framebuffers(std::ostream &out) const {
    out.write((const char *)m_spriteFB[m_drawFB].data(), m_spriteFB[m_drawFB].size());
    out.write((const char *)m_spriteFB[m_drawFB ^ 1].data(), m_spriteFB[m_drawFB ^ 1].size());
}

template <bool debug>
void VDP::OnPhaseUpdateEvent(core::EventContext &eventContext, void *userContext, uint64 cyclesLate) {
    auto &vdp = *static_cast<VDP *>(userContext);
    vdp.UpdatePhase<debug>();
    const uint64 cycles = vdp.GetPhaseCycles();
    eventContext.RescheduleFromPrevious(cycles);
}

void VDP::SetVideoStandard(sys::VideoStandard videoStandard) {
    const bool pal = videoStandard == sys::VideoStandard::PAL;
    if (m_VDP2.TVSTAT.PAL != pal) {
        m_VDP2.TVSTAT.PAL = pal;
        m_VDP2.TVMDDirty = true;
    }
}

void VDP::MapMemory(sh2::SH2Bus &bus) {
    // VDP1 VRAM
    bus.MapMemory(0x5C0'0000, 0x5C7'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<VDP *>(ctx)->VDP1ReadVRAM<uint8>(address);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<VDP *>(ctx)->VDP1ReadVRAM<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<VDP *>(ctx)->VDP1ReadVRAM<uint16>(address + 0) << 16u;
                          value |= static_cast<VDP *>(ctx)->VDP1ReadVRAM<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<VDP *>(ctx)->VDP1WriteVRAM<uint8>(address, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<VDP *>(ctx)->VDP1WriteVRAM<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<VDP *>(ctx)->VDP1WriteVRAM<uint16>(address + 0, value >> 16u);
                              static_cast<VDP *>(ctx)->VDP1WriteVRAM<uint16>(address + 2, value >> 0u);
                          },
                  });

    // VDP1 framebuffer
    bus.MapMemory(0x5C8'0000, 0x5CF'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<VDP *>(ctx)->VDP1ReadFB<uint8>(address);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<VDP *>(ctx)->VDP1ReadFB<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<VDP *>(ctx)->VDP1ReadFB<uint16>(address + 0) << 16u;
                          value |= static_cast<VDP *>(ctx)->VDP1ReadFB<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<VDP *>(ctx)->VDP1WriteFB<uint8>(address, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<VDP *>(ctx)->VDP1WriteFB<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<VDP *>(ctx)->VDP1WriteFB<uint16>(address + 0, value >> 16u);
                              static_cast<VDP *>(ctx)->VDP1WriteFB<uint16>(address + 2, value >> 0u);
                          },
                  });

    // VDP1 registers
    bus.MapMemory(0x5D0'0000, 0x5D7'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void * /*ctx*/) -> uint8 {
                          address &= 0x7FFFF;
                          regsLog1.debug("Illegal 8-bit VDP1 register read from {:05X}", address);
                          return 0;
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<VDP *>(ctx)->VDP1ReadReg<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<VDP *>(ctx)->VDP1ReadReg<uint16>(address + 0) << 16u;
                          value |= static_cast<VDP *>(ctx)->VDP1ReadReg<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 =
                          [](uint32 address, uint8 value, void * /*ctx*/) {
                              address &= 0x7FFFF;
                              regsLog1.debug("Illegal 8-bit VDP1 register write to {:05X} = {:02X}", address, value);
                          },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<VDP *>(ctx)->VDP1WriteReg<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<VDP *>(ctx)->VDP1WriteReg<uint16>(address + 0, value >> 16u);
                              static_cast<VDP *>(ctx)->VDP1WriteReg<uint16>(address + 2, value >> 0u);
                          },
                  });

    // VDP2 VRAM
    bus.MapMemory(0x5E0'0000, 0x5EF'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<VDP *>(ctx)->VDP2ReadVRAM<uint8>(address);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<VDP *>(ctx)->VDP2ReadVRAM<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<VDP *>(ctx)->VDP2ReadVRAM<uint16>(address + 0) << 16u;
                          value |= static_cast<VDP *>(ctx)->VDP2ReadVRAM<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<VDP *>(ctx)->VDP2WriteVRAM<uint8>(address, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<VDP *>(ctx)->VDP2WriteVRAM<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<VDP *>(ctx)->VDP2WriteVRAM<uint16>(address + 0, value >> 16u);
                              static_cast<VDP *>(ctx)->VDP2WriteVRAM<uint16>(address + 2, value >> 0u);
                          },
                  });

    // VDP2 CRAM
    bus.MapMemory(0x5F0'0000, 0x5F7'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<VDP *>(ctx)->VDP2ReadCRAM<uint8>(address);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<VDP *>(ctx)->VDP2ReadCRAM<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<VDP *>(ctx)->VDP2ReadCRAM<uint16>(address + 0) << 16u;
                          value |= static_cast<VDP *>(ctx)->VDP2ReadCRAM<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<VDP *>(ctx)->VDP2WriteCRAM<uint8>(address, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<VDP *>(ctx)->VDP2WriteCRAM<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<VDP *>(ctx)->VDP2WriteCRAM<uint16>(address + 0, value >> 16u);
                              static_cast<VDP *>(ctx)->VDP2WriteCRAM<uint16>(address + 2, value >> 0u);
                          },
                  });

    // VDP2 registers
    bus.MapMemory(0x5F8'0000, 0x5FB'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void * /*ctx*/) -> uint8 {
                          address &= 0x1FF;
                          regsLog1.debug("Illegal 8-bit VDP2 register read from {:05X}", address);
                          return 0;
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<VDP *>(ctx)->VDP2ReadReg<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          uint32 value = static_cast<VDP *>(ctx)->VDP2ReadReg<uint16>(address + 0) << 16u;
                          value |= static_cast<VDP *>(ctx)->VDP2ReadReg<uint16>(address + 2) << 0u;
                          return value;
                      },
                      .write8 =
                          [](uint32 address, uint8 value, void * /*ctx*/) {
                              address &= 0x1FF;
                              regsLog1.debug("Illegal 8-bit VDP2 register write to {:05X} = {:02X}", address, value);
                          },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<VDP *>(ctx)->VDP2WriteReg<uint16>(address, value); },
                      .write32 =
                          [](uint32 address, uint32 value, void *ctx) {
                              static_cast<VDP *>(ctx)->VDP2WriteReg<uint16>(address + 0, value >> 16u);
                              static_cast<VDP *>(ctx)->VDP2WriteReg<uint16>(address + 2, value >> 0u);
                          },
                  });
}

template <mem_primitive T>
FORCE_INLINE T VDP::VDP1ReadVRAM(uint32 address) {
    return util::ReadBE<T>(&m_VRAM1[address & 0x7FFFF]);
}

template <mem_primitive T>
FORCE_INLINE void VDP::VDP1WriteVRAM(uint32 address, T value) {
    util::WriteBE<T>(&m_VRAM1[address & 0x7FFFF], value);
}

template <mem_primitive T>
FORCE_INLINE T VDP::VDP1ReadFB(uint32 address) {
    return util::ReadBE<T>(&m_spriteFB[m_drawFB][address & 0x3FFFF]);
}

template <mem_primitive T>
FORCE_INLINE void VDP::VDP1WriteFB(uint32 address, T value) {
    util::WriteBE<T>(&m_spriteFB[m_drawFB][address & 0x3FFFF], value);
}

template <mem_primitive T>
FORCE_INLINE T VDP::VDP1ReadReg(uint32 address) {
    address &= 0x7FFFF;

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

    default: regsLog1.debug("unhandled {}-bit VDP1 register read from {:02X}", sizeof(T) * 8, address); return 0;
    }
}

template <mem_primitive T>
FORCE_INLINE void VDP::VDP1WriteReg(uint32 address, T value) {
    address &= 0x7FFFF;

    switch (address) {
    case 0x00:
        m_VDP1.WriteTVMR(value);
        regsLog1.trace("Write to TVM={:d}{:d}{:d}", m_VDP1.hdtvEnable, m_VDP1.fbRotEnable, m_VDP1.pixel8Bits);
        regsLog1.trace("Write to VBE={:d}", m_VDP1.vblankErase);
        break;
    case 0x02: {
        m_VDP1.WriteFBCR(value);
        regsLog1.trace("Write to DIE={:d} DIL={:d}", m_VDP1.dblInterlaceEnable, m_VDP1.dblInterlaceDrawLine);
        regsLog1.trace("Write to FCM={:d} FCT={:d} manualswap={:d} manualerase={:d}", m_VDP1.fbSwapMode,
                       m_VDP1.fbSwapTrigger, m_VDP1.fbManualSwap, m_VDP1.fbManualErase);
        break;
    }
    case 0x04:
        m_VDP1.WritePTMR(value);
        regsLog1.trace("Write to PTM={:d}", m_VDP1.plotTrigger);
        if (m_VDP1.plotTrigger == 0b01) {
            VDP1BeginFrame();
        }
        break;
    case 0x06: m_VDP1.WriteEWDR(value); break;
    case 0x08: m_VDP1.WriteEWLR(value); break;
    case 0x0A: m_VDP1.WriteEWRR(value); break;
    case 0x0C: // ENDR
        // TODO: schedule drawing termination after 30 cycles
        m_VDP1RenderContext.rendering = false;
        break;

    case 0x10: break; // EDSR is read-only
    case 0x12: break; // LOPR is read-only
    case 0x14: break; // COPR is read-only
    case 0x16: break; // MODR is read-only

    default:
        regsLog1.debug("unhandled {}-bit VDP1 register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
        break;
    }
}

template <mem_primitive T>
FORCE_INLINE T VDP::VDP2ReadVRAM(uint32 address) {
    // TODO: handle VRSIZE.VRAMSZ
    address &= 0x7FFFF;
    return util::ReadBE<T>(&m_VRAM2[address]);
}

template <mem_primitive T>
FORCE_INLINE void VDP::VDP2WriteVRAM(uint32 address, T value) {
    // TODO: handle VRSIZE.VRAMSZ
    address &= 0x7FFFF;
    util::WriteBE<T>(&m_VRAM2[address], value);
}

template <mem_primitive T>
FORCE_INLINE T VDP::VDP2ReadCRAM(uint32 address) {
    if constexpr (std::is_same_v<T, uint32>) {
        uint32 value = VDP2ReadCRAM<uint16>(address + 0) << 16u;
        value |= VDP2ReadCRAM<uint16>(address + 2) << 0u;
        return value;
    }

    address = MapCRAMAddress(address);
    T value = util::ReadBE<T>(&m_CRAM[address]);
    regsLog2.trace("{}-bit VDP2 CRAM read from {:03X} = {:X}", sizeof(T) * 8, address, value);
    return value;
}

template <mem_primitive T>
FORCE_INLINE void VDP::VDP2WriteCRAM(uint32 address, T value) {
    if constexpr (std::is_same_v<T, uint32>) {
        VDP2WriteCRAM<uint16>(address + 0, value >> 16u);
        VDP2WriteCRAM<uint16>(address + 2, value >> 0u);
        return;
    }

    address = MapCRAMAddress(address);
    regsLog2.trace("{}-bit VDP2 CRAM write to {:05X} = {:X}", sizeof(T) * 8, address, value);
    util::WriteBE<T>(&m_CRAM[address], value);
    if (m_VDP2.RAMCTL.CRMDn == 0) {
        regsLog2.trace("   replicated to {:05X}", address ^ 0x800);
        util::WriteBE<T>(&m_CRAM[address ^ 0x800], value);
    }
}

template <mem_primitive T>
FORCE_INLINE T VDP::VDP2ReadReg(uint32 address) {
    address &= 0x1FF;

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
    case 0x0C0: return m_VDP2.ReadWPSXn(0);  // write-only?
    case 0x0C2: return m_VDP2.ReadWPSYn(0);  // write-only?
    case 0x0C4: return m_VDP2.ReadWPEXn(0);  // write-only?
    case 0x0C6: return m_VDP2.ReadWPEYn(0);  // write-only?
    case 0x0C8: return m_VDP2.ReadWPSXn(1);  // write-only?
    case 0x0CA: return m_VDP2.ReadWPSYn(1);  // write-only?
    case 0x0CC: return m_VDP2.ReadWPEXn(1);  // write-only?
    case 0x0CE: return m_VDP2.ReadWPEYn(1);  // write-only?
    case 0x0D0: return m_VDP2.ReadWCTLA();   // write-only?
    case 0x0D2: return m_VDP2.ReadWCTLB();   // write-only?
    case 0x0D4: return m_VDP2.ReadWCTLC();   // write-only?
    case 0x0D6: return m_VDP2.ReadWCTLD();   // write-only?
    case 0x0D8: return m_VDP2.ReadLWTAnU(0); // write-only?
    case 0x0DA: return m_VDP2.ReadLWTAnL(0); // write-only?
    case 0x0DC: return m_VDP2.ReadLWTAnU(1); // write-only?
    case 0x0DE: return m_VDP2.ReadLWTAnL(1); // write-only?
    case 0x0E0: return m_VDP2.ReadSPCTL();   // write-only?
    case 0x0E2: return m_VDP2.ReadSDCTL();   // write-only?
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
    default: regsLog2.debug("unhandled {}-bit VDP2 register read from {:03X}", sizeof(T) * 8, address); return 0;
    }
}

template <mem_primitive T>
FORCE_INLINE void VDP::VDP2WriteReg(uint32 address, T value) {
    address &= 0x1FF;

    switch (address) {
    case 0x000: {
        const uint16 oldTVMD = m_VDP2.TVMD.u16;
        m_VDP2.TVMD.u16 = value & 0x81F7;
        m_VDP2.TVMDDirty |= m_VDP2.TVMD.u16 != oldTVMD;
        regsLog2.trace("TVMD write: {:04X} - HRESO={:d} VRESO={:d} LSMD={:d} BDCLMD={:d} DISP={:d}{}", m_VDP2.TVMD.u16,
                       (uint16)m_VDP2.TVMD.HRESOn, (uint16)m_VDP2.TVMD.VRESOn, (uint16)m_VDP2.TVMD.LSMDn,
                       (uint16)m_VDP2.TVMD.BDCLMD, (uint16)m_VDP2.TVMD.DISP, (m_VDP2.TVMDDirty ? " (dirty)" : ""));
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
    case 0x0C0: m_VDP2.WriteWPSXn(0, value); break;
    case 0x0C2: m_VDP2.WriteWPSYn(0, value); break;
    case 0x0C4: m_VDP2.WriteWPEXn(0, value); break;
    case 0x0C6: m_VDP2.WriteWPEYn(0, value); break;
    case 0x0C8: m_VDP2.WriteWPSXn(1, value); break;
    case 0x0CA: m_VDP2.WriteWPSYn(1, value); break;
    case 0x0CC: m_VDP2.WriteWPEXn(1, value); break;
    case 0x0CE: m_VDP2.WriteWPEYn(1, value); break;
    case 0x0D0: m_VDP2.WriteWCTLA(value); break;
    case 0x0D2: m_VDP2.WriteWCTLB(value); break;
    case 0x0D4: m_VDP2.WriteWCTLC(value); break;
    case 0x0D6: m_VDP2.WriteWCTLD(value); break;
    case 0x0D8: m_VDP2.WriteLWTAnU(0, value); break;
    case 0x0DA: m_VDP2.WriteLWTAnL(0, value); break;
    case 0x0DC: m_VDP2.WriteLWTAnU(1, value); break;
    case 0x0DE: m_VDP2.WriteLWTAnL(1, value); break;
    case 0x0E0: m_VDP2.WriteSPCTL(value); break;
    case 0x0E2: m_VDP2.WriteSDCTL(value); break;
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
        regsLog2.debug("unhandled {}-bit VDP2 register write to {:03X} = {:X}", sizeof(T) * 8, address, value);
        break;
    }
}

template <bool debug>
FORCE_INLINE void VDP::UpdatePhase() {
    auto nextPhase = static_cast<uint32>(m_HPhase) + 1;
    if (nextPhase == m_HTimings.size()) {
        nextPhase = 0;
    }

    m_HPhase = static_cast<HorizontalPhase>(nextPhase);
    switch (m_HPhase) {
    case HorizontalPhase::Active: BeginHPhaseActiveDisplay(); break;
    case HorizontalPhase::RightBorder: BeginHPhaseRightBorder(); break;
    case HorizontalPhase::Sync: BeginHPhaseSync(); break;
    case HorizontalPhase::VBlankOut: BeginHPhaseVBlankOut(); break;
    case HorizontalPhase::LeftBorder: BeginHPhaseLeftBorder(); break;
    case HorizontalPhase::LastDot: BeginHPhaseLastDot(); break;
    }
}

FORCE_INLINE uint64 VDP::GetPhaseCycles() const {
    return m_HTimings[static_cast<uint32>(m_HPhase)];
}

void VDP::UpdateResolution() {
    if (!m_VDP2.TVMDDirty) {
        return;
    }

    m_VDP2.TVMDDirty = false;

    // Horizontal/vertical resolution tables
    // NTSC uses the first two vRes entries, PAL uses the full table, and exclusive monitors use 480 lines
    static constexpr uint32 hRes[] = {320, 352, 640, 704};
    static constexpr uint32 vRes[] = {224, 240, 256, 256};

    // TODO: check for NTSC, PAL or exclusive monitor; assuming NTSC for now
    // TODO: exclusive monitor: even hRes entries are valid for 31 KHz monitors, odd are for Hi-Vision
    m_HRes = hRes[m_VDP2.TVMD.HRESOn];
    m_VRes = vRes[m_VDP2.TVMD.VRESOn & (m_VDP2.TVSTAT.PAL ? 3 : 1)];
    if (m_VDP2.TVMD.LSMDn == 3) {
        // Double-density interlace doubles the vertical resolution
        m_VRes *= 2;
    }

    rootLog2.info("Screen resolution set to {}x{}", m_HRes, m_VRes);
    switch (m_VDP2.TVMD.LSMDn) {
    case 0: rootLog2.info("Non-interlace mode"); break;
    case 1: rootLog2.info("Invalid interlace mode"); break;
    case 2: rootLog2.info("Single-density interlace mode"); break;
    case 3: rootLog2.info("Double-density interlace mode"); break;
    }

    m_framebuffer = m_cbRequestFramebuffer(m_HRes, m_VRes);

    // Timing tables

    // Horizontal phase timings (cycles until):
    //   RBd = Right Border
    //   HSy = Horizontal Sync
    //   VBC = VBlank Clear
    //   LBd = Left Border
    //   LDt = Last Dot
    //   ADp = Active Display
    // NOTE: these timings specify the HCNT to advance to the specified phase
    static constexpr std::array<std::array<uint32, 6>, 8> hTimings{{
        // RBd, HSy, VBC, LBd, LDt, ADp
        {320, 27, 27, 26, 26, 1}, // {320, 347, 374, 400, 426, 427},
        {352, 23, 28, 29, 22, 1}, // {352, 375, 403, 432, 454, 455},
        {640, 54, 54, 52, 52, 2}, // {640, 694, 748, 800, 852, 854},
        {704, 46, 56, 58, 44, 2}, // {704, 750, 806, 864, 908, 910},
    }};
    m_HTimings = hTimings[m_VDP2.TVMD.HRESOn & 3]; // TODO: check exclusive monitor timings

    // Vertical phase timings (to reach):
    //   BBd = Bottom Border
    //   BSy = Blanking and Vertical Sync
    //   TBd = Top Border
    //   LLn = Last Line
    //   ADp = Active Display
    // NOTE: these timings indicate the VCNT at which the specified phase begins
    static constexpr std::array<std::array<std::array<uint32, 5>, 4>, 2> vTimings{{
        // NTSC
        {{
            // BBd, BSy, TBd, LLn, ADp
            {224, 232, 255, 262, 263},
            {240, 240, 255, 262, 263},
            {224, 232, 255, 262, 263},
            {240, 240, 255, 262, 263},
        }},
        // PAL
        {{
            // BBd, BSy, TBd, LLn, ADp
            {224, 256, 281, 312, 313},
            {240, 264, 289, 312, 313},
            {256, 272, 297, 312, 313},
            {256, 272, 297, 312, 313},
        }},
    }};
    m_VTimings = vTimings[m_VDP2.TVSTAT.PAL][m_VDP2.TVMD.VRESOn];

    // Adjust for dot clock
    const uint32 dotClockMult = (m_VDP2.TVMD.HRESOn & 2) ? 2 : 4;
    for (auto &timing : m_HTimings) {
        timing *= dotClockMult;
    }

    rootLog2.info("Dot clock mult = {}, display {}", dotClockMult, (m_VDP2.TVMD.DISP ? "ON" : "OFF"));
}

FORCE_INLINE void VDP::IncrementVCounter() {
    ++m_VCounter;
    while (m_VCounter >= m_VTimings[static_cast<uint32>(m_VPhase)]) {
        auto nextPhase = static_cast<uint32>(m_VPhase) + 1;
        if (nextPhase == m_VTimings.size()) {
            m_VCounter = 0;
            nextPhase = 0;
        }

        m_VPhase = static_cast<VerticalPhase>(nextPhase);
        switch (m_VPhase) {
        case VerticalPhase::Active: BeginVPhaseActiveDisplay(); break;
        case VerticalPhase::BottomBorder: BeginVPhaseBottomBorder(); break;
        case VerticalPhase::BlankingAndSync: BeginVPhaseBlankingAndSync(); break;
        case VerticalPhase::TopBorder: BeginVPhaseTopBorder(); break;
        case VerticalPhase::LastLine: BeginVPhaseLastLine(); break;
        }
    }
}

// ----

void VDP::BeginHPhaseActiveDisplay() {
    rootLog2.trace("(VCNT = {:3d})  Entering horizontal active display phase", m_VCounter);
    if (m_VPhase == VerticalPhase::Active) {
        if (m_VCounter == 0) {
            rootLog2.trace("Begin VDP2 frame, VDP1 framebuffer {}", m_drawFB ^ 1);

            VDP2InitFrame();
        } else if (m_VCounter == m_VRes - 14) { // ~1ms before VBlank IN
            m_SMPC.TriggerOptimizedINTBACKRead();
        }
        VDP2DrawLine();
    }
}

void VDP::BeginHPhaseRightBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering right border phase", m_VCounter);

    rootLog2.trace("## HBlank IN {:3d}", m_VCounter);

    m_VDP2.TVSTAT.HBLANK = 1;
    m_SCU.TriggerHBlankIN();

    // Start erasing if we just entered VBlank IN
    if (m_VCounter == m_VTimings[static_cast<uint32>(VerticalPhase::Active)]) {
        rootLog2.trace("## HBlank IN + VBlank IN  VBE={:d} manualerase={:d}", m_VDP1.vblankErase, m_VDP1.fbManualErase);

        if (m_VDP1.vblankErase || !m_VDP1.fbSwapMode) {
            // TODO: cycle-count the erase process, starting here
            VDP1EraseFramebuffer();
        }
    }

    // TODO: draw border
}

void VDP::BeginHPhaseSync() {
    IncrementVCounter();
    rootLog2.trace("(VCNT = {:3d})  Entering horizontal sync phase", m_VCounter);
}

void VDP::BeginHPhaseVBlankOut() {
    rootLog2.trace("(VCNT = {:3d})  Entering VBlank OUT horizontal phase", m_VCounter);

    if (m_VPhase == VerticalPhase::LastLine) {
        rootLog2.trace("## HBlank half + VBlank OUT  FCM={:d} FCT={:d} manualswap={:d} PTM={:d}", m_VDP1.fbSwapMode,
                       m_VDP1.fbSwapTrigger, m_VDP1.fbManualSwap, m_VDP1.plotTrigger);

        // Swap framebuffer in manual swap requested or in 1-cycle mode
        if (!m_VDP1.fbSwapMode || m_VDP1.fbManualSwap) {
            m_VDP1.fbManualSwap = false;
            VDP1SwapFramebuffer();
        }
    }
}

void VDP::BeginHPhaseLeftBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering left border phase", m_VCounter);

    m_VDP2.TVSTAT.HBLANK = 0;

    // TODO: draw border
}

void VDP::BeginHPhaseLastDot() {
    rootLog2.trace("(VCNT = {:3d})  Entering last dot phase", m_VCounter);

    // If we just entered the bottom blanking vertical phase, switch fields
    if (m_VCounter == m_VTimings[static_cast<uint32>(VerticalPhase::Active)]) {
        if (m_VDP2.TVMD.LSMDn != 0) {
            m_VDP2.TVSTAT.ODD ^= 1;
            rootLog2.trace("Switched to {} field", (m_VDP2.TVSTAT.ODD ? "odd" : "even"));
        } else {
            m_VDP2.TVSTAT.ODD = 1;
        }
    }
}

// ----

void VDP::BeginVPhaseActiveDisplay() {
    rootLog2.trace("(VCNT = {:3d})  Entering vertical active display phase", m_VCounter);
}

void VDP::BeginVPhaseBottomBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering bottom border phase", m_VCounter);

    rootLog2.trace("## VBlank IN");

    m_VDP2.TVSTAT.VBLANK = 1;
    m_SCU.TriggerVBlankIN();

    // TODO: draw border
}

void VDP::BeginVPhaseBlankingAndSync() {
    rootLog2.trace("(VCNT = {:3d})  Entering blanking/vertical sync phase", m_VCounter);

    // End frame
    rootLog2.trace("End VDP2 frame");
    m_cbFrameComplete(m_framebuffer, m_HRes, m_VRes);
}

void VDP::BeginVPhaseTopBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering top border phase", m_VCounter);

    UpdateResolution();

    // TODO: draw border
}

void VDP::BeginVPhaseLastLine() {
    rootLog2.trace("(VCNT = {:3d})  Entering last line phase", m_VCounter);

    rootLog2.trace("## VBlank OUT");

    m_VDP2.TVSTAT.VBLANK = 0;
    m_SCU.TriggerVBlankOUT();
}

// -----------------------------------------------------------------------------
// VDP1

FORCE_INLINE std::array<uint8, kVDP1FramebufferRAMSize> &VDP::VDP1GetDrawFB() {
    return m_spriteFB[m_drawFB];
}

FORCE_INLINE std::array<uint8, kVDP1FramebufferRAMSize> &VDP::VDP1GetDisplayFB() {
    return m_spriteFB[m_drawFB ^ 1];
}

FORCE_INLINE void VDP::VDP1EraseFramebuffer() {
    renderLog1.trace("Erasing framebuffer {} - {}x{} to {}x{} -> {:04X}  {}x{}  {}-bit", m_drawFB ^ 1, m_VDP1.eraseX1,
                     m_VDP1.eraseY1, m_VDP1.eraseX3, m_VDP1.eraseY3, m_VDP1.eraseWriteValue, m_VDP1.fbSizeH,
                     m_VDP1.fbSizeV, (m_VDP1.pixel8Bits ? 8 : 16));

    auto &fb = VDP1GetDisplayFB();

    // Horizontal scale is doubled in hi-res modes or when targeting rotation background
    const uint32 scaleH = (m_VDP2.TVMD.HRESOn & 0b010) || m_VDP1.fbRotEnable ? 1 : 0;
    // Vertical scale is doubled in double-interlace mode
    const uint32 scaleV = m_VDP2.TVMD.LSMDn == 3 ? 1 : 0;

    const uint32 offsetShift = m_VDP1.pixel8Bits ? 0 : 1;

    const uint32 x1 = m_VDP1.eraseX1 << scaleH;
    const uint32 x3 = m_VDP1.eraseX3 << scaleH;
    const uint32 y1 = m_VDP1.eraseY1 << scaleV;
    const uint32 y3 = m_VDP1.eraseY3 << scaleV;

    for (uint32 y = y1; y <= y3; y++) {
        const uint32 fbOffset = y * m_VDP1.fbSizeH;
        for (uint32 x = x1; x <= x3; x++) {
            const uint32 address = (fbOffset + x) << offsetShift;
            util::WriteBE<uint16>(&fb[address & 0x3FFFE], m_VDP1.eraseWriteValue);
        }
    }
}

FORCE_INLINE void VDP::VDP1SwapFramebuffer() {
    renderLog1.trace("Swapping framebuffers - draw {}, display {}", m_drawFB ^ 1, m_drawFB);

    if (m_VDP1.fbManualErase) {
        m_VDP1.fbManualErase = false;
        VDP1EraseFramebuffer();
    }

    m_drawFB ^= 1;

    if (bit::extract<1>(m_VDP1.plotTrigger)) {
        VDP1BeginFrame();
    }
}

void VDP::VDP1BeginFrame() {
    renderLog1.trace("Begin VDP1 frame on framebuffer {}", m_drawFB);

    // TODO: setup rendering
    // TODO: figure out VDP1 timings

    m_VDP1.prevCommandAddress = m_VDP1.currCommandAddress;
    m_VDP1.currCommandAddress = 0;
    m_VDP1.returnAddress = ~0;
    m_VDP1.prevFrameEnded = m_VDP1.currFrameEnded;
    m_VDP1.currFrameEnded = false;

    m_VDP1RenderContext.rendering = true;

    VDP1ProcessCommands();
}

void VDP::VDP1EndFrame() {
    renderLog1.trace("End VDP1 frame on framebuffer {}", m_drawFB);
    m_VDP1RenderContext.rendering = false;
    m_VDP1.currFrameEnded = true;
    m_SCU.TriggerSpriteDrawEnd();
}

void VDP::VDP1ProcessCommands() {
    static constexpr uint32 kNoReturn = ~0;

    if (!m_VDP1RenderContext.rendering) {
        return;
    }

    auto &cmdAddress = m_VDP1.currCommandAddress;

    const VDP1Command::Control control{.u16 = VDP1ReadVRAM<uint16>(cmdAddress)};
    renderLog1.trace("Processing command {:04X} @ {:05X}", control.u16, cmdAddress);
    if (control.end) [[unlikely]] {
        renderLog1.trace("End of command list");
        VDP1EndFrame();
    } else if (!control.skip) {
        // Process command
        using enum VDP1Command::CommandType;

        switch (control.command) {
        case DrawNormalSprite: VDP1Cmd_DrawNormalSprite(cmdAddress, control); break;
        case DrawScaledSprite: VDP1Cmd_DrawScaledSprite(cmdAddress, control); break;
        case DrawDistortedSprite: // fallthrough
        case DrawDistortedSpriteAlt: VDP1Cmd_DrawDistortedSprite(cmdAddress, control); break;

        case DrawPolygon: VDP1Cmd_DrawPolygon(cmdAddress); break;
        case DrawPolylines: // fallthrough
        case DrawPolylinesAlt: VDP1Cmd_DrawPolylines(cmdAddress); break;
        case DrawLine: VDP1Cmd_DrawLine(cmdAddress); break;

        case UserClipping: // fallthrough
        case UserClippingAlt: VDP1Cmd_SetUserClipping(cmdAddress); break;
        case SystemClipping: VDP1Cmd_SetSystemClipping(cmdAddress); break;
        case SetLocalCoordinates: VDP1Cmd_SetLocalCoordinates(cmdAddress); break;

        default:
            renderLog1.debug("Unexpected command type {:X}", static_cast<uint16>(control.command));
            VDP1EndFrame();
            return;
        }
    }

    // Go to the next command
    {
        using enum VDP1Command::JumpType;

        switch (control.jumpMode) {
        case Next: cmdAddress += 0x20; break;
        case Assign: {
            cmdAddress = (VDP1ReadVRAM<uint16>(cmdAddress + 0x02) << 3u) & ~0x1F;
            renderLog1.trace("Jump to {:05X}", cmdAddress);

            // HACK: Sonic R attempts to jump back to 0 in some cases
            if (cmdAddress == 0) {
                renderLog1.warn("Possible infinite loop detected; stopping");
                VDP1EndFrame();
                return;
            }
            break;
        }
        case Call: {
            // Nested calls seem to not update the return address
            if (m_VDP1.returnAddress == kNoReturn) {
                m_VDP1.returnAddress = cmdAddress + 0x20;
            }
            cmdAddress = (VDP1ReadVRAM<uint16>(cmdAddress + 0x02) << 3u) & ~0x1F;
            renderLog1.trace("Call {:05X}", cmdAddress);
            break;
        }
        case Return: {
            // Return seems to only return if there was a previous Call
            if (m_VDP1.returnAddress != kNoReturn) {
                cmdAddress = m_VDP1.returnAddress;
                m_VDP1.returnAddress = kNoReturn;
            } else {
                cmdAddress += 0x20;
            }
            renderLog1.trace("Return to {:05X}", cmdAddress);
            break;
        }
        }
        cmdAddress &= 0x7FFFF;
    }
}

FORCE_INLINE bool VDP::VDP1IsPixelUserClipped(CoordS32 coord) const {
    auto [x, y] = coord;
    const auto &ctx = m_VDP1RenderContext;
    if (x < ctx.userClipX0 || x > ctx.userClipX1) {
        return true;
    }
    if (y < ctx.userClipY0 || y > ctx.userClipY1) {
        return true;
    }
    return false;
}

FORCE_INLINE bool VDP::VDP1IsPixelSystemClipped(CoordS32 coord) const {
    auto [x, y] = coord;
    const auto &ctx = m_VDP1RenderContext;
    if (x < 0 || x > ctx.sysClipH) {
        return true;
    }
    if (y < 0 || y > ctx.sysClipV) {
        return true;
    }
    return false;
}

FORCE_INLINE bool VDP::VDP1IsLineSystemClipped(CoordS32 coord1, CoordS32 coord2) const {
    auto [x1, y1] = coord1;
    auto [x2, y2] = coord2;
    const auto &ctx = m_VDP1RenderContext;
    if (x1 < 0 && x2 < 0) {
        return true;
    }
    if (y1 < 0 && y2 < 0) {
        return true;
    }
    if (x1 > ctx.sysClipH && x2 > ctx.sysClipH) {
        return true;
    }
    if (y1 > ctx.sysClipV && y2 > ctx.sysClipV) {
        return true;
    }
    return false;
}

bool VDP::VDP1IsQuadSystemClipped(CoordS32 coord1, CoordS32 coord2, CoordS32 coord3, CoordS32 coord4) const {
    auto [x1, y1] = coord1;
    auto [x2, y2] = coord2;
    auto [x3, y3] = coord3;
    auto [x4, y4] = coord4;
    const auto &ctx = m_VDP1RenderContext;
    if (x1 < 0 && x2 < 0 && x3 < 0 && x4 < 0) {
        return true;
    }
    if (y1 < 0 && y2 < 0 && y3 < 0 && y4 < 0) {
        return true;
    }
    if (x1 > ctx.sysClipH && x2 > ctx.sysClipH && x3 > ctx.sysClipH && x4 > ctx.sysClipH) {
        return true;
    }
    if (y1 > ctx.sysClipV && y2 > ctx.sysClipV && y3 > ctx.sysClipV && y4 > ctx.sysClipV) {
        return true;
    }
    return false;
}

FORCE_INLINE void VDP::VDP1PlotPixel(CoordS32 coord, const VDP1PixelParams &pixelParams,
                                     const VDP1GouraudParams &gouraudParams) {
    auto [x, y] = coord;

    if (pixelParams.mode.meshEnable && ((x ^ y) & 1)) {
        return;
    }

    if (m_VDP1.dblInterlaceEnable && m_VDP2.TVMD.LSMDn == 3) {
        if ((y & 1) == m_VDP1.dblInterlaceDrawLine) {
            return;
        }
        y >>= 1;
    }

    // Reject pixels outside of clipping area
    if (VDP1IsPixelSystemClipped(coord)) {
        return;
    }
    if (pixelParams.mode.userClippingEnable) {
        // clippingMode = false -> draw inside, reject outside
        // clippingMode = true -> draw outside, reject inside
        // The function returns true if the pixel is clipped, therefore we want to reject pixels that return the
        // opposite of clippingMode on that function.
        if (VDP1IsPixelUserClipped(coord) != pixelParams.mode.clippingMode) {
            return;
        }
    }

    // TODO: pixelParams.mode.preClippingDisable

    const uint32 fbOffset = y * m_VDP1.fbSizeH + x;
    auto &drawFB = VDP1GetDrawFB();
    if (m_VDP1.pixel8Bits) {
        // TODO: what happens if pixelParams.mode.colorCalcBits/gouraudEnable != 0?
        if (pixelParams.mode.msbOn) {
            drawFB[fbOffset & 0x3FFFF] |= 0x80;
        } else {
            drawFB[fbOffset & 0x3FFFF] = pixelParams.color;
        }
    } else {
        uint8 *pixel = &drawFB[(fbOffset * sizeof(uint16)) & 0x3FFFE];

        if (pixelParams.mode.msbOn) {
            drawFB[(fbOffset * sizeof(uint16)) & 0x3FFFE] |= 0x80;
        } else {
            Color555 srcColor{.u16 = pixelParams.color};
            Color555 dstColor{.u16 = util::ReadBE<uint16>(pixel)};

            // Apply color calculations
            //
            // In all cases where calculation is done, the raw color data to be drawn ("original graphic") or from
            // the background are interpreted as 5:5:5 RGB.

            if (pixelParams.mode.gouraudEnable) {
                // Calculate gouraud shading on source color
                // Interpolate between A, B, C and D (ordered in the standard Saturn quad orientation) using U and V
                // Gouraud channel values are offset by -16

                auto lerp = [](sint64 x, sint64 y, uint64 t) -> sint16 {
                    static constexpr uint64 shift = Slope::kFracBits;
                    return ((x << shift) + (y - x) * t) >> shift;
                };

                const Color555 A = gouraudParams.colorA;
                const Color555 B = gouraudParams.colorB;
                const Color555 C = gouraudParams.colorC;
                const Color555 D = gouraudParams.colorD;
                const uint64 U = gouraudParams.U;
                const uint64 V = gouraudParams.V;

                const sint16 ABr = lerp(static_cast<sint16>(A.r), static_cast<sint16>(B.r), U);
                const sint16 ABg = lerp(static_cast<sint16>(A.g), static_cast<sint16>(B.g), U);
                const sint16 ABb = lerp(static_cast<sint16>(A.b), static_cast<sint16>(B.b), U);

                const sint16 DCr = lerp(static_cast<sint16>(D.r), static_cast<sint16>(C.r), U);
                const sint16 DCg = lerp(static_cast<sint16>(D.g), static_cast<sint16>(C.g), U);
                const sint16 DCb = lerp(static_cast<sint16>(D.b), static_cast<sint16>(C.b), U);

                srcColor.r = std::clamp(srcColor.r + lerp(ABr, DCr, V) - 0x10, 0, 31);
                srcColor.g = std::clamp(srcColor.g + lerp(ABg, DCg, V) - 0x10, 0, 31);
                srcColor.b = std::clamp(srcColor.b + lerp(ABb, DCb, V) - 0x10, 0, 31);

                // HACK: replace with U/V coordinates
                // srcColor.r = U >> (Slope::kFracBits - 5);
                // srcColor.g = V >> (Slope::kFracBits - 5);

                // HACK: replace with computed gouraud gradient
                // srcColor.r = lerp(ABr, DCr, V);
                // srcColor.g = lerp(ABg, DCg, V);
                // srcColor.b = lerp(ABb, DCb, V);
            }

            switch (pixelParams.mode.colorCalcBits) {
            case 0: // Replace
                util::WriteBE<uint16>(pixel, srcColor.u16);
                break;
            case 1: // Shadow
                // Halve destination luminosity if it's not transparent
                if (dstColor.msb) {
                    dstColor.r >>= 1u;
                    dstColor.g >>= 1u;
                    dstColor.b >>= 1u;
                    util::WriteBE<uint16>(pixel, dstColor.u16);
                }
                break;
            case 2: // Half-luminance
                // Draw original graphic with halved luminance
                srcColor.r >>= 1u;
                srcColor.g >>= 1u;
                srcColor.b >>= 1u;
                util::WriteBE<uint16>(pixel, srcColor.u16);
                break;
            case 3: // Half-transparency
                // If background is not transparent, blend half of original graphic and half of background
                // Otherwise, draw original graphic as is
                if (dstColor.msb) {
                    srcColor.r = (srcColor.r + dstColor.r) >> 1u;
                    srcColor.g = (srcColor.g + dstColor.g) >> 1u;
                    srcColor.b = (srcColor.b + dstColor.b) >> 1u;
                }
                util::WriteBE<uint16>(pixel, srcColor.u16);
                break;
            }
        }
    }
}

FORCE_INLINE void VDP::VDP1PlotLine(CoordS32 coord1, CoordS32 coord2, const VDP1PixelParams &pixelParams,
                                    VDP1GouraudParams &gouraudParams) {
    for (LineStepper line{coord1, coord2}; line.CanStep(); line.Step()) {
        gouraudParams.U = line.FracPos();
        VDP1PlotPixel(line.Coord(), pixelParams, gouraudParams);
        if (line.NeedsAntiAliasing()) {
            VDP1PlotPixel(line.AACoord(), pixelParams, gouraudParams);
        }
    }
}

void VDP::VDP1PlotTexturedLine(CoordS32 coord1, CoordS32 coord2, const VDP1TexturedLineParams &lineParams,
                               VDP1GouraudParams &gouraudParams) {
    const uint32 charSizeH = lineParams.charSizeH;
    const uint32 charSizeV = lineParams.charSizeV;
    const auto mode = lineParams.mode;
    const auto control = lineParams.control;

    const uint32 v = lineParams.texFracV >> Slope::kFracBits;
    // Bail out if V coordinate is out of range
    if (v >= charSizeV) {
        return;
    }
    gouraudParams.V = lineParams.texFracV / charSizeV;

    uint16 color = 0;
    bool transparent = true;
    const bool flipU = control.flipH;
    bool hasEndCode = false;
    int endCodeCount = 0;
    for (TexturedLineStepper line{coord1, coord2, charSizeH, flipU}; line.CanStep(); line.Step()) {
        // Load new texel if U coordinate changed.
        // Note that the very first pixel in the line always passes the check.
        if (line.UChanged()) {
            const uint32 u = line.U();
            // Bail out if U coordinate is out of range
            if (u >= charSizeH) {
                break;
            }

            const bool useHighSpeedShrink = mode.highSpeedShrink && line.uinc > Slope::kFracOne;
            const uint32 adjustedU = useHighSpeedShrink ? ((u & ~1) | m_VDP1.evenOddCoordSelect) : u;

            const uint32 charIndex = adjustedU + v * charSizeH;

            auto processEndCode = [&](bool endCode) {
                if (endCode && !mode.endCodeDisable && !useHighSpeedShrink) {
                    hasEndCode = true;
                    endCodeCount++;
                } else {
                    hasEndCode = false;
                }
            };

            // Read next texel
            switch (mode.colorMode) {
            case 0: // 4 bpp, 16 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + (charIndex >> 1));
                color = (color >> (((u ^ 1) & 1) * 4)) & 0xF;
                processEndCode(color == 0xF);
                transparent = color == 0x0;
                color |= lineParams.colorBank;
                break;
            case 1: // 4 bpp, 16 colors, lookup table mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + (charIndex >> 1));
                color = (color >> (((u ^ 1) & 1) * 4)) & 0xF;
                processEndCode(color == 0xF);
                transparent = color == 0x0;
                color = VDP1ReadVRAM<uint16>(color * sizeof(uint16) + lineParams.colorBank * 8);
                break;
            case 2: // 8 bpp, 64 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + charIndex) & 0x3F;
                processEndCode(color == 0xFF);
                transparent = color == 0x0;
                color |= lineParams.colorBank & 0xFFC0;
                break;
            case 3: // 8 bpp, 128 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + charIndex) & 0x7F;
                processEndCode(color == 0xFF);
                transparent = color == 0x00;
                color |= lineParams.colorBank & 0xFF80;
                break;
            case 4: // 8 bpp, 256 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + charIndex);
                processEndCode(color == 0xFF);
                transparent = color == 0x00;
                color |= lineParams.colorBank & 0xFF00;
                break;
            case 5: // 16 bpp, 32768 colors, RGB mode
                color = VDP1ReadVRAM<uint16>(lineParams.charAddr + charIndex * sizeof(uint16));
                processEndCode(color == 0x7FFF);
                transparent = color == 0x0000;
                break;
            }

            if (endCodeCount == 2) {
                break;
            }
        }

        if (hasEndCode || (transparent && !mode.transparentPixelDisable)) {
            continue;
        }

        VDP1PixelParams pixelParams{
            .mode = mode,
            .color = color,
        };

        gouraudParams.U = line.FracU() / charSizeH;

        VDP1PlotPixel(line.Coord(), pixelParams, gouraudParams);
        if (line.NeedsAntiAliasing()) {
            VDP1PlotPixel(line.AACoord(), pixelParams, gouraudParams);
        }
    }
}

void VDP::VDP1Cmd_DrawNormalSprite(uint32 cmdAddress, VDP1Command::Control control) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};
    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const uint32 charAddr = VDP1ReadVRAM<uint16>(cmdAddress + 0x08) * 8u;
    const VDP1Command::Size size{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x0A)};
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const uint32 charSizeH = size.H * 8;
    const uint32 charSizeV = size.V;

    const sint32 lx = xa;             // left X
    const sint32 ty = ya;             // top Y
    const sint32 rx = xa + charSizeH; // right X
    const sint32 by = ya + charSizeV; // bottom Y

    const CoordS32 coordA{lx, ty};
    const CoordS32 coordB{rx, ty};
    const CoordS32 coordC{rx, by};
    const CoordS32 coordD{lx, by};

    renderLog1.trace("Draw normal sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
                     "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
                     lx, ty, rx, ty, rx, by, lx, by, color, gouraudTable, mode.u16, charSizeH, charSizeV, charAddr);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    VDP1TexturedLineParams lineParams{
        .control = control,
        .mode = mode,
        .colorBank = color,
        .charAddr = charAddr,
        .charSizeH = charSizeH,
        .charSizeV = charSizeV,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };
    if (control.flipH) {
        std::swap(gouraudParams.colorA, gouraudParams.colorB);
        std::swap(gouraudParams.colorD, gouraudParams.colorC);
    }
    if (control.flipV) {
        std::swap(gouraudParams.colorA, gouraudParams.colorD);
        std::swap(gouraudParams.colorB, gouraudParams.colorC);
    }

    // Interpolate linearly over edges A-D and B-C
    const bool flipV = control.flipV;
    for (TexturedQuadEdgesStepper edge{coordA, coordB, coordC, coordD, charSizeV, flipV}; edge.CanStep(); edge.Step()) {
        // Plot lines between the interpolated points
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};
        lineParams.texFracV = edge.FracV();
        VDP1PlotTexturedLine(coordL, coordR, lineParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawScaledSprite(uint32 cmdAddress, VDP1Command::Control control) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};
    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const uint32 charAddr = VDP1ReadVRAM<uint16>(cmdAddress + 0x08) * 8u;
    const VDP1Command::Size size{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x0A)};
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const uint32 charSizeH = size.H * 8;
    const uint32 charSizeV = size.V;

    // Calculated quad coordinates
    sint32 qxa;
    sint32 qya;
    sint32 qxb;
    sint32 qyb;
    sint32 qxc;
    sint32 qyc;
    sint32 qxd;
    sint32 qyd;

    const uint8 zoomPointH = bit::extract<0, 1>(control.zoomPoint);
    const uint8 zoomPointV = bit::extract<2, 3>(control.zoomPoint);
    if (zoomPointH == 0 || zoomPointV == 0) {
        const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
        const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));

        // Top-left coordinates on vertex A
        // Bottom-right coordinates on vertex C
        qxa = xa;
        qya = ya;
        qxb = xc;
        qyb = ya;
        qxc = xc;
        qyc = yc;
        qxd = xa;
        qyd = yc;
    } else {
        const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10));
        const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12));

        // Zoom origin on vertex A
        // Zoom dimensions on vertex B

        // X axis
        switch (zoomPointH) {
        case 1: // left
            qxa = xa;
            qxb = xa + xb;
            qxc = xa + xb;
            qxd = xa;
            break;
        case 2: // center
            qxa = xa - xb / 2;
            qxb = xa + (xb + 1) / 2;
            qxc = xa + (xb + 1) / 2;
            qxd = xa - xb / 2;
            break;
        case 3: // right
            qxa = xa - xb;
            qxb = xa;
            qxc = xa;
            qxd = xa - xb;
            break;
        }

        // Y axis
        switch (zoomPointV) {
        case 1: // upper
            qya = ya;
            qyb = ya;
            qyc = ya + yb;
            qyd = ya + yb;
            break;
        case 2: // center
            qya = ya - yb / 2;
            qyb = ya - yb / 2;
            qyc = ya + (yb + 1) / 2;
            qyd = ya + (yb + 1) / 2;
            break;
        case 3: // lower
            qya = ya - yb;
            qyb = ya - yb;
            qyc = ya;
            qyd = ya;
            break;
        }
    }

    qxa += ctx.localCoordX;
    qya += ctx.localCoordY;
    qxb += ctx.localCoordX;
    qyb += ctx.localCoordY;
    qxc += ctx.localCoordX;
    qyc += ctx.localCoordY;
    qxd += ctx.localCoordX;
    qyd += ctx.localCoordY;

    const CoordS32 coordA{qxa, qya};
    const CoordS32 coordB{qxb, qyb};
    const CoordS32 coordC{qxc, qyc};
    const CoordS32 coordD{qxd, qyd};

    renderLog1.trace("Draw scaled sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
                     "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
                     qxa, qya, qxb, qyb, qxc, qyc, qxd, qyd, color, gouraudTable, mode.u16, charSizeH, charSizeV,
                     charAddr);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    VDP1TexturedLineParams lineParams{
        .control = control,
        .mode = mode,
        .colorBank = color,
        .charAddr = charAddr,
        .charSizeH = charSizeH,
        .charSizeV = charSizeV,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };
    if (control.flipH) {
        std::swap(gouraudParams.colorA, gouraudParams.colorB);
        std::swap(gouraudParams.colorD, gouraudParams.colorC);
    }
    if (control.flipV) {
        std::swap(gouraudParams.colorA, gouraudParams.colorD);
        std::swap(gouraudParams.colorB, gouraudParams.colorC);
    }

    // Interpolate linearly over edges A-D and B-C
    const bool flipV = control.flipV;
    for (TexturedQuadEdgesStepper edge{coordA, coordB, coordC, coordD, charSizeV, flipV}; edge.CanStep(); edge.Step()) {
        // Plot lines between the interpolated points
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};
        lineParams.texFracV = edge.FracV();
        VDP1PlotTexturedLine(coordL, coordR, lineParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawDistortedSprite(uint32 cmdAddress, VDP1Command::Control control) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};
    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const uint32 charAddr = VDP1ReadVRAM<uint16>(cmdAddress + 0x08) * 8u;
    const VDP1Command::Size size{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x0A)};
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14)) + ctx.localCoordX;
    const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16)) + ctx.localCoordY;
    const sint32 xd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x18)) + ctx.localCoordX;
    const sint32 yd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1A)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const uint32 charSizeH = size.H * 8;
    const uint32 charSizeV = size.V;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};
    const CoordS32 coordC{xc, yc};
    const CoordS32 coordD{xd, yd};

    renderLog1.trace("Draw distorted sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
                     "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
                     xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable, mode.u16, charSizeH, charSizeV, charAddr);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    VDP1TexturedLineParams lineParams{
        .control = control,
        .mode = mode,
        .colorBank = color,
        .charAddr = charAddr,
        .charSizeH = charSizeH,
        .charSizeV = charSizeV,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };
    if (control.flipH) {
        std::swap(gouraudParams.colorA, gouraudParams.colorB);
        std::swap(gouraudParams.colorD, gouraudParams.colorC);
    }
    if (control.flipV) {
        std::swap(gouraudParams.colorA, gouraudParams.colorD);
        std::swap(gouraudParams.colorB, gouraudParams.colorC);
    }

    // Interpolate linearly over edges A-D and B-C
    const bool flipV = control.flipV;
    for (TexturedQuadEdgesStepper edge{coordA, coordB, coordC, coordD, charSizeV, flipV}; edge.CanStep(); edge.Step()) {
        // Plot lines between the interpolated points
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};
        lineParams.texFracV = edge.FracV();
        VDP1PlotTexturedLine(coordL, coordR, lineParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawPolygon(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};

    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14)) + ctx.localCoordX;
    const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16)) + ctx.localCoordY;
    const sint32 xd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x18)) + ctx.localCoordX;
    const sint32 yd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1A)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};
    const CoordS32 coordC{xc, yc};
    const CoordS32 coordD{xd, yd};

    renderLog1.trace("Draw polygon: {}x{} - {}x{} - {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}",
                     xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable, mode.u16);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    const VDP1PixelParams pixelParams{
        .mode = mode,
        .color = color,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };

    // Interpolate linearly over edges A-D and B-C
    for (QuadEdgesStepper edge{coordA, coordB, coordC, coordD}; edge.CanStep(); edge.Step()) {
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};

        gouraudParams.V = edge.FracPos();

        // Plot lines between the interpolated points
        VDP1PlotLine(coordL, coordR, pixelParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawPolylines(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};

    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14)) + ctx.localCoordX;
    const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16)) + ctx.localCoordY;
    const sint32 xd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x18)) + ctx.localCoordX;
    const sint32 yd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1A)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};
    const CoordS32 coordC{xc, yc};
    const CoordS32 coordD{xd, yd};

    renderLog1.trace("Draw polylines: {}x{} - {}x{} - {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}",
                     xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable >> 3u, mode.u16);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    const VDP1PixelParams pixelParams{
        .mode = mode,
        .color = color,
    };

    const Color555 A{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)};
    const Color555 B{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)};
    const Color555 C{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)};
    const Color555 D{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)};

    VDP1GouraudParams gouraudParamsAB{.colorA = A, .colorB = B, .V = 0};
    VDP1GouraudParams gouraudParamsBC{.colorA = B, .colorB = C, .V = 0};
    VDP1GouraudParams gouraudParamsCD{.colorA = C, .colorB = D, .V = 0};
    VDP1GouraudParams gouraudParamsDA{.colorA = D, .colorB = A, .V = 0};

    VDP1PlotLine(coordA, coordB, pixelParams, gouraudParamsAB);
    VDP1PlotLine(coordB, coordC, pixelParams, gouraudParamsBC);
    VDP1PlotLine(coordC, coordD, pixelParams, gouraudParamsCD);
    VDP1PlotLine(coordD, coordA, pixelParams, gouraudParamsDA);
}

void VDP::VDP1Cmd_DrawLine(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};

    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};

    renderLog1.trace("Draw line: {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}", xa, ya, xb, yb,
                     color, gouraudTable, mode.u16);

    if (VDP1IsLineSystemClipped(coordA, coordB)) {
        return;
    }

    const VDP1PixelParams pixelParams{
        .mode = mode,
        .color = color,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .V = 0,
    };

    VDP1PlotLine(coordA, coordB, pixelParams, gouraudParams);
}

void VDP::VDP1Cmd_SetSystemClipping(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.sysClipH = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
    ctx.sysClipV = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));
    renderLog1.trace("Set system clipping: {}x{}", ctx.sysClipH, ctx.sysClipV);
}

void VDP::VDP1Cmd_SetUserClipping(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.userClipX0 = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    ctx.userClipY0 = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    ctx.userClipX1 = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
    ctx.userClipY1 = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));
    renderLog1.trace("Set user clipping: {}x{} - {}x{}", ctx.userClipX0, ctx.userClipY0, ctx.userClipX1,
                     ctx.userClipY1);
}

void VDP::VDP1Cmd_SetLocalCoordinates(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.localCoordX = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    ctx.localCoordY = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    renderLog1.trace("Set local coordinates: {}x{}", ctx.localCoordX, ctx.localCoordY);
}

// -----------------------------------------------------------------------------
// VDP2

void VDP::VDP2InitFrame() {
    if (m_VDP2.bgEnabled[5]) {
        VDP2InitRotationBG<0>();
        VDP2InitRotationBG<1>();
    } else {
        VDP2InitRotationBG<0>();
        VDP2InitNormalBG<0>();
        VDP2InitNormalBG<1>();
        VDP2InitNormalBG<2>();
        VDP2InitNormalBG<3>();
    }
}

template <uint32 index>
FORCE_INLINE void VDP::VDP2InitNormalBG() {
    static_assert(index < 4, "Invalid NBG index");

    if (!m_VDP2.bgEnabled[index]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[index + 1];
    NormBGLayerState &bgState = m_normBGLayerStates[index];
    bgState.fracScrollX = bgParams.scrollAmountH;
    bgState.fracScrollY = bgParams.scrollAmountV;
    if (m_VDP2.TVMD.LSMDn == 3 && m_VDP2.TVSTAT.ODD) {
        bgState.fracScrollY += bgParams.scrollIncV;
    }

    bgState.scrollIncH = bgParams.scrollIncH;
    bgState.mosaicCounterY = 0;
    if constexpr (index < 2) {
        bgState.lineScrollTableAddress = bgParams.lineScrollTableAddress;
    }
}

template <uint32 index>
FORCE_INLINE void VDP::VDP2InitRotationBG() {
    static_assert(index < 2, "Invalid RBG index");

    if (!m_VDP2.bgEnabled[index + 4]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[index];
    const bool cellSizeShift = bgParams.cellSizeShift;
    const bool twoWordChar = bgParams.twoWordChar;

    for (int param = 0; param < 2; param++) {
        const RotationParams &rotParam = m_VDP2.rotParams[param];
        auto &pageBaseAddresses = m_rotParamStates[param].pageBaseAddresses;
        const uint16 plsz = rotParam.plsz;
        for (int plane = 0; plane < 16; plane++) {
            const uint32 mapIndex = rotParam.mapIndices[plane];
            pageBaseAddresses[plane] = CalcPageBaseAddress(cellSizeShift, twoWordChar, plsz, mapIndex);
        }
    }
}

void VDP::VDP2UpdateEnabledBGs() {
    // Sprite layer is always enabled
    m_layerStates[0].enabled = true;

    if (m_VDP2.bgEnabled[5]) {
        m_layerStates[1].enabled = true;  // RBG0
        m_layerStates[2].enabled = true;  // RBG1
        m_layerStates[3].enabled = false; // EXBG
        m_layerStates[4].enabled = false; // not used
        m_layerStates[5].enabled = false; // not used
    } else {
        m_layerStates[1].enabled = m_VDP2.bgEnabled[4]; // RBG0
        m_layerStates[2].enabled = m_VDP2.bgEnabled[0]; // NBG0
        m_layerStates[3].enabled = m_VDP2.bgEnabled[1]; // NBG1/EXBG
        m_layerStates[4].enabled = m_VDP2.bgEnabled[2]; // NBG2
        m_layerStates[5].enabled = m_VDP2.bgEnabled[3]; // NBG3
    }
}

void VDP::VDP2UpdateLineScreenScroll(const BGParams &bgParams, NormBGLayerState &bgState) {
    uint32 address = bgState.lineScrollTableAddress;
    auto read = [&] {
        const uint32 value = VDP2ReadVRAM<uint32>(address);
        address += sizeof(uint32);
        return value;
    };

    if (bgParams.lineScrollXEnable) {
        bgState.fracScrollX = bgParams.scrollAmountH + bit::extract<8, 26>(read());
    }
    if (bgParams.lineScrollYEnable) {
        // TODO: check/optimize this
        bgState.fracScrollY = bgParams.scrollAmountV + bit::extract<8, 26>(read());
    }
    if (bgParams.lineZoomEnable) {
        bgState.scrollIncH = bit::extract<8, 18>(read());
    }
    if (m_VCounter > 0 && (m_VCounter & ((1u << bgParams.lineScrollInterval) - 1)) == 0) {
        bgState.lineScrollTableAddress = address;
    }
}

void VDP::VDP2CalcRotationParameterTables() {
    const uint32 baseAddress = m_VDP2.commonRotParams.baseAddress & 0xFFF7C; // mask bit 6 (shifted left by 1)
    const bool readAll = m_VCounter == 0;

    for (int i = 0; i < 2; i++) {
        RotationParams &params = m_VDP2.rotParams[i];
        RotationParamState &state = m_rotParamStates[i];

        const bool readXst = readAll || params.readXst;
        const bool readYst = readAll || params.readYst;
        const bool readKAst = readAll || params.readKAst;

        // Tables are located at the base address 0x80 bytes apart
        RotationParamTable t{};
        const uint32 address = baseAddress + i * 0x80;
        t.ReadFrom(&m_VRAM2[address & 0x7FFFF]);

        // Calculate parameters

        // Transformed starting screen coordinates
        // 16*(16-16) + 16*(16-16) + 16*(16-16) = 32 frac bits
        // reduce to 16 frac bits
        const sint64 Xsp = (t.A * (t.Xst - t.Px) + t.B * (t.Yst - t.Py) + t.C * (t.Zst - t.Pz)) >> 16ll;
        const sint64 Ysp = (t.D * (t.Xst - t.Px) + t.E * (t.Yst - t.Py) + t.F * (t.Zst - t.Pz)) >> 16ll;

        // Transformed view coordinates
        // 16*(16-16) + 16*(16-16) + 16*(16-16) + 16 + 16 = 32+32+32 + 16+16
        // reduce 32 to 16 frac bits, result is 16 frac bits
        /***/ sint64 Xp = ((t.A * (t.Px - t.Cx) + t.B * (t.Py - t.Cy) + t.C * (t.Pz - t.Cz)) >> 16ll) + t.Cx + t.Mx;
        const sint64 Yp = ((t.D * (t.Px - t.Cx) + t.E * (t.Py - t.Cy) + t.F * (t.Pz - t.Cz)) >> 16ll) + t.Cy + t.My;

        // Screen coordinate increments per Vcnt
        // 16*16 + 16*16 = 32
        // reduce to 16 frac bits
        const sint64 scrXIncV = (t.A * t.deltaXst + t.B * t.deltaYst) >> 16ll;
        const sint64 scrYIncV = (t.D * t.deltaXst + t.E * t.deltaYst) >> 16ll;

        // Screen coordinate increments per Hcnt
        // 16*16 + 16*16 = 32 frac bits
        // reduce to 16 frac bits
        const sint64 scrXIncH = (t.A * t.deltaX + t.B * t.deltaY) >> 16ll;
        const sint64 scrYIncH = (t.D * t.deltaX + t.E * t.deltaY) >> 16ll;

        // Scaling factors
        // 16 frac bits
        sint64 kx = t.kx;
        sint64 ky = t.ky;

        if (readXst) {
            state.scrX = Xsp;
        }
        if (readYst) {
            state.scrY = Ysp;
        }
        if (readKAst) {
            state.KA = t.KAst;
        }

        // Current screen coordinates (16 frac bits) and coefficient address (10 frac bits)
        sint32 scrX = state.scrX;
        sint32 scrY = state.scrY;
        uint32 KA = state.KA;

        const bool doubleResH = m_VDP2.TVMD.HRESOn & 0b010;
        const uint32 xShift = doubleResH ? 1 : 0;
        const uint32 maxX = m_HRes >> xShift;

        // Use per-dot coefficient if reading from CRAM or if any of the VRAM banks was designated as coefficient data
        bool perDotCoeff = m_VDP2.RAMCTL.CRKTE;
        if (!m_VDP2.RAMCTL.CRKTE) {
            perDotCoeff = m_VDP2.RAMCTL.RDBSA0n == 1 || m_VDP2.RAMCTL.RDBSB0n == 1;
            if (m_VDP2.RAMCTL.VRAMD) {
                perDotCoeff |= m_VDP2.RAMCTL.RDBSA1n == 1;
            }
            if (m_VDP2.RAMCTL.VRBMD) {
                perDotCoeff |= m_VDP2.RAMCTL.RDBSB1n == 1;
            }
        }

        // Precompute line color data parameters
        const LineBackScreenParams &lineParams = m_VDP2.lineScreenParams;
        const uint32 line = lineParams.perLine ? m_VCounter : 0;
        const uint32 lineColorAddress = lineParams.baseAddress + line * sizeof(uint16);
        const uint32 baseLineColorCRAMAddress = VDP2ReadVRAM<uint16>(lineColorAddress) * sizeof(uint16);

        // Fetch first coefficient
        Coefficient coeff = VDP2FetchRotationCoefficient(params, KA);

        // Precompute whole line
        for (uint32 x = 0; x < maxX; x++) {
            // Process coefficient table
            if (params.coeffTableEnable) {
                state.transparent[x] = coeff.transparent;

                // Replace parameters with those obtained from the coefficient table if enabled
                using enum CoefficientDataMode;
                switch (params.coeffDataMode) {
                case ScaleCoeffXY: kx = ky = coeff.value; break;
                case ScaleCoeffX: kx = coeff.value; break;
                case ScaleCoeffY: ky = coeff.value; break;
                case ViewpointX: Xp = coeff.value; break;
                }

                // Compute line colors
                if (params.coeffUseLineColorData) {
                    const uint32 cramAddress = bit::deposit<1, 8>(baseLineColorCRAMAddress, coeff.lineColorData);
                    const Color555 color555{.u16 = VDP2ReadCRAM<uint16>(cramAddress)};
                    state.lineColor[x] = ConvertRGB555to888(color555);
                }

                // Increment coefficient table address by Hcnt if using per-dot coefficients
                if (perDotCoeff) {
                    KA += t.dKAx;
                    if (VDP2CanFetchCoefficient(params, KA)) {
                        coeff = VDP2FetchRotationCoefficient(params, KA);
                    }
                }
            }

            // Store screen coordinates
            state.screenCoords[x].x = ((kx * scrX) >> 16ll) + Xp;
            state.screenCoords[x].y = ((ky * scrY) >> 16ll) + Yp;

            // Increment screen coordinates and coefficient table address by Hcnt
            scrX += scrXIncH;
            scrY += scrYIncH;
        }

        // Increment screen coordinates and coefficient table address by Vcnt for the next iteration
        state.scrX += scrXIncV;
        state.scrY += scrYIncV;
        state.KA += t.dKAst;

        // Disable read flags now that we've dealt with them
        params.readXst = false;
        params.readYst = false;
        params.readKAst = false;
    }
}

void VDP::VDP2DrawLine() {
    renderLog2.trace("Drawing line {}", m_VCounter);

    using FnDrawLayer = void (VDP::*)();

    // Lookup table of sprite drawing functions
    // Indexing: [colorMode][rotate]
    static constexpr auto fnDrawSprite = [] {
        std::array<std::array<FnDrawLayer, 2>, 4> arr{};

        util::constexpr_for<2 * 4>([&](auto index) {
            const uint32 cmIndex = bit::extract<0, 1>(index());
            const uint32 rotIndex = bit::extract<2>(index());

            const uint32 colorMode = cmIndex <= 2 ? cmIndex : 2;
            const bool rotate = rotIndex;
            arr[cmIndex][rotate] = &VDP::VDP2DrawSpriteLayer<colorMode, rotate>;
        });

        return arr;
    }();

    const uint32 colorMode = m_VDP2.RAMCTL.CRMDn;
    const bool rotate = m_VDP1.fbRotEnable;

    // Load rotation parameters if any of the RBG layers is enabled
    if (m_VDP2.bgEnabled[4] || m_VDP2.bgEnabled[5]) {
        VDP2CalcRotationParameterTables();
    }

    // Draw line color and back screen layers
    VDP2DrawLineColorAndBackScreens();

    // Draw sprite layer
    (this->*fnDrawSprite[colorMode][rotate])();

    // Draw background layers
    if (m_VDP2.bgEnabled[5]) {
        VDP2DrawRotationBG<0>(colorMode); // RBG0
        VDP2DrawRotationBG<1>(colorMode); // RBG1
    } else {
        VDP2DrawRotationBG<0>(colorMode); // RBG0
        VDP2DrawNormalBG<0>(colorMode);   // NBG0
        VDP2DrawNormalBG<1>(colorMode);   // NBG1
        VDP2DrawNormalBG<2>(colorMode);   // NBG2
        VDP2DrawNormalBG<3>(colorMode);   // NBG3
    }

    // Compose image
    VDP2ComposeLine();
}

void VDP::VDP2DrawLineColorAndBackScreens() {
    const LineBackScreenParams &lineParams = m_VDP2.lineScreenParams;
    const LineBackScreenParams &backParams = m_VDP2.backScreenParams;

    const uint32 y = m_VCounter;

    // Read line color screen color
    {
        const uint32 line = lineParams.perLine ? y : 0;
        const uint32 address = lineParams.baseAddress + line * sizeof(uint16);
        const uint32 cramAddress = VDP2ReadVRAM<uint16>(address) * sizeof(uint16);
        const Color555 color555{.u16 = VDP2ReadCRAM<uint16>(cramAddress)};
        m_lineBackLayerState.lineColor = ConvertRGB555to888(color555);
    }

    // Read back screen color
    {
        const uint32 line = backParams.perLine ? y : 0;
        const uint32 address = backParams.baseAddress + line * sizeof(Color555);
        const Color555 color555{.u16 = VDP2ReadVRAM<uint16>(address)};
        m_lineBackLayerState.backColor = ConvertRGB555to888(color555);
    }
}

template <uint32 colorMode, bool rotate>
NO_INLINE void VDP::VDP2DrawSpriteLayer() {
    const uint32 y = m_VCounter;

    // VDP1 scaling:
    // 2x horz: VDP1 TVM=000 and VDP2 HRESO=01x
    const bool doubleResH =
        !m_VDP1.hdtvEnable && !m_VDP1.fbRotEnable && !m_VDP1.pixel8Bits && (m_VDP2.TVMD.HRESOn & 0b110) == 0b010;
    const uint32 xShift = doubleResH ? 1 : 0;
    const uint32 maxX = m_HRes >> xShift;

    auto &layerState = m_layerStates[0];
    auto &spriteLayerState = m_spriteLayerState;

    for (uint32 x = 0; x < maxX; x++) {
        const uint32 xx = x << xShift;

        const auto &spriteFB = VDP1GetDisplayFB();
        const uint32 spriteFBOffset = [&] {
            if constexpr (rotate) {
                const auto &rotParamState = m_rotParamStates[0];
                const auto &screenCoord = rotParamState.screenCoords[x];
                const sint32 sx = screenCoord.x >> 16;
                const sint32 sy = screenCoord.y >> 16;
                return sx + sy * m_VDP1.fbSizeH;
            } else {
                return x + y * m_VDP1.fbSizeH;
            }
        }();

        const SpriteParams &params = m_VDP2.spriteParams;
        auto &pixel = layerState.pixels[xx];
        auto &attr = spriteLayerState.attrs[xx];

        util::ScopeGuard sgDoublePixel{[&] {
            if (doubleResH) {
                layerState.pixels[xx + 1] = pixel;
                spriteLayerState.attrs[xx + 1] = attr;
            }
        }};

        if (params.mixedFormat) {
            const uint16 spriteDataValue = util::ReadBE<uint16>(&spriteFB[(spriteFBOffset * sizeof(uint16)) & 0x3FFFE]);
            if (bit::extract<15>(spriteDataValue)) {
                // RGB data
                pixel.color = ConvertRGB555to888(Color555{spriteDataValue});
                pixel.transparent = false;
                pixel.priority = params.priorities[0];
                attr.msbSet = pixel.color.msb;
                attr.colorCalcRatio = params.colorCalcRatios[0];
                attr.shadowOrWindow = false;
                attr.normalShadow = false;
                continue;
            }
        }

        // Palette data
        const SpriteData spriteData = VDP2FetchSpriteData(spriteFBOffset);
        const uint32 colorIndex = params.colorDataOffset + spriteData.colorData;
        pixel.color = VDP2FetchCRAMColor<colorMode>(0, colorIndex);
        pixel.transparent = spriteData.colorData == 0;
        pixel.priority = params.priorities[spriteData.priority];
        attr.msbSet = spriteData.colorDataMSB;
        attr.colorCalcRatio = params.colorCalcRatios[spriteData.colorCalcRatio];
        attr.shadowOrWindow = spriteData.shadowOrWindow;
        attr.normalShadow = spriteData.normalShadow;
    }
}

template <uint32 bgIndex>
FORCE_INLINE void VDP::VDP2DrawNormalBG(uint32 colorMode) {
    static_assert(bgIndex < 4, "Invalid NBG index");

    using FnDraw = void (VDP::*)(const BGParams &, LayerState &, NormBGLayerState &);

    // Lookup table of scroll BG drawing functions
    // Indexing: [charMode][fourCellChar][colorFormat][colorMode]
    static constexpr auto fnDrawScroll = [] {
        std::array<std::array<std::array<std::array<FnDraw, 4>, 8>, 2>, 3> arr{};

        util::constexpr_for<3 * 2 * 8 * 4>([&](auto index) {
            const uint32 fcc = bit::extract<0>(index());
            const uint32 cf = bit::extract<1, 3>(index());
            const uint32 clm = bit::extract<4, 5>(index());
            const uint32 chm = bit::extract<6, 7>(index());

            const CharacterMode chmEnum = static_cast<CharacterMode>(chm);
            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = clm <= 2 ? clm : 2;
            arr[chm][fcc][cf][clm] = &VDP::VDP2DrawNormalScrollBG<chmEnum, fcc, cfEnum, colorMode>;
        });

        return arr;
    }();

    // Lookup table of bitmap BG drawing functions
    // Indexing: [colorFormat]
    static constexpr auto fnDrawBitmap = [] {
        std::array<std::array<FnDraw, 4>, 8> arr{};

        util::constexpr_for<8 * 4>([&](auto index) {
            const uint32 cf = bit::extract<0, 2>(index());
            const uint32 cm = bit::extract<3, 4>(index());

            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = cm <= 2 ? cm : 2;
            arr[cf][cm] = &VDP::VDP2DrawNormalBitmapBG<cfEnum, colorMode>;
        });

        return arr;
    }();

    if (!m_VDP2.bgEnabled[bgIndex]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[bgIndex + 1];
    LayerState &layerState = m_layerStates[bgIndex + 2];
    NormBGLayerState &bgState = m_normBGLayerStates[bgIndex];

    if constexpr (bgIndex < 2) {
        VDP2UpdateLineScreenScroll(bgParams, bgState);
    }

    const uint32 cf = static_cast<uint32>(bgParams.colorFormat);
    if (bgParams.bitmap) {
        (this->*fnDrawBitmap[cf][colorMode])(bgParams, layerState, bgState);
    } else {
        const bool twc = bgParams.twoWordChar;
        const bool fcc = bgParams.cellSizeShift;
        const bool exc = bgParams.extChar;
        const uint32 chm = static_cast<uint32>(twc   ? CharacterMode::TwoWord
                                               : exc ? CharacterMode::OneWordExtended
                                                     : CharacterMode::OneWordStandard);
        (this->*fnDrawScroll[chm][fcc][cf][colorMode])(bgParams, layerState, bgState);
    }

    bgState.mosaicCounterY++;
    if (bgState.mosaicCounterY >= m_VDP2.mosaicV) {
        bgState.mosaicCounterY = 0;
    }
}

template <uint32 bgIndex>
FORCE_INLINE void VDP::VDP2DrawRotationBG(uint32 colorMode) {
    static_assert(bgIndex < 2, "Invalid RBG index");

    static constexpr bool selRotParam = bgIndex == 0;

    using FnDraw = void (VDP::*)(const BGParams &, LayerState &);

    // Lookup table of scroll BG drawing functions
    // Indexing: [charMode][fourCellChar][colorFormat][colorMode]
    static constexpr auto fnDrawScroll = [] {
        std::array<std::array<std::array<std::array<FnDraw, 4>, 8>, 2>, 3> arr{};

        util::constexpr_for<3 * 2 * 8 * 4>([&](auto index) {
            const uint32 fcc = bit::extract<0>(index());
            const uint32 cf = bit::extract<1, 3>(index());
            const uint32 clm = bit::extract<4, 5>(index());
            const uint32 chm = bit::extract<6, 7>(index());

            const CharacterMode chmEnum = static_cast<CharacterMode>(chm);
            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = clm <= 2 ? clm : 2;
            arr[chm][fcc][cf][clm] = &VDP::VDP2DrawRotationScrollBG<selRotParam, chmEnum, fcc, cfEnum, colorMode>;
        });

        return arr;
    }();

    // Lookup table of bitmap BG drawing functions
    // Indexing: [colorFormat][colorMode]
    static constexpr auto fnDrawBitmap = [] {
        std::array<std::array<FnDraw, 4>, 8> arr{};

        util::constexpr_for<8 * 4>([&](auto index) {
            const uint32 cf = bit::extract<0, 2>(index());
            const uint32 cm = bit::extract<3, 4>(index());

            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = cm <= 2 ? cm : 2;
            arr[cf][cm] = &VDP::VDP2DrawRotationBitmapBG<selRotParam, cfEnum, colorMode>;
        });

        return arr;
    }();

    if (!m_VDP2.bgEnabled[bgIndex + 4]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[bgIndex];
    LayerState &layerState = m_layerStates[bgIndex + 1];

    const uint32 cf = static_cast<uint32>(bgParams.colorFormat);
    if (bgParams.bitmap) {
        (this->*fnDrawBitmap[cf][colorMode])(bgParams, layerState);
    } else {
        const bool twc = bgParams.twoWordChar;
        const bool fcc = bgParams.cellSizeShift;
        const bool exc = bgParams.extChar;
        const uint32 chm = static_cast<uint32>(twc   ? CharacterMode::TwoWord
                                               : exc ? CharacterMode::OneWordExtended
                                                     : CharacterMode::OneWordStandard);
        (this->*fnDrawScroll[chm][fcc][cf][colorMode])(bgParams, layerState);
    }
}

FORCE_INLINE void VDP::VDP2ComposeLine() {
    if (m_framebuffer == nullptr) {
        return;
    }

    const uint32 y = VDP2GetY(m_VCounter);

    if (!m_VDP2.TVMD.DISP) {
        std::fill_n(&m_framebuffer[y * m_HRes], m_HRes, 0);
        return;
    }

    for (uint32 x = 0; x < m_HRes; x++) {
        std::array<Layer, 3> layers = {LYR_Back, LYR_Back, LYR_Back};
        std::array<uint8, 3> layerPrios = {0, 0, 0};

        // Determine layer order
        for (int layer = 0; layer < m_layerStates.size(); layer++) {
            const LayerState &state = m_layerStates[layer];
            if (!state.enabled) {
                continue;
            }

            const Pixel &pixel = state.pixels[x];
            if (pixel.transparent) {
                continue;
            }
            if (pixel.priority == 0) {
                continue;
            }
            if (layer == LYR_Sprite) {
                const auto &attr = m_spriteLayerState.attrs[x];
                if (attr.normalShadow) {
                    continue;
                }
            }

            // Insert the layer into the appropriate position in the stack
            // - Higher priority beats lower priority
            // - If same priority, lower Layer index beats higher Layer index
            // - layers[0] is topmost (first) layer
            for (int i = 0; i < 3; i++) {
                if (pixel.priority > layerPrios[i] || (pixel.priority == layerPrios[i] && layer < layers[i])) {
                    // Push layers back
                    for (int j = 2; j > i; j--) {
                        layers[j] = layers[j - 1];
                        layerPrios[j] = layerPrios[j - 1];
                    }
                    layers[i] = static_cast<Layer>(layer);
                    layerPrios[i] = pixel.priority;
                    break;
                }
            }
        }

        // Retrieves the color of the given layer
        auto getLayerColor = [&](Layer layer) -> Color888 {
            if (layer == LYR_Back) {
                return m_lineBackLayerState.backColor;
            } else {
                const LayerState &state = m_layerStates[layer];
                const Pixel &pixel = state.pixels[x];
                return pixel.color;
            }
        };

        auto isColorCalcEnabled = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                const SpriteParams &spriteParams = m_VDP2.spriteParams;
                if (!spriteParams.colorCalcEnable) {
                    return false;
                }

                const Pixel &pixel = m_layerStates[LYR_Sprite].pixels[x];

                using enum SpriteColorCalculationCondition;
                switch (spriteParams.colorCalcCond) {
                case PriorityLessThanOrEqual: return pixel.priority <= spriteParams.colorCalcValue;
                case PriorityEqual: return pixel.priority == spriteParams.colorCalcValue;
                case PriorityGreaterThanOrEqual: return pixel.priority >= spriteParams.colorCalcValue;
                case MsbEqualsOne: return m_spriteLayerState.attrs[x].msbSet;
                default: util::unreachable();
                }
            } else if (layer == LYR_Back) {
                return m_VDP2.backScreenParams.colorCalcEnable;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].colorCalcEnable;
            }
        };

        auto getColorCalcRatio = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                return m_spriteLayerState.attrs[x].colorCalcRatio;
            } else if (layer == LYR_Back) {
                return m_VDP2.backScreenParams.colorCalcRatio;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].colorCalcRatio;
            }
        };

        auto isLineColorEnabled = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                return m_VDP2.spriteParams.lineColorScreenEnable;
            } else if (layer == LYR_Back) {
                return false;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].lineColorScreenEnable;
            }
        };

        auto getLineColor = [&](Layer layer) {
            if (layer == LYR_RBG0 || (layer == LYR_NBG0_RBG1 && m_VDP2.bgEnabled[5])) {
                const auto &rotParams = m_VDP2.rotParams[layer - LYR_RBG0];
                if (rotParams.coeffTableEnable && rotParams.coeffUseLineColorData) {
                    return m_rotParamStates[layer - LYR_RBG0].lineColor[x];
                } else {
                    return m_lineBackLayerState.lineColor;
                }
            } else {
                return m_lineBackLayerState.lineColor;
            }
        };

        auto isShadowEnabled = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                return m_spriteLayerState.attrs[x].shadowOrWindow;
            } else if (layer == LYR_Back) {
                return m_VDP2.backScreenParams.shadowEnable;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].shadowEnable;
            }
        };

        const bool isTopLayerColorCalcEnabled = [&] {
            if (!isColorCalcEnabled(layers[0])) {
                return false;
            }
            if (VDP2IsInsideWindow(m_VDP2.colorCalcParams.windowSet, x)) {
                return false;
            }
            if (layers[0] == LYR_Back || layers[0] == LYR_Sprite) {
                return true;
            }
            return m_layerStates[layers[0]].pixels[x].specialColorCalc;
        }();

        const auto &colorCalcParams = m_VDP2.colorCalcParams;

        // Calculate color
        Color888 outputColor{};
        if (isTopLayerColorCalcEnabled) {
            const Color888 topColor = getLayerColor(layers[0]);
            Color888 btmColor = getLayerColor(layers[1]);

            // Apply extended color calculations (only in normal TV modes)
            const bool useExtendedColorCalc = colorCalcParams.extendedColorCalcEnable && m_VDP2.TVMD.HRESOn < 2;
            if (useExtendedColorCalc) {
                // TODO: honor color RAM mode + palette/RGB format restrictions
                // - modes 1 and 2 don't blend layers if the bottom layer uses palette color

                // HACK: assuming color RAM mode 0 for now (aka no restrictions)
                if (isColorCalcEnabled(layers[1])) {
                    const Color888 l2Color = getLayerColor(layers[2]);
                    btmColor.r = (btmColor.r + l2Color.r) / 2;
                    btmColor.g = (btmColor.g + l2Color.g) / 2;
                    btmColor.b = (btmColor.b + l2Color.b) / 2;
                }
            }

            // Insert and blend line color screen if top layer uses it
            if (isLineColorEnabled(layers[0])) {
                const Color888 lineColor = getLineColor(layers[0]);
                if (useExtendedColorCalc) {
                    btmColor.r = (lineColor.r + btmColor.r) / 2;
                    btmColor.g = (lineColor.g + btmColor.g) / 2;
                    btmColor.b = (lineColor.b + btmColor.b) / 2;
                } else {
                    const uint8 ratio = m_VDP2.lineScreenParams.colorCalcRatio;
                    const uint8 complRatio = 32 - ratio;
                    btmColor.r = (lineColor.r * complRatio + btmColor.r * ratio) / 32;
                    btmColor.g = (lineColor.g * complRatio + btmColor.g * ratio) / 32;
                    btmColor.b = (lineColor.b * complRatio + btmColor.b * ratio) / 32;
                }
            }

            // Blend top and blended bottom layers
            if (colorCalcParams.useAdditiveBlend) {
                outputColor.r = std::min(topColor.r + btmColor.r, 255);
                outputColor.g = std::min(topColor.g + btmColor.g, 255);
                outputColor.b = std::min(topColor.b + btmColor.b, 255);
            } else {
                const uint8 ratio = getColorCalcRatio(colorCalcParams.useSecondScreenRatio ? layers[1] : layers[0]);
                const uint8 complRatio = 32 - ratio;
                outputColor.r = (topColor.r * complRatio + btmColor.r * ratio) / 32;
                outputColor.g = (topColor.g * complRatio + btmColor.g * ratio) / 32;
                outputColor.b = (topColor.b * complRatio + btmColor.b * ratio) / 32;
            }
        } else {
            outputColor = getLayerColor(layers[0]);
        }

        // Apply sprite shadow
        if (isShadowEnabled(layers[0])) {
            const bool isNormalShadow = m_spriteLayerState.attrs[x].normalShadow;
            const bool isMSBShadow =
                !m_VDP2.spriteParams.spriteWindowEnable && m_spriteLayerState.attrs[x].shadowOrWindow;
            if (isNormalShadow || isMSBShadow) {
                outputColor.r >>= 1u;
                outputColor.g >>= 1u;
                outputColor.b >>= 1u;
            }
        }

        // Get color offset parameters
        bool colorOffsetEnable = false;
        bool colorOffsetSelect = false;
        if (layers[0] == LYR_Back) {
            const LineBackScreenParams &backParams = m_VDP2.backScreenParams;
            colorOffsetEnable = backParams.colorOffsetEnable;
            colorOffsetSelect = backParams.colorOffsetSelect;
        } else if (layers[0] == LYR_Sprite) {
            const SpriteParams &spriteParams = m_VDP2.spriteParams;
            colorOffsetEnable = spriteParams.colorOffsetEnable;
            colorOffsetSelect = spriteParams.colorOffsetSelect;
        } else {
            const BGParams &bgParams = m_VDP2.bgParams[layers[0] - LYR_RBG0];
            colorOffsetEnable = bgParams.colorOffsetEnable;
            colorOffsetSelect = bgParams.colorOffsetSelect;
        }

        // Apply color offset if enabled
        if (colorOffsetEnable) {
            const auto &colorOffset = m_VDP2.colorOffsetParams[colorOffsetSelect];
            outputColor.r = std::clamp(outputColor.r + colorOffset.r, 0, 255);
            outputColor.g = std::clamp(outputColor.g + colorOffset.g, 0, 255);
            outputColor.b = std::clamp(outputColor.b + colorOffset.b, 0, 255);
        }

        m_framebuffer[x + y * m_HRes] = outputColor.u32;
    }
}

template <VDP::CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawNormalScrollBG(const BGParams &bgParams, LayerState &layerState,
                                           NormBGLayerState &bgState) {
    uint32 fracScrollX = bgState.fracScrollX;
    const uint32 fracScrollY = bgState.fracScrollY;
    bgState.fracScrollY += bgParams.scrollIncV;
    if (m_VDP2.TVMD.LSMDn == 3) {
        bgState.fracScrollY += bgParams.scrollIncV;
    }

    uint32 cellScrollTableAddress = m_VDP2.verticalCellScrollTableAddress;

    auto readCellScrollY = [&] {
        const uint32 value = VDP2ReadVRAM<uint32>(cellScrollTableAddress);
        cellScrollTableAddress += sizeof(uint32);
        return bit::extract<8, 26>(value);
    };

    uint8 mosaicCounterX = 0;
    uint32 cellScrollY = 0;

    if (bgParams.verticalCellScrollEnable) {
        // Read first vertical scroll amount if scrolled partway through a cell at the start of the line
        if (((fracScrollX >> 8u) & 7) != 0) {
            cellScrollY = readCellScrollY();
        }
    }

    for (uint32 x = 0; x < m_HRes; x++) {
        // Apply vertical cell-scrolling or horizontal mosaic
        if (bgParams.verticalCellScrollEnable) {
            // Update vertical cell scroll amount
            if (((fracScrollX >> 8u) & 7) == 0) {
                cellScrollY = readCellScrollY();
            }
        } else if (bgParams.mosaicEnable) {
            // Apply horizontal mosaic
            // TODO: should mosaic have priority over vertical cell scroll?
            const uint8 currMosaicCounterX = mosaicCounterX;
            mosaicCounterX++;
            if (mosaicCounterX >= m_VDP2.mosaicH) {
                mosaicCounterX = 0;
            }
            if (currMosaicCounterX > 0) {
                // Simply copy over the data from the previous pixel
                layerState.pixels[x] = layerState.pixels[x - 1];

                // Increment horizontal coordinate
                fracScrollX += bgState.scrollIncH;
                continue;
            }
        }

        if (VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside active window area
            layerState.pixels[x].transparent = true;
        } else {
            // Compute integer scroll screen coordinates
            const uint32 scrollX = fracScrollX >> 8u;
            const uint32 scrollY = ((fracScrollY + cellScrollY) >> 8u) - bgState.mosaicCounterY;
            const CoordU32 scrollCoord{scrollX, scrollY};

            // Plot pixel
            layerState.pixels[x] = VDP2FetchScrollBGPixel<false, charMode, fourCellChar, colorFormat, colorMode>(
                bgParams, bgParams.pageBaseAddresses, bgParams.pageShiftH, bgParams.pageShiftV, scrollCoord);
        }

        // Increment horizontal coordinate
        fracScrollX += bgState.scrollIncH;
    }
}

template <ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawNormalBitmapBG(const BGParams &bgParams, LayerState &layerState,
                                           NormBGLayerState &bgState) {
    uint32 fracScrollX = bgState.fracScrollX;
    const uint32 fracScrollY = bgState.fracScrollY;
    bgState.fracScrollY += bgParams.scrollIncV;
    if (m_VDP2.TVMD.LSMDn == 3) {
        bgState.fracScrollY += bgParams.scrollIncV;
    }

    uint32 cellScrollTableAddress = m_VDP2.verticalCellScrollTableAddress;

    auto readCellScrollY = [&] {
        const uint32 value = VDP2ReadVRAM<uint32>(cellScrollTableAddress);
        cellScrollTableAddress += sizeof(uint32);
        return bit::extract<8, 26>(value);
    };

    uint32 mosaicCounterX = 0;
    uint32 cellScrollY = 0;

    for (uint32 x = 0; x < m_HRes; x++) {
        // Update vertical cell scroll amount
        if (bgParams.verticalCellScrollEnable) {
            if (((fracScrollX >> 8u) & 7) == 0) {
                cellScrollY = readCellScrollY();
            }
        } else if (bgParams.mosaicEnable) {
            // Apply horizontal mosaic
            // TODO: should mosaic have priority over vertical cell scroll?
            const uint8 currMosaicCounterX = mosaicCounterX;
            mosaicCounterX++;
            if (mosaicCounterX >= m_VDP2.mosaicH) {
                mosaicCounterX = 0;
            }
            if (currMosaicCounterX > 0) {
                // Simply copy over the data from the previous pixel
                layerState.pixels[x] = layerState.pixels[x - 1];

                // Increment horizontal coordinate
                fracScrollX += bgState.scrollIncH;
                continue;
            }
        }

        if (VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside active window area
            layerState.pixels[x].transparent = true;
        } else {
            // Compute integer scroll screen coordinates
            const uint32 scrollX = fracScrollX >> 8u;
            const uint32 scrollY = ((fracScrollY + cellScrollY) >> 8u) - bgState.mosaicCounterY;
            const CoordU32 scrollCoord{scrollX, scrollY};

            // Plot pixel
            layerState.pixels[x] = VDP2FetchBitmapPixel<colorFormat, colorMode>(bgParams, scrollCoord);
        }

        // Increment horizontal coordinate
        fracScrollX += bgState.scrollIncH;
    }
}

template <bool selRotParam, VDP::CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawRotationScrollBG(const BGParams &bgParams, LayerState &layerState) {
    const bool doubleResH = m_VDP2.TVMD.HRESOn & 0b010;
    const uint32 xShift = doubleResH ? 1 : 0;
    const uint32 maxX = m_HRes >> xShift;

    for (uint32 x = 0; x < maxX; x++) {
        const uint32 xx = x << xShift;
        auto &pixel = layerState.pixels[xx];
        util::ScopeGuard sgDoublePixel{[&] {
            if (doubleResH) {
                layerState.pixels[xx + 1] = pixel;
            }
        }};

        const RotParamSelector rotParamSelector = selRotParam ? VDP2SelectRotationParameter(x) : RotParamA;

        const RotationParams &rotParams = m_VDP2.rotParams[rotParamSelector];
        const RotationParamState &rotParamState = m_rotParamStates[rotParamSelector];

        // Handle transparent pixels in coefficient table
        if (rotParams.coeffTableEnable && rotParamState.transparent[x]) {
            pixel.transparent = true;
            continue;
        }

        const sint32 fracScrollX = rotParamState.screenCoords[x].x;
        const sint32 fracScrollY = rotParamState.screenCoords[x].y;

        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 16u;
        const uint32 scrollY = fracScrollY >> 16u;
        const CoordU32 scrollCoord{scrollX, scrollY};

        // Determine maximum coordinates and screen over process
        const bool usingFixed512 = rotParams.screenOverProcess == ScreenOverProcess::Fixed512;
        const bool usingRepeat = rotParams.screenOverProcess == ScreenOverProcess::Repeat;
        const uint32 maxScrollX = usingFixed512 ? 512 : ((512 * 4) << rotParams.pageShiftH);
        const uint32 maxScrollY = usingFixed512 ? 512 : ((512 * 4) << rotParams.pageShiftV);

        if (VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside a window
            pixel.transparent = true;
        } else if ((scrollX < maxScrollX && scrollY < maxScrollY) || usingRepeat) {
            // Plot pixel
            pixel = VDP2FetchScrollBGPixel<true, charMode, fourCellChar, colorFormat, colorMode>(
                bgParams, rotParamState.pageBaseAddresses, rotParams.pageShiftH, rotParams.pageShiftV, scrollCoord);
        } else if (rotParams.screenOverProcess == ScreenOverProcess::RepeatChar) {
            // Out of bounds - repeat character
            const uint16 charData = rotParams.screenOverPatternName;

            // TODO: deduplicate code: VDP2FetchOneWordCharacter
            static constexpr bool largePalette = colorFormat != ColorFormat::Palette16;
            static constexpr bool extChar = charMode == CharacterMode::OneWordExtended;

            // Character number bit range from the 1-word character pattern data (charData)
            static constexpr uint32 baseCharNumStart = 0;
            static constexpr uint32 baseCharNumEnd = 9 + 2 * extChar;
            static constexpr uint32 baseCharNumPos = 2 * fourCellChar;

            // Upper character number bit range from the supplementary character number (bgParams.supplCharNum)
            static constexpr uint32 supplCharNumStart = 2 * fourCellChar + 2 * extChar;
            static constexpr uint32 supplCharNumEnd = 4;
            static constexpr uint32 supplCharNumPos = 10 + supplCharNumStart;
            // The lower bits are always in range 0..1 and only used if fourCellChar == true

            const uint32 baseCharNum = bit::extract<baseCharNumStart, baseCharNumEnd>(charData);
            const uint32 supplCharNum = bit::extract<supplCharNumStart, supplCharNumEnd>(bgParams.supplScrollCharNum);

            Character ch{};
            ch.charNum = (baseCharNum << baseCharNumPos) | (supplCharNum << supplCharNumPos);
            if constexpr (fourCellChar) {
                ch.charNum |= bit::extract<0, 1>(bgParams.supplScrollCharNum);
            }
            if constexpr (largePalette) {
                ch.palNum = bit::extract<12, 14>(charData) << 4u;
            } else {
                ch.palNum = bit::extract<12, 15>(charData) | bgParams.supplScrollPalNum;
            }
            ch.specColorCalc = bgParams.supplScrollSpecialColorCalc;
            ch.specPriority = bgParams.supplScrollSpecialPriority;
            ch.flipH = !extChar && bit::extract<10>(charData);
            ch.flipV = !extChar && bit::extract<11>(charData);

            const uint32 dotX = bit::extract<0, 2>(scrollX);
            const uint32 dotY = bit::extract<0, 2>(scrollY);
            const CoordU32 dotCoord{dotX, dotY};
            pixel = VDP2FetchCharacterPixel<colorFormat, colorMode>(bgParams, ch, dotCoord, 0);
        } else {
            // Out of bounds - transparent
            pixel.transparent = true;
        }
    }
}

template <bool selRotParam, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawRotationBitmapBG(const BGParams &bgParams, LayerState &layerState) {
    const bool doubleResH = m_VDP2.TVMD.HRESOn & 0b010;
    const uint32 xShift = doubleResH ? 1 : 0;
    const uint32 maxX = m_HRes >> xShift;

    for (uint32 x = 0; x < maxX; x++) {
        const uint32 xx = x << xShift;
        auto &pixel = layerState.pixels[xx];
        util::ScopeGuard sgDoublePixel{[&] {
            if (doubleResH) {
                layerState.pixels[xx + 1] = pixel;
            }
        }};
        const RotParamSelector rotParamSelector = selRotParam ? VDP2SelectRotationParameter(x) : RotParamA;

        const RotationParams &rotParams = m_VDP2.rotParams[rotParamSelector];
        const RotationParamState &rotParamState = m_rotParamStates[rotParamSelector];

        // Handle transparent pixels in coefficient table
        if (rotParams.coeffTableEnable && rotParamState.transparent[x]) {
            pixel.transparent = true;
            continue;
        }

        const sint32 fracScrollX = rotParamState.screenCoords[x].x;
        const sint32 fracScrollY = rotParamState.screenCoords[x].y;

        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 16u;
        const uint32 scrollY = fracScrollY >> 16u;
        const CoordU32 scrollCoord{scrollX, scrollY};

        const bool usingFixed512 = rotParams.screenOverProcess == ScreenOverProcess::Fixed512;
        const bool usingRepeat = rotParams.screenOverProcess == ScreenOverProcess::Repeat;
        const uint32 maxScrollX = usingFixed512 ? 512 : bgParams.bitmapSizeH;
        const uint32 maxScrollY = usingFixed512 ? 512 : bgParams.bitmapSizeV;

        if (VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside a window
            pixel.transparent = true;
        } else if ((scrollX < maxScrollX && scrollY < maxScrollY) || usingRepeat) {
            // Plot pixel
            pixel = VDP2FetchBitmapPixel<colorFormat, colorMode>(bgParams, scrollCoord);
        } else {
            // Out of bounds and no repeat
            pixel.transparent = true;
        }
    }
}

VDP::RotParamSelector VDP::VDP2SelectRotationParameter(uint32 x) {
    const CommonRotationParams &commonRotParams = m_VDP2.commonRotParams;

    using enum RotationParamMode;
    switch (commonRotParams.rotParamMode) {
    case RotationParamA: return RotParamA;
    case RotationParamB: return RotParamB;
    case Coefficient:
        return m_VDP2.rotParams[0].coeffTableEnable && m_rotParamStates[0].transparent[x] ? RotParamB : RotParamA;
    case Window: return VDP2IsInsideWindow(m_VDP2.commonRotParams.windowSet, x) ? RotParamB : RotParamA;
    }
}

bool VDP::VDP2CanFetchCoefficient(const RotationParams &params, uint32 coeffAddress) const {
    // Coefficients can always be fetched from CRAM
    if (m_VDP2.RAMCTL.CRKTE) {
        return true;
    }

    const uint32 baseAddress = params.coeffTableAddressOffset;
    const uint32 offset = coeffAddress >> 10u;

    // Check that the VRAM bank containing the coefficient table is designated for coefficient data.
    // Return a default (transparent) coefficient if not.
    // Determine which bank is targeted
    const uint32 address = ((baseAddress + offset) * sizeof(uint32)) >> params.coeffDataSize;

    // Address is 19 bits wide when using 512 KiB VRAM.
    // Bank is designated by bits 17-18.
    uint32 bank = bit::extract<17, 18>(address);

    // RAMCTL.VRAMD and VRBMD specify if VRAM A and B respectively are partitioned into two blocks (when set).
    // If they're not partitioned, RDBSA0n/RDBSB0n designate the role of the whole block (VRAM-A or -B).
    // RDBSA1n/RDBSB1n designates the roles of the second half of the partitioned banks (VRAM-A1 or -A2).
    // Masking the bank index with VRAMD/VRBMD adjusts the bank index of the second half back to the first half so
    // we can uniformly handle both cases with one simple switch table.
    if (bank < 2) {
        bank &= ~(m_VDP2.RAMCTL.VRAMD ^ 1);
    } else {
        bank &= ~(m_VDP2.RAMCTL.VRBMD ^ 1);
    }

    switch (bank) {
    case 0: // VRAM-A0 or VRAM-A
        if (m_VDP2.RAMCTL.RDBSA0n != 1) {
            return false;
        }
        break;
    case 1: // VRAM-A1
        if (m_VDP2.RAMCTL.RDBSA1n != 1) {
            return false;
        }
        break;
    case 2: // VRAM-B0 or VRAM-B
        if (m_VDP2.RAMCTL.RDBSB0n != 1) {
            return false;
        }
        break;
    case 3: // VRAM-B1
        if (m_VDP2.RAMCTL.RDBSB1n != 1) {
            return false;
        }
        break;
    }

    return true;
}

Coefficient VDP::VDP2FetchRotationCoefficient(const RotationParams &params, uint32 coeffAddress) {
    Coefficient coeff{};

    // Coefficient data formats:
    //
    // 1 word   15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // kx/ky   |TP|SN|Coeff. IP  | Coefficient fractional part |
    // Px      |TP|SN|Coefficient integer part            | FP |
    //
    // 2 words  31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // kx/ky   |TP| Line color data    |SN|Coeff. integer part |Coefficient fractional part                    |
    // Px      |TP| Line color data    |SN|Coefficient integer part                    |Coeff. fractional part |
    //
    // TP=transparent bit   SN=coefficient sign bit   IP=coefficient integer part   FP=coefficient fractional part

    const uint32 baseAddress = params.coeffTableAddressOffset;
    const uint32 offset = coeffAddress >> 10u;

    if (params.coeffDataSize == 1) {
        // One-word coefficient data
        const uint32 address = (baseAddress + offset) * sizeof(uint16);
        const uint16 data = m_VDP2.RAMCTL.CRKTE ? VDP2ReadCRAM<uint16>(address | 0x800) : VDP2ReadVRAM<uint16>(address);
        coeff.value = bit::extract_signed<0, 14>(data);
        coeff.lineColorData = 0;
        coeff.transparent = bit::extract<15>(data);

        if (params.coeffDataMode == CoefficientDataMode::ViewpointX) {
            coeff.value <<= 14;
        } else {
            coeff.value <<= 6;
        }
    } else {
        // Two-word coefficient data
        const uint32 address = (baseAddress + offset) * sizeof(uint32);
        const uint32 data = m_VDP2.RAMCTL.CRKTE ? VDP2ReadCRAM<uint32>(address | 0x800) : VDP2ReadVRAM<uint32>(address);
        coeff.value = bit::extract_signed<0, 23>(data);
        coeff.lineColorData = bit::extract<24, 30>(data);
        coeff.transparent = bit::extract<31>(data);

        if (params.coeffDataMode == CoefficientDataMode::ViewpointX) {
            coeff.value <<= 8;
        }
    }

    return coeff;
}

template <bool hasSpriteWindow>
bool VDP::VDP2IsInsideWindow(const WindowSet<hasSpriteWindow> &windowSet, uint32 x) {
    // If no windows are enabled, consider the pixel outside of windows
    if (!std::any_of(windowSet.enabled.begin(), windowSet.enabled.end(), std::identity{})) {
        return false;
    }

    // Check normal windows
    for (int i = 0; i < 2; i++) {
        // Skip if disabled
        if (!windowSet.enabled[i]) {
            continue;
        }

        const WindowParams &windowParam = m_VDP2.windowParams[i];
        const bool inverted = windowSet.inverted[i];

        // Check vertical coordinate
        const uint32 y = VDP2GetY(m_VCounter);
        const bool insideY = y >= windowParam.startY && y <= windowParam.endY;

        sint16 startX = windowParam.startX;
        sint16 endX = windowParam.endX;

        // Read line window if enabled
        if (windowParam.lineWindowTableEnable) {
            const uint32 address = windowParam.lineWindowTableAddress + m_VCounter * sizeof(uint16) * 2;
            sint16 startVal = VDP2ReadVRAM<uint16>(address + 0);
            sint16 endVal = VDP2ReadVRAM<uint16>(address + 2);

            // Some games set out-of-range window parameters and expects them to work.
            // It seems like window coordinates should be signed...
            //
            // Panzer Dragoon 2 Zwei:
            //   0000 to FFFE -> empty window
            //   FFFE to 02C0 -> full line
            //
            // Panzer Dragoon Saga:
            //   0000 to FFFF -> empty window
            //
            // Handle these cases here
            if (startVal < 0) {
                startVal = 0;
            }
            if (endVal < 0) {
                if (startVal >= endVal) {
                    startVal = 0x3FF;
                }
                endVal = 0;
            }

            startX = bit::extract<0, 9>(startVal);
            endX = bit::extract<0, 9>(endVal);
        }

        // For normal screen modes, X coordinates don't use bit 0
        if (m_VDP2.TVMD.HRESOn < 2) {
            startX >>= 1;
            endX >>= 1;
        }

        // Check horizontal coordinate
        const bool insideX = x >= startX && x <= endX;

        // Truth table: (state: false=outside, true=inside)
        // state  inverted  result   st != ao
        // false  false     outside  false
        // true   false     inside   true
        // false  true      inside   true
        // true   true      outside  false
        const bool inside = (insideX && insideY) != inverted;

        // Short-circuit the output if the logic allows for it
        // true short-circuits OR logic
        // false short-circuits AND logic
        if (inside == (windowSet.logic == WindowLogic::Or)) {
            return inside;
        }
    }

    // Check sprite window
    if constexpr (hasSpriteWindow) {
        if (windowSet.enabled[2]) {
            const bool inverted = windowSet.inverted[2];
            return m_spriteLayerState.attrs[x].shadowOrWindow != inverted;
        }
    }

    // Return the appropriate value for the given logic mode.
    // If we got to this point using OR logic, then the pixel is outside all enabled windows.
    // If we got to this point using AND logic, then the pixel is inside all enabled windows.
    return windowSet.logic == WindowLogic::And;
}

// TODO: optimize - remove pageShiftH and pageShiftV params
template <bool rot, VDP::CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE VDP::Pixel VDP::VDP2FetchScrollBGPixel(const BGParams &bgParams, std::span<const uint32> pageBaseAddresses,
                                                    uint32 pageShiftH, uint32 pageShiftV, CoordU32 scrollCoord) {
    //      Map (NBGs)              Map (RBGs)
    // +---------+---------+   +----+----+----+----+
    // |         |         |   | A  | B  | C  | D  |
    // | Plane A | Plane B |   +----+----+----+----+
    // |         |         |   | E  | F  | G  | H  |
    // +---------+---------+   +----+----+----+----+
    // |         |         |   | I  | J  | K  | L  |
    // | Plane C | Plane D |   +----+----+----+----+
    // |         |         |   | M  | N  | O  | P  |
    // +---------+---------+   +----+----+----+----+
    //
    // Normal and rotation BGs are divided into planes in the exact configurations illustrated above.
    // The BG's Map Offset Register is combined with the BG plane's Map Register (MPxxN#) to produce a base address for
    // each plane:
    //   Address bits  Source
    //            8-6  Map Offset Register (MPOFN)
    //            5-0  Map Register (MPxxN#)
    //
    // These addresses are precomputed in pageBaseAddresses.
    //
    //       2x2 Plane               2x1 Plane          1x1 Plane
    //        PLSZ=3                  PLSZ=1             PLSZ=0
    // +---------+---------+   +---------+---------+   +---------+
    // |         |         |   |         |         |   |         |
    // | Page 1  | Page 2  |   | Page 1  | Page 2  |   | Page 1  |
    // |         |         |   |         |         |   |         |
    // +---------+---------+   +---------+---------+   +---------+
    // |         |         |
    // | Page 3  | Page 4  |
    // |         |         |
    // +---------+---------+
    //
    // Each plane is composed of 1x1, 2x1 or 2x2 pages, determined by Plane Size in the Plane Size Register (PLSZ).
    // Pages are stored sequentially in VRAM left to right, top to bottom, as shown.
    //
    // The size is stored as a bit shift in bgParams.pageShiftH and bgParams.pageShiftV.
    //
    //        64x64 Page                 32x32 Page
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |CP 1|CP 2|  |CP63|CP64|   |CP 1|CP 2|  |CP31|CP32|
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |  65|  66|  | 127| 128|   |  33|  34|  |  63|  64|
    // +----+----+..+----+----+   +----+----+..+----+----+
    // :    :    :  :    :    :   :    :    :  :    :    :
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |3969|3970|  |4031|4032|   | 961| 962|  | 991| 992|
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |4033|4034|  |4095|4096|   | 993| 994|  |1023|1024|
    // +----+----+..+----+----+   +----+----+..+----+----+
    //
    // Pages contain 32x32 or 64x64 character patterns, which are groups of 1x1 or 2x2 cells, determined by Character
    // Size in the Character Control Register (CHCTLA-B).
    //
    // Pages always contain a total of 64x64 cells - a grid of 64x64 1x1 character patterns or 32x32 2x2 character
    // patterns. Because of this, pages always have 512x512 dots.
    //
    // Character patterns in a page are stored sequentially in VRAM left to right, top to bottom, as shown above.
    //
    // fourCellChar specifies the size of the character patterns (1x1 when false, 2x2 when true) and, by extension, the
    // dimensions of the page (32x32 or 64x64 respectively).
    //
    // 2x2 Character Pattern     1x1 C.P.
    // +---------+---------+   +---------+
    // |         |         |   |         |
    // | Cell 1  | Cell 2  |   | Cell 1  |
    // |         |         |   |         |
    // +---------+---------+   +---------+
    // |         |         |
    // | Cell 3  | Cell 4  |
    // |         |         |
    // +---------+---------+
    //
    // Character patterns are groups of 1x1 or 2x2 cells, determined by Character Size in the Character Control Register
    // (CHCTLA-B).
    //
    // Cells are stored sequentially in VRAM left to right, top to bottom, as shown above.
    //
    // Character patterns contain a character number (15 bits), a palette number (7 bits, only used with 16 or 256 color
    // palette modes), two special function bits (Special Priority and Special Color Calculation) and two flip bits
    // (horizontal and vertical).
    //
    // Character patterns can be one or two words long, as defined by Pattern Name Data Size in the Pattern Name Control
    // Register (PNCN0-3, PNCR). When using one word characters, some of the data comes from supplementary registers.
    //
    // fourCellChar stores the character pattern size (1x1 when false, 2x2 when true).
    // twoWordChar determines if characters are one (false) or two (true) words long.
    // extChar determines the length of the character data field in one word characters --
    // when true, they're extended by two bits, taking over the two flip bits.
    //
    //           Cell
    // +--+--+--+--+--+--+--+--+
    // | 1| 2| 3| 4| 5| 6| 7| 8|
    // +--+--+--+--+--+--+--+--+
    // | 9|10|11|12|13|14|15|16|
    // +--+--+--+--+--+--+--+--+
    // |17|18|19|20|21|22|23|24|
    // +--+--+--+--+--+--+--+--+
    // |25|26|27|28|29|30|31|32|
    // +--+--+--+--+--+--+--+--+
    // |33|34|35|36|37|38|39|40|
    // +--+--+--+--+--+--+--+--+
    // |41|42|43|44|45|46|47|48|
    // +--+--+--+--+--+--+--+--+
    // |49|50|51|52|53|54|55|56|
    // +--+--+--+--+--+--+--+--+
    // |57|58|59|60|61|62|63|64|
    // +--+--+--+--+--+--+--+--+
    //
    // Cells contain 8x8 dots (pixels) in one of the following color formats:
    //   - 16 color palette
    //   - 256 color palette
    //   - 1024 or 2048 color palette (depending on Color Mode)
    //   - 5:5:5 RGB (32768 colors)
    //   - 8:8:8 RGB (16777216 colors)
    //
    // colorFormat specifies one of the color formats above.
    // colorMode determines the palette color format in CRAM, one of:
    //   - 16-bit 5:5:5 RGB, 1024 words
    //   - 16-bit 5:5:5 RGB, 2048 words
    //   - 32-bit 8:8:8 RGB, 1024 longwords

    static constexpr std::size_t planeMSB = rot ? 11 : 10;
    static constexpr uint32 planeWidth = rot ? 4u : 2u;
    static constexpr uint32 planeMask = planeWidth - 1;

    static constexpr bool twoWordChar = charMode == CharacterMode::TwoWord;
    static constexpr bool extChar = charMode == CharacterMode::OneWordExtended;

    auto [scrollX, scrollY] = scrollCoord;

    // Determine plane index from the scroll coordinates
    const uint32 planeX = (bit::extract<9, planeMSB>(scrollX) >> pageShiftH) & planeMask;
    const uint32 planeY = (bit::extract<9, planeMSB>(scrollY) >> pageShiftV) & planeMask;
    const uint32 plane = planeX + planeY * planeWidth;

    // Determine page index from the scroll coordinates
    const uint32 pageX = bit::extract<9>(scrollX) & pageShiftH;
    const uint32 pageY = bit::extract<9>(scrollY) & pageShiftV;
    const uint32 page = pageX + pageY * 2u;

    // Determine character pattern from the scroll coordinates
    const uint32 charPatX = bit::extract<3, 8>(scrollX) >> fourCellChar;
    const uint32 charPatY = bit::extract<3, 8>(scrollY) >> fourCellChar;
    const uint32 charIndex = charPatX + charPatY * (64u >> fourCellChar);

    // Determine cell index from the scroll coordinates
    const uint32 cellX = bit::extract<3>(scrollX) & fourCellChar;
    const uint32 cellY = bit::extract<3>(scrollY) & fourCellChar;
    const uint32 cellIndex = cellX + cellY * 2u;

    // Determine dot coordinates
    const uint32 dotX = bit::extract<0, 2>(scrollX);
    const uint32 dotY = bit::extract<0, 2>(scrollY);
    const CoordU32 dotCoord{dotX, dotY};

    // Fetch character
    const uint32 pageBaseAddress = pageBaseAddresses[plane];
    const uint32 pageOffset = page << kPageSizes[fourCellChar][twoWordChar];
    const uint32 pageAddress = pageBaseAddress + pageOffset;
    constexpr bool largePalette = colorFormat != ColorFormat::Palette16;
    const Character ch =
        twoWordChar ? VDP2FetchTwoWordCharacter(pageAddress, charIndex)
                    : VDP2FetchOneWordCharacter<fourCellChar, largePalette, extChar>(bgParams, pageAddress, charIndex);

    // Fetch pixel using character data
    return VDP2FetchCharacterPixel<colorFormat, colorMode>(bgParams, ch, dotCoord, cellIndex);
}

FORCE_INLINE VDP::Character VDP::VDP2FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex) {
    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint32);
    const uint32 charData = VDP2ReadVRAM<uint32>(charAddress);

    Character ch{};
    ch.charNum = bit::extract<0, 14>(charData);
    ch.palNum = bit::extract<16, 22>(charData);
    ch.specColorCalc = bit::extract<28>(charData);
    ch.specPriority = bit::extract<29>(charData);
    ch.flipH = bit::extract<30>(charData);
    ch.flipV = bit::extract<31>(charData);
    return ch;
}

template <bool fourCellChar, bool largePalette, bool extChar>
FORCE_INLINE VDP::Character VDP::VDP2FetchOneWordCharacter(const BGParams &bgParams, uint32 pageBaseAddress,
                                                           uint32 charIndex) {
    // Contents of 1 word character patterns vary based on Character Size, Character Color Count and Auxiliary Mode:
    //     Character Size        = CHCTLA/CHCTLB.xxCHSZ  = !fourCellChar = !FCC
    //     Character Color Count = CHCTLA/CHCTLB.xxCHCNn = largePalette  = LP
    //     Auxiliary Mode        = PNCN0/PNCR.xxCNSM     = extChar      = EC
    //             ---------------- Character data ----------------    Supplement in Pattern Name Control Register
    // FCC LP  EC  |15 14 13 12 11 10 9  8  7  6  5  4  3  2  1  0|    | 9  8  7  6  5  4  3  2  1  0|
    //  F   F   F  |palnum 3-0 |VF|HF| character number 9-0       |    |PR|CC| PN 6-4 |charnum 14-10 |
    //  F   T   F  |--| PN 6-4 |VF|HF| character number 9-0       |    |PR|CC|--------|charnum 14-10 |
    //  T   F   F  |palnum 3-0 |VF|HF| character number 11-2      |    |PR|CC| PN 6-4 |CN 14-12|CN1-0|
    //  T   T   F  |--| PN 6-4 |VF|HF| character number 11-2      |    |PR|CC|--------|CN 14-12|CN1-0|
    //  F   F   T  |palnum 3-0 |       character number 11-0      |    |PR|CC| PN 6-4 |CN 14-12|-----|
    //  F   T   T  |--| PN 6-4 |       character number 11-0      |    |PR|CC|--------|CN 14-12|-----|
    //  T   F   T  |palnum 3-0 |       character number 13-2      |    |PR|CC| PN 6-4 |cn|-----|CN1-0|   cn=CN14
    //  T   T   T  |--| PN 6-4 |       character number 13-2      |    |PR|CC|--------|cn|-----|CN1-0|   cn=CN14

    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint16);
    const uint16 charData = VDP2ReadVRAM<uint16>(charAddress);

    // Character number bit range from the 1-word character pattern data (charData)
    static constexpr uint32 baseCharNumStart = 0;
    static constexpr uint32 baseCharNumEnd = 9 + 2 * extChar;
    static constexpr uint32 baseCharNumPos = 2 * fourCellChar;

    // Upper character number bit range from the supplementary character number (bgParams.supplCharNum)
    static constexpr uint32 supplCharNumStart = 2 * fourCellChar + 2 * extChar;
    static constexpr uint32 supplCharNumEnd = 4;
    static constexpr uint32 supplCharNumPos = 10 + supplCharNumStart;
    // The lower bits are always in range 0..1 and only used if fourCellChar == true

    const uint32 baseCharNum = bit::extract<baseCharNumStart, baseCharNumEnd>(charData);
    const uint32 supplCharNum = bit::extract<supplCharNumStart, supplCharNumEnd>(bgParams.supplScrollCharNum);

    Character ch{};
    ch.charNum = (baseCharNum << baseCharNumPos) | (supplCharNum << supplCharNumPos);
    if constexpr (fourCellChar) {
        ch.charNum |= bit::extract<0, 1>(bgParams.supplScrollCharNum);
    }
    if constexpr (largePalette) {
        ch.palNum = bit::extract<12, 14>(charData) << 4u;
    } else {
        ch.palNum = bit::extract<12, 15>(charData) | bgParams.supplScrollPalNum;
    }
    ch.specColorCalc = bgParams.supplScrollSpecialColorCalc;
    ch.specPriority = bgParams.supplScrollSpecialPriority;
    ch.flipH = !extChar && bit::extract<10>(charData);
    ch.flipV = !extChar && bit::extract<11>(charData);
    return ch;
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE VDP::Pixel VDP::VDP2FetchCharacterPixel(const BGParams &bgParams, Character ch, CoordU32 dotCoord,
                                                     uint32 cellIndex) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");

    Pixel pixel{};

    auto [dotX, dotY] = dotCoord;

    assert(dotX < 8);
    assert(dotY < 8);

    // Flip dot coordinates if requested
    if (ch.flipH) {
        dotX ^= 7;
        if (bgParams.cellSizeShift > 0) {
            cellIndex ^= 1;
        }
    }
    if (ch.flipV) {
        dotY ^= 7;
        if (bgParams.cellSizeShift > 0) {
            cellIndex ^= 2;
        }
    }

    // Adjust cell index based on color format
    if constexpr (!IsPaletteColorFormat(colorFormat)) {
        cellIndex <<= 2;
    } else if constexpr (colorFormat != ColorFormat::Palette16) {
        cellIndex <<= 1;
    }

    // Cell addressing uses a fixed offset of 32 bytes
    const uint32 cellAddress = (ch.charNum + cellIndex) * 0x20;
    const uint32 dotOffset = dotX + dotY * 8;

    // Determine special color calculation flag
    const auto &specFuncCode = m_VDP2.specialFunctionCodes[bgParams.specialFunctionSelect];
    auto getSpecialColorCalcFlag = [&](uint8 colorData) {
        using enum SpecialColorCalcMode;
        switch (bgParams.specialColorCalcMode) {
        case PerScreen: return bgParams.colorCalcEnable;
        case PerCharacter: return bgParams.colorCalcEnable && ch.specColorCalc;
        case PerDot: return bgParams.colorCalcEnable && ch.specColorCalc && specFuncCode.colorMatches[colorData];
        case ColorDataMSB: return bgParams.colorCalcEnable && ch.specColorCalc && bit::extract<2>(colorData);
        }
    };

    // Fetch color and determine transparency.
    // Also determine special color calculation flag if using per-dot or color data MSB.
    uint8 colorData = 0;
    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = cellAddress + (dotOffset >> 1u);
        const uint8 dotData = (VDP2ReadVRAM<uint8>(dotAddress) >> (((dotX & 1) ^ 1) * 4)) & 0xF;
        const uint32 colorIndex = (ch.palNum << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(colorData);
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = cellAddress + dotOffset;
        const uint8 dotData = VDP2ReadVRAM<uint8>(dotAddress);
        const uint32 colorIndex = ((ch.palNum & 0x70) << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(colorData);
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        const uint32 colorIndex = dotData & 0x7FF;
        colorData = bit::extract<1, 3>(dotData);
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && (dotData & 0x7FF) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(colorData);
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        pixel.color = ConvertRGB555to888(Color555{.u16 = dotData});
        pixel.transparent = bgParams.enableTransparency && bit::extract<15>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(0b111);
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint32);
        const uint32 dotData = VDP2ReadVRAM<uint32>(dotAddress);
        pixel.color.u32 = dotData;
        pixel.transparent = bgParams.enableTransparency && bit::extract<31>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(0b111);
    }

    // Compute priority
    pixel.priority = bgParams.priorityNumber;
    if (bgParams.priorityMode == PriorityMode::PerCharacter) {
        pixel.priority &= ~1;
        pixel.priority |= ch.specPriority;
    } else if (bgParams.priorityMode == PriorityMode::PerDot) {
        if constexpr (IsPaletteColorFormat(colorFormat)) {
            pixel.priority &= ~1;
            if (ch.specPriority && specFuncCode.colorMatches[colorData]) {
                pixel.priority |= 1;
            }
        }
    }

    return pixel;
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE VDP::Pixel VDP::VDP2FetchBitmapPixel(const BGParams &bgParams, CoordU32 dotCoord) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");

    Pixel pixel{};

    auto [dotX, dotY] = dotCoord;

    // Bitmap data wraps around infinitely
    dotX &= bgParams.bitmapSizeH - 1;
    dotY &= bgParams.bitmapSizeV - 1;

    // Bitmap addressing uses a fixed offset of 0x20000 bytes which is precalculated when MPOFN/MPOFR is written to
    const uint32 bitmapBaseAddress = bgParams.bitmapBaseAddress;
    const uint32 dotOffset = dotX + dotY * bgParams.bitmapSizeH;
    const uint32 palNum = bgParams.supplBitmapPalNum;

    // Determine special color calculation flag
    auto getSpecialColorCalcFlag = [&](bool colorDataMSB) {
        using enum SpecialColorCalcMode;
        switch (bgParams.specialColorCalcMode) {
        case PerScreen: return bgParams.colorCalcEnable;
        case PerCharacter: return bgParams.colorCalcEnable && bgParams.supplBitmapSpecialColorCalc;
        case PerDot: return bgParams.colorCalcEnable && bgParams.supplBitmapSpecialColorCalc;
        case ColorDataMSB: return bgParams.colorCalcEnable && colorDataMSB;
        }
    };

    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = bitmapBaseAddress + (dotOffset >> 1u);
        const uint8 dotData = (VDP2ReadVRAM<uint8>(dotAddress) >> (((dotX & 1) ^ 1) * 4)) & 0xF;
        const uint32 colorIndex = palNum | dotData;
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(bit::extract<3>(dotData));
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset;
        const uint8 dotData = VDP2ReadVRAM<uint8>(dotAddress);
        const uint32 colorIndex = palNum | dotData;
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(bit::extract<3>(dotData));
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        const uint32 colorIndex = dotData & 0x7FF;
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && (dotData & 0x7FF) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(bit::extract<3>(dotData));
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        pixel.color = ConvertRGB555to888(Color555{.u16 = dotData});
        pixel.transparent = bgParams.enableTransparency && bit::extract<15>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(true);
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint32);
        const uint32 dotData = VDP2ReadVRAM<uint32>(dotAddress);
        pixel.color = Color888{.u32 = dotData};
        pixel.transparent = bgParams.enableTransparency && bit::extract<31>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(true);
    }

    // Compute priority
    pixel.priority = bgParams.priorityNumber;
    if (bgParams.priorityMode == PriorityMode::PerCharacter || bgParams.priorityMode == PriorityMode::PerDot) {
        pixel.priority &= ~1;
        pixel.priority |= bgParams.supplBitmapSpecialPriority;
    }

    return pixel;
}

template <uint32 colorMode>
FORCE_INLINE Color888 VDP::VDP2FetchCRAMColor(uint32 cramOffset, uint32 colorIndex) {
    static_assert(colorMode <= 2, "Invalid CRMD value");

    if constexpr (colorMode == 0) {
        // RGB 5:5:5, 1024 words
        const uint32 address = (cramOffset + colorIndex) * sizeof(uint16);
        const uint16 data = VDP2ReadCRAM<uint16>(address & 0x7FF);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else if constexpr (colorMode == 1) {
        // RGB 5:5:5, 2048 words
        const uint32 address = (cramOffset + colorIndex) * sizeof(uint16);
        const uint16 data = VDP2ReadCRAM<uint16>(address);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else { // colorMode == 2
        // RGB 8:8:8, 1024 words
        const uint32 address = (cramOffset + colorIndex) * sizeof(uint32);
        const uint32 data = VDP2ReadCRAM<uint32>(address);
        return Color888{.u32 = data};
    }
}

FLATTEN FORCE_INLINE SpriteData VDP::VDP2FetchSpriteData(uint32 fbOffset) {
    const uint8 type = m_VDP2.spriteParams.type;
    if (type < 8) {
        return VDP2FetchWordSpriteData(fbOffset * sizeof(uint16), type);
    } else {
        return VDP2FetchByteSpriteData(fbOffset, type);
    }
}

FORCE_INLINE SpriteData VDP::VDP2FetchWordSpriteData(uint32 fbOffset, uint8 type) {
    assert(type < 8);

    const uint16 rawData = util::ReadBE<uint16>(&VDP1GetDisplayFB()[fbOffset & 0x3FFFE]);

    SpriteData data{};
    switch (m_VDP2.spriteParams.type) {
    case 0x0:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 13>(rawData);
        data.priority = bit::extract<14, 15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x1:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 12>(rawData);
        data.priority = bit::extract<13, 15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x2:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 13>(rawData);
        data.priority = bit::extract<14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x3:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 12>(rawData);
        data.priority = bit::extract<13, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x4:
        data.colorData = bit::extract<0, 9>(rawData);
        data.colorDataMSB = bit::extract<9>(rawData);
        data.colorCalcRatio = bit::extract<10, 12>(rawData);
        data.priority = bit::extract<13, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<9>(data.colorData);
        break;
    case 0x5:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x6:
        data.colorData = bit::extract<0, 9>(rawData);
        data.colorDataMSB = bit::extract<9>(rawData);
        data.colorCalcRatio = bit::extract<10, 11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<9>(data.colorData);
        break;
    case 0x7:
        data.colorData = bit::extract<0, 8>(rawData);
        data.colorDataMSB = bit::extract<8>(rawData);
        data.colorCalcRatio = bit::extract<9, 11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<8>(data.colorData);
        break;
    }
    return data;
}

FORCE_INLINE SpriteData VDP::VDP2FetchByteSpriteData(uint32 fbOffset, uint8 type) {
    assert(type >= 8);

    const uint8 rawData = VDP1GetDisplayFB()[fbOffset & 0x3FFFF];

    SpriteData data{};
    switch (m_VDP2.spriteParams.type) {
    case 0x8:
        data.colorData = bit::extract<0, 6>(rawData);
        data.colorDataMSB = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<6>(data.colorData);
        break;
    case 0x9:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorDataMSB = bit::extract<5>(rawData);
        data.colorCalcRatio = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<5>(data.colorData);
        break;
    case 0xA:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorDataMSB = bit::extract<5>(rawData);
        data.priority = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<5>(data.colorData);
        break;
    case 0xB:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorDataMSB = bit::extract<5>(rawData);
        data.colorCalcRatio = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<5>(data.colorData);
        break;
    case 0xC:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    case 0xD:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.colorCalcRatio = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    case 0xE:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.priority = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    case 0xF:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.colorCalcRatio = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    }
    return data;
}

template <uint32 colorDataBits>
FORCE_INLINE bool VDP::VDP2IsNormalShadow(uint16 colorData) {
    // Check against normal shadow pattern (LSB = 0, rest of the bits = 1)
    static constexpr uint16 kNormalShadowValue = ~(~0 << (colorDataBits + 1)) & ~1;
    return (colorData == kNormalShadowValue);
}

FORCE_INLINE uint32 VDP::VDP2GetY(uint32 y) const {
    if (m_VDP2.TVMD.LSMDn == 3) {
        return (y << 1) | m_VDP2.TVSTAT.ODD;
    } else {
        return y;
    }
}

} // namespace satemu::vdp

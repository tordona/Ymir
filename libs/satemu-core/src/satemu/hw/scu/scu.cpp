#include <satemu/hw/scu/scu.hpp>

#include "scu_devlog.hpp"

#include <satemu/util/inline.hpp>

#include <bit>

namespace satemu::scu {

// -----------------------------------------------------------------------------
// Debugger

FORCE_INLINE static void TraceRaiseInterrupt(debug::ISCUTracer *tracer, uint8 index, uint8 level) {
    if (tracer) {
        return tracer->RaiseInterrupt(index, level);
    }
}

FORCE_INLINE static void TraceAcknowledgeInterrupt(debug::ISCUTracer *tracer, uint8 index) {
    if (tracer) {
        return tracer->AcknowledgeInterrupt(index);
    }
}

FORCE_INLINE static void TraceDebugPortWrite(debug::ISCUTracer *tracer, uint8 ch) {
    if (tracer) {
        return tracer->DebugPortWrite(ch);
    }
}

// -----------------------------------------------------------------------------
// Implementation

SCU::SCU(core::Scheduler &scheduler, sys::Bus &bus)
    : m_bus(bus)
    , m_scheduler(scheduler)
    , m_dsp(bus) {

    m_dsp.SetTriggerDSPEndCallback(CbTriggerDSPEnd);

    EjectCartridge();

    m_timer1Event = m_scheduler.RegisterEvent(core::events::SCUTimer1, this, OnTimer1Event);

    Reset(true);
}

void SCU::Reset(bool hard) {
    m_cartSlot.Reset(hard);

    m_intrMask.u32 = 0;
    m_intrStatus.u32 = 0;
    m_abusIntrAck = false;

    if (hard) {
        for (auto &ch : m_dmaChannels) {
            ch.Reset();
        }
        m_activeDMAChannelLevel = m_dmaChannels.size();
    }

    m_dsp.Reset(hard);

    if (hard) {
        m_timer0Counter = 0;
        m_timer0Compare = 0;

        m_scheduler.Cancel(m_timer1Event);
        m_timer1Reload = 0;
        m_timer1Enable = false;
        m_timer1Mode = false;
    }

    m_WRAMSizeSelect = false;
}

void SCU::MapMemory(sys::Bus &bus) {
    // A-Bus CS0 and CS1 - Cartridge
    auto readCart8 = [](uint32 address, void *ctx) -> uint8 {
        return static_cast<SCU *>(ctx)->ReadCartridge<uint8, false>(address);
    };
    auto readCart16 = [](uint32 address, void *ctx) -> uint16 {
        return static_cast<SCU *>(ctx)->ReadCartridge<uint16, false>(address);
    };
    auto readCart32 = [](uint32 address, void *ctx) -> uint32 {
        return static_cast<SCU *>(ctx)->ReadCartridge<uint32, false>(address);
    };

    auto writeCart8 = [](uint32 address, uint8 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteCartridge<uint8, false>(address, value);
    };
    auto writeCart16 = [](uint32 address, uint16 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteCartridge<uint16, false>(address, value);
    };
    auto writeCart32 = [](uint32 address, uint32 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteCartridge<uint32, false>(address, value);
    };

    auto peekCart8 = [](uint32 address, void *ctx) -> uint8 {
        return static_cast<SCU *>(ctx)->ReadCartridge<uint8, true>(address);
    };
    auto peekCart16 = [](uint32 address, void *ctx) -> uint16 {
        return static_cast<SCU *>(ctx)->ReadCartridge<uint16, true>(address);
    };
    auto peekCart32 = [](uint32 address, void *ctx) -> uint32 {
        return static_cast<SCU *>(ctx)->ReadCartridge<uint32, true>(address);
    };

    auto pokeCart8 = [](uint32 address, uint8 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteCartridge<uint8, true>(address, value);
    };
    auto pokeCart16 = [](uint32 address, uint16 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteCartridge<uint16, true>(address, value);
    };
    auto pokeCart32 = [](uint32 address, uint32 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteCartridge<uint32, true>(address, value);
    };

    bus.MapMemory(0x200'0000, 0x4FF'FFFF,
                  {
                      .ctx = this,
                      .read8 = readCart8,
                      .read16 = readCart16,
                      .read32 = readCart32,
                      .write8 = writeCart8,
                      .write16 = writeCart16,
                      .write32 = writeCart32,
                      .peek8 = peekCart8,
                      .peek16 = peekCart16,
                      .peek32 = peekCart32,
                      .poke8 = pokeCart8,
                      .poke16 = pokeCart16,
                      .poke32 = pokeCart32,
                  });

    // A-Bus CS2 - 0x580'0000..0x58F'FFFF
    // CD block maps itself here

    // B-Bus - 0x5A0'0000..0x5FB'FFFF
    // VDP and SCSP map themselves here

    // TODO: 0x5FC'0000..0x5FD'FFFF - reads 0x000E0000

    // SCU registers
    auto readReg8 = [](uint32 address, void *ctx) -> uint8 {
        return static_cast<SCU *>(ctx)->ReadReg<uint8, false>(address & 0xFF);
    };
    auto readReg16 = [](uint32 address, void *ctx) -> uint16 {
        return static_cast<SCU *>(ctx)->ReadReg<uint16, false>(address & 0xFF);
    };
    auto readReg32 = [](uint32 address, void *ctx) -> uint32 {
        return static_cast<SCU *>(ctx)->ReadReg<uint32, false>(address & 0xFF);
    };

    auto writeReg8 = [](uint32 address, uint8 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteRegByte<false>(address & 0xFF, value);
    };
    auto writeReg16 = [](uint32 address, uint16 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteRegWord<false>(address & 0xFF, value);
    };
    auto writeReg32 = [](uint32 address, uint32 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteRegLong<false>(address & 0xFF, value);
    };

    auto peekReg8 = [](uint32 address, void *ctx) -> uint8 {
        return static_cast<SCU *>(ctx)->ReadReg<uint8, true>(address & 0xFF);
    };
    auto peekReg16 = [](uint32 address, void *ctx) -> uint16 {
        return static_cast<SCU *>(ctx)->ReadReg<uint16, true>(address & 0xFF);
    };
    auto peekReg32 = [](uint32 address, void *ctx) -> uint32 {
        return static_cast<SCU *>(ctx)->ReadReg<uint32, true>(address & 0xFF);
    };

    auto pokeReg8 = [](uint32 address, uint8 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteRegByte<true>(address & 0xFF, value);
    };
    auto pokeReg16 = [](uint32 address, uint16 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteRegWord<true>(address & 0xFF, value);
    };
    auto pokeReg32 = [](uint32 address, uint32 value, void *ctx) {
        static_cast<SCU *>(ctx)->WriteRegLong<true>(address & 0xFF, value);
    };

    bus.MapMemory(0x5FE'0000, 0x5FE'FFFF,
                  {
                      .ctx = this,
                      .read8 = readReg8,
                      .read16 = readReg16,
                      .read32 = readReg32,
                      .write8 = writeReg8,
                      .write16 = writeReg16,
                      .write32 = writeReg32,
                      .peek8 = peekReg8,
                      .peek16 = peekReg16,
                      .peek32 = peekReg32,
                      .poke8 = pokeReg8,
                      .poke16 = pokeReg16,
                      .poke32 = pokeReg32,
                  });

    // TODO: 0x5FF'0000..0x5FF'FFFF - Unknown registers
}

template <bool debug>
void SCU::Advance(uint64 cycles) {
    // RunDMA(cycles);

    m_dsp.Run<debug>(cycles);
}

template void SCU::Advance<false>(uint64 cycles);
template void SCU::Advance<true>(uint64 cycles);

void SCU::TriggerVBlankIN() {
    m_intrStatus.VDP2_VBlankIN = 1;
    UpdateInterruptLevel<false>();
    TriggerDMATransfer(DMATrigger::VBlankIN);
}

void SCU::TriggerVBlankOUT() {
    m_intrStatus.VDP2_VBlankOUT = 1;
    m_timer0Counter = 0;
    UpdateInterruptLevel<false>();
    TriggerDMATransfer(DMATrigger::VBlankOUT);
}

void SCU::TriggerHBlankIN() {
    m_intrStatus.VDP2_HBlankIN = 1;
    m_timer0Counter++;
    if (m_timer0Counter == m_timer0Compare) {
        TriggerTimer0();
    }
    m_scheduler.ScheduleFromNow(m_timer1Event, m_timer1Reload);
    UpdateInterruptLevel<false>();
    TriggerDMATransfer(DMATrigger::HBlankIN);
}

void SCU::TriggerTimer0() {
    m_intrStatus.SCU_Timer0 = 1;
    UpdateInterruptLevel<false>();
    TriggerDMATransfer(DMATrigger::Timer0);
}

void SCU::TriggerTimer1() {
    m_intrStatus.SCU_Timer1 = 1;
    UpdateInterruptLevel<false>();
    TriggerDMATransfer(DMATrigger::Timer1);
}

void SCU::TriggerDSPEnd() {
    m_intrStatus.SCU_DSPEnd = 1;
    UpdateInterruptLevel<false>();
}

void SCU::TriggerSoundRequest(bool level) {
    m_intrStatus.SCSP_SoundRequest = level;
    UpdateInterruptLevel<false>();
    if (level) {
        TriggerDMATransfer(DMATrigger::SoundRequest);
    }
}

void SCU::TriggerSystemManager() {
    m_intrStatus.SM_SystemManager = 1;
    UpdateInterruptLevel<false>();
}

void SCU::TriggerDMAEnd(uint32 level) {
    assert(level < 3);
    switch (level) {
    case 0: m_intrStatus.ABus_Level0DMAEnd = 1; break;
    case 1: m_intrStatus.ABus_Level1DMAEnd = 1; break;
    case 2: m_intrStatus.ABus_Level2DMAEnd = 1; break;
    }
    UpdateInterruptLevel<false>();
}

void SCU::TriggerSpriteDrawEnd() {
    m_intrStatus.VDP1_SpriteDrawEnd = 1;
    UpdateInterruptLevel<false>();
    TriggerDMATransfer(DMATrigger::SpriteDrawEnd);
}

void SCU::TriggerExternalInterrupt0() {
    m_intrStatus.ABus_ExtIntr0 = 1;
    UpdateInterruptLevel<false>();
}

void SCU::AcknowledgeExternalInterrupt() {
    UpdateInterruptLevel<true>();
}

void SCU::DumpDSPProgramRAM(std::ostream &out) {
    out.write((const char *)m_dsp.programRAM.data(), sizeof(m_dsp.programRAM));
}

void SCU::DumpDSPDataRAM(std::ostream &out) {
    out.write((const char *)m_dsp.dataRAM.data(), sizeof(m_dsp.dataRAM));
}

void SCU::DumpDSPRegs(std::ostream &out) {
    auto write = [&](const auto &reg) { out.write((const char *)&reg, sizeof(reg)); };
    write(m_dsp.programExecuting);
    write(m_dsp.programPaused);
    write(m_dsp.programEnded);
    write(m_dsp.programStep);
    write(m_dsp.PC);
    write(m_dsp.dataAddress);
    write(m_dsp.nextPC);
    write(m_dsp.jmpCounter);
    write(m_dsp.sign);
    write(m_dsp.zero);
    write(m_dsp.carry);
    write(m_dsp.overflow);
    write(m_dsp.CT);
    write(m_dsp.ALU);
    write(m_dsp.AC);
    write(m_dsp.P);
    write(m_dsp.RX);
    write(m_dsp.RY);
    write(m_dsp.loopTop);
    write(m_dsp.loopCount);
    write(m_dsp.dmaRun);
    write(m_dsp.dmaToD0);
    write(m_dsp.dmaHold);
    write(m_dsp.dmaCount);
    write(m_dsp.dmaSrc);
    write(m_dsp.dmaDst);
    write(m_dsp.dmaReadAddr);
    write(m_dsp.dmaWriteAddr);
    write(m_dsp.dmaAddrInc);
}

void SCU::OnTimer1Event(core::EventContext &eventContext, void *userContext) {
    auto &scu = *static_cast<SCU *>(userContext);
    scu.TickTimer1();
}

template <mem_primitive T, bool peek>
T SCU::ReadCartridge(uint32 address) {
    if constexpr (std::is_same_v<T, uint32>) {
        // 32-bit reads are split into two 16-bit reads
        uint32 value = ReadCartridge<uint16, peek>(address + 0) << 16u;
        value |= ReadCartridge<uint16, peek>(address + 2) << 0u;
        return value;
    } else if constexpr (std::is_same_v<T, uint16>) {
        if (address >= 0x4FF'FFFE) [[unlikely]] {
            // Return cartridge ID
            return 0xFF00 | m_cartSlot.GetID();
        } else {
            return m_cartSlot.ReadWord<peek>(address);
        }
    } else if constexpr (std::is_same_v<T, uint8>) {
        if (address >= 0x4FF'FFFE) [[unlikely]] {
            // Return cartridge ID
            if ((address & 1) == 0) {
                return 0xFF;
            } else {
                return m_cartSlot.GetID();
            }
        } else {
            return m_cartSlot.ReadByte<peek>(address);
        }
    }
}

template <mem_primitive T, bool poke>
void SCU::WriteCartridge(uint32 address, T value) {
    if constexpr (std::is_same_v<T, uint32>) {
        // 32-bit writes are split into two 16-bit writes
        WriteCartridge<uint16, poke>(address + 0, value >> 16u);
        WriteCartridge<uint16, poke>(address + 2, value >> 0u);
    } else if constexpr (std::is_same_v<T, uint16>) {
        m_cartSlot.WriteWord<poke>(address, value);
    } else if constexpr (std::is_same_v<T, uint8>) {
        if (address == 0x210'0001) [[unlikely]] {
            // mednafen debug port
            TraceDebugPortWrite(m_tracer, value);
            if constexpr (devlog::debug_enabled<grp::debug>) {
                if (value == '\n') {
                    devlog::debug<grp::debug>("{}", m_debugOutput);
                    m_debugOutput.clear();
                } else if (value != '\r') {
                    m_debugOutput.push_back(value);
                }
            }
        } else {
            m_cartSlot.WriteByte<poke>(address, value);
        }
    }
}

void SCU::RunDMA() {
    const uint8 level = m_activeDMAChannelLevel;
    if (level >= m_dmaChannels.size()) {
        return;
    }

    // TODO: proper cycle counting
    auto &ch = m_dmaChannels[level];

    // TODO: deduplicate code
    // - these lambdas are duplicated on RecalcDMAChannel() below
    auto setXferIncs = [&] {
        // source address increment:
        // - either 0 or 4 bytes when in CS2 area
        // - always 4 bytes elsewhere
        const bool srcCS2 = util::AddressInRange<0x580'0000, 0x58F'FFFF>(ch.currSrcAddr);
        const bool srcWRAM = ch.currSrcAddr >= 0x600'0000;
        if (srcCS2 || srcWRAM) {
            ch.currSrcAddrInc = ch.srcAddrInc;
        } else {
            ch.currSrcAddrInc = 4u;
        }

        // destination address increment:
        // - 0, 2, 4, 8, 16, 32, 64 or 128 bytes when in B-Bus
        // - 0 or 4 bytes when in CS2
        // - always 4 bytes elsewhere
        const bool dstBBus = util::AddressInRange<0x5A0'0000, 0x5FF'FFFF>(ch.currDstAddr);
        const bool dstCS2 = util::AddressInRange<0x580'0000, 0x58F'FFFF>(ch.currDstAddr);
        const bool dstWRAM = ch.currDstAddr >= 0x600'0000;
        if (dstBBus || dstWRAM) {
            ch.currDstAddrInc = ch.dstAddrInc;
        } else if (dstCS2) {
            ch.currDstAddrInc = ch.dstAddrInc ? 4u : 0u;
        } else {
            ch.currDstAddrInc = 4u;
        }
    };

    auto readIndirect = [&] {
        ch.currXferCount = m_bus.Read<uint32>(ch.currIndirectSrc + 0);
        ch.currDstAddr = m_bus.Read<uint32>(ch.currIndirectSrc + 4);
        ch.currSrcAddr = m_bus.Read<uint32>(ch.currIndirectSrc + 8);
        ch.currIndirectSrc += 3 * sizeof(uint32);
        ch.endIndirect = bit::extract<31>(ch.currSrcAddr);
        ch.currSrcAddr &= 0x7FF'FFFF;
        ch.currDstAddr &= 0x7FF'FFFF;
        setXferIncs();

        devlog::trace<grp::dma>(
            "SCU DMA{}: Starting indirect transfer at {:08X} - {:06X} bytes from {:08X} (+{:02X}) to "
            "{:08X} (+{:02X}){}",
            level, ch.currIndirectSrc - 3 * sizeof(uint32), ch.currXferCount, ch.currSrcAddr, ch.currSrcAddrInc,
            ch.currDstAddr, ch.currDstAddrInc, (ch.endIndirect ? " (final)" : ""));
        if (ch.currSrcAddr & 1) {
            devlog::debug<grp::dma>("SCU DMA{}: Unaligned indirect transfer read from {:08X}", level, ch.currSrcAddr);
        }
        if (ch.currDstAddr & 1) {
            devlog::debug<grp::dma>("SCU DMA{}: Unaligned indirect transfer write to {:08X}", level, ch.currDstAddr);
        }
    };

    while (ch.active) {
        const Bus srcBus = GetBus(ch.currSrcAddr);
        const Bus dstBus = GetBus(ch.currDstAddr);

        if (srcBus != dstBus && srcBus != Bus::None && dstBus != Bus::None) {
            uint32 value{};
            if (ch.currSrcAddr & 1) {
                // TODO: handle unaligned transfer
                devlog::trace<grp::dma>("SCU DMA{}: Unaligned read from {:08X}", level, ch.currSrcAddr);
            }
            if (srcBus == Bus::BBus) {
                value = m_bus.Read<uint16>(ch.currSrcAddr) << 16u;
                devlog::trace<grp::dma>("SCU DMA{}: B-Bus read from {:08X} -> {:04X}", level, ch.currSrcAddr,
                                        value >> 16u);
                ch.currSrcAddr += ch.currSrcAddrInc / 2u;
                value |= m_bus.Read<uint16>(ch.currSrcAddr) << 0u;
                devlog::trace<grp::dma>("SCU DMA{}: B-Bus read from {:08X} -> {:04X}", level, ch.currSrcAddr,
                                        value & 0xFFFF);
                ch.currSrcAddr += ch.currSrcAddrInc / 2u;
            } else {
                value = m_bus.Read<uint16>(ch.currSrcAddr + 0) << 16u;
                value |= m_bus.Read<uint16>(ch.currSrcAddr + 2) << 0u;
                devlog::trace<grp::dma>("SCU DMA{}: Read from {:08X} -> {:08X}", level, ch.currSrcAddr, value);
                ch.currSrcAddr += ch.currSrcAddrInc;
            }

            if (ch.currDstAddr & 1) {
                // TODO: handle unaligned transfer
                devlog::trace<grp::dma>("SCU DMA{}: Unaligned write to {:08X}", level, ch.currDstAddr);
            }
            if (dstBus == Bus::BBus) {
                m_bus.Write<uint16>(ch.currDstAddr, value >> 16u);
                devlog::trace<grp::dma>("SCU DMA{}: B-Bus write to {:08X} -> {:04X}", level, ch.currDstAddr,
                                        value >> 16u);
                ch.currDstAddr += ch.currDstAddrInc;
                m_bus.Write<uint16>(ch.currDstAddr, value >> 0u);
                devlog::trace<grp::dma>("SCU DMA{}: B-Bus write to {:08X} -> {:04X}", level, ch.currDstAddr,
                                        value & 0xFFFF);
                ch.currDstAddr += ch.currDstAddrInc;
            } else {
                m_bus.Write<uint16>(ch.currDstAddr + 0, value >> 16u);
                m_bus.Write<uint16>(ch.currDstAddr + 2, value >> 0u);
                devlog::trace<grp::dma>("SCU DMA{}: Write to {:08X} -> {:08X}", level, ch.currDstAddr, value);
                ch.currDstAddr += ch.currDstAddrInc;
            }

            ch.currSrcAddr &= 0x7FF'FFFF;
            ch.currDstAddr &= 0x7FF'FFFF;

            devlog::trace<grp::dma>("SCU DMA{}: Addresses incremented to {:08X}, {:08X}", level, ch.currSrcAddr,
                                    ch.currDstAddr);
        } else if (srcBus == dstBus) {
            devlog::trace<grp::dma>("SCU DMA{}: Invalid same-bus transfer; ignored", level);
        } else if (srcBus == Bus::None) {
            devlog::trace<grp::dma>("SCU DMA{}: Invalid source bus; transfer ignored", level);
        } else if (dstBus == Bus::None) {
            devlog::trace<grp::dma>("SCU DMA{}: Invalid destination bus; transfer ignored", level);
        }

        if (ch.currXferCount > sizeof(uint32)) {
            ch.currXferCount -= sizeof(uint32);
            devlog::trace<grp::dma>("SCU DMA{}: Transfer remaining: {:X} bytes", level, ch.currXferCount);
            // break; // higher-level DMA transfers interrupt lower-level ones
        } else if (ch.indirect && !ch.endIndirect) {
            readIndirect();
            // break; // higher-level DMA transfers interrupt lower-level ones
        } else {
            devlog::trace<grp::dma>("SCU DMA{}: Finished transfer", level);
            ch.active = false;
            ch.currXferCount = 0;
            if (ch.updateSrcAddr) {
                ch.srcAddr = ch.currSrcAddr;
            }
            if (ch.updateDstAddr) {
                ch.dstAddr = ch.currDstAddr;
            }
            TriggerDMAEnd(level);
            RecalcDMAChannel();
        }
    }
}

void SCU::RecalcDMAChannel() {
    m_activeDMAChannelLevel = m_dmaChannels.size();

    for (int level = 2; level >= 0; level--) {
        auto &ch = m_dmaChannels[level];

        if (!ch.enabled) {
            continue;
        }

        auto adjustZeroSizeXferCount = [&](uint32 xferCount) -> uint32 {
            if (xferCount != 0) {
                return xferCount;
            }
            if (level == 0) {
                return 0x100000u;
            } else {
                return 0x1000u;
            }
        };

        auto setXferIncs = [&] {
            // source address increment:
            // - either 0 or 4 bytes when in CS2 area
            // - always 4 bytes elsewhere
            const bool srcCS2 = util::AddressInRange<0x580'0000, 0x58F'FFFF>(ch.currSrcAddr);
            const bool srcWRAM = ch.currSrcAddr >= 0x600'0000;
            if (srcCS2 || srcWRAM) {
                ch.currSrcAddrInc = ch.srcAddrInc;
            } else {
                ch.currSrcAddrInc = 4u;
            }

            // destination address increment:
            // - 0, 2, 4, 8, 16, 32, 64 or 128 bytes when in B-Bus
            // - 0 or 4 bytes when in CS2
            // - always 4 bytes elsewhere
            const bool dstBBus = util::AddressInRange<0x5A0'0000, 0x5FF'FFFF>(ch.currDstAddr);
            const bool dstCS2 = util::AddressInRange<0x580'0000, 0x58F'FFFF>(ch.currDstAddr);
            const bool dstWRAM = ch.currDstAddr >= 0x600'0000;
            if (dstBBus || dstWRAM) {
                ch.currDstAddrInc = ch.dstAddrInc;
            } else if (dstCS2) {
                ch.currDstAddrInc = ch.dstAddrInc ? 4u : 0u;
            } else {
                ch.currDstAddrInc = 4u;
            }
        };

        auto readIndirect = [&] {
            ch.currXferCount = m_bus.Read<uint32>(ch.currIndirectSrc + 0);
            ch.currDstAddr = m_bus.Read<uint32>(ch.currIndirectSrc + 4);
            ch.currSrcAddr = m_bus.Read<uint32>(ch.currIndirectSrc + 8);
            ch.currIndirectSrc += 3 * sizeof(uint32);
            ch.endIndirect = bit::extract<31>(ch.currSrcAddr);
            ch.currSrcAddr &= 0x7FF'FFFF;
            ch.currDstAddr &= 0x7FF'FFFF;
            setXferIncs();

            devlog::trace<grp::dma>(
                "SCU DMA{}: Starting indirect transfer at {:08X} - {:06X} bytes from {:08X} (+{:02X}) to "
                "{:08X} (+{:02X}){}",
                level, ch.currIndirectSrc - 3 * sizeof(uint32), ch.currXferCount, ch.currSrcAddr, ch.currSrcAddrInc,
                ch.currDstAddr, ch.currDstAddrInc, (ch.endIndirect ? " (final)" : ""));
            if (ch.currSrcAddr & 1) {
                devlog::debug<grp::dma>("SCU DMA{}: Unaligned indirect transfer read from {:08X}", level,
                                        ch.currSrcAddr);
            }
            if (ch.currDstAddr & 1) {
                devlog::debug<grp::dma>("SCU DMA{}: Unaligned indirect transfer write to {:08X}", level,
                                        ch.currDstAddr);
            }
        };

        if (ch.start && !ch.active) {
            ch.start = false;
            ch.active = true;
            if (ch.indirect) {
                ch.currIndirectSrc = ch.dstAddr;
                readIndirect();
            } else {
                ch.currSrcAddr = ch.srcAddr & 0x7FF'FFFF;
                ch.currDstAddr = ch.dstAddr & 0x7FF'FFFF;
                ch.currXferCount = adjustZeroSizeXferCount(ch.xferCount);
                ch.currDstAddrInc = ch.dstAddrInc;
                setXferIncs();

                devlog::trace<grp::dma>(
                    "SCU DMA{}: Starting direct transfer of {:06X} bytes from {:08X} (+{:02X}) to {:08X} (+{:02X})",
                    level, ch.currXferCount, ch.currSrcAddr, ch.currSrcAddrInc, ch.currDstAddr, ch.currDstAddrInc);
                if (ch.currSrcAddr & 1) {
                    devlog::debug<grp::dma>("SCU DMA{}: Unaligned direct transfer read from {:08X}", level,
                                            ch.currSrcAddr);
                }
                if (ch.currDstAddr & 1) {
                    devlog::debug<grp::dma>("SCU DMA{}: Unaligned direct transfer write to {:08X}", level,
                                            ch.currDstAddr);
                }
            }
            m_activeDMAChannelLevel = level;
            break;
        }
    }
}

void SCU::TriggerDMATransfer(DMATrigger trigger) {
    for (auto &ch : m_dmaChannels) {
        if (ch.enabled && !ch.active && ch.trigger == trigger) {
            ch.start = true;
        }
    }
    RecalcDMAChannel();
    RunDMA(); // HACK: run all event-triggered DMA transfers immediately
}

FORCE_INLINE void SCU::TickTimer1() {
    if (m_timer1Enable && (!m_timer1Mode || m_timer0Counter == m_timer0Compare)) {
        TriggerTimer1();
    }
}

template <mem_primitive T, bool peek>
FORCE_INLINE T SCU::ReadReg(uint32 address) {
    if constexpr (std::is_same_v<T, uint8>) {
        const uint32 value = ReadReg<uint32, peek>(address & ~3u);
        return value >> ((~address & 3u) * 8u);
    } else if constexpr (std::is_same_v<T, uint16>) {
        const uint32 value = ReadReg<uint32, peek>(address & ~3u);
        return value >> ((~address & 2u) * 8u);
    } else {
        switch (address) {
        case 0x00: // Level 0 DMA Read Address
        case 0x20: // Level 1 DMA Read Address
        case 0x40: // Level 2 DMA Read Address
        {
            auto &ch = m_dmaChannels[address >> 5u];
            return ch.srcAddr;
        }
        case 0x04: // Level 0 DMA Write Address
        case 0x24: // Level 1 DMA Write Address
        case 0x44: // Level 2 DMA Write Address
        {
            auto &ch = m_dmaChannels[address >> 5u];
            return ch.dstAddr;
        }
        case 0x08: // Level 0 DMA Transfer Number
        case 0x28: // Level 1 DMA Transfer Number
        case 0x48: // Level 2 DMA Transfer Number
        {
            auto &ch = m_dmaChannels[address >> 5u];
            return ch.xferCount;
        }
        case 0x0C: // Level 0 DMA Increment (write-only)
        case 0x2C: // Level 1 DMA Increment (write-only)
        case 0x4C: // Level 2 DMA Increment (write-only)
            if constexpr (peek) {
                auto &ch = m_dmaChannels[address >> 5u];
                uint32 value = 0;
                bit::deposit_into<8>(value, ch.srcAddrInc / 4u);
                bit::deposit_into<0, 2>(value, ch.dstAddrInc == 0 ? 0 : std::countr_zero(ch.dstAddrInc));
                return value;
            } else {
                return 0;
            }
        case 0x10: // Level 0 DMA Enable (write-only)
        case 0x30: // Level 1 DMA Enable (write-only)
        case 0x50: // Level 2 DMA Enable (write-only)
            if constexpr (peek) {
                auto &ch = m_dmaChannels[address >> 5u];
                uint32 value = 0;
                bit::deposit_into<8>(value, ch.enabled);
                return value;
            } else {
                return 0;
            }
        case 0x14: // Level 0 DMA Mode (write-only)
        case 0x34: // Level 1 DMA Mode (write-only)
        case 0x54: // Level 2 DMA Mode (write-only)
            if constexpr (peek) {
                auto &ch = m_dmaChannels[address >> 5u];
                uint32 value = 0;
                bit::deposit_into<24>(value, ch.indirect);
                bit::deposit_into<16>(value, ch.updateSrcAddr);
                bit::deposit_into<8>(value, ch.updateDstAddr);
                bit::deposit_into<0, 2>(value, static_cast<uint8>(ch.trigger));
                return value;
            } else {
                return 0;
            }

        case 0x60: // DMA Force Stop (write-only)
            return 0;
        case 0x7C: // DMA Status
        {
            uint32 value = 0;
            // bit::deposit_into<0>(value, m_dsp.dmaRun); // TODO: is this correct?
            // bit::deposit_into<1>(value, m_dsp.dmaStandby?);
            bit::deposit_into<4>(value, m_dmaChannels[0].active);
            // TODO: bit 5: DMA0 standby
            bit::deposit_into<8>(value, m_dmaChannels[1].active);
            // TODO: bit 9: DMA1 standby
            bit::deposit_into<12>(value, m_dmaChannels[2].active);
            // TODO: bit 13: DMA2 standby
            bit::deposit_into<16>(value,
                                  m_dmaChannels[0].active && (m_dmaChannels[1].active || m_dmaChannels[2].active));
            bit::deposit_into<17>(value, m_dmaChannels[1].active && m_dmaChannels[2].active);
            // TODO: bit 20: DMA accessing A-Bus
            // TODO: bit 21: DMA accessing B-Bus
            // TODO: bit 22: DMA accessing DSP-Bus
            return value;
        }

        case 0x80: // DSP Program Control Port
        {
            uint32 value = 0;
            bit::deposit_into<0, 7>(value, m_dsp.PC);
            bit::deposit_into<16>(value, m_dsp.programExecuting);
            bit::deposit_into<18>(value, m_dsp.programEnded);
            bit::deposit_into<19>(value, m_dsp.overflow);
            bit::deposit_into<20>(value, m_dsp.carry);
            bit::deposit_into<21>(value, m_dsp.zero);
            bit::deposit_into<22>(value, m_dsp.sign);
            bit::deposit_into<23>(value, m_dsp.dmaRun);
            return value;
        }
        case 0x84: // DSP Program RAM Data Port (write-only)
            if constexpr (peek) {
                return m_dsp.ReadProgram();
            } else {
                return 0;
            }
        case 0x88: // DSP Data RAM Address Port (write-only)
            if constexpr (peek) {
                return m_dsp.dataAddress;
            } else {
                return 0;
            }
        case 0x8C: // DSP Data RAM Data Port
            return m_dsp.ReadData<peek>();

        case 0x90: // Timer 0 Compare (write-only)
            if constexpr (peek) {
                return ReadTimer0Compare();
            } else {
                return 0;
            }
        case 0x94: // Timer 1 Set Data (write-only)
            if constexpr (peek) {
                return ReadTimer1Reload();
            } else {
                return 0;
            }
        case 0x98: // Timer 1 Mode (write-only)
            if constexpr (peek) {
                uint32 value = 0;
                bit::deposit_into<0>(value, m_timer1Enable);
                bit::deposit_into<8>(value, m_timer1Mode);
                return value;
            } else {
                return 0;
            }

        case 0xA0: // Interrupt Mask
            return m_intrMask.u32;
        case 0xA4: // Interrupt Status
            return m_intrStatus.u32;
        case 0xA8: // A-Bus Interrupt Acknowledge
            return m_abusIntrAck;

        case 0xB0: // A-Bus Set (part 1) (write-only)
            if constexpr (peek) {
                // ignored for now
                return 0;
            } else {
                return 0;
            }
        case 0xB4: // A-Bus Set (part 2) (write-only)
            if constexpr (peek) {
                // ignored for now
                return 0;
            } else {
                return 0;
            }
        case 0xB8: // A-Bus Refresh (write-only)
            if constexpr (peek) {
                // ignored for now
                return 0;
            } else {
                return 0;
            }

        case 0xC4: // SCU SDRAM Select
            return m_WRAMSizeSelect;
        case 0xC8: // SCU Version
            return 0x4;

        default: //
            if constexpr (!peek) {
                devlog::debug<grp::regs>("unhandled {}-bit SCU register read from {:02X}", sizeof(T) * 8, address);
            }
            return 0;
        }
    }
}

template <bool poke>
FORCE_INLINE void SCU::WriteRegByte(uint32 address, uint8 value) {
    switch (address) {
    case 0xA0 ... 0xA1: break;                                     // Interrupt Mask (bits 16-31)
    case 0xA2: m_intrMask.u32 = (value << 8u) & 0x0000BF00; break; // Interrupt Mask (bits 8-15)
    case 0xA3: m_intrMask.u32 = (value << 0u) & 0x000000FF; break; // Interrupt Mask (bits 0-7)

    case 0xA4: m_intrStatus.u32 &= (value << 24u) | 0x00FFFFFF; break; // Interrupt Status (bits 24-31)
    case 0xA5: m_intrStatus.u32 &= (value << 16u) | 0xFF00FFFF; break; // Interrupt Status (bits 16-23)
    case 0xA6: m_intrStatus.u32 &= (value << 8u) | 0xFFFF00FF; break;  // Interrupt Status (bits 8-15)
    case 0xA7: m_intrStatus.u32 &= (value << 0u) | 0xFFFFFF00; break;  // Interrupt Status (bits 0-7)

    case 0xA8 ... 0xAA: break; // A-Bus Interrupt Acknowledge (bits 8-31)
    case 0xAB:                 // A-Bus Interrupt Acknowledge (bits 0-7)
        m_abusIntrAck = bit::extract<0>(value);
        if constexpr (!poke) {
            UpdateInterruptLevel<false>();
        }
        break;

    case 0xB0 ... 0xB3: // A-Bus Set (part 1)
        // ignored for now
        break;
    case 0xB4 ... 0xB7: // A-Bus Set (part 2)
        // ignored for now
        break;
    case 0xB8 ... 0xBB: // A-Bus Refresh
        // ignored for now
        break;

    case 0xC8 ... 0xCB: // SCU Version (read-only)
        break;

    default:
        if constexpr (poke) {
            uint32 currValue = ReadReg<uint32, true>(address & ~3u);
            const uint32 shift = (~address & 3u) * 8u;
            const uint32 mask = ~(0xFF << shift);
            currValue = (currValue & mask) | (value << shift);
            WriteRegLong<true>(address & ~3u, currValue);
        } else {
            devlog::debug<grp::regs>("unhandled 8-bit SCU register write to {:02X} = {:X}", address, value);
        }
        break;
    }
}

template <bool poke>
FORCE_INLINE void SCU::WriteRegWord(uint32 address, uint16 value) {
    if constexpr (poke) {
        uint32 currValue = ReadReg<uint32, true>(address & ~3u);
        const uint32 shift = (~address & 2u) * 8u;
        const uint32 mask = ~(0xFFFF << shift);
        currValue = (currValue & mask) | (value << shift);
        WriteRegLong<true>(address & ~3u, currValue);
    } else {
        devlog::debug<grp::regs>("unhandled 16-bit SCU register write to {:02X} = {:X}", address, value);
    }
}

template <bool poke>
FORCE_INLINE void SCU::WriteRegLong(uint32 address, uint32 value) {
    // TODO: handle 8-bit and 16-bit register writes if needed

    switch (address) {
    case 0x00: // Level 0 DMA Read Address
    case 0x20: // Level 1 DMA Read Address
    case 0x40: // Level 2 DMA Read Address
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.srcAddr = bit::extract<0, 26>(value);
        break;
    }
    case 0x04: // Level 0 DMA Write Address
    case 0x24: // Level 1 DMA Write Address
    case 0x44: // Level 2 DMA Write Address
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.dstAddr = bit::extract<0, 26>(value);
        break;
    }
    case 0x08: // Level 0 DMA Transfer Number
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.xferCount = bit::extract<0, 19>(value);
        break;
    }
    case 0x28: // Level 1 DMA Transfer Number
    case 0x48: // Level 2 DMA Transfer Number
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.xferCount = bit::extract<0, 11>(value);
        break;
    }
    case 0x0C: // Level 0 DMA Increment
    case 0x2C: // Level 1 DMA Increment
    case 0x4C: // Level 2 DMA Increment
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.srcAddrInc = bit::extract<8>(value) * 4u;
        ch.dstAddrInc = (1u << bit::extract<0, 2>(value)) & ~1u;
        break;
    }
    case 0x10: // Level 0 DMA Enable
    case 0x30: // Level 1 DMA Enable
    case 0x50: // Level 2 DMA Enable
    {
        const uint32 index = address >> 5u;
        auto &ch = m_dmaChannels[index];
        ch.enabled = bit::extract<8>(value);
        if constexpr (!poke) {
            if (ch.enabled) {
                devlog::trace<grp::dma>("DMA{} enabled - {:08X} (+{:02X}) -> {:08X} (+{:02X})", index, ch.srcAddr,
                                        ch.srcAddrInc, ch.dstAddr, ch.dstAddrInc);
            }
            if (ch.enabled && ch.trigger == DMATrigger::Immediate && bit::extract<0>(value)) {
                if (ch.active) {
                    devlog::trace<grp::dma>("DMA{} triggering immediate transfer while another transfer is in progress",
                                            index);
                    // Finish previous transfer
                    RunDMA();
                }
                ch.start = true;
                RecalcDMAChannel();
                RunDMA(); // HACK: run immediate DMA transfers immediately and instantly
            }
        }
        break;
    }
    case 0x14: // Level 0 DMA Mode
    case 0x34: // Level 1 DMA Mode
    case 0x54: // Level 2 DMA Mode
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.indirect = bit::extract<24>(value);
        ch.updateSrcAddr = bit::extract<16>(value);
        ch.updateDstAddr = bit::extract<8>(value);
        ch.trigger = static_cast<DMATrigger>(bit::extract<0, 2>(value));
        break;
    }

    case 0x60: // DMA Force Stop
        if constexpr (!poke) {
            if (bit::extract<0>(value)) {
                for (auto &ch : m_dmaChannels) {
                    ch.active = false;
                }
                m_activeDMAChannelLevel = m_dmaChannels.size();
            }
        }
        break;
    case 0x7C: // DMA Status (read-only)
        break;

    case 0x80: // DSP Program Control Port
        if (bit::extract<15>(value)) {
            m_dsp.PC = bit::extract<0, 7>(value);
        }
        if (bit::extract<25>(value)) {
            m_dsp.programPaused = true;
        } else if (bit::extract<26>(value)) {
            m_dsp.programPaused = false;
        } else if (!m_dsp.programExecuting) {
            m_dsp.programExecuting = bit::extract<16>(value);
            m_dsp.programStep = bit::extract<17>(value);
            m_dsp.programEnded = false;
        }
        break;
    case 0x84: // DSP Program RAM Data Port
        m_dsp.WriteProgram<poke>(value);
        break;
    case 0x88: // DSP Data RAM Address Port
        m_dsp.dataAddress = bit::extract<0, 7>(value);
        break;
    case 0x8C: // DSP Data RAM Data Port
        m_dsp.WriteData<poke>(value);
        break;

    case 0x90: // Timer 0 Compare
        WriteTimer0Compare(value);
        break;
    case 0x94: // Timer 1 Set Data
        WriteTimer1Reload(value);
        break;
    case 0x98: // Timer 1 Mode
        m_timer1Enable = bit::extract<0>(value);
        m_timer1Mode = bit::extract<8>(value);
        break;

    case 0xA0: // Interrupt Mask
        m_intrMask.u32 = value & 0x0000BFFF;
        break;
    case 0xA4: // Interrupt Status
        if constexpr (poke) {
            m_intrStatus.u32 = value & 0xFFFFBFFF;
        } else {
            m_intrStatus.u32 &= value;
        }
        break;
    case 0xA8: // A-Bus Interrupt Acknowledge
        m_abusIntrAck = bit::extract<0>(value);
        if constexpr (!poke) {
            UpdateInterruptLevel<false>();
        }
        break;

    case 0xB0: // A-Bus Set (part 1)
        // ignored for now
        break;
    case 0xB4: // A-Bus Set (part 2)
        // ignored for now
        break;
    case 0xB8: // A-Bus Refresh
        // ignored for now
        break;

    case 0xC4: // SCU SDRAM Select
        WriteWRAMSizeSelect(value);
        break;
    case 0xC8: // SCU Version (read-only)
        break;

    default:
        if constexpr (!poke) {
            devlog::debug<grp::regs>("unhandled 32-bit SCU register write to {:02X} = {:X}", address, value);
        }
        break;
    }
}

template <bool acknowledge>
FORCE_INLINE void SCU::UpdateInterruptLevel() {
    // SCU interrupts
    //  bit  vec   lvl  source  reason
    //    0   40     F  VDP2    VBlank IN
    //    1   41     E  VDP2    VBlank OUT
    //    2   42     D  VDP2    HBlank IN
    //    3   43     C  SCU     Timer 0
    //    4   44     B  SCU     Timer 1
    //    5   45     A  SCU     DSP End
    //    6   46     9  SCSP    Sound Request
    //    7   47     8  SM      System Manager
    //    8   48     8  PAD     PAD Interrupt
    //    9   49     6  A-Bus   Level 2 DMA End
    //   10   4A     6  A-Bus   Level 1 DMA End
    //   11   4B     5  A-Bus   Level 0 DMA End
    //   12   4C     3  SCU     DMA-illegal
    //   13   4D     2  VDP1    Sprite Draw End
    //   14   -
    //   15   -
    //   16   50     7  A-Bus   External Interrupt 00
    //   17   51     7  A-Bus   External Interrupt 01
    //   18   52     7  A-Bus   External Interrupt 02
    //   19   53     7  A-Bus   External Interrupt 03
    //   20   54     4  A-Bus   External Interrupt 04
    //   21   55     4  A-Bus   External Interrupt 05
    //   22   56     4  A-Bus   External Interrupt 06
    //   23   57     4  A-Bus   External Interrupt 07
    //   24   58     1  A-Bus   External Interrupt 08
    //   25   59     1  A-Bus   External Interrupt 09
    //   26   5A     1  A-Bus   External Interrupt 0A
    //   27   5B     1  A-Bus   External Interrupt 0B
    //   28   5C     1  A-Bus   External Interrupt 0C
    //   29   5D     1  A-Bus   External Interrupt 0D
    //   30   5E     1  A-Bus   External Interrupt 0E
    //   31   5F     1  A-Bus   External Interrupt 0F

    static constexpr uint32 kInternalLevels[] = {0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, //
                                                 0x8, 0x6, 0x6, 0x5, 0x3, 0x2, 0x0, 0x0, //
                                                 0x0};
    static constexpr uint32 kExternalLevels[] = {0x7, 0x7, 0x7, 0x7, 0x4, 0x4, 0x4, 0x4, //
                                                 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, //
                                                 0x0};
    const uint16 internalBits = m_intrStatus.internal & ~m_intrMask.internal;
    const uint16 externalBits = m_intrMask.ABus_ExtIntrs ? m_intrStatus.external : 0u;
    if (internalBits != 0 || externalBits != 0) {
        const uint16 internalIndex = std::countr_zero(internalBits);
        const uint16 externalIndex = std::countr_zero(externalBits);

        const uint8 internalLevel = kInternalLevels[internalIndex];
        const uint8 externalLevel = kExternalLevels[externalIndex];
        devlog::trace<grp::base>("Intr states:  {:04X} {:04X}", m_intrStatus.internal, m_intrStatus.external);
        devlog::trace<grp::base>("Intr masks:   {:04X} {:04X} {}", (uint16)m_intrMask.internal,
                                 m_intrMask.ABus_ExtIntrs * 0xFFFF, m_abusIntrAck);
        devlog::trace<grp::base>("Intr bits:    {:04X} {:04X}", internalBits, externalBits);
        devlog::trace<grp::base>("Intr indices: {:X} {:X}", internalIndex, externalIndex);
        devlog::trace<grp::base>("Intr levels:  {:X} {:X}", internalLevel, externalLevel);

        if constexpr (acknowledge) {
            if (internalLevel >= externalLevel) {
                m_intrStatus.internal &= ~(1u << internalIndex);
                devlog::trace<grp::base>("Acknowledging internal interrupt {:X}", internalIndex);
                TraceAcknowledgeInterrupt(m_tracer, internalIndex);
            } else {
                m_intrStatus.external &= ~(1u << externalIndex);
                devlog::trace<grp::base>("Acknowledging external interrupt {:X}", externalIndex);
                TraceAcknowledgeInterrupt(m_tracer, externalIndex + 16);
            }
            UpdateInterruptLevel<false>();
        } else {
            if (internalLevel >= externalLevel) {
                m_cbExternalMasterInterrupt(internalLevel, internalIndex + 0x40);
                devlog::trace<grp::base>("Raising internal interrupt {:X}", internalIndex);
                TraceRaiseInterrupt(m_tracer, internalIndex, internalLevel);

                // Also send VBlank IN and HBlank IN to slave SH2 if it is enabled
                if (internalIndex == 0) {
                    m_cbExternalSlaveInterrupt(2, 0x43);
                } else if (internalIndex == 2) {
                    m_cbExternalSlaveInterrupt(1, 0x41);
                } else {
                    m_cbExternalSlaveInterrupt(0, 0);
                }
            } else if (m_abusIntrAck) {
                devlog::trace<grp::base>("Raising external interrupt {:X}", externalIndex);
                TraceRaiseInterrupt(m_tracer, externalIndex + 16, externalLevel);
                m_abusIntrAck = false;
                m_cbExternalMasterInterrupt(externalLevel, externalIndex + 0x50);
                m_cbExternalSlaveInterrupt(0, 0);
            } else {
                devlog::trace<grp::base>("Lowering interrupt signal");
                m_cbExternalMasterInterrupt(0, 0);
                m_cbExternalSlaveInterrupt(0, 0);
            }
        }
    } else {
        m_cbExternalMasterInterrupt(0, 0);
        m_cbExternalSlaveInterrupt(0, 0);
    }
}

} // namespace satemu::scu

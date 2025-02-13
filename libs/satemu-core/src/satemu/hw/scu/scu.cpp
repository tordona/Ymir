#include <satemu/hw/scu/scu.hpp>

#include <satemu/hw/sh2/sh2_block.hpp>

#include <bit>

namespace satemu::scu {

enum class Bus {
    ABus,
    BBus,
    WRAM,
    None,
};

static Bus GetBus(uint32 address) {
    address &= 0x7FF'FFFF;
    /**/ if (util::AddressInRange<0x200'0000, 0x58F'FFFF>(address)) {
        return Bus::ABus;
    } else if (util::AddressInRange<0x5A0'0000, 0x5FB'FFFF>(address)) {
        return Bus::BBus;
    } else if (address >= 0x600'0000) {
        return Bus::WRAM;
    } else {
        return Bus::None;
    }
}

SCU::SCU(core::Scheduler &scheduler, sh2::SH2Block &sh2)
    : m_SH2(sh2)
    , m_scheduler(scheduler) {

    m_timer1Event = m_scheduler.RegisterEvent(core::events::SCUTimer1, this, OnTimer1Event<false>, OnTimer1Event<true>);

    static constexpr std::size_t kExternalBackupRAMSize = 4_MiB; // HACK: should be in its own class
    // TODO: configurable path and mode
    std::error_code error{};
    m_externalBackupRAM.LoadFrom("bup-ext.bin", kExternalBackupRAMSize, error);
    // TODO: handle error

    Reset(true);
}

void SCU::Reset(bool hard) {
    m_intrMask.u32 = 0;
    m_intrStatus.u32 = 0;
    m_abusIntrAck = false;

    if (hard) {
        for (auto &ch : m_dmaChannels) {
            ch.Reset();
        }
        m_activeDMAChannelLevel = m_dmaChannels.size();
    }

    m_dspState.Reset(hard);

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

template <bool debug>
void SCU::Advance(uint64 cycles) {
    // RunDMA(cycles);

    RunDSP(cycles);
}

template void SCU::Advance<false>(uint64 cycles);
template void SCU::Advance<true>(uint64 cycles);

void SCU::TriggerVBlankIN() {
    m_intrStatus.VDP2_VBlankIN = 1;
    UpdateInterruptLevel(false);
    TriggerDMATransfer(DMATrigger::VBlankIN);
}

void SCU::TriggerVBlankOUT() {
    m_intrStatus.VDP2_VBlankOUT = 1;
    m_timer0Counter = 0;
    UpdateInterruptLevel(false);
    TriggerDMATransfer(DMATrigger::VBlankOUT);
}

void SCU::TriggerHBlankIN() {
    m_intrStatus.VDP2_HBlankIN = 1;
    m_timer0Counter++;
    if (m_timer0Counter == m_timer0Compare) {
        TriggerTimer0();
    }
    m_scheduler.ScheduleFromNow(m_timer1Event, m_timer1Reload);
    UpdateInterruptLevel(false);
    TriggerDMATransfer(DMATrigger::HBlankIN);
}

void SCU::TriggerTimer0() {
    m_intrStatus.SCU_Timer0 = 1;
    UpdateInterruptLevel(false);
    TriggerDMATransfer(DMATrigger::Timer0);
}

void SCU::TriggerTimer1() {
    m_intrStatus.SCU_Timer1 = 1;
    UpdateInterruptLevel(false);
    TriggerDMATransfer(DMATrigger::Timer1);
}

void SCU::TriggerDSPEnd() {
    m_intrStatus.SCU_DSPEnd = 1;
    UpdateInterruptLevel(false);
}

void SCU::TriggerSoundRequest(bool level) {
    m_intrStatus.SCSP_SoundRequest = level;
    UpdateInterruptLevel(false);
    if (level) {
        TriggerDMATransfer(DMATrigger::SoundRequest);
    }
}

void SCU::TriggerSystemManager() {
    m_intrStatus.SM_SystemManager = 1;
    UpdateInterruptLevel(false);
}

void SCU::TriggerDMAEnd(uint32 level) {
    assert(level < 3);
    switch (level) {
    case 0: m_intrStatus.ABus_Level0DMAEnd = 1; break;
    case 1: m_intrStatus.ABus_Level1DMAEnd = 1; break;
    case 2: m_intrStatus.ABus_Level2DMAEnd = 1; break;
    }
    UpdateInterruptLevel(false);
}

void SCU::TriggerSpriteDrawEnd() {
    m_intrStatus.VDP1_SpriteDrawEnd = 1;
    UpdateInterruptLevel(false);
    TriggerDMATransfer(DMATrigger::SpriteDrawEnd);
}

void SCU::TriggerExternalInterrupt0() {
    m_intrStatus.ABus_ExtIntr0 = 1;
    UpdateInterruptLevel(false);
}

void SCU::AcknowledgeExternalInterrupt() {
    UpdateInterruptLevel(true);
}

void SCU::DumpDSPProgramRAM(std::ostream &out) {
    out.write((const char *)m_dspState.programRAM.data(), sizeof(m_dspState.programRAM));
}

void SCU::DumpDSPDataRAM(std::ostream &out) {
    out.write((const char *)m_dspState.dataRAM.data(), sizeof(m_dspState.dataRAM));
}

void SCU::DumpDSPRegs(std::ostream &out) {
    auto write = [&](const auto &reg) { out.write((const char *)&reg, sizeof(reg)); };
    write(m_dspState.programExecuting);
    write(m_dspState.programPaused);
    write(m_dspState.programEnded);
    write(m_dspState.programStep);
    write(m_dspState.PC);
    write(m_dspState.dataAddress);
    write(m_dspState.nextPC);
    write(m_dspState.jmpCounter);
    write(m_dspState.sign);
    write(m_dspState.zero);
    write(m_dspState.carry);
    write(m_dspState.overflow);
    write(m_dspState.CT);
    write(m_dspState.ALU);
    write(m_dspState.AC);
    write(m_dspState.P);
    write(m_dspState.RX);
    write(m_dspState.RY);
    write(m_dspState.loopTop);
    write(m_dspState.loopCount);
    write(m_dspState.dmaRun);
    write(m_dspState.dmaToD0);
    write(m_dspState.dmaHold);
    write(m_dspState.dmaCount);
    write(m_dspState.dmaSrc);
    write(m_dspState.dmaDst);
    write(m_dspState.dmaReadAddr);
    write(m_dspState.dmaWriteAddr);
    write(m_dspState.dmaAddrInc);
}

template <bool debug>
void SCU::OnTimer1Event(core::EventContext &eventContext, void *userContext, uint64 cyclesLate) {
    auto &scu = *static_cast<SCU *>(userContext);
    scu.TickTimer1<debug>();
}

void SCU::MapMemory(sh2::SH2Bus &bus) {
    // A-Bus CS0 and CS1 - Cartridge
    // TODO: let the cartridge map itself
    bus.MapMemory(0x200'0000, 0x4FF'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<SCU *>(ctx)->ReadCartridge<uint8>(address);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<SCU *>(ctx)->ReadCartridge<uint16>(address);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          return static_cast<SCU *>(ctx)->ReadCartridge<uint32>(address);
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<SCU *>(ctx)->WriteCartridge<uint8>(address, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<SCU *>(ctx)->WriteCartridge<uint16>(address, value); },
                      .write32 = [](uint32 address, uint32 value,
                                    void *ctx) { static_cast<SCU *>(ctx)->WriteCartridge<uint32>(address, value); },
                  });

    // A-Bus CS2 - 0x580'0000..0x58F'FFFF
    // CD block maps itself here

    // B-Bus - 0x5A0'0000..0x5FB'FFFF
    // VDP and SCSP map themselves here

    // TODO: 0x5FC'0000..0x5FD'FFFF - reads 0x000E0000

    // SCU registers
    bus.MapMemory(0x5FE'0000, 0x5FE'FFFF,
                  {
                      .ctx = this,
                      .read8 = [](uint32 address, void *ctx) -> uint8 {
                          return static_cast<SCU *>(ctx)->ReadReg<uint8>(address & 0xFF);
                      },
                      .read16 = [](uint32 address, void *ctx) -> uint16 {
                          return static_cast<SCU *>(ctx)->ReadReg<uint16>(address & 0xFF);
                      },
                      .read32 = [](uint32 address, void *ctx) -> uint32 {
                          return static_cast<SCU *>(ctx)->ReadReg<uint32>(address & 0xFF);
                      },
                      .write8 = [](uint32 address, uint8 value,
                                   void *ctx) { static_cast<SCU *>(ctx)->WriteRegByte(address & 0xFF, value); },
                      .write16 = [](uint32 address, uint16 value,
                                    void *ctx) { static_cast<SCU *>(ctx)->WriteRegWord(address & 0xFF, value); },
                      .write32 = [](uint32 address, uint32 value,
                                    void *ctx) { static_cast<SCU *>(ctx)->WriteRegLong(address & 0xFF, value); },
                  });

    // TODO: 0x5FF'0000..0x5FF'FFFF - Unknown registers
}

template <mem_primitive T>
T SCU::ReadCartridge(uint32 address) {
    if constexpr (std::is_same_v<T, uint32>) {
        // 32-bit reads are split into two 16-bit reads
        uint32 value = ReadCartridge<uint16>(address + 0) << 16u;
        value |= ReadCartridge<uint16>(address + 2) << 0u;
        return value;
    }

    // HACK: emulate 32 Mbit backup RAM cartridge
    // TODO: move to Backup RAM Cartridge implementation
    if (address >= 0x400'0000) {
        if (address >= 0x4FF'FFFC) {
            // Return ID for 32 Mbit Backup RAM cartridge
            if constexpr (std::is_same_v<T, uint8>) {
                if ((address & 1) == 0) {
                    return 0xFF;
                } else {
                    return 0x24;
                }
            } else {
                return 0xFF24;
            }
        }
        if constexpr (std::is_same_v<T, uint8>) {
            return m_externalBackupRAM.ReadByte(address);
        } else if constexpr (std::is_same_v<T, uint16>) {
            return m_externalBackupRAM.ReadWord(address);
        } else if constexpr (std::is_same_v<T, uint32>) {
            return m_externalBackupRAM.ReadLong(address);
        }
    }

    regsLog.trace("Unhandled {}-bit cartridge port read from {:05X}", sizeof(T) * 8, address);
    return 0xFF;
}

template <mem_primitive T>
void SCU::WriteCartridge(uint32 address, T value) {
    if constexpr (std::is_same_v<T, uint32>) {
        // 32-bit writes are split into two 16-bit writes
        WriteCartridge<uint16>(address + 0, value >> 16u);
        WriteCartridge<uint16>(address + 2, value >> 0u);
    } else if (std::is_same_v<T, uint8> && address == 0x210'0001) [[unlikely]] {
        // mednafen debug port
        if (value == '\n') {
            debugLog.debug("{}", m_debugOutput);
            m_debugOutput.clear();
        } else if (value != '\r') {
            m_debugOutput.push_back(value);
        }
    } else if (address >= 0x400'0000) {
        // HACK: emulate 32 Mbit backup RAM cartridge
        // TODO: move to Backup RAM Cartridge implementation
        if constexpr (std::is_same_v<T, uint8>) {
            m_externalBackupRAM.WriteByte(address, value);
        } else if constexpr (std::is_same_v<T, uint16>) {
            m_externalBackupRAM.WriteWord(address, value);
        } else if constexpr (std::is_same_v<T, uint32>) {
            m_externalBackupRAM.WriteLong(address, value);
        }
    } else {
        regsLog.trace("Unhandled {}-bit cartridge port write to {:05X} = {:X}", sizeof(T) * 8, address, value);
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
        ch.currXferCount = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 0);
        ch.currDstAddr = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 4);
        ch.currSrcAddr = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 8);
        ch.currIndirectSrc += 3 * sizeof(uint32);
        ch.endIndirect = bit::extract<31>(ch.currSrcAddr);
        ch.currSrcAddr &= 0x7FF'FFFF;
        ch.currDstAddr &= 0x7FF'FFFF;
        setXferIncs();

        dmaLog.trace("SCU DMA{}: Starting indirect transfer at {:08X} - {:06X} bytes from {:08X} (+{:02X}) to "
                     "{:08X} (+{:02X}){}",
                     level, ch.currIndirectSrc - 3 * sizeof(uint32), ch.currXferCount, ch.currSrcAddr,
                     ch.currSrcAddrInc, ch.currDstAddr, ch.currDstAddrInc, (ch.endIndirect ? " (final)" : ""));
        if (ch.currSrcAddr & 1) {
            dmaLog.debug("SCU DMA{}: Unaligned indirect transfer read from {:08X}", level, ch.currSrcAddr);
        }
        if (ch.currDstAddr & 1) {
            dmaLog.debug("SCU DMA{}: Unaligned indirect transfer write to {:08X}", level, ch.currDstAddr);
        }
    };

    while (ch.active) {
        const Bus srcBus = GetBus(ch.currSrcAddr);
        const Bus dstBus = GetBus(ch.currDstAddr);

        if (srcBus != dstBus && srcBus != Bus::None && dstBus != Bus::None) {
            uint32 value{};
            if (ch.currSrcAddr & 1) {
                // TODO: handle unaligned transfer
                dmaLog.trace("SCU DMA{}: Unaligned read from {:08X}", level, ch.currSrcAddr);
            }
            if (srcBus == Bus::BBus) {
                value = m_SH2.bus.Read<uint16>(ch.currSrcAddr) << 16u;
                dmaLog.trace("SCU DMA{}: B-Bus read from {:08X} -> {:04X}", level, ch.currSrcAddr, value >> 16u);
                ch.currSrcAddr += ch.currSrcAddrInc / 2u;
                value |= m_SH2.bus.Read<uint16>(ch.currSrcAddr) << 0u;
                dmaLog.trace("SCU DMA{}: B-Bus read from {:08X} -> {:04X}", level, ch.currSrcAddr, value & 0xFFFF);
                ch.currSrcAddr += ch.currSrcAddrInc / 2u;
            } else {
                value = m_SH2.bus.Read<uint16>(ch.currSrcAddr + 0) << 16u;
                value |= m_SH2.bus.Read<uint16>(ch.currSrcAddr + 2) << 0u;
                dmaLog.trace("SCU DMA{}: Read from {:08X} -> {:08X}", level, ch.currSrcAddr, value);
                ch.currSrcAddr += ch.currSrcAddrInc;
            }

            if (ch.currDstAddr & 1) {
                // TODO: handle unaligned transfer
                dmaLog.trace("SCU DMA{}: Unaligned write to {:08X}", level, ch.currDstAddr);
            }
            if (dstBus == Bus::BBus) {
                m_SH2.bus.Write<uint16>(ch.currDstAddr, value >> 16u);
                dmaLog.trace("SCU DMA{}: B-Bus write to {:08X} -> {:04X}", level, ch.currDstAddr, value >> 16u);
                ch.currDstAddr += ch.currDstAddrInc;
                m_SH2.bus.Write<uint16>(ch.currDstAddr, value >> 0u);
                dmaLog.trace("SCU DMA{}: B-Bus write to {:08X} -> {:04X}", level, ch.currDstAddr, value & 0xFFFF);
                ch.currDstAddr += ch.currDstAddrInc;
            } else {
                m_SH2.bus.Write<uint16>(ch.currDstAddr + 0, value >> 16u);
                m_SH2.bus.Write<uint16>(ch.currDstAddr + 2, value >> 0u);
                dmaLog.trace("SCU DMA{}: Write to {:08X} -> {:08X}", level, ch.currDstAddr, value);
                ch.currDstAddr += ch.currDstAddrInc;
            }

            ch.currSrcAddr &= 0x7FF'FFFF;
            ch.currDstAddr &= 0x7FF'FFFF;

            dmaLog.trace("SCU DMA{}: Addresses incremented to {:08X}, {:08X}", level, ch.currSrcAddr, ch.currDstAddr);
        } else if (srcBus == dstBus) {
            dmaLog.trace("SCU DMA{}: Invalid same-bus transfer; ignored", level);
        } else if (srcBus == Bus::None) {
            dmaLog.trace("SCU DMA{}: Invalid source bus; transfer ignored", level);
        } else if (dstBus == Bus::None) {
            dmaLog.trace("SCU DMA{}: Invalid destination bus; transfer ignored", level);
        }

        if (ch.currXferCount > sizeof(uint32)) {
            ch.currXferCount -= sizeof(uint32);
            dmaLog.trace("SCU DMA{}: Transfer remaining: {:X} bytes", level, ch.currXferCount);
            // break; // higher-level DMA transfers interrupt lower-level ones
        } else if (ch.indirect && !ch.endIndirect) {
            readIndirect();
            // break; // higher-level DMA transfers interrupt lower-level ones
        } else {
            dmaLog.trace("SCU DMA{}: Finished transfer", level);
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
            ch.currXferCount = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 0);
            ch.currDstAddr = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 4);
            ch.currSrcAddr = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 8);
            ch.currIndirectSrc += 3 * sizeof(uint32);
            ch.endIndirect = bit::extract<31>(ch.currSrcAddr);
            ch.currSrcAddr &= 0x7FF'FFFF;
            ch.currDstAddr &= 0x7FF'FFFF;
            setXferIncs();

            dmaLog.trace("SCU DMA{}: Starting indirect transfer at {:08X} - {:06X} bytes from {:08X} (+{:02X}) to "
                         "{:08X} (+{:02X}){}",
                         level, ch.currIndirectSrc - 3 * sizeof(uint32), ch.currXferCount, ch.currSrcAddr,
                         ch.currSrcAddrInc, ch.currDstAddr, ch.currDstAddrInc, (ch.endIndirect ? " (final)" : ""));
            if (ch.currSrcAddr & 1) {
                dmaLog.debug("SCU DMA{}: Unaligned indirect transfer read from {:08X}", level, ch.currSrcAddr);
            }
            if (ch.currDstAddr & 1) {
                dmaLog.debug("SCU DMA{}: Unaligned indirect transfer write to {:08X}", level, ch.currDstAddr);
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

                dmaLog.trace(
                    "SCU DMA{}: Starting direct transfer of {:06X} bytes from {:08X} (+{:02X}) to {:08X} (+{:02X})",
                    level, ch.currXferCount, ch.currSrcAddr, ch.currSrcAddrInc, ch.currDstAddr, ch.currDstAddrInc);
                if (ch.currSrcAddr & 1) {
                    dmaLog.debug("SCU DMA{}: Unaligned direct transfer read from {:08X}", level, ch.currSrcAddr);
                }
                if (ch.currDstAddr & 1) {
                    dmaLog.debug("SCU DMA{}: Unaligned direct transfer write to {:08X}", level, ch.currDstAddr);
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

void SCU::RunDSP(uint64 cycles) {
    // TODO: proper cycle counting

    RunDSPDMA(cycles);

    for (uint64 cy = 0; cy < cycles; cy++) {
        // Bail out if not executing
        if (!m_dspState.programExecuting && !m_dspState.programStep) {
            return;
        }

        // Bail out if paused
        if (m_dspState.programPaused) {
            return;
        }

        // Execute next command
        const uint32 command = m_dspState.programRAM[m_dspState.PC++];
        const uint32 cmdCategory = bit::extract<30, 31>(command);
        switch (cmdCategory) {
        case 0b00: DSPCmd_Operation(command); break;
        case 0b10: DSPCmd_LoadImm(command); break;
        case 0b11: DSPCmd_Special(command); break;
        }

        // Update PC
        if (m_dspState.jmpCounter > 0) {
            m_dspState.jmpCounter--;
            if (m_dspState.jmpCounter == 0) {
                m_dspState.PC = m_dspState.nextPC;
                m_dspState.nextPC = ~0;
            }
        }

        // Update CT0-3
        for (int i = 0; i < 4; i++) {
            if (m_dspState.incCT[i]) {
                m_dspState.CT[i]++;
                m_dspState.CT[i] &= 0x3F;
            }
        }
        m_dspState.incCT.fill(false);

        // Clear stepping flag to ensure the DSP only runs one command when stepping
        m_dspState.programStep = false;
    }
}

void SCU::RunDSPDMA(uint64 cycles) {
    // TODO: proper cycle counting

    // Bail out if DMA is not running
    if (!m_dspState.dmaRun) {
        return;
    }

    const bool toD0 = m_dspState.dmaToD0;
    uint32 addrD0 = toD0 ? m_dspState.dmaWriteAddr : m_dspState.dmaReadAddr;
    const Bus bus = GetBus(addrD0);
    if (bus == Bus::None) {
        m_dspState.dmaRun = false;
        return;
    }

    if (m_dspState.dmaToD0) {
        dspLog.trace("Running DSP DMA transfer: DSP -> {:08X} (+{:X}), {} longwords", addrD0, m_dspState.dmaAddrInc,
                     m_dspState.dmaCount);
    } else {
        dspLog.trace("Running DSP DMA transfer: {:08X} -> DSP (+{:X}), {} longwords", addrD0, m_dspState.dmaAddrInc,
                     m_dspState.dmaCount);
    }

    // Run transfer
    // TODO: should iterate through transfers based on cycle count
    for (uint32 i = 0; i < m_dspState.dmaCount; i++) {
        const uint32 ctIndex = toD0 ? m_dspState.dmaSrc : m_dspState.dmaDst;
        const bool useDataRAM = ctIndex <= 3; // else: use program RAM
        const uint32 ctAddr = useDataRAM ? m_dspState.CT[ctIndex] : 0;
        if (toD0) {
            // Data RAM -> D0
            const uint32 value = useDataRAM ? m_dspState.dataRAM[ctIndex][ctAddr] : m_dspState.programRAM[i & 0xFF];
            if (bus == Bus::ABus) {
                // A-Bus -> one 32-bit write
                m_SH2.bus.Write<uint32>(addrD0, value);
                addrD0 += m_dspState.dmaAddrInc;
            } else if (bus == Bus::BBus) {
                // B-Bus -> two 16-bit writes
                m_SH2.bus.Write<uint16>(addrD0 + 0, value >> 16u);
                m_SH2.bus.Write<uint16>(addrD0 + 2, value >> 0u);
                addrD0 += m_dspState.dmaAddrInc * 2;
            } else if (bus == Bus::WRAM) {
                // WRAM -> one 32-bit write
                util::WriteBE<uint32>(&m_SH2.bus.WRAMHigh[addrD0 & 0xFFFFC], value);
                addrD0 += m_dspState.dmaAddrInc;
            }
        } else {
            // D0 -> Data/Program RAM
            uint32 value = 0;
            if (bus == Bus::ABus) {
                // A-Bus -> one 32-bit read
                value = m_SH2.bus.Read<uint32>(addrD0);
                addrD0 += m_dspState.dmaAddrInc;
            } else if (bus == Bus::BBus) {
                // B-Bus -> two 16-bit reads
                value = m_SH2.bus.Read<uint16>(addrD0 + 0) << 16u;
                value |= m_SH2.bus.Read<uint16>(addrD0 + 2) << 0u;
                addrD0 += 4;
            } else if (bus == Bus::WRAM) {
                // WRAM -> one 32-bit read
                value = util::ReadBE<uint32>(&m_SH2.bus.WRAMHigh[addrD0 & 0xFFFFC]);
                addrD0 += m_dspState.dmaAddrInc;
            }
            if (useDataRAM) {
                m_dspState.dataRAM[ctIndex][ctAddr] = value;
            } else {
                m_dspState.programRAM[i & 0xFF] = value;
            }
        }
        addrD0 &= 0x7FF'FFFC;
        if (useDataRAM) {
            m_dspState.CT[ctIndex]++;
            m_dspState.CT[ctIndex] &= 0x3F;
        }
    }

    // Update RA0/WA0 if not holding address
    if (!m_dspState.dmaHold) {
        if (toD0) {
            m_dspState.dmaWriteAddr = addrD0;
        } else {
            m_dspState.dmaReadAddr = addrD0;
        }
    }

    m_dspState.dmaRun = false;
}

FORCE_INLINE uint32 SCU::DSPReadSource(uint8 index) {
    switch (index) {
    case 0b0000 ... 0b0111: {
        const uint8 ctIndex = bit::extract<0, 1>(index);
        const bool inc = bit::extract<2>(index);

        // Finish previous DMA transfer
        if (m_dspState.dmaRun) {
            RunDSPDMA(1); // TODO: cycles?
        }

        m_dspState.incCT[ctIndex] |= inc;
        const uint32 ctAddr = m_dspState.CT[ctIndex];
        return m_dspState.dataRAM[ctIndex][ctAddr];
    }
    case 0b1001: return m_dspState.ALU.L;
    case 0b1010: return m_dspState.ALU.u64 >> 16ull;
    default: return ~0;
    }
}

FORCE_INLINE void SCU::DSPWriteD1Bus(uint8 index, uint32 value) {
    // Finish previous DMA transfer
    if (m_dspState.dmaRun) {
        RunDSPDMA(1); // TODO: cycles?
    }

    switch (index) {
    case 0b0000 ... 0b0011: {
        const uint32 addr = m_dspState.CT[index];
        m_dspState.dataRAM[index][addr] = value;
        m_dspState.incCT[index] = true;
        break;
    }
    case 0b0100: m_dspState.RX = value; break;
    case 0b0101: m_dspState.P.s64 = static_cast<sint32>(value); break;
    case 0b0110: m_dspState.dmaReadAddr = (value << 2u) & 0x7FF'FFFC; break;
    case 0b0111: m_dspState.dmaWriteAddr = (value << 2u) & 0x7FF'FFFC; break;
    case 0b1010: m_dspState.loopCount = value & 0xFFF; break;
    case 0b1011: m_dspState.loopTop = value; break;
    case 0b1100 ... 0b1111:
        m_dspState.CT[index & 3] = value & 0x3F;
        m_dspState.incCT[index & 3] = false;
        break;
    }
}

void SCU::DSPWriteImm(uint8 index, uint32 value) {
    // Finish previous DMA transfer
    if (m_dspState.dmaRun) {
        RunDSPDMA(1); // TODO: cycles?
    }

    switch (index) {
    case 0b0000 ... 0b0011: {
        const uint32 addr = m_dspState.CT[index];
        m_dspState.dataRAM[index][addr] = value;
        m_dspState.incCT[index] = true;
        break;
    }
    case 0b0100: m_dspState.RX = value; break;
    case 0b0101: m_dspState.P.s64 = static_cast<sint32>(value); break;
    case 0b0110: m_dspState.dmaReadAddr = (value << 2u) & 0x7FF'FFFC; break;
    case 0b0111: m_dspState.dmaWriteAddr = (value << 2u) & 0x7FF'FFFC; break;
    case 0b1010: m_dspState.loopCount = value & 0xFFF; break;
    case 0b1100:
        m_dspState.loopTop = m_dspState.PC - 1;
        DSPDelayedJump(value);
        break;
    }
}

FORCE_INLINE bool SCU::DSPCondCheck(uint8 cond) const {
    // 000001: NZ  (Z=0)
    // 000010: NS  (S=0)
    // 000011: NZS (Z=0 && S=0)
    // 000100: NC  (C=0)
    // 001000: NT0 (T0=0)
    // 100001: Z   (Z=1)
    // 100010: S   (S=1)
    // 100011: ZS  (Z=1 || S=1)
    // 100100: C   (C=1)
    // 101000: T0  (T0=1)
    switch (cond) {
    case 0b000001: return !m_dspState.zero;
    case 0b000010: return !m_dspState.sign;
    case 0b000011: return !m_dspState.zero && !m_dspState.sign;
    case 0b000100: return !m_dspState.carry;
    case 0b001000: return !m_dspState.dmaRun;

    case 0b100001: return m_dspState.zero;
    case 0b100010: return m_dspState.sign;
    case 0b100011: return m_dspState.zero || m_dspState.sign;
    case 0b100100: return m_dspState.carry;
    case 0b101000: return m_dspState.dmaRun;
    }
    return false;
}

FORCE_INLINE void SCU::DSPDelayedJump(uint8 target) {
    m_dspState.nextPC = target & 0xFF;
    m_dspState.jmpCounter = 2;
}

FORCE_INLINE void SCU::DSPCmd_Operation(uint32 command) {
    auto setZS32 = [&](uint32 value) {
        m_dspState.zero = value == 0;
        m_dspState.sign = static_cast<sint32>(value) < 0;
    };

    auto setZS48 = [&](uint64 value) {
        value <<= 16ull;
        m_dspState.zero = value == 0;
        m_dspState.sign = static_cast<sint64>(value) < 0;
    };

    // ALU
    switch (bit::extract<26, 29>(command)) {
    case 0b0000: // NOP
        break;
    case 0b0001: // AND
        m_dspState.ALU.L = m_dspState.AC.L & m_dspState.P.L;
        setZS32(m_dspState.ALU.L);
        m_dspState.carry = 0;
        break;
    case 0b0010: // OR
        m_dspState.ALU.L = m_dspState.AC.L | m_dspState.P.L;
        setZS32(m_dspState.ALU.L);
        m_dspState.carry = 0;
        break;
    case 0b0011: // XOR
        m_dspState.ALU.L = m_dspState.AC.L ^ m_dspState.P.L;
        setZS32(m_dspState.ALU.L);
        m_dspState.carry = 0;
        break;
    case 0b0100: // ADD
    {
        const uint64 op1 = m_dspState.AC.L;
        const uint64 op2 = m_dspState.P.L;
        const uint64 result = op1 + op2;
        setZS32(result);
        m_dspState.carry = bit::extract<32>(result);
        m_dspState.overflow = bit::extract<31>((~(op1 ^ op2)) & (op1 ^ result));
        m_dspState.ALU.L = result;
        break;
    }
    case 0b0101: // SUB
    {
        const uint64 op1 = m_dspState.AC.L;
        const uint64 op2 = m_dspState.P.L;
        const uint64 result = op1 - op2;
        setZS32(result);
        m_dspState.carry = bit::extract<32>(result);
        m_dspState.overflow = bit::extract<31>((op1 ^ op2) & (op1 ^ result));
        m_dspState.ALU.L = result;
        break;
    }
    case 0b0110: // AD2
    {
        const uint64 op1 = m_dspState.AC.u64;
        const uint64 op2 = m_dspState.P.u64;
        const uint64 result = op1 + op2;
        setZS48(result);
        m_dspState.carry = bit::extract<48>(result);
        m_dspState.overflow = bit::extract<47>((~(op1 ^ op2)) & (op1 ^ result));
        m_dspState.ALU.s64 = result;
        break;
    }
    case 0b1000: // SR
        m_dspState.carry = bit::extract<0>(m_dspState.AC.L);
        m_dspState.ALU.L = static_cast<sint32>(m_dspState.AC.L) >> 1;
        setZS32(m_dspState.ALU.L);
        break;
    case 0b1001: // RR
        m_dspState.carry = bit::extract<0>(m_dspState.AC.L);
        m_dspState.ALU.L = std::rotr(m_dspState.AC.L, 1);
        setZS32(m_dspState.ALU.L);
        break;
    case 0b1010: // SL
        m_dspState.carry = bit::extract<31>(m_dspState.AC.L);
        m_dspState.ALU.L = m_dspState.AC.L << 1u;
        setZS32(m_dspState.ALU.L);
        break;
    case 0b1011: // RL
        m_dspState.carry = bit::extract<31>(m_dspState.AC.L);
        m_dspState.ALU.L = std::rotl(m_dspState.AC.L, 1);
        setZS32(m_dspState.ALU.L);
        break;
    case 0b1111: // RL8
        m_dspState.carry = bit::extract<24>(m_dspState.AC.L);
        m_dspState.ALU.L = std::rotl(m_dspState.AC.L, 8);
        setZS32(m_dspState.ALU.L);
        break;
    }

    // X-Bus
    //
    // X-Bus writes simultaneously to P and X in some cases:
    // bits
    // 25-23  executed operations
    //  000
    //  001
    //  010   MOV MUL,P
    //  011   MOV [s],P
    //  100               MOV [s],X
    //  101               MOV [s],X
    //  110   MOV MUL,P   MOV [s],X
    //  111   MOV [s],P   MOV [s],X
    if (bit::extract<23, 24>(command) == 0b10) {
        // MOV MUL,P
        m_dspState.P.s64 = static_cast<sint64>(m_dspState.RX) * static_cast<sint64>(m_dspState.RY);
    }
    if (bit::extract<23, 25>(command) >= 0b011) {
        const sint32 value = DSPReadSource(bit::extract<20, 22>(command));
        if (bit::extract<23, 24>(command) == 0b11) {
            // MOV [s],P
            m_dspState.P.s64 = static_cast<sint64>(value);
        }
        if (bit::extract<25>(command)) {
            // MOV [s],X
            m_dspState.RX = value;
        }
    }

    // Y-Bus
    //
    // Y-Bus writes simultaneously to A and Y in some cases:
    // bits
    // 19-17  executed operations
    // 000
    // 001    CLR A
    // 010    MOV ALU,A
    // 011    MOV [s],A
    // 100                MOV [s],Y
    // 101    CLR A       MOV [s],Y
    // 110    MOV ALU,A   MOV [s],Y
    // 111    MOV [s],A   MOV [s],Y
    if (bit::extract<17, 18>(command) == 0b01) {
        // CLR A
        m_dspState.AC.s64 = 0;
    } else if (bit::extract<17, 18>(command) == 0b10) {
        // MOV ALU,A
        m_dspState.AC.s64 = m_dspState.ALU.s64;
    }
    if (bit::extract<17, 19>(command) >= 0b11) {
        const sint32 value = DSPReadSource(bit::extract<14, 16>(command));
        if (bit::extract<17, 18>(command) == 0b11) {
            // MOV [s],A
            m_dspState.AC.s64 = static_cast<sint64>(value);
        }
        if (bit::extract<19>(command)) {
            // MOV [s],Y
            m_dspState.RY = value;
        }
    }

    // D1-Bus
    switch (bit::extract<12, 13>(command)) {
    case 0b01: // MOV SImm, [d]
    {
        const sint32 imm = bit::extract_signed<0, 7>(command);
        const uint8 dst = bit::extract<8, 11>(command);
        DSPWriteD1Bus(dst, imm);
        break;
    }
    case 0b11: // MOV [s], [d]
    {
        const uint8 src = bit::extract<0, 3>(command);
        const uint8 dst = bit::extract<8, 11>(command);
        const uint32 value = DSPReadSource(src);
        DSPWriteD1Bus(dst, value);
        break;
    }
    }
}

FORCE_INLINE void SCU::DSPCmd_LoadImm(uint32 command) {
    const uint32 dst = bit::extract<26, 29>(command);
    sint32 imm;
    if (bit::extract<25>(command)) {
        // Conditional transfer
        imm = bit::extract_signed<0, 18>(command);

        const uint8 cond = bit::extract<19, 24>(command);
        if (!DSPCondCheck(cond)) {
            return;
        }
    } else {
        // Unconditional transfer
        imm = bit::extract_signed<0, 24>(command);
    }

    DSPWriteImm(dst, imm);
}

FORCE_INLINE void SCU::DSPCmd_Special(uint32 command) {
    const uint32 cmdSubcategory = bit::extract<28, 29>(command);
    switch (cmdSubcategory) {
    case 0b00: DSPCmd_Special_DMA(command); break;
    case 0b01: DSPCmd_Special_Jump(command); break;
    case 0b10: DSPCmd_Special_LoopBottom(command); break;
    case 0b11: DSPCmd_Special_End(command); break;
    }
}

FORCE_INLINE void SCU::DSPCmd_Special_DMA(uint32 command) {
    // Finish previous DMA transfer
    if (m_dspState.dmaRun) {
        RunDSPDMA(1); // TODO: cycles?
    }

    m_dspState.dmaRun = true;
    m_dspState.dmaToD0 = bit::extract<12>(command);
    m_dspState.dmaHold = bit::extract<14>(command);

    // Get DMA transfer length
    if (bit::extract<13>(command)) {
        const uint8 ctIndex = bit::extract<0, 1>(command);
        const bool inc = bit::extract<2>(command);
        const uint32 ctAddr = m_dspState.CT[ctIndex];
        m_dspState.dmaCount = m_dspState.dataRAM[ctIndex][ctAddr];
        if (inc) {
            m_dspState.CT[ctIndex]++;
            m_dspState.CT[ctIndex] &= 0x3F;
        }
    } else {
        m_dspState.dmaCount = bit::extract<0, 7>(command);
    }

    // Get [RAM] source/destination register (CT) index and address increment
    const uint8 addrInc = bit::extract<15, 17>(command);
    if (m_dspState.dmaToD0) {
        // DMA [RAM],D0,SImm
        // DMA [RAM],D0,[s]
        // DMAH [RAM],D0,SImm
        // DMAH [RAM],D0,[s]
        m_dspState.dmaSrc = bit::extract<8, 9>(command);
        m_dspState.dmaAddrInc = (1 << addrInc) & ~1;
    } else if (bit::extract<10, 14>(command) == 0b00000) {
        // DMA D0,[RAM],SImm
        // Cannot write to program RAM, but can increment by up to 64 units
        m_dspState.dmaDst = bit::extract<8, 9>(command);
        m_dspState.dmaAddrInc = (1 << (addrInc & 0x2)) & ~1;
    } else {
        // DMA D0,[RAM],[s]
        // DMAH D0,[RAM],SImm
        // DMAH D0,[RAM],[s]
        m_dspState.dmaDst = bit::extract<8, 10>(command);
        m_dspState.dmaAddrInc = (1 << (addrInc & 0x2)) & ~1;
    }

    dspLog.trace("DSP DMA command: {:04X} @ {:02X}", command, m_dspState.PC);
}

FORCE_INLINE void SCU::DSPCmd_Special_Jump(uint32 command) {
    const uint32 cond = bit::extract<19, 24>(command);
    if (cond != 0 && !DSPCondCheck(cond)) {
        return;
    }

    const uint8 target = bit::extract<0, 7>(command);
    DSPDelayedJump(target);
}

FORCE_INLINE void SCU::DSPCmd_Special_LoopBottom(uint32 command) {
    if (m_dspState.loopCount != 0) {
        if (bit::extract<27>(command)) {
            // LPS
            DSPDelayedJump(m_dspState.PC - 1);
        } else {
            // BTM
            DSPDelayedJump(m_dspState.loopTop);
        }
    }
    m_dspState.loopCount--;
    m_dspState.loopCount &= 0xFFF;
}

FORCE_INLINE void SCU::DSPCmd_Special_End(uint32 command) {
    const bool setEndIntr = bit::extract<27>(command);
    m_dspState.programExecuting = false;
    m_dspState.programEnded = true;
    if (setEndIntr) {
        TriggerDSPEnd();
    }
}

template <bool debug>
FORCE_INLINE void SCU::TickTimer1() {
    if (m_timer1Enable && (!m_timer1Mode || m_timer0Counter == m_timer0Compare)) {
        TriggerTimer1();
    }
}

template <mem_primitive T>
FORCE_INLINE T SCU::ReadReg(uint32 address) {
    if constexpr (std::is_same_v<T, uint8>) {
        const uint32 value = ReadReg<uint32>(address & ~3u);
        return value >> (((address & 3u) ^ 3u) * 8u);
    } else if constexpr (std::is_same_v<T, uint16>) {
        const uint32 value = ReadReg<uint32>(address & ~3u);
        return value >> ((((address >> 1u) & 1u) ^ 1u) * 16u);
    }

    switch (address) {
    case 0x00: // Level 0 DMA Read Address
    case 0x20: // Level 1 DMA Read Address
    case 0x40: // Level 2 DMA Read Address
        if constexpr (std::is_same_v<T, uint32>) {
            auto &ch = m_dmaChannels[address >> 5u];
            return ch.srcAddr;
        } else {
            return 0;
        }
    case 0x04: // Level 0 DMA Write Address
    case 0x24: // Level 1 DMA Write Address
    case 0x44: // Level 2 DMA Write Address
        if constexpr (std::is_same_v<T, uint32>) {
            auto &ch = m_dmaChannels[address >> 5u];
            return ch.dstAddr;
        } else {
            return 0;
        }
    case 0x08: // Level 0 DMA Transfer Number
    case 0x28: // Level 1 DMA Transfer Number
    case 0x48: // Level 2 DMA Transfer Number
        if constexpr (std::is_same_v<T, uint32>) {
            auto &ch = m_dmaChannels[address >> 5u];
            return ch.xferCount;
        } else {
            return 0;
        }
    case 0x0C: // Level 0 DMA Increment (write-only)
    case 0x2C: // Level 1 DMA Increment (write-only)
    case 0x4C: // Level 2 DMA Increment (write-only)
        return 0;
    case 0x10: // Level 0 DMA Enable (write-only)
    case 0x30: // Level 1 DMA Enable (write-only)
    case 0x50: // Level 2 DMA Enable (write-only)
        return 0;
    case 0x14: // Level 0 DMA Mode (write-only)
    case 0x34: // Level 1 DMA Mode (write-only)
    case 0x54: // Level 2 DMA Mode (write-only)
        return 0;

    case 0x60: // DMA Force Stop (write-only)
        return 0;
    case 0x7C: // DMA Status
    {
        uint32 value = 0;
        // bit::deposit_into<0>(value, m_dspState.dmaRun); // TODO: is this correct?
        // bit::deposit_into<1>(value, m_dspState.dmaStandby?);
        bit::deposit_into<4>(value, m_dmaChannels[0].active);
        // TODO: bit 5: DMA0 standby
        bit::deposit_into<8>(value, m_dmaChannels[1].active);
        // TODO: bit 9: DMA1 standby
        bit::deposit_into<12>(value, m_dmaChannels[2].active);
        // TODO: bit 13: DMA2 standby
        bit::deposit_into<16>(value, m_dmaChannels[0].active && (m_dmaChannels[1].active || m_dmaChannels[2].active));
        bit::deposit_into<17>(value, m_dmaChannels[1].active && m_dmaChannels[2].active);
        // TODO: bit 20: DMA accessing A-Bus
        // TODO: bit 21: DMA accessing B-Bus
        // TODO: bit 22: DMA accessing DSP-Bus
        return value;
    }

    case 0x80: // DSP Program Control Port
        if constexpr (std::is_same_v<T, uint32>) {
            uint32 value = 0;
            bit::deposit_into<0, 7>(value, m_dspState.PC);
            bit::deposit_into<16>(value, m_dspState.programExecuting);
            bit::deposit_into<18>(value, m_dspState.programEnded);
            bit::deposit_into<19>(value, m_dspState.overflow);
            bit::deposit_into<20>(value, m_dspState.carry);
            bit::deposit_into<21>(value, m_dspState.zero);
            bit::deposit_into<22>(value, m_dspState.sign);
            bit::deposit_into<23>(value, m_dspState.dmaRun);
            return value;
        } else {
            return 0;
        }
    case 0x84: // DSP Program RAM Data Port (write-only)
        return 0;
    case 0x88: // DSP Data RAM Address Port (write-only)
        return 0;
    case 0x8C: // DSP Data RAM Data Port
        if constexpr (std::is_same_v<T, uint32>) {
            return m_dspState.ReadData();
        } else {
            return 0;
        }

    case 0x90: // Timer 0 Compare (write-only)
        return 0;
    case 0x94: // Timer 1 Set Data (write-only)
        return 0;
    case 0x98: // Timer 1 Mode (write-only)
        return 0;

    case 0xA0: // Interrupt Mask
        return m_intrMask.u32;
    case 0xA4: // Interrupt Status
        return m_intrStatus.u32;
    case 0xA8: // A-Bus Interrupt Acknowledge
        return m_abusIntrAck;

    case 0xB0: // A-Bus Set (part 1) (write-only)
        return 0;
    case 0xB4: // A-Bus Set (part 2) (write-only)
        return 0;
    case 0xB8: // A-Bus Refresh (write-only)
        return 0;

    case 0xC4: // SCU SDRAM Select
        return m_WRAMSizeSelect;
    case 0xC8: // SCU Version
        return 0x4;

    default: //
        regsLog.debug("unhandled {}-bit SCU register read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }
}

FORCE_INLINE void SCU::WriteRegByte(uint32 address, uint8 value) {
    // TODO: implement registers as needed

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
        UpdateInterruptLevel(false);
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

    default: regsLog.debug("unhandled 8-bit SCU register write to {:02X} = {:X}", address, value); break;
    }
}

FORCE_INLINE void SCU::WriteRegWord(uint32 address, uint16 value) {
    // TODO: implement registers as needed

    regsLog.debug("unhandled 16-bit SCU register write to {:02X} = {:X}", address, value);
}

FORCE_INLINE void SCU::WriteRegLong(uint32 address, uint32 value) {
    // TODO: handle 8-bit and 16-bit register writes if needed

    switch (address) {
    case 0x00: // Level 0 DMA Read Address
    case 0x20: // Level 1 DMA Read Address
    case 0x40: // Level 2 DMA Read Address
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.srcAddr = bit::extract<0, 26>(value);
    } break;
    case 0x04: // Level 0 DMA Write Address
    case 0x24: // Level 1 DMA Write Address
    case 0x44: // Level 2 DMA Write Address
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.dstAddr = bit::extract<0, 26>(value);
    } break;
    case 0x08: // Level 0 DMA Transfer Number
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.xferCount = bit::extract<0, 19>(value);
    } break;
    case 0x28: // Level 1 DMA Transfer Number
    case 0x48: // Level 2 DMA Transfer Number
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.xferCount = bit::extract<0, 11>(value);
    } break;
    case 0x0C: // Level 0 DMA Increment
    case 0x2C: // Level 1 DMA Increment
    case 0x4C: // Level 2 DMA Increment
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.srcAddrInc = bit::extract<8>(value) * 4u;
        ch.dstAddrInc = (1u << bit::extract<0, 2>(value)) & ~1u;
    } break;
    case 0x10: // Level 0 DMA Enable
    case 0x30: // Level 1 DMA Enable
    case 0x50: // Level 2 DMA Enable
    {
        const uint32 index = address >> 5u;
        auto &ch = m_dmaChannels[index];
        ch.enabled = bit::extract<8>(value);
        if (ch.enabled) {
            dmaLog.trace("DMA{} enabled - {:08X} (+{:02X}) -> {:08X} (+{:02X})", index, ch.srcAddr, ch.srcAddrInc,
                         ch.dstAddr, ch.dstAddrInc);
        }
        if (ch.enabled && ch.trigger == DMATrigger::Immediate && bit::extract<0>(value)) {
            if (ch.active) {
                dmaLog.trace("DMA{} triggering immediate transfer while another transfer is in progress", index);
                // Finish previous transfer
                RunDMA();
            }
            ch.start = true;
            RecalcDMAChannel();
            RunDMA(); // HACK: run immediate DMA transfers immediately and instantly
        }
    } break;
    case 0x14: // Level 0 DMA Mode
    case 0x34: // Level 1 DMA Mode
    case 0x54: // Level 2 DMA Mode
    {
        auto &ch = m_dmaChannels[address >> 5u];
        ch.indirect = bit::extract<24>(value);
        ch.updateSrcAddr = bit::extract<16>(value);
        ch.updateDstAddr = bit::extract<8>(value);
        ch.trigger = static_cast<DMATrigger>(bit::extract<0, 2>(value));
    } break;

    case 0x60: // DMA Force Stop
        if (bit::extract<0>(value)) {
            for (auto &ch : m_dmaChannels) {
                ch.active = false;
            }
            m_activeDMAChannelLevel = m_dmaChannels.size();
        }
        break;
    case 0x7C: // DMA Status (read-only)
        break;

    case 0x80: // DSP Program Control Port
        if (bit::extract<15>(value)) {
            m_dspState.PC = bit::extract<0, 7>(value);
        }
        if (bit::extract<25>(value)) {
            m_dspState.programPaused = true;
        } else if (bit::extract<26>(value)) {
            m_dspState.programPaused = false;
        } else if (!m_dspState.programExecuting) {
            m_dspState.programExecuting = bit::extract<16>(value);
            m_dspState.programStep = bit::extract<17>(value);
            m_dspState.programEnded = false;
        }
        break;
    case 0x84: // DSP Program RAM Data Port
        m_dspState.WriteProgram(value);
        break;
    case 0x88: // DSP Data RAM Address Port
        m_dspState.dataAddress = bit::extract<0, 7>(value);
        break;
    case 0x8C: // DSP Data RAM Data Port
        m_dspState.WriteData(value);
        break;

    case 0x90: // Timer 0 Compare
        m_timer0Compare = bit::extract<0, 9>(value);
        break;
    case 0x94: // Timer 1 Set Data
        m_timer1Reload = bit::extract<0, 8>(value) << 2u;
        break;
    case 0x98: // Timer 1 Mode
        m_timer1Enable = bit::extract<0>(value);
        m_timer1Mode = bit::extract<8>(value);
        break;

    case 0xA0: // Interrupt Mask
        m_intrMask.u32 = value & 0x0000BFFF;
        break;
    case 0xA4: // Interrupt Status
        m_intrStatus.u32 &= value;
        break;
    case 0xA8: // A-Bus Interrupt Acknowledge
        m_abusIntrAck = bit::extract<0>(value);
        UpdateInterruptLevel(false);
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
        m_WRAMSizeSelect = value;
        break;
    case 0xC8: // SCU Version (read-only)
        break;

    default: regsLog.debug("unhandled 32-bit SCU register write to {:02X} = {:X}", address, value); break;
    }
}

void SCU::UpdateInterruptLevel(bool acknowledge) {
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
        rootLog.trace("Intr states:  {:04X} {:04X}", m_intrStatus.internal, m_intrStatus.external);
        rootLog.trace("Intr masks:   {:04X} {:04X} {}", (uint16)m_intrMask.internal, m_intrMask.ABus_ExtIntrs * 0xFFFF,
                      m_abusIntrAck);
        rootLog.trace("Intr bits:    {:04X} {:04X}", internalBits, externalBits);
        rootLog.trace("Intr indices: {:X} {:X}", internalIndex, externalIndex);
        rootLog.trace("Intr levels:  {:X} {:X}", internalLevel, externalLevel);

        if (acknowledge) {
            if (internalLevel >= externalLevel) {
                m_intrStatus.internal &= ~(1u << internalIndex);
                rootLog.trace("Acknowledging internal interrupt {:X}", internalIndex);
            } else {
                m_intrStatus.external &= ~(1u << externalIndex);
                rootLog.trace("Acknowledging external interrupt {:X}", externalIndex);
            }
            UpdateInterruptLevel(false);
        } else {
            if (internalLevel >= externalLevel) {
                m_SH2.master.SetExternalInterrupt(internalLevel, internalIndex + 0x40);
                rootLog.trace("Raising internal interrupt {:X}", internalIndex);

                // Also send VBlank IN and HBlank IN to slave SH2 if it is enabled
                if (internalIndex == 0) {
                    m_SH2.slave.SetExternalInterrupt(2, 0x43);
                } else if (internalIndex == 2) {
                    m_SH2.slave.SetExternalInterrupt(1, 0x41);
                } else {
                    m_SH2.slave.SetExternalInterrupt(0, 0);
                }
            } else if (m_abusIntrAck) {
                rootLog.trace("Raising external interrupt {:X}", externalIndex);
                m_abusIntrAck = false;
                m_SH2.master.SetExternalInterrupt(externalLevel, externalIndex + 0x50);
                m_SH2.slave.SetExternalInterrupt(0, 0);
            } else {
                rootLog.trace("Lowering interrupt signal");
                m_SH2.master.SetExternalInterrupt(0, 0);
                m_SH2.slave.SetExternalInterrupt(0, 0);
            }
        }
    } else {
        m_SH2.master.SetExternalInterrupt(0, 0);
        m_SH2.slave.SetExternalInterrupt(0, 0);
    }
}

} // namespace satemu::scu

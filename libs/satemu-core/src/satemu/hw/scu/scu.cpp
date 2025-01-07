#include <satemu/hw/scu/scu.hpp>

#include <satemu/hw/sh2/sh2_block.hpp>

#include <bit>

namespace satemu::scu {

SCU::SCU(vdp::VDP &vdp, scsp::SCSP &scsp, cdblock::CDBlock &cdblock, sh2::SH2Block &sh2)
    : m_VDP(vdp)
    , m_SCSP(scsp)
    , m_CDBlock(cdblock)
    , m_SH2(sh2) {
    Reset(true);
}

void SCU::Reset(bool hard) {
    m_intrMask.u32 = 0;
    m_intrStatus.u32 = 0;

    for (auto &ch : m_dmaChannels) {
        ch.Reset();
    }

    m_dspState.Reset();

    m_timer0Counter = 0;
    m_timer0Compare = 0;

    m_timer1Counter = 0;
    m_timer1Reload = 0;
    m_timer1Enable = false;
    m_timer1Mode = false;

    m_WRAMSizeSelect = false;
}

void SCU::Advance(uint64 cycles) {
    RunDMA(cycles);

    RunDSP(cycles);

    if (cycles >= m_timer1Counter) {
        m_timer1Counter = 0;
        if (m_timer1Enable && (!m_timer1Mode || m_timer0Counter == m_timer0Compare)) {
            TriggerTimer1();
        }
    } else {
        m_timer1Counter -= cycles;
    }
}

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
    m_timer1Counter = m_timer1Reload;
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
    TriggerDMATransfer(DMATrigger::SoundRequest);
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
    TriggerDMATransfer(DMATrigger::SpriteDrawEnd);
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

void SCU::RunDMA(uint64 cycles) {
    // TODO: proper cycle counting
    for (int level = 2; level >= 0; level--) {
        auto &ch = m_dmaChannels[level];

        if (!ch.enabled) {
            continue;
        }

        auto readIndirect = [&] {
            ch.currXferCount = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 0);
            ch.currDstAddr = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 4);
            ch.currSrcAddr = m_SH2.bus.Read<uint32>(ch.currIndirectSrc + 8);
            ch.currIndirectSrc += 3 * sizeof(uint32);
            ch.endIndirect = bit::extract<31>(ch.currSrcAddr);
            ch.currSrcAddr &= 0x7FFF'FFFF;
            if (level == 0) {
                ch.currXferCount = bit::extract<0, 19>(ch.currXferCount);
            } else {
                ch.currXferCount = bit::extract<0, 11>(ch.currXferCount);
            }
            fmt::println("SCU DMA{}: Starting indirect transfer at {:08X} - from {:08X} to {:08X} - {:05X} bytes{}",
                         level, ch.currIndirectSrc - 3 * sizeof(uint32), ch.currSrcAddr, ch.currDstAddr,
                         ch.currXferCount, (ch.endIndirect ? " (final)" : ""));
        };

        if (ch.start && !ch.active) {
            ch.start = false;
            ch.active = true;
            if (ch.indirect) {
                ch.currIndirectSrc = ch.dstAddr;
                readIndirect();
            } else {
                fmt::println("SCU DMA{}: Starting direct transfer from {:08X} to {:08X} - {:05X} bytes", level,
                             ch.srcAddr, ch.dstAddr, ch.xferCount);
                ch.currSrcAddr = ch.srcAddr;
                ch.currDstAddr = ch.dstAddr;
                ch.currXferCount = ch.xferCount;
            }
        }

        if (ch.active) {
            const uint32 value = m_SH2.bus.Read<uint32>(ch.currSrcAddr & 0x7FF'FFFF);
            m_SH2.bus.Write<uint32>(ch.currDstAddr & 0x7FF'FFFF, value);

            if (util::AddressInRange<0x5A0'0000, 0x5FF'FFFF>(ch.currSrcAddr & 0x7FF'FFFF)) {
                ch.currSrcAddr += ch.srcAddrInc * 2;
            } else {
                ch.currSrcAddr += ch.srcAddrInc;
            }
            if (util::AddressInRange<0x5A0'0000, 0x5FF'FFFF>(ch.currDstAddr & 0x7FF'FFFF)) {
                ch.currDstAddr += ch.dstAddrInc * 2;
            } else {
                ch.currDstAddr += ch.dstAddrInc;
            }

            if (ch.currXferCount > sizeof(uint32)) {
                ch.currXferCount -= sizeof(uint32);
                break; // higher-level DMA transfers interrupt lower-level ones
            } else if (ch.indirect && !ch.endIndirect) {
                readIndirect();
                break; // higher-level DMA transfers interrupt lower-level ones
            } else {
                // fmt::println("SCU DMA{}: Finished transfer", level);
                ch.active = false;
                ch.currXferCount = 0;
                if (ch.updateSrcAddr) {
                    ch.srcAddr = ch.currSrcAddr;
                }
                if (ch.updateDstAddr) {
                    ch.dstAddr = ch.currDstAddr;
                }
                TriggerDMAEnd(level);
            }
        }
    }
}

void SCU::TriggerDMATransfer(DMATrigger trigger) {
    for (auto &ch : m_dmaChannels) {
        if (ch.enabled && !ch.active && ch.trigger == trigger) {
            ch.start = true;
        }
    }
}

void SCU::RunDSP(uint64 cycles) {
    // TODO: proper cycle counting

    RunDSPDMA(cycles);

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

void SCU::RunDSPDMA(uint64 cycles) {
    // TODO: proper cycle counting

    // Bail out if DMA is not running
    if (!m_dspState.dmaRun) {
        return;
    }

    enum class Bus {
        ABus,
        BBus,
        WRAM,
        None,
    };

    const bool toD0 = m_dspState.dmaToD0;
    uint32 addrD0 = toD0 ? m_dspState.dmaWriteAddr : m_dspState.dmaReadAddr;
    const Bus bus = util::AddressInRange<0x200'0000, 0x58F'FFFF>(addrD0)   ? Bus::ABus
                    : util::AddressInRange<0x5A0'0000, 0x5FF'FFFF>(addrD0) ? Bus::BBus
                    : addrD0 >= 0x600'0000                                 ? Bus::WRAM
                                                                           : Bus::None;

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
                WriteABus<uint32>(addrD0, value);
                addrD0 += m_dspState.dmaAddrInc;
            } else if (bus == Bus::BBus) {
                // B-Bus -> two 16-bit writes
                WriteBBus<uint32>(addrD0, value);
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
                value = ReadABus<uint32>(addrD0);
                addrD0 += m_dspState.dmaAddrInc;
            } else if (bus == Bus::BBus) {
                // B-Bus -> two 16-bit reads
                value = ReadBBus<uint32>(addrD0);
                addrD0 += 4;
            } else if (bus == Bus::WRAM) {
                // WRAM -> one 32-bit read
                value = util::ReadBE<uint32>(&m_SH2.bus.WRAMHigh[addrD0 & 0xFFFFF]);
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
    case 0b1010: m_dspState.loopCount = value; break;
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
    case 0b1010: m_dspState.loopCount = value; break;
    case 0b1100:
        m_dspState.loopTop = m_dspState.PC;
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
    m_dspState.nextPC = target;
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
        const sint64 op1 = m_dspState.AC.s64;
        const sint64 op2 = m_dspState.P.s64;
        const sint64 result = op1 + op2;
        setZS48(result);
        m_dspState.carry = bit::extract<48>(result);
        m_dspState.overflow = bit::extract<47>((~(op1 ^ op2)) & (op1 ^ result));
        m_dspState.ALU.u64 = result;
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
        m_dspState.AC.u64 = 0;
    } else if (bit::extract<17, 18>(command) == 0b10) {
        // MOV ALU,A
        m_dspState.AC.u64 = m_dspState.ALU.u64;
    }
    if (bit::extract<17, 19>(command) >= 0b11) {
        const sint32 value = DSPReadSource(bit::extract<14, 16>(command));
        if (bit::extract<17, 18>(command) == 0b11) {
            // MOV [s],A
            m_dspState.AC.u64 = static_cast<sint64>(value);
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
        const uint32 dst = bit::extract<8, 11>(command);
        DSPWriteD1Bus(dst, imm);
        break;
    }
    case 0b11: // MOV [s], [d]
    {
        const uint8 src = bit::extract<0, 3>(command);
        const uint32 dst = bit::extract<8, 11>(command);
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
        m_dspState.loopCount--;
        if (bit::extract<27>(command)) {
            // LPS
            DSPDelayedJump(m_dspState.PC);
        } else {
            // BTM
            DSPDelayedJump(m_dspState.loopTop);
        }
    }
}

FORCE_INLINE void SCU::DSPCmd_Special_End(uint32 command) {
    const bool setEndIntr = bit::extract<27>(command);
    m_dspState.programExecuting = false;
    m_dspState.programEnded = true;
    if (setEndIntr) {
        TriggerDSPEnd();
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

    static constexpr uint32 kBaseIntrLevels[] = {0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, //
                                                 0x8, 0x6, 0x6, 0x5, 0x3, 0x2, 0x0, 0x0, //
                                                 0x0};
    static constexpr uint32 kABusIntrLevels[] = {0x7, 0x7, 0x7, 0x7, 0x4, 0x4, 0x4, 0x4, //
                                                 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, //
                                                 0x0};
    const uint32 intrBits = m_intrStatus.u32 & ~m_intrMask.u16;
    if (intrBits != 0) {
        const uint16 intrIndexBase = std::countr_zero<uint16>(intrBits >> 0u);
        const uint16 intrIndexABus = std::countr_zero<uint16>(intrBits >> 16u);

        const uint8 intrLevelBase = kBaseIntrLevels[intrIndexBase];
        const uint8 intrLevelABus = kABusIntrLevels[intrIndexABus];

        if (acknowledge) {
            if (intrLevelBase >= intrLevelABus) {
                m_intrStatus.u32 &= ~(1u << intrIndexBase);
            } else {
                m_intrStatus.u32 &= ~(1u << (intrIndexABus + 16u));
            }
            m_SH2.master.SetExternalInterrupt(0, 0);
        } else {
            if (intrLevelBase >= intrLevelABus) {
                m_SH2.master.SetExternalInterrupt(intrLevelBase, intrIndexBase + 0x40);
            } else {
                m_SH2.master.SetExternalInterrupt(intrLevelABus, intrIndexABus + 0x50);
            }
        }
    } else {
        m_SH2.master.SetExternalInterrupt(0, 0);
    }
}

} // namespace satemu::scu

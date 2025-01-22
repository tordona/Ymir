#include <satemu/hw/sh2/sh2.hpp>

#include <satemu/hw/sh2/sh2_bus.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>
#include <satemu/util/unreachable.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>

namespace satemu::sh2 {

inline constexpr dbg::Category<sh2DebugLevel> MSH2{"SH2-M"};
inline constexpr dbg::Category<sh2DebugLevel> SSH2{"SH2-S"};

static constexpr const dbg::Category<sh2DebugLevel> &Logger(bool master) {
    return master ? MSH2 : SSH2;
}

RealSH2Tracer::RealSH2Tracer(bool master)
    : m_master(master) {
    Reset();
}

void RealSH2Tracer::Reset() {
    m_entries.clear();
}

void RealSH2Tracer::Dump() {
    const auto &log = Logger(m_master);

    auto buf = fmt::memory_buffer();
    auto inserter = std::back_inserter(buf);

    auto formatRegs = [&](const SH2Regs &regs) {
        fmt::format_to(inserter, "  R0-15:");
        for (int i = 0; i < 16; i++) {
            fmt::format_to(inserter, " {:08X}", regs.R[i]);
        }
        fmt::format_to(inserter, " PC={:08X}", regs.PC);
        fmt::format_to(inserter, " PR={:08X}", regs.PR);
        fmt::format_to(inserter, " SR={:08X}", regs.SR);
        fmt::format_to(inserter, " GBR={:08X}", regs.GBR);
        fmt::format_to(inserter, " VBR={:08X}", regs.VBR);
        fmt::format_to(inserter, " MAC={:016X}", regs.MAC);
    };

    log.debug("Stack trace:");
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); it++) {
        formatRegs(it->regs);
        switch (it->type) {
        case SH2BranchType::JSR: fmt::format_to(inserter, " JSR"); break;
        case SH2BranchType::BSR: fmt::format_to(inserter, " BSR"); break;
        case SH2BranchType::TRAPA: fmt::format_to(inserter, " TRAPA"); break;
        case SH2BranchType::Exception: fmt::format_to(inserter, " Exception vector {}", it->vec); break;
        case SH2BranchType::UserCapture: fmt::format_to(inserter, " User capture"); break;
        }
        log.debug("{}", fmt::to_string(buf));
        buf.clear();
    }

    log.debug("Execution backtrace:");
    std::size_t execTracePos = m_execTraceHead + m_execTrace.size() - m_execTraceCount;
    if (execTracePos >= m_execTrace.size()) {
        execTracePos -= m_execTrace.size();
    }
    for (std::size_t i = 0; i < m_execTraceCount; i++) {
        formatRegs(m_execTrace[execTracePos]);
        log.debug("{}", fmt::to_string(buf));
        buf.clear();
        execTracePos = (execTracePos + 1) % m_execTrace.size();
    }
}

void RealSH2Tracer::ExecTrace(SH2Regs regs) {
    m_execTrace[m_execTraceHead] = regs;
    m_execTraceHead = (m_execTraceHead + 1) % m_execTrace.size();
    if (m_execTraceCount < m_execTrace.size()) {
        m_execTraceCount++;
    }
}

void RealSH2Tracer::JSR(SH2Regs regs) {
    m_entries.push_back({SH2BranchType::JSR, regs});
}

void RealSH2Tracer::BSR(SH2Regs regs) {
    m_entries.push_back({SH2BranchType::BSR, regs});
}

void RealSH2Tracer::TRAPA(SH2Regs regs) {
    m_entries.push_back({SH2BranchType::TRAPA, regs});
}

void RealSH2Tracer::Exception(SH2Regs regs, uint8 vec) {
    m_entries.push_back({SH2BranchType::Exception, regs});
}

void RealSH2Tracer::UserCapture(SH2Regs regs) {
    m_entries.push_back({SH2BranchType::UserCapture, regs});
}

void RealSH2Tracer::RTE(SH2Regs regs) {
    // auto &entry = m_entries.back();
    // TODO: check if it was an exception?
    if (!m_entries.empty()) {
        m_entries.pop_back();
    }
}

void RealSH2Tracer::RTS(SH2Regs regs) {
    // auto &entry = m_entries.back();
    // TODO: check if it was a BSR or JSR?
    if (!m_entries.empty()) {
        m_entries.pop_back();
    }
}

SH2::SH2(SH2Bus &bus, bool master)
    : m_log(Logger(master))
    , m_bus(bus)
    , m_tracer(master) {
    BCR1.MASTER = !master;
    Reset(true);
}

void SH2::Reset(bool hard) {
    // Initial values:
    // - R0-R14 = undefined
    // - R15 = ReadLong(0x00000004)  [NOTE: ignores VBR]

    // - SR = bits I3-I0 set, reserved bits clear, the rest is undefined
    // - GBR = undefined
    // - VBR = 0x00000000

    // - MACH, MACL = undefined
    // - PR = undefined
    // - PC = ReadLong(0x00000000)  [NOTE: ignores VBR]

    R.fill(0);
    PR = 0;

    SR.u32 = 0;
    SR.I0 = SR.I1 = SR.I2 = SR.I3 = 1;
    GBR = 0;
    VBR = 0x00000000;

    MAC.u64 = 0;

    PC = MemReadLong(0x00000000);
    R[15] = MemReadLong(0x00000004);

    // On-chip registers
    ICR.u16 = 0x0000;
    BCR1.u15 = 0x03F0;
    BCR2.u16 = 0x00FC;
    WCR.u16 = 0xAAFF;
    MCR.u16 = 0x0000;

    FRT.Reset();

    for (auto &ch : dmaChannels) {
        ch.Reset();
    }
    DMAOR.u32 = 0x00000000;
    m_activeDMAChannel = dmaChannels.size();

    cacheEntries.fill({});
    WriteCCR(0x00);

    DVSR = 0x0;  // undefined initial value
    DVDNT = 0x0; // undefined initial value
    DVCR.u32 = 0x00000000;
    DVDNTH = 0x0;  // undefined initial value
    DVDNTL = 0x0;  // undefined initial value
    DVDNTUH = 0x0; // undefined initial value
    DVDNTUL = 0x0; // undefined initial value

    m_intrLevels.fill(0);
    m_intrVectors.fill(0);

    SetInterruptLevel(InterruptSource::IRL, 1);
    SetInterruptVector(InterruptSource::IRL, 0x40);

    SetInterruptLevel(InterruptSource::UserBreak, 15);
    SetInterruptVector(InterruptSource::UserBreak, 0x0C);

    SetInterruptLevel(InterruptSource::NMI, 16);
    SetInterruptVector(InterruptSource::NMI, 0x0B);

    m_NMI = false;

    m_pendingInterrupt.source = InterruptSource::None;
    m_pendingInterrupt.level = 0;

    m_externalIntrVector = 0;

    m_delaySlot = false;
    m_delaySlotTarget = 0;

    m_tracer.Reset();
}

FLATTEN void SH2::Advance(uint64 cycles) {
    AdvanceDMAC(cycles);
    AdvanceFRT(cycles);

    // TODO: proper cycle counting
    for (uint64 cy = 0; cy < cycles; cy++) {
        /*auto bit = [](bool value, std::string_view bit) { return value ? fmt::format(" {}", bit) : ""; };

        dbg_println(" R0 = {:08X}   R4 = {:08X}   R8 = {:08X}  R12 = {:08X}", R[0], R[4], R[8], R[12]);
        dbg_println(" R1 = {:08X}   R5 = {:08X}   R9 = {:08X}  R13 = {:08X}", R[1], R[5], R[9], R[13]);
        dbg_println(" R2 = {:08X}   R6 = {:08X}  R10 = {:08X}  R14 = {:08X}", R[2], R[6], R[10], R[14]);
        dbg_println(" R3 = {:08X}   R7 = {:08X}  R11 = {:08X}  R15 = {:08X}", R[3], R[7], R[11], R[15]);
        dbg_println("GBR = {:08X}  VBR = {:08X}  MAC = {:08X}.{:08X}", GBR, VBR, MAC.H, MAC.L);
        dbg_println(" PC = {:08X}   PR = {:08X}   SR = {:08X} {}{}{}{}{}{}{}{}", PC, PR, SR.u32, bit(SR.M, "M"),
                    bit(SR.Q, "Q"), bit(SR.I3, "I3"), bit(SR.I2, "I2"), bit(SR.I1, "I1"), bit(SR.I0, "I0"), bit(SR.S,
        "S"), bit(SR.T, "T"));*/

        // TODO: choose between interpreter (cached or uncached) and JIT recompiler
        m_tracer.ExecTrace({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
        Execute();

        // dump stack trace on SYS_EXECDMP
        if ((PC & 0x7FFFFFF) == 0x186C) {
            m_log.debug("SYS_EXECDMP triggered");
            m_tracer.UserCapture({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
            m_tracer.Dump();
            m_tracer.Reset();
        }

        // dbg_println("");
    }
}

void SH2::SetExternalInterrupt(uint8 level, uint8 vector) {
    assert(level < 16);

    const InterruptSource source = InterruptSource::IRL;

    m_externalIntrVector = vector;

    SetInterruptLevel(source, level);

    if (level > 0) {
        if (ICR.VECMD) {
            SetInterruptVector(source, vector);
        } else {
            const uint8 level = GetInterruptLevel(source);
            SetInterruptVector(source, 0x40 + (level >> 1u));
        }
        RaiseInterrupt(source);
    } else {
        SetInterruptVector(source, 0);
        LowerInterrupt(source);
    }
}

void SH2::SetNMI() {
    // HACK: should be edge-detected
    ICR.NMIL = 1;
    m_NMI = true;
    RaiseInterrupt(InterruptSource::NMI);
}

void SH2::TriggerFRTInputCapture() {
    // TODO: FRT.TCR.IEDGA
    FRT.ICR = FRT.FRC;
    FRT.FTCSR.ICF = 1;
    if (FRT.TIER.ICIE) {
        RaiseInterrupt(InterruptSource::FRT_ICI);
    }
}

// -----------------------------------------------------------------------------
// Memory accessors

template <mem_primitive T, bool instrFetch>
T SH2::MemRead(uint32 address) {
    const uint32 partition = (address >> 29u) & 0b111;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        m_log.trace("WARNING: misaligned {}-bit read from {:08X}", sizeof(T) * 8, address);
        // TODO: raise CPU address error due to misaligned access
        // - might have to store data in a class member instead of returning
    }

    switch (partition) {
    case 0b000: // cache
        if (CCR.CE) {
            // TODO: use cache
        }
        // fallthrough
    case 0b001:
    case 0b101: // cache-through
        return m_bus.Read<T>(address & 0x7FFFFFF);
    case 0b010: // associative purge

        // TODO: implement
        m_log.trace("Unhandled {}-bit SH-2 associative purge read from {:08X}", sizeof(T) * 8, address);
        return (address & 1) ? static_cast<T>(0x12231223) : static_cast<T>(0x23122312);
    case 0b011: { // cache address array
        uint32 entry = (address >> 4u) & 0x3F;
        return cacheEntries[entry].tag[CCR.Wn]; // TODO: include LRU data
    }
    case 0b100:
    case 0b110: // cache data array

        // TODO: implement
        m_log.trace("Unhandled {}-bit SH-2 cache data array read from {:08X}", sizeof(T) * 8, address);
        return 0;
    case 0b111: // I/O area
        if constexpr (instrFetch) {
            // TODO: raise CPU address error due to attempt to fetch instruction from I/O area
            m_log.trace("Attempted to fetch instruction from I/O area at {:08X}", address);
            return 0;
        } else if ((address & 0xE0004000) == 0xE0004000) {
            // bits 31-29 and 14 must be set
            // bits 8-0 index the register
            // bits 28 and 12 must be both set to access the lower half of the registers
            if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                return OnChipRegRead<T>(address & 0x1FF);
            } else {
                return OpenBusSeqRead<T>(address);
            }
        } else {
            // TODO: implement
            m_log.trace("Unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    util::unreachable();
}

template <mem_primitive T>
void SH2::MemWrite(uint32 address, T value) {
    const uint32 partition = address >> 29u;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        m_log.trace("WARNING: misaligned {}-bit write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        // TODO: address error (misaligned access)
    }

    switch (partition) {
    case 0b000: // cache
        if (CCR.CE) {
            // TODO: use cache
        }
        // fallthrough
    case 0b001:
    case 0b101: // cache-through
        m_bus.Write<T>(address & 0x7FFFFFF, value);
        break;
    case 0b010: // associative purge
        // TODO: implement
        m_log.trace("Unhandled {}-bit SH-2 associative purge write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    case 0b011: { // cache address array
        uint32 entry = (address >> 4u) & 0x3F;
        cacheEntries[entry].tag[CCR.Wn] = address & 0x1FFFFC04;
        // TODO: update LRU data
        break;
    }
    case 0b100:
    case 0b110: // cache data array
        // TODO: implement
        m_log.trace("Unhandled {}-bit SH-2 cache data array write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    case 0b111: // I/O area
        if ((address & 0xE0004000) == 0xE0004000) {
            // bits 31-29 and 14 must be set
            // bits 8-0 index the register
            // bits 28 and 12 must be both set to access the lower half of the registers
            if ((address & 0x100) || (address & 0x10001000) == 0x10001000) {
                OnChipRegWrite<T>(address & 0x1FF, value);
            }
        } else if ((address >> 12u) == 0xFFFF8) {
            // DRAM setup stuff
            switch (address) {
            case 0xFFFF8426: m_log.trace("16-bit CAS latency 1"); break;
            case 0xFFFF8446: m_log.trace("16-bit CAS latency 2"); break;
            case 0xFFFF8466: m_log.trace("16-bit CAS latency 3"); break;
            case 0xFFFF8848: m_log.trace("32-bit CAS latency 1"); break;
            case 0xFFFF8888: m_log.trace("32-bit CAS latency 2"); break;
            case 0xFFFF88C8: m_log.trace("32-bit CAS latency 3"); break;
            default: m_log.debug("Unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address); break;
            }
        } else {
            // TODO: implement
            m_log.trace("Unhandled {}-bit SH-2 I/O area write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        }
        break;
    }
}

FLATTEN FORCE_INLINE uint16 SH2::FetchInstruction(uint32 address) {
    return MemRead<uint16, true>(address);
}

FLATTEN FORCE_INLINE uint8 SH2::MemReadByte(uint32 address) {
    return MemRead<uint8, false>(address);
}

FLATTEN FORCE_INLINE uint16 SH2::MemReadWord(uint32 address) {
    return MemRead<uint16, false>(address);
}

FLATTEN FORCE_INLINE uint32 SH2::MemReadLong(uint32 address) {
    return MemRead<uint32, false>(address);
}

FLATTEN FORCE_INLINE void SH2::MemWriteByte(uint32 address, uint8 value) {
    MemWrite<uint8>(address, value);
}

FLATTEN FORCE_INLINE void SH2::MemWriteWord(uint32 address, uint16 value) {
    MemWrite<uint16>(address, value);
}

FLATTEN FORCE_INLINE void SH2::MemWriteLong(uint32 address, uint32 value) {
    MemWrite<uint32>(address, value);
}

template <mem_primitive T>
T SH2::OpenBusSeqRead(uint32 address) {
    if constexpr (std::is_same_v<T, uint8>) {
        return (address & 1u) * ((address >> 1u) & 0x7);
        // return OpenBusSeqRead<uint16>(address) >> (((address & 1) ^ 1) * 8);
    } else if constexpr (std::is_same_v<T, uint16>) {
        return (address >> 1u) & 0x7;
    } else if constexpr (std::is_same_v<T, uint32>) {
        return (OpenBusSeqRead<uint16>(address + 1) << 16u) | OpenBusSeqRead<uint16>(address);
    }
}

// -----------------------------------------------------------------------------
// On-chip peripherals

FORCE_INLINE void SH2::AdvanceFRT(uint64 cycles) {
    FRT.cycleCount += cycles;
    const uint64 steps = FRT.cycleCount >> FRT.clockDividerShift;
    FRT.cycleCount -= steps << FRT.clockDividerShift;

    bool oviIntr = false;
    bool ociIntr = false;

    uint64 nextFRC = FRT.FRC + steps;
    if (FRT.FRC < FRT.OCRA && nextFRC >= FRT.OCRA) {
        FRT.FTCSR.OCFA = FRT.TOCR.OLVLA;
        if (FRT.FTCSR.CCLRA) {
            nextFRC = 0;
        }
        if (FRT.TIER.OCIAE) {
            ociIntr = true;
        }
    }
    if (FRT.FRC < FRT.OCRB && nextFRC >= FRT.OCRB) {
        FRT.FTCSR.OCFB = FRT.TOCR.OLVLB;
        if (FRT.TIER.OCIBE) {
            ociIntr = true;
        }
    }
    if (nextFRC >= 0x10000) {
        FRT.FTCSR.OVF = 1;
        if (FRT.TIER.OVIE) {
            oviIntr = true;
        }
    }
    FRT.FRC = nextFRC;

    if (oviIntr) {
        RaiseInterrupt(InterruptSource::FRT_OVI);
    } else if (ociIntr) {
        RaiseInterrupt(InterruptSource::FRT_OCI);
    }
}

FLATTEN FORCE_INLINE bool SH2::IsDMATransferActive(const DMAChannel &ch) const {
    return ch.IsEnabled() && DMAOR.DME && !DMAOR.NMIF && !DMAOR.AE;
}

void SH2::UpdateDMAC() {
    m_activeDMAChannel = dmaChannels.size();

    for (uint32 index = 0; auto &ch : dmaChannels) {
        if (!IsDMATransferActive(ch)) {
            index++;
            continue;
        }

        // Auto request mode will start the transfer right now.
        // Module request mode checks if the signal from the configured source has been raised.
        if (!ch.autoRequest) {
            bool signal = false;
            switch (ch.resSelect) {
            case DMAResourceSelect::DREQ: /*TODO*/ signal = false; break;
            case DMAResourceSelect::RXI: /*TODO*/ signal = false; break;
            case DMAResourceSelect::TXI: /*TODO*/ signal = false; break;
            case DMAResourceSelect::Reserved: signal = false; break;
            }
            if (!signal) {
                index++;
                continue;
            }
        }

        // TODO: prioritize channels based on DMAOR.PR
        m_activeDMAChannel = index;
        break;
    }
}

void SH2::AdvanceDMAC(uint64 cycles) {
    const uint32 index = m_activeDMAChannel;
    if (index >= dmaChannels.size()) {
        return;
    }

    auto &ch = dmaChannels[index];

    // TODO: proper timings, cycle-stealing, etc. (suspend instructions if not cached)
    static constexpr uint32 kXferSize[] = {1, 2, 4, 16};
    const uint32 xferSize = kXferSize[static_cast<uint32>(ch.xferSize)];

    auto incAddress = [&](uint32 address, DMATransferIncrementMode mode) -> uint32 {
        using enum DMATransferIncrementMode;
        switch (mode) {
        case Fixed: return address;
        case Increment: return address + xferSize;
        case Decrement: return address - xferSize;
        case Reserved: return address;
        }
    };

    // Perform one unit of transfer
    switch (ch.xferSize) {
    case DMATransferSize::Byte: {
        const uint8 value = MemReadByte(ch.srcAddress);
        m_log.trace("DMAC{} 8-bit transfer from {:08X} to {:08X} -> {:X}", index, ch.srcAddress, ch.dstAddress, value);
        MemWriteByte(ch.dstAddress, value);
        break;
    }
    case DMATransferSize::Word: {
        const uint16 value = MemReadWord(ch.srcAddress);
        m_log.trace("DMAC{} 16-bit transfer from {:08X} to {:08X} -> {:X}", index, ch.srcAddress, ch.dstAddress, value);
        MemWriteWord(ch.dstAddress, value);
        break;
    }
    case DMATransferSize::Longword: {
        const uint32 value = MemReadLong(ch.srcAddress);
        m_log.trace("DMAC{} 32-bit transfer from {:08X} to {:08X} -> {:X}", index, ch.srcAddress, ch.dstAddress, value);
        MemWriteLong(ch.dstAddress, value);
        break;
    }
    case DMATransferSize::QuadLongword:
        for (int i = 0; i < 4; i++) {
            const uint32 value = MemReadLong(ch.srcAddress + i * sizeof(uint32));
            m_log.trace("DMAC{} 16-byte transfer {:d} from {:08X} to {:08X} -> {:X}", index, i, ch.srcAddress,
                        ch.dstAddress, value);
            MemWriteLong(ch.dstAddress + i * sizeof(uint32), value);
        }
        break;
    }

    // Update address and remaining count
    ch.srcAddress = incAddress(ch.srcAddress, ch.srcMode);
    ch.dstAddress = incAddress(ch.dstAddress, ch.dstMode);

    if (ch.xferSize == DMATransferSize::QuadLongword) {
        if (ch.xferCount >= 4) {
            ch.xferCount -= 4;
        } else {
            m_log.trace("DMAC{} 16-byte transfer count misaligned", index);
            ch.xferCount = 0;
        }
    } else {
        ch.xferCount--;
    }

    // Check if transfer ended
    if (ch.xferCount == 0) {
        ch.xferEnded = true;
        m_log.trace("DMAC{} transfer finished", index);
        if (ch.irqEnable) {
            switch (index) {
            case 0: RaiseInterrupt(InterruptSource::DMAC0_XferEnd); break;
            case 1: RaiseInterrupt(InterruptSource::DMAC1_XferEnd); break;
            }
        }
        UpdateDMAC();
    }
}

void SH2::WriteCCR(uint8 value) {
    if (CCR.u8 == value) {
        return;
    }

    CCR.u8 = value;
    if (CCR.CP) {
        // TODO: purge cache
        CCR.CP = 0;
    }
}

void SH2::DIVUBegin32() {
    static constexpr sint32 kMinValue = std::numeric_limits<sint32>::min();
    static constexpr sint32 kMaxValue = std::numeric_limits<sint32>::max();

    const sint32 dividend = static_cast<sint32>(DVDNTL);
    const sint32 divisor = static_cast<sint32>(DVSR);

    if (divisor != 0) {
        // TODO: schedule event to run this after 39 cycles

        if (dividend == kMinValue && divisor == -1) [[unlikely]] {
            // Handle extreme case
            DVDNTL = DVDNT = kMinValue;
            DVDNTH = 0;
        } else {
            DVDNTL = DVDNT = dividend / divisor;
            DVDNTH = dividend % divisor;
        }
    } else {
        // Overflow
        // TODO: schedule event to run this after 6 cycles

        // Perform partial division
        // The division unit uses 3 cycles to set up flags, leaving 3 cycles for calculations
        DVDNTH = dividend >> 29;
        if (DVCR.OVFIE) {
            DVDNTL = DVDNT = (dividend << 3) | ((dividend >> 31) & 7);
        } else {
            // DVDNT/DVDNTL is saturated if the interrupt signal is disabled
            DVDNTL = DVDNT = dividend < 0 ? kMinValue : kMaxValue;
        }

        // Signal overflow
        DVCR.OVF = 1;
        if (DVCR.OVFIE) {
            RaiseInterrupt(InterruptSource::DIVU_OVFI);
        }
    }

    DVDNTUH = DVDNTH;
    DVDNTUL = DVDNTL;
}

void SH2::DIVUBegin64() {
    static constexpr sint32 kMinValue32 = std::numeric_limits<sint32>::min();
    static constexpr sint32 kMaxValue32 = std::numeric_limits<sint32>::max();
    static constexpr sint64 kMinValue64 = std::numeric_limits<sint64>::min();

    sint64 dividend = (static_cast<sint64>(DVDNTH) << 32ll) | static_cast<sint64>(DVDNTL);
    const sint32 divisor = static_cast<sint32>(DVSR);

    bool overflow = divisor == 0;

    if (dividend == -0x80000000ll && divisor == -1) {
        DVDNTH = DVDNTUH = 0;
        DVDNTL = DVDNTUL = -0x80000000l;
        return;
    }

    if (!overflow) {
        const sint64 quotient = dividend / divisor;
        const sint32 remainder = dividend % divisor;

        if (quotient <= kMinValue32 || quotient > kMaxValue32) [[unlikely]] {
            // Overflow cases
            overflow = true;
        } else if (dividend == kMinValue64 && divisor == -1) [[unlikely]] {
            // Handle extreme case
            overflow = true;
        } else {
            // TODO: schedule event to run this after 39 cycles
            DVDNTL = DVDNT = quotient;
            DVDNTH = remainder;
        }
    }

    if (overflow) {
        // Overflow is detected after 6 cycles

        // Perform partial division
        // The division unit uses 3 cycles to set up flags, leaving 3 cycles for calculations
        const sint64 origDividend = dividend;
        bool Q = dividend < 0;
        const bool M = divisor < 0;
        for (int i = 0; i < 3; i++) {
            if (Q == M) {
                dividend -= static_cast<uint64>(divisor) << 32ull;
            } else {
                dividend += static_cast<uint64>(divisor) << 32ull;
            }

            Q = dividend < 0;
            dividend = (dividend << 1ll) | (Q == M);
        }

        // Update output registers
        if (DVCR.OVFIE) {
            DVDNTL = DVDNT = dividend;
        } else {
            // DVDNT/DVDNTL is saturated if the interrupt signal is disabled
            DVDNTL = DVDNT = static_cast<sint32>((origDividend >> 32) ^ divisor) < 0 ? kMinValue32 : kMaxValue32;
        }
        DVDNTH = dividend >> 32ll;

        // Signal overflow
        DVCR.OVF = 1;
        if (DVCR.OVFIE) {
            RaiseInterrupt(InterruptSource::DIVU_OVFI);
        }
    }

    DVDNTUH = DVDNTH;
    DVDNTUL = DVDNTL;
}

template <mem_primitive T>
T SH2::OnChipRegRead(uint32 address) {
    // Misaligned memory accesses raise an address error, therefore:
    //   (address & 3) == 2 is only valid for 16-bit accesses
    //   (address & 1) == 1 is only valid for 8-bit accesses
    // Additionally:
    //   (address & 1) == 0 has special cases for registers 0-255:
    //     8-bit read from a 16-bit register:  r >> 8u
    //     16-bit read from a 8-bit register: (r << 8u) | r
    //     Every other access returns just r

    if constexpr (std::is_same_v<T, uint32>) {
        return OnChipRegReadLong(address);
    } else if constexpr (std::is_same_v<T, uint16>) {
        return OnChipRegReadWord(address);
    } else if constexpr (std::is_same_v<T, uint8>) {
        return OnChipRegReadByte(address);
    }
}

FORCE_INLINE uint8 SH2::OnChipRegReadByte(uint32 address) {
    if (address >= 0x100) {
        // Registers 0x100-0x1FF do not accept 8-bit accesses
        // TODO: raise CPU address error
        m_log.debug("Illegal 8-bit on-chip register read from {:03X}", address);
        return 0;
    }

    switch (address) {
    case 0x04: return 0; // TODO: SCI SSR
    case 0x10: return FRT.ReadTIER();
    case 0x11: return FRT.ReadFTCSR();
    case 0x12: return FRT.ReadFRCH();
    case 0x13: return FRT.ReadFRCL();
    case 0x14: return FRT.ReadOCRH();
    case 0x15: return FRT.ReadOCRL();
    case 0x16: return FRT.ReadTCR();
    case 0x17: return FRT.ReadTOCR();
    case 0x18: return FRT.ReadICRH();
    case 0x19: return FRT.ReadICRL();

    case 0x60: return (GetInterruptLevel(InterruptSource::SCI_ERI) << 4u) | GetInterruptLevel(InterruptSource::FRT_ICI);
    case 0x61: return 0;
    case 0x62: return GetInterruptVector(InterruptSource::SCI_ERI);
    case 0x63: return GetInterruptVector(InterruptSource::SCI_RXI);
    case 0x64: return GetInterruptVector(InterruptSource::SCI_TXI);
    case 0x65: return GetInterruptVector(InterruptSource::SCI_TEI);
    case 0x66: return GetInterruptVector(InterruptSource::FRT_ICI);
    case 0x67: return GetInterruptVector(InterruptSource::FRT_OCI);
    case 0x68: return GetInterruptVector(InterruptSource::FRT_OVI);
    case 0x69: return 0;

    case 0x71: return dmaChannels[0].ReadDRCR();
    case 0x72: return dmaChannels[1].ReadDRCR();

    case 0x92 ... 0x9F: return CCR.u8;

    case 0xE0: return OnChipRegReadWord(address) >> 8u;
    case 0xE1: return OnChipRegReadWord(address & ~1) >> 0u;
    case 0xE2:
        return (GetInterruptLevel(InterruptSource::DIVU_OVFI) << 4u) |
               GetInterruptLevel(InterruptSource::DMAC0_XferEnd);
    case 0xE3: return GetInterruptLevel(InterruptSource::WDT_ITI) << 4u;
    case 0xE4: return GetInterruptVector(InterruptSource::WDT_ITI);
    case 0xE5: return GetInterruptVector(InterruptSource::BSC_REF_CMI);

    default: //
        m_log.debug("Unhandled 8-bit on-chip register read from {:03X}", address);
        return 0;
    }
}

FORCE_INLINE uint16 SH2::OnChipRegReadWord(uint32 address) {
    if (address < 0x100) {
        if (address == 0xE0) {
            return ICR.u16;
        }
        const uint16 value = OnChipRegReadByte(address);
        return (value << 8u) | value;
    } else {
        return OnChipRegReadLong(address & ~2);
    }
}

FORCE_INLINE uint32 SH2::OnChipRegReadLong(uint32 address) {
    if (address < 0x100) {
        // Registers 0x000-0x0FF do not accept 32-bit accesses
        // TODO: raise CPU address error
        m_log.debug("Illegal 32-bit on-chip register read from {:03X}", address);
        return 0;
    }

    switch (address) {
    case 0x100:
    case 0x120: return DVSR;

    case 0x104:
    case 0x124: return DVDNT;

    case 0x108:
    case 0x128: return DVCR.u32;

    case 0x10C:
    case 0x12C: return GetInterruptVector(InterruptSource::DIVU_OVFI);

    case 0x110:
    case 0x130: return DVDNTH;

    case 0x114:
    case 0x134: return DVDNTL;

    case 0x118:
    case 0x138: return DVDNTUH;

    case 0x11C:
    case 0x13C: return DVDNTUL;

    case 0x180: return dmaChannels[0].srcAddress;
    case 0x184: return dmaChannels[0].dstAddress;
    case 0x188: return dmaChannels[0].xferCount;
    case 0x18C: return dmaChannels[0].ReadCHCR();

    case 0x190: return dmaChannels[1].srcAddress;
    case 0x194: return dmaChannels[1].dstAddress;
    case 0x198: return dmaChannels[1].xferCount;
    case 0x19C: return dmaChannels[1].ReadCHCR();

    case 0x1A0: return GetInterruptVector(InterruptSource::DMAC0_XferEnd);
    case 0x1A8: return GetInterruptVector(InterruptSource::DMAC1_XferEnd);

    case 0x1B0: return DMAOR.u32;

    case 0x1E0: return BCR1.u16;
    case 0x1E4: return BCR2.u16;
    case 0x1E8: return WCR.u16;
    case 0x1EC: return MCR.u16;
    case 0x1F0: return RTCSR.u16;
    case 0x1F4: return RTCNT;
    case 0x1F8: return RTCOR;

    default: //
        m_log.debug("Unhandled 32-bit on-chip register read from {:03X}", address);
        return 0;
    }
}

template <mem_primitive T>
void SH2::OnChipRegWrite(uint32 address, T value) {
    // Misaligned memory accesses raise an address error, therefore:
    //   (address & 3) == 2 is only valid for 16-bit accesses
    //   (address & 1) == 1 is only valid for 8-bit accesses
    if constexpr (std::is_same_v<T, uint32>) {
        OnChipRegWriteLong(address, value);
    } else if constexpr (std::is_same_v<T, uint16>) {
        OnChipRegWriteWord(address, value);
    } else if constexpr (std::is_same_v<T, uint8>) {
        OnChipRegWriteByte(address, value);
    }
}

FORCE_INLINE void SH2::OnChipRegWriteByte(uint32 address, uint8 value) {
    if (address >= 0x100) {
        // Registers 0x100-0x1FF do not accept 8-bit accesses
        // TODO: raise CPU address error
        m_log.debug("Illegal 8-bit on-chip register write to {:03X} = {:X}", address, value);
        return;
    }

    switch (address) {
    case 0x10: FRT.WriteTIER(value); break;
    case 0x11: FRT.WriteFTCSR(value); break;
    case 0x12: FRT.WriteFRCH(value); break;
    case 0x13: FRT.WriteFRCL(value); break;
    case 0x14: FRT.WriteOCRH(value); break;
    case 0x15: FRT.WriteOCRL(value); break;
    case 0x16: FRT.WriteTCR(value); break;
    case 0x17: FRT.WriteTOCR(value); break;
    case 0x18: /* ICRH is read-only */ break;
    case 0x19: /* ICRL is read-only */ break;

    case 0x60: //
    {
        const uint8 frtIntrLevel = bit::extract<0, 3>(value);
        const uint8 sciIntrLevel = bit::extract<4, 7>(value);

        using enum InterruptSource;
        SetInterruptLevel(FRT_ICI, frtIntrLevel);
        SetInterruptLevel(FRT_OCI, frtIntrLevel);
        SetInterruptLevel(FRT_OVI, frtIntrLevel);
        SetInterruptLevel(SCI_ERI, sciIntrLevel);
        SetInterruptLevel(SCI_RXI, sciIntrLevel);
        SetInterruptLevel(SCI_TXI, sciIntrLevel);
        SetInterruptLevel(SCI_TEI, sciIntrLevel);
        UpdateInterruptLevels<FRT_ICI, FRT_OCI, FRT_OVI, SCI_ERI, SCI_RXI, SCI_TXI, SCI_TEI>();
        break;
    }
    case 0x61: /* IPRB bits 7-0 are all reserved */ break;
    case 0x62: SetInterruptVector(InterruptSource::SCI_ERI, bit::extract<0, 6>(value)); break;
    case 0x63: SetInterruptVector(InterruptSource::SCI_RXI, bit::extract<0, 6>(value)); break;
    case 0x64: SetInterruptVector(InterruptSource::SCI_TXI, bit::extract<0, 6>(value)); break;
    case 0x65: SetInterruptVector(InterruptSource::SCI_TEI, bit::extract<0, 6>(value)); break;
    case 0x66: SetInterruptVector(InterruptSource::FRT_ICI, bit::extract<0, 6>(value)); break;
    case 0x67: SetInterruptVector(InterruptSource::FRT_OCI, bit::extract<0, 6>(value)); break;
    case 0x68: SetInterruptVector(InterruptSource::FRT_OVI, bit::extract<0, 6>(value)); break;
    case 0x69: /* VCRD bits 7-0 are all reserved */ break;

    case 0x71: dmaChannels[0].WriteDRCR(value); break;
    case 0x72: dmaChannels[1].WriteDRCR(value); break;

    case 0x92: WriteCCR(value); break;

    case 0xE0: ICR.NMIE = bit::extract<0>(value); break;
    case 0xE1: //
    {
        ICR.VECMD = bit::extract<0>(value);
        if (ICR.VECMD) {
            SetInterruptVector(InterruptSource::IRL, m_externalIntrVector);
        } else {
            const uint8 level = GetInterruptLevel(InterruptSource::IRL);
            SetInterruptVector(InterruptSource::IRL, 0x40 + (level >> 1u));
        }
        break;
    }
    case 0xE2: //
    {
        const uint8 dmacIntrLevel = bit::extract<0, 3>(value);
        const uint8 divuIntrLevel = bit::extract<4, 7>(value);

        using enum InterruptSource;
        SetInterruptLevel(DMAC0_XferEnd, dmacIntrLevel);
        SetInterruptLevel(DMAC1_XferEnd, dmacIntrLevel);
        SetInterruptLevel(DIVU_OVFI, divuIntrLevel);
        UpdateInterruptLevels<DMAC0_XferEnd, DMAC1_XferEnd, DIVU_OVFI>();
        break;
    }
    case 0xE3: //
    {
        const uint8 wdtIntrLevel = bit::extract<4, 7>(value);

        using enum InterruptSource;
        SetInterruptLevel(WDT_ITI, wdtIntrLevel);
        UpdateInterruptLevels<WDT_ITI>();
        break;
    }
    case 0xE4: SetInterruptVector(InterruptSource::WDT_ITI, bit::extract<0, 6>(value)); break;
    case 0xE5: SetInterruptVector(InterruptSource::BSC_REF_CMI, bit::extract<0, 6>(value)); break;

    default: //
        m_log.debug("Unhandled 8-bit on-chip register write to {:03X} = {:X}", address, value);
        break;
    }
}

FORCE_INLINE void SH2::OnChipRegWriteWord(uint32 address, uint16 value) {
    switch (address) {
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:

    case 0xE0:
    case 0xE1:
    case 0xE2:
    case 0xE3:
    case 0xE4:
    case 0xE5:
        OnChipRegWriteByte(address & ~1, value >> 8u);
        OnChipRegWriteByte(address | 1, value >> 0u);
        break;

    case 0x108:
    case 0x10C:

    case 0x1E0:
    case 0x1E4:
    case 0x1E8:
    case 0x1EC:
    case 0x1F0:
    case 0x1F4:
    case 0x1F8: //
        OnChipRegWriteLong(address & ~3, value);
        break;

    default: //
        m_log.debug("Illegal 16-bit on-chip register write to {:03X} = {:X}", address, value);
        break;
    }
}

FORCE_INLINE void SH2::OnChipRegWriteLong(uint32 address, uint32 value) {
    if (address < 0x100) {
        // Registers 0x000-0x0FF do not accept 32-bit accesses
        // TODO: raise CPU address error
        m_log.debug("Illegal 32-bit on-chip register write to {:03X} = {:X}", address, value);
        return;
    }

    switch (address) {
    case 0x100:
    case 0x120: DVSR = value; break;

    case 0x104:
    case 0x124:
        DVDNT = value;
        DVDNTL = value;
        DVDNTH = static_cast<sint32>(value) >> 31;
        DIVUBegin32();
        break;

    case 0x108:
    case 0x128: DVCR.u32 = value & 0x00000003; break;

    case 0x10C:
    case 0x12C: SetInterruptVector(InterruptSource::DIVU_OVFI, bit::extract<0, 6>(value)); break;

    case 0x110:
    case 0x130: DVDNTH = value; break;

    case 0x114:
    case 0x134:
        DVDNTL = value;
        DIVUBegin64();
        break;

    case 0x118:
    case 0x138: DVDNTUH = value; break;

    case 0x11C:
    case 0x13C: DVDNTUL = value; break;

    case 0x180: dmaChannels[0].srcAddress = value; break;
    case 0x184: dmaChannels[0].dstAddress = value; break;
    case 0x188: dmaChannels[0].xferCount = bit::extract<0, 23>(value); break;
    case 0x18C:
        dmaChannels[0].WriteCHCR(value);
        UpdateDMAC();
        break;

    case 0x190: dmaChannels[1].srcAddress = value; break;
    case 0x194: dmaChannels[1].dstAddress = value; break;
    case 0x198: dmaChannels[1].xferCount = bit::extract<0, 23>(value); break;
    case 0x19C:
        dmaChannels[1].WriteCHCR(value);
        UpdateDMAC();
        break;

    case 0x1A0: SetInterruptVector(InterruptSource::DMAC0_XferEnd, bit::extract<0, 6>(value)); break;
    case 0x1A8: SetInterruptVector(InterruptSource::DMAC1_XferEnd, bit::extract<0, 6>(value)); break;

    case 0x1B0:
        DMAOR.DME = bit::extract<0>(value);
        DMAOR.NMIF &= bit::extract<1>(value);
        DMAOR.AE &= bit::extract<2>(value);
        DMAOR.PR = bit::extract<3>(value);
        UpdateDMAC();
        break;

    case 0x1E0: // BCR1
        if ((value >> 16u) == 0xA55A) {
            BCR1.u15 = value & 0x1FF7;
        }
        break;
    case 0x1E4: // BCR2
        if ((value >> 16u) == 0xA55A) {
            BCR2.u16 = value & 0xFC;
        }
        break;
    case 0x1E8: // WCR
        if ((value >> 16u) == 0xA55A) {
            WCR.u16 = value;
        }
        break;
    case 0x1EC: // MCR
        if ((value >> 16u) == 0xA55A) {
            MCR.u16 = value & 0xFEFC;
        }
        break;
    case 0x1F0: // RTCSR
        if ((value >> 16u) == 0xA55A) {
            // TODO: implement the set/clear rules for RTCSR.CMF
            RTCSR.u16 = (value & 0x78) | (RTCSR.u16 & 0x80);
        }
        break;
    case 0x1F4: // RTCNT
        if ((value >> 16u) == 0xA55A) {
            RTCNT = value;
        }
        break;
    case 0x1F8: // RTCOR
        if ((value >> 16u) == 0xA55A) {
            RTCOR = value;
        }
        break;
    default: //
        m_log.debug("Unhandled 32-bit on-chip register write to {:03X} = {:X}", address, value);
        break;
    }
}

// -----------------------------------------------------------------------------
// Interrupts

FORCE_INLINE uint8 SH2::GetInterruptVector(InterruptSource source) {
    return m_intrVectors[static_cast<size_t>(source)];
}

FORCE_INLINE void SH2::SetInterruptVector(InterruptSource source, uint8 vector) {
    m_intrVectors[static_cast<size_t>(source)] = vector;
}

FORCE_INLINE uint8 SH2::GetInterruptLevel(InterruptSource source) {
    return m_intrLevels[static_cast<size_t>(source)];
}

FORCE_INLINE void SH2::SetInterruptLevel(InterruptSource source, uint8 priority) {
    m_intrLevels[static_cast<size_t>(source)] = priority;
}

FORCE_INLINE void SH2::RaiseInterrupt(InterruptSource source) {
    const uint8 level = GetInterruptLevel(source);
    if (level < m_pendingInterrupt.level) {
        return;
    }
    if (level == m_pendingInterrupt.level &&
        static_cast<uint8>(source) < static_cast<uint8>(m_pendingInterrupt.source)) {
        return;
    }
    m_pendingInterrupt.level = level;
    m_pendingInterrupt.source = source;
}

FORCE_INLINE void SH2::LowerInterrupt(InterruptSource source) {
    if (m_pendingInterrupt.source == source) {
        RecalcInterrupts();
    }
}

template <SH2::InterruptSource source, SH2::InterruptSource... sources>
void SH2::UpdateInterruptLevels() {
    if (m_pendingInterrupt.source == source) {
        const uint8 newLevel = GetInterruptLevel(source);
        if (newLevel < m_pendingInterrupt.level) {
            // Interrupt may no longer have the highest priority; recalculate
            RecalcInterrupts();
        } else {
            // Interrupt still has the highest priority; update
            m_pendingInterrupt.level = newLevel;
        }
        return;
    }
    if constexpr (sizeof...(sources) > 1) {
        UpdateInterruptLevels<sources...>();
    }
}

void SH2::RecalcInterrupts() {
    // Check interrupts from these sources (in order of priority, when priority numbers are the same):
    //   name             priority       vecnum
    //   NMI              16             0x0B
    //   User break       15             0x0C
    //   IRLs 15-1        15-1           0x40 + (level >> 1)
    //   DIVU OVFI        IPRA.DIVUIPn   VCRDIV
    //   DMAC0 xfer end   IPRA.DMACIPn   VCRDMA0
    //   DMAC1 xfer end   IPRA.DMACIPn   VCRDMA1
    //   WDT ITI          IPRA.WDTIPn    VCRWDT
    //   BSC REF CMI      IPRA.WDTIPn    VCRWDT
    //   SCI ERI          IPRB.SCIIPn    VCRA.SERVn
    //   SCI RXI          IPRB.SCIIPn    VCRA.SRXVn
    //   SCI TXI          IPRB.SCIIPn    VCRB.STXVn
    //   SCI TEI          IPRB.SCIIPn    VCRB.STEVn
    //   FRT ICI          IPRB.FRTIPn    VCRC.FICVn
    //   FRT OCI          IPRB.FRTIPn    VCRC.FOCVn
    //   FRT OVI          IPRB.FRTIPn    VCRD.FOVVn
    // Use the vector number of the exception with highest priority

    m_pendingInterrupt.level = 0;
    m_pendingInterrupt.source = InterruptSource::None;

    // HACK: should be edge-detected
    if (m_NMI) {
        m_NMI = false;
        RaiseInterrupt(InterruptSource::NMI);
        return;
    }

    // TODO: user break
    /*if (...) {
        RaiseInterrupt(InterruptSource::UserBreak);
        return;
    }*/

    // IRLs
    if (GetInterruptLevel(InterruptSource::IRL) > 0) {
        RaiseInterrupt(InterruptSource::IRL);
        return;
    }

    // Division overflow
    if (DVCR.OVF && DVCR.OVFIE) {
        RaiseInterrupt(InterruptSource::DIVU_OVFI);
        return;
    }

    // DMA channel transfer end
    if (dmaChannels[0].xferEnded && dmaChannels[0].irqEnable) {
        RaiseInterrupt(InterruptSource::DMAC0_XferEnd);
        return;
    }
    if (dmaChannels[1].xferEnded && dmaChannels[1].irqEnable) {
        RaiseInterrupt(InterruptSource::DMAC1_XferEnd);
        return;
    }

    // TODO: WDT ITI
    // Watchdog timer
    /*if (...) {
        RaiseInterrupt(InterruptSource::WDT_ITI);
        return;
    }*/

    // TODO: BSC REF CMI
    /*if (...) {
        RaiseInterrupt(InterruptSource::BSC_REF_CMI);
        return;
    }*/

    // TODO: SCI ERI, RXI, TXI, TEI
    /*if (...) {
        RaiseInterrupt(InterruptSource::SCI_ERI);
        return;
    }*/
    /*if (...) {
        RaiseInterrupt(InterruptSource::SCI_RXI);
        return;
    }*/
    /*if (...) {
        RaiseInterrupt(InterruptSource::SCI_TXI);
        return;
    }*/
    /*if (...) {
        RaiseInterrupt(InterruptSource::SCI_TEI);
        return;
    }*/

    // Free-running timer interrupts
    if (FRT.FTCSR.ICF && FRT.TIER.ICIE) {
        RaiseInterrupt(InterruptSource::FRT_ICI);
        return;
    }
    if ((FRT.FTCSR.OCFA && FRT.TIER.OCIAE) || (FRT.FTCSR.OCFB && FRT.TIER.OCIBE)) {
        RaiseInterrupt(InterruptSource::FRT_OCI);
        return;
    }
    if (FRT.FTCSR.OVF && FRT.TIER.OVIE) {
        RaiseInterrupt(InterruptSource::FRT_OVI);
        return;
    }
}

FORCE_INLINE bool SH2::CheckInterrupts() {
    return m_pendingInterrupt.level > SR.ILevel;
}

// -------------------------------------------------------------------------
// Helper functions

FORCE_INLINE void SH2::SetupDelaySlot(uint32 targetAddress) {
    m_delaySlot = true;
    m_delaySlotTarget = targetAddress;
}

FORCE_INLINE void SH2::EnterException(uint8 vectorNumber) {
    m_tracer.Exception({R, PC, PR, SR.u32, VBR, GBR, MAC.u64}, vectorNumber);
    R[15] -= 4;
    MemWriteLong(R[15], SR.u32);
    R[15] -= 4;
    MemWriteLong(R[15], PC - 4);
    PC = MemReadLong(VBR + (static_cast<uint32>(vectorNumber) << 2u));
}
// -----------------------------------------------------------------------------
// Interpreter

void SH2::Execute() {
    if (!m_delaySlot && CheckInterrupts()) [[unlikely]] {
        // Service interrupt
        const uint8 vecNum = GetInterruptVector(m_pendingInterrupt.source);
        m_log.trace("Handling interrupt level {:02X}, vector number {:02X}", m_pendingInterrupt.level, vecNum);
        EnterException(vecNum);
        SR.ILevel = std::min<uint8>(m_pendingInterrupt.level, 0xF);

        // Acknowledge interrupt
        switch (m_pendingInterrupt.source) {
        case InterruptSource::IRL: m_bus.AcknowledgeExternalInterrupt(); break;
        case InterruptSource::NMI:
            m_NMI = false;
            LowerInterrupt(InterruptSource::NMI);
            break;
        default: break;
        }
    }

    // TODO: emulate fetch - decode - execute - memory access - writeback pipeline
    // TODO: figure out a way to optimize delay slots for performance
    // - perhaps decoding instructions beforehand

    auto jumpToDelaySlot = [&] {
        PC = m_delaySlotTarget;
        m_delaySlot = false;
    };

    auto dump = [&] {
        static bool dumped = false;
        if (!dumped) {
            dumped = true;
            m_tracer.UserCapture({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
            m_tracer.Dump();
            m_tracer.Reset();
        }
    };

    const uint16 instr = FetchInstruction(PC);
    const OpcodeType opcode = g_decodeTable.opcodes[m_delaySlot][instr];

    switch (opcode) {
    case OpcodeType::NOP: NOP(), PC += 2; break;

    case OpcodeType::SLEEP: SLEEP(), PC += 2; break;

    case OpcodeType::MOV_R: MOV(instr), PC += 2; break;
    case OpcodeType::MOVB_L: MOVBL(instr), PC += 2; break;
    case OpcodeType::MOVW_L: MOVWL(instr), PC += 2; break;
    case OpcodeType::MOVL_L: MOVLL(instr), PC += 2; break;
    case OpcodeType::MOVB_L0: MOVBL0(instr), PC += 2; break;
    case OpcodeType::MOVW_L0: MOVWL0(instr), PC += 2; break;
    case OpcodeType::MOVL_L0: MOVLL0(instr), PC += 2; break;
    case OpcodeType::MOVB_L4: MOVBL4(instr), PC += 2; break;
    case OpcodeType::MOVW_L4: MOVWL4(instr), PC += 2; break;
    case OpcodeType::MOVL_L4: MOVLL4(instr), PC += 2; break;
    case OpcodeType::MOVB_LG: MOVBLG(instr), PC += 2; break;
    case OpcodeType::MOVW_LG: MOVWLG(instr), PC += 2; break;
    case OpcodeType::MOVL_LG: MOVLLG(instr), PC += 2; break;
    case OpcodeType::MOVB_M: MOVBM(instr), PC += 2; break;
    case OpcodeType::MOVW_M: MOVWM(instr), PC += 2; break;
    case OpcodeType::MOVL_M: MOVLM(instr), PC += 2; break;
    case OpcodeType::MOVB_P: MOVBP(instr), PC += 2; break;
    case OpcodeType::MOVW_P: MOVWP(instr), PC += 2; break;
    case OpcodeType::MOVL_P: MOVLP(instr), PC += 2; break;
    case OpcodeType::MOVB_S: MOVBS(instr), PC += 2; break;
    case OpcodeType::MOVW_S: MOVWS(instr), PC += 2; break;
    case OpcodeType::MOVL_S: MOVLS(instr), PC += 2; break;
    case OpcodeType::MOVB_S0: MOVBS0(instr), PC += 2; break;
    case OpcodeType::MOVW_S0: MOVWS0(instr), PC += 2; break;
    case OpcodeType::MOVL_S0: MOVLS0(instr), PC += 2; break;
    case OpcodeType::MOVB_S4: MOVBS4(instr), PC += 2; break;
    case OpcodeType::MOVW_S4: MOVWS4(instr), PC += 2; break;
    case OpcodeType::MOVL_S4: MOVLS4(instr), PC += 2; break;
    case OpcodeType::MOVB_SG: MOVBSG(instr), PC += 2; break;
    case OpcodeType::MOVW_SG: MOVWSG(instr), PC += 2; break;
    case OpcodeType::MOVL_SG: MOVLSG(instr), PC += 2; break;
    case OpcodeType::MOV_I: MOVI(instr), PC += 2; break;
    case OpcodeType::MOVW_I: MOVWI(instr), PC += 2; break;
    case OpcodeType::MOVL_I: MOVLI(instr), PC += 2; break;
    case OpcodeType::MOVA: MOVA(instr), PC += 2; break;
    case OpcodeType::MOVT: MOVT(instr), PC += 2; break;
    case OpcodeType::CLRT: CLRT(), PC += 2; break;
    case OpcodeType::SETT: SETT(), PC += 2; break;

    case OpcodeType::EXTUB: EXTUB(instr), PC += 2; break;
    case OpcodeType::EXTUW: EXTUW(instr), PC += 2; break;
    case OpcodeType::EXTSB: EXTSB(instr), PC += 2; break;
    case OpcodeType::EXTSW: EXTSW(instr), PC += 2; break;
    case OpcodeType::SWAPB: SWAPB(instr), PC += 2; break;
    case OpcodeType::SWAPW: SWAPW(instr), PC += 2; break;
    case OpcodeType::XTRCT: XTRCT(instr), PC += 2; break;

    case OpcodeType::LDC_GBR_R: LDCGBR(instr), PC += 2; break;
    case OpcodeType::LDC_SR_R: LDCSR(instr), PC += 2; break;
    case OpcodeType::LDC_VBR_R: LDCVBR(instr), PC += 2; break;
    case OpcodeType::LDC_GBR_M: LDCMGBR(instr), PC += 2; break;
    case OpcodeType::LDC_SR_M: LDCMSR(instr), PC += 2; break;
    case OpcodeType::LDC_VBR_M: LDCMVBR(instr), PC += 2; break;
    case OpcodeType::LDS_MACH_R: LDSMACH(instr), PC += 2; break;
    case OpcodeType::LDS_MACL_R: LDSMACL(instr), PC += 2; break;
    case OpcodeType::LDS_PR_R: LDSPR(instr), PC += 2; break;
    case OpcodeType::LDS_MACH_M: LDSMMACH(instr), PC += 2; break;
    case OpcodeType::LDS_MACL_M: LDSMMACL(instr), PC += 2; break;
    case OpcodeType::LDS_PR_M: LDSMPR(instr), PC += 2; break;
    case OpcodeType::STC_GBR_R: STCGBR(instr), PC += 2; break;
    case OpcodeType::STC_SR_R: STCSR(instr), PC += 2; break;
    case OpcodeType::STC_VBR_R: STCVBR(instr), PC += 2; break;
    case OpcodeType::STC_GBR_M: STCMGBR(instr), PC += 2; break;
    case OpcodeType::STC_SR_M: STCMSR(instr), PC += 2; break;
    case OpcodeType::STC_VBR_M: STCMVBR(instr), PC += 2; break;
    case OpcodeType::STS_MACH_R: STSMACH(instr), PC += 2; break;
    case OpcodeType::STS_MACL_R: STSMACL(instr), PC += 2; break;
    case OpcodeType::STS_PR_R: STSPR(instr), PC += 2; break;
    case OpcodeType::STS_MACH_M: STSMMACH(instr), PC += 2; break;
    case OpcodeType::STS_MACL_M: STSMMACL(instr), PC += 2; break;
    case OpcodeType::STS_PR_M: STSMPR(instr), PC += 2; break;

    case OpcodeType::ADD: ADD(instr), PC += 2; break;
    case OpcodeType::ADD_I: ADDI(instr), PC += 2; break;
    case OpcodeType::ADDC: ADDC(instr), PC += 2; break;
    case OpcodeType::ADDV: ADDV(instr), PC += 2; break;
    case OpcodeType::AND_R: AND(instr), PC += 2; break;
    case OpcodeType::AND_I: ANDI(instr), PC += 2; break;
    case OpcodeType::AND_M: ANDM(instr), PC += 2; break;
    case OpcodeType::NEG: NEG(instr), PC += 2; break;
    case OpcodeType::NEGC: NEGC(instr), PC += 2; break;
    case OpcodeType::NOT: NOT(instr), PC += 2; break;
    case OpcodeType::OR_R: OR(instr), PC += 2; break;
    case OpcodeType::OR_I: ORI(instr), PC += 2; break;
    case OpcodeType::OR_M: ORM(instr), PC += 2; break;
    case OpcodeType::ROTCL: ROTCL(instr), PC += 2; break;
    case OpcodeType::ROTCR: ROTCR(instr), PC += 2; break;
    case OpcodeType::ROTL: ROTL(instr), PC += 2; break;
    case OpcodeType::ROTR: ROTR(instr), PC += 2; break;
    case OpcodeType::SHAL: SHAL(instr), PC += 2; break;
    case OpcodeType::SHAR: SHAR(instr), PC += 2; break;
    case OpcodeType::SHLL: SHLL(instr), PC += 2; break;
    case OpcodeType::SHLL2: SHLL2(instr), PC += 2; break;
    case OpcodeType::SHLL8: SHLL8(instr), PC += 2; break;
    case OpcodeType::SHLL16: SHLL16(instr), PC += 2; break;
    case OpcodeType::SHLR: SHLR(instr), PC += 2; break;
    case OpcodeType::SHLR2: SHLR2(instr), PC += 2; break;
    case OpcodeType::SHLR8: SHLR8(instr), PC += 2; break;
    case OpcodeType::SHLR16: SHLR16(instr), PC += 2; break;
    case OpcodeType::SUB: SUB(instr), PC += 2; break;
    case OpcodeType::SUBC: SUBC(instr), PC += 2; break;
    case OpcodeType::SUBV: SUBV(instr), PC += 2; break;
    case OpcodeType::XOR_R: XOR(instr), PC += 2; break;
    case OpcodeType::XOR_I: XORI(instr), PC += 2; break;
    case OpcodeType::XOR_M: XORM(instr), PC += 2; break;

    case OpcodeType::DT: DT(instr), PC += 2; break;

    case OpcodeType::CLRMAC: CLRMAC(), PC += 2; break;
    case OpcodeType::MACW: MACW(instr), PC += 2; break;
    case OpcodeType::MACL: MACL(instr), PC += 2; break;
    case OpcodeType::MUL: MULL(instr), PC += 2; break;
    case OpcodeType::MULS: MULS(instr), PC += 2; break;
    case OpcodeType::MULU: MULU(instr), PC += 2; break;
    case OpcodeType::DMULS: DMULS(instr), PC += 2; break;
    case OpcodeType::DMULU: DMULU(instr), PC += 2; break;

    case OpcodeType::DIV0S: DIV0S(instr), PC += 2; break;
    case OpcodeType::DIV0U: DIV0U(), PC += 2; break;
    case OpcodeType::DIV1: DIV1(instr), PC += 2; break;

    case OpcodeType::CMP_EQ_I: CMPIM(instr), PC += 2; break;
    case OpcodeType::CMP_EQ_R: CMPEQ(instr), PC += 2; break;
    case OpcodeType::CMP_GE: CMPGE(instr), PC += 2; break;
    case OpcodeType::CMP_GT: CMPGT(instr), PC += 2; break;
    case OpcodeType::CMP_HI: CMPHI(instr), PC += 2; break;
    case OpcodeType::CMP_HS: CMPHS(instr), PC += 2; break;
    case OpcodeType::CMP_PL: CMPPL(instr), PC += 2; break;
    case OpcodeType::CMP_PZ: CMPPZ(instr), PC += 2; break;
    case OpcodeType::CMP_STR: CMPSTR(instr), PC += 2; break;
    case OpcodeType::TAS: TAS(instr), PC += 2; break;
    case OpcodeType::TST_R: TST(instr), PC += 2; break;
    case OpcodeType::TST_I: TSTI(instr), PC += 2; break;
    case OpcodeType::TST_M: TSTM(instr), PC += 2; break;

    case OpcodeType::Delay_NOP: NOP(), jumpToDelaySlot(); break;

    case OpcodeType::Delay_SLEEP: SLEEP(), jumpToDelaySlot(); break;

    case OpcodeType::Delay_MOV_R: MOV(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_L: MOVBL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_L: MOVWL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_L: MOVLL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_L0: MOVBL0(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_L0: MOVWL0(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_L0: MOVLL0(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_L4: MOVBL4(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_L4: MOVWL4(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_L4: MOVLL4(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_LG: MOVBLG(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_LG: MOVWLG(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_LG: MOVLLG(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_M: MOVBM(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_M: MOVWM(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_M: MOVLM(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_P: MOVBP(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_P: MOVWP(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_P: MOVLP(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_S: MOVBS(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_S: MOVWS(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_S: MOVLS(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_S0: MOVBS0(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_S0: MOVWS0(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_S0: MOVLS0(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_S4: MOVBS4(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_S4: MOVWS4(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_S4: MOVLS4(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_SG: MOVBSG(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_SG: MOVWSG(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_SG: MOVLSG(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOV_I: MOVI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_I: MOVWI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_I: MOVLI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVA: MOVA(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVT: MOVT(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CLRT: CLRT(), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SETT: SETT(), jumpToDelaySlot(); break;

    case OpcodeType::Delay_EXTUB: EXTUB(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_EXTUW: EXTUW(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_EXTSB: EXTSB(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_EXTSW: EXTSW(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SWAPB: SWAPB(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SWAPW: SWAPW(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XTRCT: XTRCT(instr), jumpToDelaySlot(); break;

    case OpcodeType::Delay_LDC_GBR_R: LDCGBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_SR_R: LDCSR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_VBR_R: LDCVBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_GBR_M: LDCMGBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_SR_M: LDCMSR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_VBR_M: LDCMVBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACH_R: LDSMACH(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACL_R: LDSMACL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_PR_R: LDSPR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACH_M: LDSMMACH(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACL_M: LDSMMACL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_PR_M: LDSMPR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_GBR_R: STCGBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_SR_R: STCSR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_VBR_R: STCVBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_GBR_M: STCMGBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_SR_M: STCMSR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_VBR_M: STCMVBR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACH_R: STSMACH(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACL_R: STSMACL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_PR_R: STSPR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACH_M: STSMMACH(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACL_M: STSMMACL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_PR_M: STSMPR(instr), jumpToDelaySlot(); break;

    case OpcodeType::Delay_ADD: ADD(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ADD_I: ADDI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ADDC: ADDC(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ADDV: ADDV(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_AND_R: AND(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_AND_I: ANDI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_AND_M: ANDM(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_NEG: NEG(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_NEGC: NEGC(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_NOT: NOT(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_OR_R: OR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_OR_I: ORI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_OR_M: ORM(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTCL: ROTCL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTCR: ROTCR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTL: ROTL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTR: ROTR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHAL: SHAL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHAR: SHAR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL: SHLL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL2: SHLL2(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL8: SHLL8(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL16: SHLL16(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR: SHLR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR2: SHLR2(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR8: SHLR8(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR16: SHLR16(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SUB: SUB(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SUBC: SUBC(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SUBV: SUBV(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XOR_R: XOR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XOR_I: XORI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XOR_M: XORM(instr), jumpToDelaySlot(); break;

    case OpcodeType::Delay_DT: DT(instr), jumpToDelaySlot(); break;

    case OpcodeType::Delay_CLRMAC: CLRMAC(), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MACW: MACW(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MACL: MACL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MUL: MULL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MULS: MULS(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MULU: MULU(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DMULS: DMULS(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DMULU: DMULU(instr), jumpToDelaySlot(); break;

    case OpcodeType::Delay_DIV0S: DIV0S(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DIV0U: DIV0U(), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DIV1: DIV1(instr), jumpToDelaySlot(); break;

    case OpcodeType::Delay_CMP_EQ_I: CMPIM(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_EQ_R: CMPEQ(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_GE: CMPGE(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_GT: CMPGT(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_HI: CMPHI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_HS: CMPHS(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_PL: CMPPL(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_PZ: CMPPZ(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_STR: CMPSTR(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TAS: TAS(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TST_R: TST(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TST_I: TSTI(instr), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TST_M: TSTM(instr), jumpToDelaySlot(); break;

    case OpcodeType::BF: BF(instr); break;
    case OpcodeType::BFS: BFS(instr); break;
    case OpcodeType::BT: BT(instr); break;
    case OpcodeType::BTS: BTS(instr); break;
    case OpcodeType::BRA: BRA(instr); break;
    case OpcodeType::BRAF: BRAF(instr); break;
    case OpcodeType::BSR: BSR(instr); break;
    case OpcodeType::BSRF: BSRF(instr); break;
    case OpcodeType::JMP: JMP(instr); break;
    case OpcodeType::JSR: JSR(instr); break;
    case OpcodeType::TRAPA: TRAPA(instr); break;

    case OpcodeType::RTE: RTE(); break;
    case OpcodeType::RTS: RTS(); break;

    case OpcodeType::Illegal: EnterException(xvGenIllegalInstr), dump(); break;
    case OpcodeType::IllegalSlot: EnterException(xvSlotIllegalInstr), dump(); break;
    }
}

// -----------------------------------------------------------------------------
// Instruction interpreters

FORCE_INLINE void SH2::NOP() {
    // dbg_println("nop");
}

FORCE_INLINE void SH2::SLEEP() {
    // dbg_println("sleep");
    PC -= 2;
    // TODO: enter sleep or standby mode depending on SBYCR.SBY
    //__debugbreak();
    m_log.debug("Entering standby");
}

FORCE_INLINE void SH2::MOV(InstrNM instr) {
    // dbg_println("mov r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = R[instr.Rm];
}

FORCE_INLINE void SH2::MOVBL(InstrNM instr) {
    // dbg_println("mov.b @r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<8>(MemReadByte(R[instr.Rm]));
}

FORCE_INLINE void SH2::MOVWL(InstrNM instr) {
    // dbg_println("mov.w @r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<16>(MemReadWord(R[instr.Rm]));
}

FORCE_INLINE void SH2::MOVLL(InstrNM instr) {
    // dbg_println("mov.l @r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = MemReadLong(R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVBL0(InstrNM instr) {
    // dbg_println("mov.b @(r0,r{}), r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<8>(MemReadByte(R[instr.Rm] + R[0]));
}

FORCE_INLINE void SH2::MOVWL0(InstrNM instr) {
    // dbg_println("mov.w @(r0,r{}), r{})", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<16>(MemReadWord(R[instr.Rm] + R[0]));
}

FORCE_INLINE void SH2::MOVLL0(InstrNM instr) {
    // dbg_println("mov.l @(r0,r{}), r{})", instr.Rm, instr.Rn);
    R[instr.Rn] = MemReadLong(R[instr.Rm] + R[0]);
}

FORCE_INLINE void SH2::MOVBL4(InstrMD instr) {
    const uint16 disp = instr.disp;
    // dbg_println("mov.b @(0x{:X},r{}), r0", disp, instr.Rm);
    R[0] = bit::sign_extend<8>(MemReadByte(R[instr.Rm] + disp));
}

FORCE_INLINE void SH2::MOVWL4(InstrMD instr) {
    const uint16 disp = instr.disp << 1u;
    // dbg_println("mov.w @(0x{:X},r{}), r0", disp, instr.Rm);
    R[0] = bit::sign_extend<16>(MemReadWord(R[instr.Rm] + disp));
}

FORCE_INLINE void SH2::MOVLL4(InstrNMD instr) {
    const uint16 disp = instr.disp << 2u;
    // dbg_println("mov.l @(0x{:X},r{}), r{}", disp, rm, rn);
    R[instr.Rn] = MemReadLong(R[instr.Rm] + disp);
}

FORCE_INLINE void SH2::MOVBLG(InstrD instr) {
    const uint16 disp = instr.disp;
    // dbg_println("mov.b @(0x{:X},gbr), r0", disp);
    R[0] = bit::sign_extend<8>(MemReadByte(GBR + disp));
}

FORCE_INLINE void SH2::MOVWLG(InstrD instr) {
    const uint16 disp = instr.disp << 1u;
    // dbg_println("mov.w @(0x{:X},gbr), r0", disp);
    R[0] = bit::sign_extend<16>(MemReadWord(GBR + disp));
}

FORCE_INLINE void SH2::MOVLLG(InstrD instr) {
    const uint16 disp = instr.disp << 2u;
    // dbg_println("mov.l @(0x{:X},gbr), r0", disp);
    R[0] = MemReadLong(GBR + disp);
}

FORCE_INLINE void SH2::MOVBM(InstrNM instr) {
    // dbg_println("mov.b r{}, @-r{}", instr.Rm, instr.Rn);
    MemWriteByte(R[instr.Rn] - 1, R[instr.Rm]);
    R[instr.Rn] -= 1;
}

FORCE_INLINE void SH2::MOVWM(InstrNM instr) {
    // dbg_println("mov.w r{}, @-r{}", instr.Rm, instr.Rn);
    MemWriteWord(R[instr.Rn] - 2, R[instr.Rm]);
    R[instr.Rn] -= 2;
}

FORCE_INLINE void SH2::MOVLM(InstrNM instr) {
    // dbg_println("mov.l r{}, @-r{}", instr.Rm, instr.Rn);
    MemWriteLong(R[instr.Rn] - 4, R[instr.Rm]);
    R[instr.Rn] -= 4;
}

FORCE_INLINE void SH2::MOVBP(InstrNM instr) {
    // dbg_println("mov.b @r{}+, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<8>(MemReadByte(R[instr.Rm]));
    if (instr.Rn != instr.Rm) {
        R[instr.Rm] += 1;
    }
}

FORCE_INLINE void SH2::MOVWP(InstrNM instr) {
    // dbg_println("mov.w @r{}+, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<16>(MemReadWord(R[instr.Rm]));
    if (instr.Rn != instr.Rm) {
        R[instr.Rm] += 2;
    }
}

FORCE_INLINE void SH2::MOVLP(InstrNM instr) {
    // dbg_println("mov.l @r{}+, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = MemReadLong(R[instr.Rm]);
    if (instr.Rn != instr.Rm) {
        R[instr.Rm] += 4;
    }
}

FORCE_INLINE void SH2::MOVBS(InstrNM instr) {
    // dbg_println("mov.b r{}, @r{}", instr.Rm, instr.Rn);
    MemWriteByte(R[instr.Rn], R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVWS(InstrNM instr) {
    // dbg_println("mov.w r{}, @r{}", instr.Rm, instr.Rn);
    MemWriteWord(R[instr.Rn], R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVLS(InstrNM instr) {
    // dbg_println("mov.l r{}, @r{}", instr.Rm, instr.Rn);
    MemWriteLong(R[instr.Rn], R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVBS0(InstrNM instr) {
    // dbg_println("mov.b r{}, @(r0,r{})", instr.Rm, instr.Rn);
    MemWriteByte(R[instr.Rn] + R[0], R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVWS0(InstrNM instr) {
    // dbg_println("mov.w r{}, @(r0,r{})", instr.Rm, instr.Rn);
    MemWriteWord(R[instr.Rn] + R[0], R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVLS0(InstrNM instr) {
    // dbg_println("mov.l r{}, @(r0,r{})", instr.Rm, instr.Rn);
    MemWriteLong(R[instr.Rn] + R[0], R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVBS4(InstrND4 instr) {
    const uint16 disp = instr.disp;
    // dbg_println("mov.b r0, @(0x{:X},r{})", disp, instr.Rn);
    MemWriteByte(R[instr.Rn] + disp, R[0]);
}

FORCE_INLINE void SH2::MOVWS4(InstrND4 instr) {
    const uint16 disp = instr.disp << 1u;
    // dbg_println("mov.w r0, @(0x{:X},r{})", disp, instr.Rn);
    MemWriteWord(R[instr.Rn] + disp, R[0]);
}

FORCE_INLINE void SH2::MOVLS4(InstrNMD instr) {
    const uint16 disp = instr.disp << 2u;
    // dbg_println("mov.l r{}, @(0x{:X},r{})", instr.Rm, disp, instr.Rn);
    MemWriteLong(R[instr.Rn] + disp, R[instr.Rm]);
}

FORCE_INLINE void SH2::MOVBSG(InstrD instr) {
    const uint16 disp = instr.disp;
    // dbg_println("mov.b r0, @(0x{:X},gbr)", disp);
    MemWriteByte(GBR + disp, R[0]);
}

FORCE_INLINE void SH2::MOVWSG(InstrD instr) {
    const uint16 disp = instr.disp << 1u;
    // dbg_println("mov.w r0, @(0x{:X},gbr)", disp);
    MemWriteWord(GBR + disp, R[0]);
}

FORCE_INLINE void SH2::MOVLSG(InstrD instr) {
    const uint16 disp = instr.disp << 2u;
    // dbg_println("mov.l r0, @(0x{:X},gbr)", disp);
    MemWriteLong(GBR + disp, R[0]);
}

FORCE_INLINE void SH2::MOVI(InstrNI instr) {
    sint32 simm = bit::sign_extend<8>(instr.imm);
    // dbg_println("mov #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), instr.Rn);
    R[instr.Rn] = simm;
}

FORCE_INLINE void SH2::MOVWI(InstrND8 instr) {
    const uint16 disp = instr.disp << 1u;
    // dbg_println("mov.w @(0x{:08X},pc), r{}", PC + 4 + disp, instr.Rn);
    R[instr.Rn] = bit::sign_extend<16>(MemReadWord(PC + 4 + disp));
}

FORCE_INLINE void SH2::MOVLI(InstrND8 instr) {
    const uint16 disp = instr.disp << 2u;
    // dbg_println("mov.l @(0x{:08X},pc), r{}", ((PC + 4) & ~3) + disp, instr.Rn);
    R[instr.Rn] = MemReadLong(((PC + 4) & ~3u) + disp);
}

FORCE_INLINE void SH2::MOVA(InstrD instr) {
    const uint16 disp = (instr.disp << 2u) + 4u;
    // dbg_println("mova @(0x{:X},pc), r0", (PC & ~3) + disp);
    R[0] = (PC & ~3) + disp;
}

FORCE_INLINE void SH2::MOVT(InstrN instr) {
    // dbg_println("movt r{}", instr.Rn);
    R[instr.Rn] = SR.T;
}

FORCE_INLINE void SH2::CLRT() {
    // dbg_println("clrt");
    SR.T = 0;
}

FORCE_INLINE void SH2::SETT() {
    // dbg_println("sett");
    SR.T = 1;
}

FORCE_INLINE void SH2::EXTSB(InstrNM instr) {
    // dbg_println("exts.b r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<8>(R[instr.Rm]);
}

FORCE_INLINE void SH2::EXTSW(InstrNM instr) {
    // dbg_println("exts.w r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = bit::sign_extend<16>(R[instr.Rm]);
}

FORCE_INLINE void SH2::EXTUB(InstrNM instr) {
    // dbg_println("extu.b r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = R[instr.Rm] & 0xFF;
}

FORCE_INLINE void SH2::EXTUW(InstrNM instr) {
    // dbg_println("extu.w r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = R[instr.Rm] & 0xFFFF;
}

FORCE_INLINE void SH2::SWAPB(InstrNM instr) {
    // dbg_println("swap.b r{}, r{}", instr.Rm, instr.Rn);

    const uint32 tmp0 = R[instr.Rm] & 0xFFFF0000;
    const uint32 tmp1 = (R[instr.Rm] & 0xFF) << 8u;
    R[instr.Rn] = ((R[instr.Rm] >> 8u) & 0xFF) | tmp1 | tmp0;
}

FORCE_INLINE void SH2::SWAPW(InstrNM instr) {
    // dbg_println("swap.w r{}, r{}", instr.Rm, instr.Rn);

    const uint32 tmp = R[instr.Rm] >> 16u;
    R[instr.Rn] = (R[instr.Rm] << 16u) | tmp;
}

FORCE_INLINE void SH2::XTRCT(InstrNM instr) {
    // dbg_println("xtrct r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = (R[instr.Rn] >> 16u) | (R[instr.Rm] << 16u);
}

FORCE_INLINE void SH2::LDCGBR(InstrM instr) {
    // dbg_println("ldc r{}, gbr", instr.Rm);
    GBR = R[instr.Rm];
}

FORCE_INLINE void SH2::LDCSR(InstrM instr) {
    // dbg_println("ldc r{}, sr", instr.Rm);
    SR.u32 = R[instr.Rm] & 0x000003F3;
}

FORCE_INLINE void SH2::LDCVBR(InstrM instr) {
    // dbg_println("ldc r{}, vbr", instr.Rm);
    VBR = R[instr.Rm];
}

FORCE_INLINE void SH2::LDCMGBR(InstrM instr) {
    // dbg_println("ldc.l @r{}+, gbr", instr.Rm);
    GBR = MemReadLong(R[instr.Rm]);
    R[instr.Rm] += 4;
}

FORCE_INLINE void SH2::LDCMSR(InstrM instr) {
    // dbg_println("ldc.l @r{}+, sr", instr.Rm);
    SR.u32 = MemReadLong(R[instr.Rm]) & 0x000003F3;
    R[instr.Rm] += 4;
}

FORCE_INLINE void SH2::LDCMVBR(InstrM instr) {
    // dbg_println("ldc.l @r{}+, vbr", instr.Rm);
    VBR = MemReadLong(R[instr.Rm]);
    R[instr.Rm] += 4;
}

FORCE_INLINE void SH2::LDSMACH(InstrM instr) {
    // dbg_println("lds r{}, mach", instr.Rm);
    MAC.H = R[instr.Rm];
}

FORCE_INLINE void SH2::LDSMACL(InstrM instr) {
    // dbg_println("lds r{}, macl", instr.Rm);
    MAC.L = R[instr.Rm];
}

FORCE_INLINE void SH2::LDSPR(InstrM instr) {
    // dbg_println("lds r{}, pr", instr.Rm);
    PR = R[instr.Rm];
}

FORCE_INLINE void SH2::LDSMMACH(InstrM instr) {
    // dbg_println("lds.l @r{}+, mach", instr.Rm);
    MAC.H = MemReadLong(R[instr.Rm]);
    R[instr.Rm] += 4;
}

FORCE_INLINE void SH2::LDSMMACL(InstrM instr) {
    // dbg_println("lds.l @r{}+, macl", instr.Rm);
    MAC.L = MemReadLong(R[instr.Rm]);
    R[instr.Rm] += 4;
}

FORCE_INLINE void SH2::LDSMPR(InstrM instr) {
    // dbg_println("lds.l @r{}+, pr", instr.Rm);
    PR = MemReadLong(R[instr.Rm]);
    R[instr.Rm] += 4;
}

FORCE_INLINE void SH2::STCGBR(InstrN instr) {
    // dbg_println("stc gbr, r{}", instr.Rn);
    R[instr.Rn] = GBR;
}

FORCE_INLINE void SH2::STCSR(InstrN instr) {
    // dbg_println("stc sr, r{}", instr.Rn);
    R[instr.Rn] = SR.u32;
}

FORCE_INLINE void SH2::STCVBR(InstrN instr) {
    // dbg_println("stc vbr, r{}", instr.Rn);
    R[instr.Rn] = VBR;
}

FORCE_INLINE void SH2::STCMGBR(InstrN instr) {
    // dbg_println("stc.l gbr, @-r{}", instr.Rn);
    R[instr.Rn] -= 4;
    MemWriteLong(R[instr.Rn], GBR);
}

FORCE_INLINE void SH2::STCMSR(InstrN instr) {
    // dbg_println("stc.l sr, @-r{}", instr.Rn);
    R[instr.Rn] -= 4;
    MemWriteLong(R[instr.Rn], SR.u32);
}

FORCE_INLINE void SH2::STCMVBR(InstrN instr) {
    // dbg_println("stc.l vbr, @-r{}", instr.Rn);
    R[instr.Rn] -= 4;
    MemWriteLong(R[instr.Rn], VBR);
}

FORCE_INLINE void SH2::STSMACH(InstrN instr) {
    // dbg_println("sts mach, r{}", instr.Rn);
    R[instr.Rn] = MAC.H;
}

FORCE_INLINE void SH2::STSMACL(InstrN instr) {
    // dbg_println("sts macl, r{}", instr.Rn);
    R[instr.Rn] = MAC.L;
}

FORCE_INLINE void SH2::STSPR(InstrN instr) {
    // dbg_println("sts pr, r{}", instr.Rn);
    R[instr.Rn] = PR;
}

FORCE_INLINE void SH2::STSMMACH(InstrN instr) {
    // dbg_println("sts.l mach, @-r{}", instr.Rn);
    R[instr.Rn] -= 4;
    MemWriteLong(R[instr.Rn], MAC.H);
}

FORCE_INLINE void SH2::STSMMACL(InstrN instr) {
    // dbg_println("sts.l macl, @-r{}", instr.Rn);
    R[instr.Rn] -= 4;
    MemWriteLong(R[instr.Rn], MAC.L);
}

FORCE_INLINE void SH2::STSMPR(InstrN instr) {
    // dbg_println("sts.l pr, @-r{}", instr.Rn);
    R[instr.Rn] -= 4;
    MemWriteLong(R[instr.Rn], PR);
}

FORCE_INLINE void SH2::ADD(InstrNM instr) {
    // dbg_println("add r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] += R[instr.Rm];
}

FORCE_INLINE void SH2::ADDI(InstrNI instr) {
    const sint32 simm = bit::sign_extend<8>(instr.imm);
    // dbg_println("add #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), instr.Rn);
    R[instr.Rn] += simm;
}

FORCE_INLINE void SH2::ADDC(InstrNM instr) {
    // dbg_println("addc r{}, r{}", instr.Rm, instr.Rn);
    const uint32 tmp1 = R[instr.Rn] + R[instr.Rm];
    const uint32 tmp0 = R[instr.Rn];
    R[instr.Rn] = tmp1 + SR.T;
    SR.T = (tmp0 > tmp1) || (tmp1 > R[instr.Rn]);
}

FORCE_INLINE void SH2::ADDV(InstrNM instr) {
    // dbg_println("addv r{}, r{}", instr.Rm, instr.Rn);

    const bool dst = static_cast<sint32>(R[instr.Rn]) < 0;
    const bool src = static_cast<sint32>(R[instr.Rm]) < 0;

    R[instr.Rn] += R[instr.Rm];

    bool ans = static_cast<sint32>(R[instr.Rn]) < 0;
    ans ^= dst;
    SR.T = (src == dst) & ans;
}

FORCE_INLINE void SH2::AND(InstrNM instr) {
    // dbg_println("and r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] &= R[instr.Rm];
}

FORCE_INLINE void SH2::ANDI(InstrI instr) {
    // dbg_println("and #0x{:X}, r0", instr.imm);
    R[0] &= instr.imm;
}

FORCE_INLINE void SH2::ANDM(InstrI instr) {
    // dbg_println("and.b #0x{:X}, @(r0,gbr)", instr.imm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp &= instr.imm;
    MemWriteByte(GBR + R[0], tmp);
}

FORCE_INLINE void SH2::NEG(InstrNM instr) {
    // dbg_println("neg r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = -R[instr.Rm];
}

FORCE_INLINE void SH2::NEGC(InstrNM instr) {
    // dbg_println("negc r{}, r{}", instr.Rm, instr.Rn);
    const uint32 tmp = -R[instr.Rm];
    R[instr.Rn] = tmp - SR.T;
    SR.T = (0 < tmp) || (tmp < R[instr.Rn]);
}

FORCE_INLINE void SH2::NOT(InstrNM instr) {
    // dbg_println("not r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] = ~R[instr.Rm];
}

FORCE_INLINE void SH2::OR(InstrNM instr) {
    // dbg_println("or r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] |= R[instr.Rm];
}

FORCE_INLINE void SH2::ORI(InstrI instr) {
    // dbg_println("or #0x{:X}, r0", instr.imm);
    R[0] |= instr.imm;
}

FORCE_INLINE void SH2::ORM(InstrI instr) {
    // dbg_println("or.b #0x{:X}, @(r0,gbr)", instr.imm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp |= instr.imm;
    MemWriteByte(GBR + R[0], tmp);
}

FORCE_INLINE void SH2::ROTCL(InstrN instr) {
    // dbg_println("rotcl r{}", instr.Rn);
    const bool tmp = R[instr.Rn] >> 31u;
    R[instr.Rn] = (R[instr.Rn] << 1u) | SR.T;
    SR.T = tmp;
}

FORCE_INLINE void SH2::ROTCR(InstrN instr) {
    // dbg_println("rotcr r{}", instr.Rn);
    const bool tmp = R[instr.Rn] & 1u;
    R[instr.Rn] = (R[instr.Rn] >> 1u) | (SR.T << 31u);
    SR.T = tmp;
}

FORCE_INLINE void SH2::ROTL(InstrN instr) {
    // dbg_println("rotl r{}", instr.Rn);
    SR.T = R[instr.Rn] >> 31u;
    R[instr.Rn] = (R[instr.Rn] << 1u) | SR.T;
}

FORCE_INLINE void SH2::ROTR(InstrN instr) {
    // dbg_println("rotr r{}", instr.Rn);
    SR.T = R[instr.Rn] & 1u;
    R[instr.Rn] = (R[instr.Rn] >> 1u) | (SR.T << 31u);
}

FORCE_INLINE void SH2::SHAL(InstrN instr) {
    // dbg_println("shal r{}", instr.Rn);
    SR.T = R[instr.Rn] >> 31u;
    R[instr.Rn] <<= 1u;
}

FORCE_INLINE void SH2::SHAR(InstrN instr) {
    // dbg_println("shar r{}", instr.Rn);
    SR.T = R[instr.Rn] & 1u;
    R[instr.Rn] = static_cast<sint32>(R[instr.Rn]) >> 1;
}

FORCE_INLINE void SH2::SHLL(InstrN instr) {
    // dbg_println("shll r{}", instr.Rn);
    SR.T = R[instr.Rn] >> 31u;
    R[instr.Rn] <<= 1u;
}

FORCE_INLINE void SH2::SHLL2(InstrN instr) {
    // dbg_println("shll2 r{}", instr.Rn);
    R[instr.Rn] <<= 2u;
}

FORCE_INLINE void SH2::SHLL8(InstrN instr) {
    // dbg_println("shll8 r{}", instr.Rn);
    R[instr.Rn] <<= 8u;
}

FORCE_INLINE void SH2::SHLL16(InstrN instr) {
    // dbg_println("shll16 r{}", instr.Rn);
    R[instr.Rn] <<= 16u;
}

FORCE_INLINE void SH2::SHLR(InstrN instr) {
    // dbg_println("shlr r{}", instr.Rn);
    SR.T = R[instr.Rn] & 1u;
    R[instr.Rn] >>= 1u;
}

FORCE_INLINE void SH2::SHLR2(InstrN instr) {
    // dbg_println("shlr2 r{}", instr.Rn);
    R[instr.Rn] >>= 2u;
}

FORCE_INLINE void SH2::SHLR8(InstrN instr) {
    // dbg_println("shlr8 r{}", instr.Rn);
    R[instr.Rn] >>= 8u;
}

FORCE_INLINE void SH2::SHLR16(InstrN instr) {
    // dbg_println("shlr16 r{}", instr.Rn);
    R[instr.Rn] >>= 16u;
}

FORCE_INLINE void SH2::SUB(InstrNM instr) {
    // dbg_println("sub r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] -= R[instr.Rm];
}

FORCE_INLINE void SH2::SUBC(InstrNM instr) {
    // dbg_println("subc r{}, r{}", instr.Rm, instr.Rn);
    const uint32 tmp1 = R[instr.Rn] - R[instr.Rm];
    const uint32 tmp0 = R[instr.Rn];
    R[instr.Rn] = tmp1 - SR.T;
    SR.T = (tmp0 < tmp1) || (tmp1 < R[instr.Rn]);
}

FORCE_INLINE void SH2::SUBV(InstrNM instr) {
    // dbg_println("subv r{}, r{}", instr.Rm, instr.Rn);

    const bool dst = static_cast<sint32>(R[instr.Rn]) < 0;
    const bool src = static_cast<sint32>(R[instr.Rm]) < 0;

    R[instr.Rn] -= R[instr.Rm];

    bool ans = static_cast<sint32>(R[instr.Rn]) < 0;
    ans ^= dst;
    SR.T = (src != dst) & ans;
}

FORCE_INLINE void SH2::XOR(InstrNM instr) {
    // dbg_println("xor r{}, r{}", instr.Rm, instr.Rn);
    R[instr.Rn] ^= R[instr.Rm];
}

FORCE_INLINE void SH2::XORI(InstrI instr) {
    // dbg_println("xor #0x{:X}, r0", instr.imm);
    R[0] ^= instr.imm;
}

FORCE_INLINE void SH2::XORM(InstrI instr) {
    // dbg_println("xor.b #0x{:X}, @(r0,gbr)", instr.imm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp ^= instr.imm;
    MemWriteByte(GBR + R[0], tmp);
}

FORCE_INLINE void SH2::DT(InstrN instr) {
    // dbg_println("dt r{}", instr.Rn);
    R[instr.Rn]--;
    SR.T = R[instr.Rn] == 0;
}

FORCE_INLINE void SH2::CLRMAC() {
    // dbg_println("clrmac");
    MAC.u64 = 0;
}

FORCE_INLINE void SH2::MACW(InstrNM instr) {
    // dbg_println("mac.w @r{}+, @r{}+)", instr.Rm, instr.Rn);

    const sint32 op1 = static_cast<sint16>(MemReadWord(R[instr.Rm]));
    R[instr.Rm] += 2;
    const sint32 op2 = static_cast<sint16>(MemReadWord(R[instr.Rn]));
    R[instr.Rn] += 2;

    const sint32 mul = op1 * op2;
    if (SR.S) {
        const sint64 result = static_cast<sint64>(static_cast<sint32>(MAC.L)) + mul;
        const sint32 saturatedResult = std::clamp<sint64>(result, -0x80000000LL, 0x7FFFFFFFLL);
        if (result == saturatedResult) {
            MAC.L = result;
        } else {
            MAC.L = saturatedResult;
            MAC.H |= 1;
        }
    } else {
        MAC.u64 += mul;
    }
}

FORCE_INLINE void SH2::MACL(InstrNM instr) {
    // dbg_println("mac.l @r{}+, @r{}+)", instr.Rm, instr.Rn);

    const sint64 op1 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[instr.Rm])));
    R[instr.Rm] += 4;
    const sint64 op2 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[instr.Rn])));
    R[instr.Rn] += 4;

    const sint64 mul = op1 * op2;
    sint64 result = mul + MAC.u64;
    if (SR.S && result > 0x00007FFFFFFFFFFFull && result < 0xFFFF800000000000ull) {
        if (static_cast<sint32>(op1 ^ op2) < 0) {
            result = 0xFFFF800000000000ull;
        } else {
            result = 0x00007FFFFFFFFFFFull;
        }
    }
    MAC.u64 = result;
}

FORCE_INLINE void SH2::MULL(InstrNM instr) {
    // dbg_println("mul.l r{}, r{}", instr.Rm, instr.Rn);
    MAC.L = R[instr.Rm] * R[instr.Rn];
}

FORCE_INLINE void SH2::MULS(InstrNM instr) {
    // dbg_println("muls.w r{}, r{}", instr.Rm, instr.Rn);
    MAC.L = bit::sign_extend<16>(R[instr.Rm]) * bit::sign_extend<16>(R[instr.Rn]);
}

FORCE_INLINE void SH2::MULU(InstrNM instr) {
    // dbg_println("mulu.w r{}, r{}", instr.Rm, instr.Rn);
    auto cast = [](uint32 val) { return static_cast<uint32>(static_cast<uint16>(val)); };
    MAC.L = cast(R[instr.Rm]) * cast(R[instr.Rn]);
}

FORCE_INLINE void SH2::DMULS(InstrNM instr) {
    // dbg_println("dmuls.l r{}, r{}", instr.Rm, instr.Rn);
    auto cast = [](uint32 val) { return static_cast<sint64>(static_cast<sint32>(val)); };
    MAC.u64 = cast(R[instr.Rm]) * cast(R[instr.Rn]);
}

FORCE_INLINE void SH2::DMULU(InstrNM instr) {
    // dbg_println("dmulu.l r{}, r{}", instr.Rm, instr.Rn);
    MAC.u64 = static_cast<uint64>(R[instr.Rm]) * static_cast<uint64>(R[instr.Rn]);
}

FORCE_INLINE void SH2::DIV0S(InstrNM instr) {
    // dbg_println("div0s r{}, r{}", instr.Rm, instr.Rn);
    SR.M = static_cast<sint32>(R[instr.Rm]) < 0;
    SR.Q = static_cast<sint32>(R[instr.Rn]) < 0;
    SR.T = SR.M != SR.Q;
}

FORCE_INLINE void SH2::DIV0U() {
    // dbg_println("div0u");
    SR.M = 0;
    SR.Q = 0;
    SR.T = 0;
}

FORCE_INLINE void SH2::DIV1(InstrNM instr) {
    // dbg_println("div1 r{}, r{}", instr.Rm, instr.Rn);

    const bool oldQ = SR.Q;
    SR.Q = static_cast<sint32>(R[instr.Rn]) < 0;
    R[instr.Rn] = (R[instr.Rn] << 1u) | SR.T;

    const uint32 prevVal = R[instr.Rn];
    if (oldQ == SR.M) {
        R[instr.Rn] -= R[instr.Rm];
    } else {
        R[instr.Rn] += R[instr.Rm];
    }

    if (oldQ) {
        if (SR.M) {
            SR.Q ^= R[instr.Rn] <= prevVal;
        } else {
            SR.Q ^= R[instr.Rn] < prevVal;
        }
    } else {
        if (SR.M) {
            SR.Q ^= R[instr.Rn] >= prevVal;
        } else {
            SR.Q ^= R[instr.Rn] > prevVal;
        }
    }

    SR.T = SR.Q == SR.M;
}

FORCE_INLINE void SH2::CMPIM(InstrI instr) {
    const sint32 simm = bit::sign_extend<8>(instr.imm);
    // dbg_println("cmp/eq #{}0x{:X}, r0", (simm < 0 ? "-" : ""), abs(simm));
    SR.T = static_cast<sint32>(R[0]) == simm;
}

FORCE_INLINE void SH2::CMPEQ(InstrNM instr) {
    // dbg_println("cmp/eq r{}, r{}", instr.Rm, instr.Rn);
    SR.T = R[instr.Rn] == R[instr.Rm];
}

FORCE_INLINE void SH2::CMPGE(InstrNM instr) {
    // dbg_println("cmp/ge r{}, r{}", instr.Rm, instr.Rn);
    SR.T = static_cast<sint32>(R[instr.Rn]) >= static_cast<sint32>(R[instr.Rm]);
}

FORCE_INLINE void SH2::CMPGT(InstrNM instr) {
    // dbg_println("cmp/gt r{}, r{}", instr.Rm, instr.Rn);
    SR.T = static_cast<sint32>(R[instr.Rn]) > static_cast<sint32>(R[instr.Rm]);
}

FORCE_INLINE void SH2::CMPHI(InstrNM instr) {
    // dbg_println("cmp/hi r{}, r{}", instr.Rm, instr.Rn);
    SR.T = R[instr.Rn] > R[instr.Rm];
}

FORCE_INLINE void SH2::CMPHS(InstrNM instr) {
    // dbg_println("cmp/hs r{}, r{}", instr.Rm, instr.Rn);
    SR.T = R[instr.Rn] >= R[instr.Rm];
}

FORCE_INLINE void SH2::CMPPL(InstrN instr) {
    // dbg_println("cmp/pl r{}", instr.Rn);
    SR.T = static_cast<sint32>(R[instr.Rn]) > 0;
}

FORCE_INLINE void SH2::CMPPZ(InstrN instr) {
    // dbg_println("cmp/pz r{}", instr.Rn);
    SR.T = static_cast<sint32>(R[instr.Rn]) >= 0;
}

FORCE_INLINE void SH2::CMPSTR(InstrNM instr) {
    // dbg_println("cmp/str r{}, r{}", instr.Rm, instr.Rn);
    const uint32 tmp = R[instr.Rm] ^ R[instr.Rn];
    const uint8 hh = tmp >> 24u;
    const uint8 hl = tmp >> 16u;
    const uint8 lh = tmp >> 8u;
    const uint8 ll = tmp >> 0u;
    SR.T = !(hh && hl && lh && ll);
}

FORCE_INLINE void SH2::TAS(InstrN instr) {
    // dbg_println("tas.b @r{}", instr.Rn);
    // dbg_println("WARNING: bus lock not implemented!");

    // TODO: enable bus lock on this read
    const uint8 tmp = MemReadByte(R[instr.Rn]);
    SR.T = tmp == 0;
    // TODO: disable bus lock on this write
    MemWriteByte(R[instr.Rn], tmp | 0x80);
}

FORCE_INLINE void SH2::TST(InstrNM instr) {
    // dbg_println("tst r{}, r{}", instr.Rm, instr.Rn);
    SR.T = (R[instr.Rn] & R[instr.Rm]) == 0;
}

FORCE_INLINE void SH2::TSTI(InstrI instr) {
    // dbg_println("tst #0x{:X}, r0", instr.imm);
    SR.T = (R[0] & instr.imm) == 0;
}

FORCE_INLINE void SH2::TSTM(InstrI instr) {
    // dbg_println("tst.b #0x{:X}, @(r0,gbr)", instr.imm);
    const uint8 tmp = MemReadByte(GBR + R[0]);
    SR.T = (tmp & instr.imm) == 0;
}

FORCE_INLINE void SH2::BF(InstrD instr) {
    const sint32 sdisp = (bit::sign_extend<8>(instr.disp) << 1) + 4;
    // dbg_println("bf 0x{:08X}", PC + sdisp);

    if (!SR.T) {
        PC += sdisp;
    } else {
        PC += 2;
    }
}

FORCE_INLINE void SH2::BFS(InstrD instr) {
    const sint32 sdisp = (bit::sign_extend<8>(instr.disp) << 1) + 4;
    // dbg_println("bf/s 0x{:08X}", PC + sdisp);

    if (!SR.T) {
        SetupDelaySlot(PC + sdisp);
    }
    PC += 2;
}

FORCE_INLINE void SH2::BT(InstrD instr) {
    const sint32 sdisp = (bit::sign_extend<8>(instr.disp) << 1) + 4;
    // dbg_println("bt 0x{:08X}", PC + sdisp);

    if (SR.T) {
        PC += sdisp;
    } else {
        PC += 2;
    }
}

FORCE_INLINE void SH2::BTS(InstrD instr) {
    const sint32 sdisp = (bit::sign_extend<8>(instr.disp) << 1) + 4;
    // dbg_println("bt/s 0x{:08X}", PC + sdisp);

    if (SR.T) {
        SetupDelaySlot(PC + sdisp);
    }
    PC += 2;
}

FORCE_INLINE void SH2::BRA(InstrD12 instr) {
    const sint32 sdisp = (bit::sign_extend<12>(instr.disp) << 1) + 4;
    // dbg_println("bra 0x{:08X}", PC + sdisp);
    SetupDelaySlot(PC + sdisp);
    PC += 2;
}

FORCE_INLINE void SH2::BRAF(InstrM instr) {
    // dbg_println("braf r{}", instr.Rm);
    SetupDelaySlot(PC + R[instr.Rm] + 4);
    PC += 2;
}

FORCE_INLINE void SH2::BSR(InstrD12 instr) {
    m_tracer.BSR({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    const sint32 sdisp = (bit::sign_extend<12>(instr.disp) << 1) + 4;
    // dbg_println("bsr 0x{:08X}", PC + sdisp);

    PR = PC + 4;
    SetupDelaySlot(PC + sdisp);
    PC += 2;
}

FORCE_INLINE void SH2::BSRF(InstrM instr) {
    m_tracer.BSR({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    // dbg_println("bsrf r{}", instr.Rm);
    PR = PC + 4;
    SetupDelaySlot(PC + R[instr.Rm] + 4);
    PC += 2;
}

FORCE_INLINE void SH2::JMP(InstrM instr) {
    // dbg_println("jmp @r{}", instr.Rm);
    SetupDelaySlot(R[instr.Rm]);
    PC += 2;
}

FORCE_INLINE void SH2::JSR(InstrM instr) {
    // dbg_println("jsr @r{}", instr.Rm);
    m_tracer.JSR({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    PR = PC + 4;
    SetupDelaySlot(R[instr.Rm]);
    PC += 2;
}

FORCE_INLINE void SH2::TRAPA(InstrI instr) {
    m_tracer.TRAPA({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    // dbg_println("trapa #0x{:X}", instr.imm);
    R[15] -= 4;
    MemWriteLong(R[15], SR.u32);
    R[15] -= 4;
    MemWriteLong(R[15], PC - 2);
    PC = MemReadLong(VBR + (instr.imm << 2u));
}

FORCE_INLINE void SH2::RTE() {
    m_tracer.RTE({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    // dbg_println("rte");
    SetupDelaySlot(MemReadLong(R[15]) + 4);
    PC += 2;
    R[15] += 4;
    SR.u32 = MemReadLong(R[15]) & 0x000003F3;
    R[15] += 4;
}

FORCE_INLINE void SH2::RTS() {
    m_tracer.RTS({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    // dbg_println("rts");
    SetupDelaySlot(PR);
    PC += 2;
}

} // namespace satemu::sh2

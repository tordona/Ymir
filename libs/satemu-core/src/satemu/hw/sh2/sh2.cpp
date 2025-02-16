#include <satemu/hw/sh2/sh2.hpp>

#include <satemu/hw/sh2/sh2_bus.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>
#include <satemu/util/unreachable.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>

namespace satemu::sh2 {

namespace config {
    // Detect and log SYS_EXECDMP invocations.
    // The addres is specified by sysExecDumpAddress;
    inline constexpr bool logSysExecDump = false;

    // Address of SYS_EXECDMP function.
    // 0x186C is valid in most BIOS images.
    // 0x197C on JP (v1.003)
    inline constexpr uint32 sysExecDumpAddress = 0x186C;
} // namespace config

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
    : m_tracer(master)
    , m_log(Logger(master))
    , m_bus(bus) {
    BCR1.MASTER = !master;
    Reset(true);
}

void SH2::Reset(bool hard, bool watchdogInitiated) {
    // Initial values:
    // - R0-R14 = undefined
    // - R15 = ReadLong(0x00000004)  [NOTE: ignores VBR]

    // - SR = bits I3-I0 set, reserved bits clear, the rest is undefined
    // - GBR = undefined
    // - VBR = 0x00000000

    // - MACH, MACL = undefined
    // - PR = undefined
    // - PC = ReadLong(0x00000000)  [NOTE: ignores VBR]

    // On-chip peripherals:
    // - BSC, USB and FMR are not reset on power-on/hard reset
    // - all other modules reset always

    R.fill(0);
    PR = 0;

    MAC.u64 = 0;

    SR.u32 = 0;
    SR.I0 = SR.I1 = SR.I2 = SR.I3 = 1;
    GBR = 0;
    VBR = 0x00000000;

    PC = MemReadLong(0x00000000);
    R[15] = MemReadLong(0x00000004);

    // On-chip registers
    BCR1.u15 = 0x03F0;
    BCR2.u16 = 0x00FC;
    WCR.u16 = 0xAAFF;
    MCR.u16 = 0x0000;
    RTCSR.u16 = 0x0000;
    RTCNT = 0x0000;
    RTCOR = 0x0000;

    DMAOR.u32 = 0x00000000;
    for (auto &ch : m_dmaChannels) {
        ch.Reset();
    }

    WDT.Reset(watchdogInitiated);

    SBYCR.u8 = 0x00;

    DVSR = 0x0;  // undefined initial value
    DVDNT = 0x0; // undefined initial value
    DVCR.u32 = 0x00000000;
    DVDNTH = 0x0;  // undefined initial value
    DVDNTL = 0x0;  // undefined initial value
    DVDNTUH = 0x0; // undefined initial value
    DVDNTUL = 0x0; // undefined initial value

    FRT.Reset();

    ICR.u16 = 0x0000;

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

    m_delaySlotTarget = 0;
    m_delaySlot = false;

    WriteCCR(0x00);
    m_cacheEntries.fill({});

    m_tracer.Reset();
}

template <bool debug>
FLATTEN void SH2::Advance(uint64 cycles) {
    while (cycles > 0) {
        uint64 cyclesToRun = cycles;

        if (WDT.WTCSR.TME) {
            cyclesToRun = std::min(cyclesToRun, WDT.CyclesUntilNextTick());
        }
        // TODO: skip FRT updates if interrupt disabled
        // - update on reads
        // - needs to keep track of global cycle count to update properly
        cyclesToRun = std::min(cyclesToRun, FRT.CyclesUntilNextTick());

        cycles -= cyclesToRun;

        AdvanceWDT(cyclesToRun);
        AdvanceFRT(cyclesToRun);

        for (uint64 cy = 0; cy < cyclesToRun; cy++) {
            /*auto bit = [](bool value, std::string_view bit) { return value ? fmt::format(" {}", bit) : ""; };

            dbg_println(" R0 = {:08X}   R4 = {:08X}   R8 = {:08X}  R12 = {:08X}", R[0], R[4], R[8], R[12]);
            dbg_println(" R1 = {:08X}   R5 = {:08X}   R9 = {:08X}  R13 = {:08X}", R[1], R[5], R[9], R[13]);
            dbg_println(" R2 = {:08X}   R6 = {:08X}  R10 = {:08X}  R14 = {:08X}", R[2], R[6], R[10], R[14]);
            dbg_println(" R3 = {:08X}   R7 = {:08X}  R11 = {:08X}  R15 = {:08X}", R[3], R[7], R[11], R[15]);
            dbg_println("GBR = {:08X}  VBR = {:08X}  MAC = {:08X}.{:08X}", GBR, VBR, MAC.H, MAC.L);
            dbg_println(" PC = {:08X}   PR = {:08X}   SR = {:08X} {}{}{}{}{}{}{}{}", PC, PR, SR.u32, bit(SR.M, "M"),
                        bit(SR.Q, "Q"), bit(SR.I3, "I3"), bit(SR.I2, "I2"), bit(SR.I1, "I1"), bit(SR.I0, "I0"),
            bit(SR.S, "S"), bit(SR.T, "T"));*/

            // TODO: choose between interpreter (cached or uncached) and JIT recompiler
            // TODO: proper instruction cycle counting
            m_tracer.ExecTrace({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
            Execute();

            if constexpr (config::logSysExecDump) {
                // Dump stack trace on SYS_EXECDMP
                if ((PC & 0x7FFFFFF) == config::sysExecDumpAddress) {
                    m_log.debug("SYS_EXECDMP triggered");
                    m_tracer.UserCapture({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
                    m_tracer.Dump();
                    m_tracer.Reset();
                }
            }

            // dbg_println("");
        }
    }
}

template void SH2::Advance<false>(uint64 cycles);
template void SH2::Advance<true>(uint64 cycles);

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
            m_log.trace("Unhandled {}-bit SH-2 cached area read from {:08X}", sizeof(T) * 8, address);
            // TODO: use cache
        }
        // fallthrough
    case 0b001:
    case 0b101: // cache-through
        return m_bus.Read<T>(address & 0x7FFFFFF);
    case 0b010: // associative purge
    {
        const uint32 index = bit::extract<4, 9>(address);
        const uint32 tagAddress = bit::extract<10, 28>(address);
        auto &entry = m_cacheEntries[index];
        for (auto &tag : entry.tag) {
            /*if (tag.tagAddress == tagAddress) {
                tag.valid = false;
            }*/
            tag.valid &= tag.tagAddress != tagAddress;
        }
        m_log.trace("{}-bit SH-2 associative purge read from {:08X}", sizeof(T) * 8, address);
        return (address & 1) ? static_cast<T>(0x12231223) : static_cast<T>(0x23122312);
    }
    case 0b011: // cache address array
    {
        const uint32 index = bit::extract<4, 9>(address);
        const auto &entry = m_cacheEntries[index];
        const T value = entry.tag[CCR.Wn].u32;
        m_log.trace("{}-bit SH-2 cache address array read from {:08X} = {:X}", sizeof(T) * 8, address, value);
        return value;
    }
    case 0b100:
    case 0b110: // cache data array
    {
        const uint32 index = bit::extract<4, 9>(address);
        const uint32 way = bit::extract<10, 12>(address);
        const uint32 byte = (bit::extract<0, 3>(address) & ~(sizeof(T) - 1)) ^ (4 - sizeof(T));
        const auto &entry = m_cacheEntries[index];
        const auto &line = entry.line[way];
        const T value = util::ReadBE<T>(&line[byte]);
        m_log.trace("{}-bit SH-2 cache data array read from {:08X} = {:X}", sizeof(T) * 8, address, value);
        return value;
    }
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
            m_log.trace("Unhandled {}-bit SH-2 cached area write to {:08X} = {:X}", sizeof(T) * 8, address, value);
            // TODO: use cache
        }
        // fallthrough
    case 0b001:
    case 0b101: // cache-through
        m_bus.Write<T>(address & 0x7FFFFFF, value);
        break;
    case 0b010: // associative purge
    {
        const uint32 index = bit::extract<4, 9>(address);
        const uint32 tagAddress = bit::extract<10, 28>(address);
        auto &entry = m_cacheEntries[index];
        for (auto &tag : entry.tag) {
            /*if (tag.tagAddress == tagAddress) {
                tag.valid = false;
            }*/
            tag.valid &= tag.tagAddress != tagAddress;
        }
        m_log.trace("{}-bit SH-2 associative purge write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    }
    case 0b011: // cache address array
    {
        const uint32 index = bit::extract<4, 9>(address);
        auto &entry = m_cacheEntries[index];
        entry.tag[CCR.Wn].u32 = address & 0x1FFFFFF4;
        m_log.trace("{}-bit SH-2 cache address array write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    }
    case 0b100:
    case 0b110: // cache data array
    {
        const uint32 index = bit::extract<4, 9>(address);
        const uint32 way = bit::extract<10, 12>(address);
        const uint32 byte = (bit::extract<0, 3>(address) & ~(sizeof(T) - 1)) ^ (4 - sizeof(T));
        auto &entry = m_cacheEntries[index];
        auto &line = entry.line[way];
        util::WriteBE<T>(&line[byte], value);
        m_log.trace("{}-bit SH-2 cache data array write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    }
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

    case 0x71: return m_dmaChannels[0].ReadDRCR();
    case 0x72: return m_dmaChannels[1].ReadDRCR();

    case 0x80: return WDT.ReadWTCSR();
    case 0x81: return WDT.ReadWTCNT();
    case 0x83: return WDT.ReadRSTCSR();

    case 0x91: return SBYCR.u8;
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

    case 0x180: return m_dmaChannels[0].srcAddress;
    case 0x184: return m_dmaChannels[0].dstAddress;
    case 0x188: return m_dmaChannels[0].xferCount;
    case 0x18C: return m_dmaChannels[0].ReadCHCR();

    case 0x190: return m_dmaChannels[1].srcAddress;
    case 0x194: return m_dmaChannels[1].dstAddress;
    case 0x198: return m_dmaChannels[1].xferCount;
    case 0x19C: return m_dmaChannels[1].ReadCHCR();

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

    case 0x71: m_dmaChannels[0].WriteDRCR(value); break;
    case 0x72: m_dmaChannels[1].WriteDRCR(value); break;

    case 0x91: SBYCR.u8 = value & 0xDF; break;
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

    case 0x80:
        if ((value >> 8u) == 0x5A) {
            WDT.WriteWTCNT(value);
        } else if ((value >> 8u) == 0xA5) {
            WDT.WriteWTCSR(value);
        }
        break;
    case 0x82:
        if ((value >> 8u) == 0x5A) {
            WDT.WriteRSTE_RSTS(value);
        } else if ((value >> 8u) == 0xA5) {
            WDT.WriteWOVF(value);
        }
        break;

    case 0x92: WriteCCR(value); break;

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

    case 0x180: m_dmaChannels[0].srcAddress = value; break;
    case 0x184: m_dmaChannels[0].dstAddress = value; break;
    case 0x188: m_dmaChannels[0].xferCount = bit::extract<0, 23>(value); break;
    case 0x18C:
        m_dmaChannels[0].WriteCHCR(value);
        RunDMAC(0); // TODO: should be scheduled
        break;

    case 0x190: m_dmaChannels[1].srcAddress = value; break;
    case 0x194: m_dmaChannels[1].dstAddress = value; break;
    case 0x198: m_dmaChannels[1].xferCount = bit::extract<0, 23>(value); break;
    case 0x19C:
        m_dmaChannels[1].WriteCHCR(value);
        RunDMAC(1); // TODO: should be scheduled
        break;

    case 0x1A0: SetInterruptVector(InterruptSource::DMAC0_XferEnd, bit::extract<0, 6>(value)); break;
    case 0x1A8: SetInterruptVector(InterruptSource::DMAC1_XferEnd, bit::extract<0, 6>(value)); break;

    case 0x1B0:
        DMAOR.DME = bit::extract<0>(value);
        DMAOR.NMIF &= bit::extract<1>(value);
        DMAOR.AE &= bit::extract<2>(value);
        DMAOR.PR = bit::extract<3>(value);
        RunDMAC(0); // TODO: should be scheduled
        RunDMAC(1); // TODO: should be scheduled
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

FLATTEN FORCE_INLINE bool SH2::IsDMATransferActive(const DMAChannel &ch) const {
    return ch.IsEnabled() && DMAOR.DME && !DMAOR.NMIF && !DMAOR.AE;
}

void SH2::RunDMAC(uint32 channel) {
    auto &ch = m_dmaChannels[channel];

    if (!IsDMATransferActive(ch)) {
        return;
    }

    do {
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
                return;
            }
        }

        // TODO: prioritize channels based on DMAOR.PR
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
            m_log.trace("DMAC{} 8-bit transfer from {:08X} to {:08X} -> {:X}", channel, ch.srcAddress, ch.dstAddress,
                        value);
            MemWriteByte(ch.dstAddress, value);
            break;
        }
        case DMATransferSize::Word: {
            const uint16 value = MemReadWord(ch.srcAddress);
            m_log.trace("DMAC{} 16-bit transfer from {:08X} to {:08X} -> {:X}", channel, ch.srcAddress, ch.dstAddress,
                        value);
            MemWriteWord(ch.dstAddress, value);
            break;
        }
        case DMATransferSize::Longword: {
            const uint32 value = MemReadLong(ch.srcAddress);
            m_log.trace("DMAC{} 32-bit transfer from {:08X} to {:08X} -> {:X}", channel, ch.srcAddress, ch.dstAddress,
                        value);
            MemWriteLong(ch.dstAddress, value);
            break;
        }
        case DMATransferSize::QuadLongword:
            for (int i = 0; i < 4; i++) {
                const uint32 value = MemReadLong(ch.srcAddress + i * sizeof(uint32));
                m_log.trace("DMAC{} 16-byte transfer {:d} from {:08X} to {:08X} -> {:X}", channel, i, ch.srcAddress,
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
                m_log.trace("DMAC{} 16-byte transfer count misaligned", channel);
                ch.xferCount = 0;
            }
        } else {
            ch.xferCount--;
        }
    } while (ch.xferCount > 0);

    ch.xferEnded = true;
    m_log.trace("DMAC{} transfer finished", channel);
    if (ch.irqEnable) {
        switch (channel) {
        case 0: RaiseInterrupt(InterruptSource::DMAC0_XferEnd); break;
        case 1: RaiseInterrupt(InterruptSource::DMAC1_XferEnd); break;
        }
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

FORCE_INLINE void SH2::AdvanceWDT(uint64 cycles) {
    if (!WDT.WTCSR.TME) {
        return;
    }

    WDT.cycleCount += cycles;
    const uint64 steps = WDT.cycleCount >> WDT.clockDividerShift;
    WDT.cycleCount -= steps << WDT.clockDividerShift;

    uint64 nextCount = WDT.WTCNT + steps;
    if (nextCount >= 0x10000) {
        if (WDT.WTCSR.WT_nIT) {
            // Watchdog timer mode
            WDT.RSTCSR.WOVF = 1;
            if (WDT.RSTCSR.RSTE) {
                // TODO: needs to preserve RSTCSR
                Reset(WDT.RSTCSR.RSTS, true);
            }
        } else {
            // Interval timer mode
            WDT.WTCSR.OVF = 1;
            RaiseInterrupt(InterruptSource::WDT_ITI);
        }
    }
    WDT.WTCNT = nextCount;
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
    if (m_dmaChannels[0].xferEnded && m_dmaChannels[0].irqEnable) {
        RaiseInterrupt(InterruptSource::DMAC0_XferEnd);
        return;
    }
    if (m_dmaChannels[1].xferEnded && m_dmaChannels[1].irqEnable) {
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
    const DecodedArgs &args = g_decodeTable.args[instr];

    switch (opcode) {
    case OpcodeType::NOP: NOP(), PC += 2; break;

    case OpcodeType::SLEEP: SLEEP(), PC += 2; break;

    case OpcodeType::MOV_R: MOV(args), PC += 2; break;
    case OpcodeType::MOVB_L: MOVBL(args), PC += 2; break;
    case OpcodeType::MOVW_L: MOVWL(args), PC += 2; break;
    case OpcodeType::MOVL_L: MOVLL(args), PC += 2; break;
    case OpcodeType::MOVB_L0: MOVBL0(args), PC += 2; break;
    case OpcodeType::MOVW_L0: MOVWL0(args), PC += 2; break;
    case OpcodeType::MOVL_L0: MOVLL0(args), PC += 2; break;
    case OpcodeType::MOVB_L4: MOVBL4(args), PC += 2; break;
    case OpcodeType::MOVW_L4: MOVWL4(args), PC += 2; break;
    case OpcodeType::MOVL_L4: MOVLL4(args), PC += 2; break;
    case OpcodeType::MOVB_LG: MOVBLG(args), PC += 2; break;
    case OpcodeType::MOVW_LG: MOVWLG(args), PC += 2; break;
    case OpcodeType::MOVL_LG: MOVLLG(args), PC += 2; break;
    case OpcodeType::MOVB_M: MOVBM(args), PC += 2; break;
    case OpcodeType::MOVW_M: MOVWM(args), PC += 2; break;
    case OpcodeType::MOVL_M: MOVLM(args), PC += 2; break;
    case OpcodeType::MOVB_P: MOVBP(args), PC += 2; break;
    case OpcodeType::MOVW_P: MOVWP(args), PC += 2; break;
    case OpcodeType::MOVL_P: MOVLP(args), PC += 2; break;
    case OpcodeType::MOVB_S: MOVBS(args), PC += 2; break;
    case OpcodeType::MOVW_S: MOVWS(args), PC += 2; break;
    case OpcodeType::MOVL_S: MOVLS(args), PC += 2; break;
    case OpcodeType::MOVB_S0: MOVBS0(args), PC += 2; break;
    case OpcodeType::MOVW_S0: MOVWS0(args), PC += 2; break;
    case OpcodeType::MOVL_S0: MOVLS0(args), PC += 2; break;
    case OpcodeType::MOVB_S4: MOVBS4(args), PC += 2; break;
    case OpcodeType::MOVW_S4: MOVWS4(args), PC += 2; break;
    case OpcodeType::MOVL_S4: MOVLS4(args), PC += 2; break;
    case OpcodeType::MOVB_SG: MOVBSG(args), PC += 2; break;
    case OpcodeType::MOVW_SG: MOVWSG(args), PC += 2; break;
    case OpcodeType::MOVL_SG: MOVLSG(args), PC += 2; break;
    case OpcodeType::MOV_I: MOVI(args), PC += 2; break;
    case OpcodeType::MOVW_I: MOVWI(args), PC += 2; break;
    case OpcodeType::MOVL_I: MOVLI(args), PC += 2; break;
    case OpcodeType::MOVA: MOVA(args), PC += 2; break;
    case OpcodeType::MOVT: MOVT(args), PC += 2; break;
    case OpcodeType::CLRT: CLRT(), PC += 2; break;
    case OpcodeType::SETT: SETT(), PC += 2; break;

    case OpcodeType::EXTUB: EXTUB(args), PC += 2; break;
    case OpcodeType::EXTUW: EXTUW(args), PC += 2; break;
    case OpcodeType::EXTSB: EXTSB(args), PC += 2; break;
    case OpcodeType::EXTSW: EXTSW(args), PC += 2; break;
    case OpcodeType::SWAPB: SWAPB(args), PC += 2; break;
    case OpcodeType::SWAPW: SWAPW(args), PC += 2; break;
    case OpcodeType::XTRCT: XTRCT(args), PC += 2; break;

    case OpcodeType::LDC_GBR_R: LDCGBR(args), PC += 2; break;
    case OpcodeType::LDC_SR_R: LDCSR(args), PC += 2; break;
    case OpcodeType::LDC_VBR_R: LDCVBR(args), PC += 2; break;
    case OpcodeType::LDC_GBR_M: LDCMGBR(args), PC += 2; break;
    case OpcodeType::LDC_SR_M: LDCMSR(args), PC += 2; break;
    case OpcodeType::LDC_VBR_M: LDCMVBR(args), PC += 2; break;
    case OpcodeType::LDS_MACH_R: LDSMACH(args), PC += 2; break;
    case OpcodeType::LDS_MACL_R: LDSMACL(args), PC += 2; break;
    case OpcodeType::LDS_PR_R: LDSPR(args), PC += 2; break;
    case OpcodeType::LDS_MACH_M: LDSMMACH(args), PC += 2; break;
    case OpcodeType::LDS_MACL_M: LDSMMACL(args), PC += 2; break;
    case OpcodeType::LDS_PR_M: LDSMPR(args), PC += 2; break;
    case OpcodeType::STC_GBR_R: STCGBR(args), PC += 2; break;
    case OpcodeType::STC_SR_R: STCSR(args), PC += 2; break;
    case OpcodeType::STC_VBR_R: STCVBR(args), PC += 2; break;
    case OpcodeType::STC_GBR_M: STCMGBR(args), PC += 2; break;
    case OpcodeType::STC_SR_M: STCMSR(args), PC += 2; break;
    case OpcodeType::STC_VBR_M: STCMVBR(args), PC += 2; break;
    case OpcodeType::STS_MACH_R: STSMACH(args), PC += 2; break;
    case OpcodeType::STS_MACL_R: STSMACL(args), PC += 2; break;
    case OpcodeType::STS_PR_R: STSPR(args), PC += 2; break;
    case OpcodeType::STS_MACH_M: STSMMACH(args), PC += 2; break;
    case OpcodeType::STS_MACL_M: STSMMACL(args), PC += 2; break;
    case OpcodeType::STS_PR_M: STSMPR(args), PC += 2; break;

    case OpcodeType::ADD: ADD(args), PC += 2; break;
    case OpcodeType::ADD_I: ADDI(args), PC += 2; break;
    case OpcodeType::ADDC: ADDC(args), PC += 2; break;
    case OpcodeType::ADDV: ADDV(args), PC += 2; break;
    case OpcodeType::AND_R: AND(args), PC += 2; break;
    case OpcodeType::AND_I: ANDI(args), PC += 2; break;
    case OpcodeType::AND_M: ANDM(args), PC += 2; break;
    case OpcodeType::NEG: NEG(args), PC += 2; break;
    case OpcodeType::NEGC: NEGC(args), PC += 2; break;
    case OpcodeType::NOT: NOT(args), PC += 2; break;
    case OpcodeType::OR_R: OR(args), PC += 2; break;
    case OpcodeType::OR_I: ORI(args), PC += 2; break;
    case OpcodeType::OR_M: ORM(args), PC += 2; break;
    case OpcodeType::ROTCL: ROTCL(args), PC += 2; break;
    case OpcodeType::ROTCR: ROTCR(args), PC += 2; break;
    case OpcodeType::ROTL: ROTL(args), PC += 2; break;
    case OpcodeType::ROTR: ROTR(args), PC += 2; break;
    case OpcodeType::SHAL: SHAL(args), PC += 2; break;
    case OpcodeType::SHAR: SHAR(args), PC += 2; break;
    case OpcodeType::SHLL: SHLL(args), PC += 2; break;
    case OpcodeType::SHLL2: SHLL2(args), PC += 2; break;
    case OpcodeType::SHLL8: SHLL8(args), PC += 2; break;
    case OpcodeType::SHLL16: SHLL16(args), PC += 2; break;
    case OpcodeType::SHLR: SHLR(args), PC += 2; break;
    case OpcodeType::SHLR2: SHLR2(args), PC += 2; break;
    case OpcodeType::SHLR8: SHLR8(args), PC += 2; break;
    case OpcodeType::SHLR16: SHLR16(args), PC += 2; break;
    case OpcodeType::SUB: SUB(args), PC += 2; break;
    case OpcodeType::SUBC: SUBC(args), PC += 2; break;
    case OpcodeType::SUBV: SUBV(args), PC += 2; break;
    case OpcodeType::XOR_R: XOR(args), PC += 2; break;
    case OpcodeType::XOR_I: XORI(args), PC += 2; break;
    case OpcodeType::XOR_M: XORM(args), PC += 2; break;

    case OpcodeType::DT: DT(args), PC += 2; break;

    case OpcodeType::CLRMAC: CLRMAC(), PC += 2; break;
    case OpcodeType::MACW: MACW(args), PC += 2; break;
    case OpcodeType::MACL: MACL(args), PC += 2; break;
    case OpcodeType::MUL: MULL(args), PC += 2; break;
    case OpcodeType::MULS: MULS(args), PC += 2; break;
    case OpcodeType::MULU: MULU(args), PC += 2; break;
    case OpcodeType::DMULS: DMULS(args), PC += 2; break;
    case OpcodeType::DMULU: DMULU(args), PC += 2; break;

    case OpcodeType::DIV0S: DIV0S(args), PC += 2; break;
    case OpcodeType::DIV0U: DIV0U(), PC += 2; break;
    case OpcodeType::DIV1: DIV1(args), PC += 2; break;

    case OpcodeType::CMP_EQ_I: CMPIM(args), PC += 2; break;
    case OpcodeType::CMP_EQ_R: CMPEQ(args), PC += 2; break;
    case OpcodeType::CMP_GE: CMPGE(args), PC += 2; break;
    case OpcodeType::CMP_GT: CMPGT(args), PC += 2; break;
    case OpcodeType::CMP_HI: CMPHI(args), PC += 2; break;
    case OpcodeType::CMP_HS: CMPHS(args), PC += 2; break;
    case OpcodeType::CMP_PL: CMPPL(args), PC += 2; break;
    case OpcodeType::CMP_PZ: CMPPZ(args), PC += 2; break;
    case OpcodeType::CMP_STR: CMPSTR(args), PC += 2; break;
    case OpcodeType::TAS: TAS(args), PC += 2; break;
    case OpcodeType::TST_R: TST(args), PC += 2; break;
    case OpcodeType::TST_I: TSTI(args), PC += 2; break;
    case OpcodeType::TST_M: TSTM(args), PC += 2; break;

    case OpcodeType::Delay_NOP: NOP(), jumpToDelaySlot(); break;

    case OpcodeType::Delay_SLEEP: SLEEP(), jumpToDelaySlot(); break;

    case OpcodeType::Delay_MOV_R: MOV(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_L: MOVBL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_L: MOVWL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_L: MOVLL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_L0: MOVBL0(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_L0: MOVWL0(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_L0: MOVLL0(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_L4: MOVBL4(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_L4: MOVWL4(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_L4: MOVLL4(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_LG: MOVBLG(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_LG: MOVWLG(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_LG: MOVLLG(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_M: MOVBM(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_M: MOVWM(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_M: MOVLM(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_P: MOVBP(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_P: MOVWP(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_P: MOVLP(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_S: MOVBS(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_S: MOVWS(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_S: MOVLS(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_S0: MOVBS0(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_S0: MOVWS0(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_S0: MOVLS0(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_S4: MOVBS4(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_S4: MOVWS4(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_S4: MOVLS4(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVB_SG: MOVBSG(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_SG: MOVWSG(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_SG: MOVLSG(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOV_I: MOVI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVW_I: MOVWI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVL_I: MOVLI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVA: MOVA(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MOVT: MOVT(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CLRT: CLRT(), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SETT: SETT(), jumpToDelaySlot(); break;

    case OpcodeType::Delay_EXTUB: EXTUB(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_EXTUW: EXTUW(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_EXTSB: EXTSB(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_EXTSW: EXTSW(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SWAPB: SWAPB(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SWAPW: SWAPW(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XTRCT: XTRCT(args), jumpToDelaySlot(); break;

    case OpcodeType::Delay_LDC_GBR_R: LDCGBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_SR_R: LDCSR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_VBR_R: LDCVBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_GBR_M: LDCMGBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_SR_M: LDCMSR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDC_VBR_M: LDCMVBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACH_R: LDSMACH(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACL_R: LDSMACL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_PR_R: LDSPR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACH_M: LDSMMACH(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_MACL_M: LDSMMACL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_LDS_PR_M: LDSMPR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_GBR_R: STCGBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_SR_R: STCSR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_VBR_R: STCVBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_GBR_M: STCMGBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_SR_M: STCMSR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STC_VBR_M: STCMVBR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACH_R: STSMACH(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACL_R: STSMACL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_PR_R: STSPR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACH_M: STSMMACH(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_MACL_M: STSMMACL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_STS_PR_M: STSMPR(args), jumpToDelaySlot(); break;

    case OpcodeType::Delay_ADD: ADD(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ADD_I: ADDI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ADDC: ADDC(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ADDV: ADDV(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_AND_R: AND(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_AND_I: ANDI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_AND_M: ANDM(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_NEG: NEG(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_NEGC: NEGC(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_NOT: NOT(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_OR_R: OR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_OR_I: ORI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_OR_M: ORM(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTCL: ROTCL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTCR: ROTCR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTL: ROTL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_ROTR: ROTR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHAL: SHAL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHAR: SHAR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL: SHLL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL2: SHLL2(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL8: SHLL8(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLL16: SHLL16(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR: SHLR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR2: SHLR2(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR8: SHLR8(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SHLR16: SHLR16(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SUB: SUB(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SUBC: SUBC(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_SUBV: SUBV(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XOR_R: XOR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XOR_I: XORI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_XOR_M: XORM(args), jumpToDelaySlot(); break;

    case OpcodeType::Delay_DT: DT(args), jumpToDelaySlot(); break;

    case OpcodeType::Delay_CLRMAC: CLRMAC(), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MACW: MACW(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MACL: MACL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MUL: MULL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MULS: MULS(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_MULU: MULU(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DMULS: DMULS(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DMULU: DMULU(args), jumpToDelaySlot(); break;

    case OpcodeType::Delay_DIV0S: DIV0S(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DIV0U: DIV0U(), jumpToDelaySlot(); break;
    case OpcodeType::Delay_DIV1: DIV1(args), jumpToDelaySlot(); break;

    case OpcodeType::Delay_CMP_EQ_I: CMPIM(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_EQ_R: CMPEQ(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_GE: CMPGE(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_GT: CMPGT(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_HI: CMPHI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_HS: CMPHS(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_PL: CMPPL(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_PZ: CMPPZ(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_CMP_STR: CMPSTR(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TAS: TAS(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TST_R: TST(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TST_I: TSTI(args), jumpToDelaySlot(); break;
    case OpcodeType::Delay_TST_M: TSTM(args), jumpToDelaySlot(); break;

    case OpcodeType::BF: BF(args); break;
    case OpcodeType::BFS: BFS(args); break;
    case OpcodeType::BT: BT(args); break;
    case OpcodeType::BTS: BTS(args); break;
    case OpcodeType::BRA: BRA(args); break;
    case OpcodeType::BRAF: BRAF(args); break;
    case OpcodeType::BSR: BSR(args); break;
    case OpcodeType::BSRF: BSRF(args); break;
    case OpcodeType::JMP: JMP(args); break;
    case OpcodeType::JSR: JSR(args); break;
    case OpcodeType::TRAPA: TRAPA(args); break;

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

    if (SBYCR.SBY) {
        m_log.trace("Entering standby");

        // Initialize DMAC, FRT, WDT and SCI
        for (auto &ch : m_dmaChannels) {
            ch.WriteCHCR(0);
        }
        DMAOR.u32 = 0x0;
        FRT.Reset();
        // TODO: reset WDT
        // TODO: reset SCI

        // TODO: enter standby state
    } else {
        m_log.trace("Entering sleep");
        // TODO: enter sleep state
    }
}

FORCE_INLINE void SH2::MOV(const DecodedArgs &args) {
    // dbg_println("mov r{}, r{}", args.rm, args.rn);
    R[args.rn] = R[args.rm];
}

FORCE_INLINE void SH2::MOVBL(const DecodedArgs &args) {
    // dbg_println("mov.b @r{}, r{}", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<8>(MemReadByte(R[args.rm]));
}

FORCE_INLINE void SH2::MOVWL(const DecodedArgs &args) {
    // dbg_println("mov.w @r{}, r{}", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<16>(MemReadWord(R[args.rm]));
}

FORCE_INLINE void SH2::MOVLL(const DecodedArgs &args) {
    // dbg_println("mov.l @r{}, r{}", args.rm, args.rn);
    R[args.rn] = MemReadLong(R[args.rm]);
}

FORCE_INLINE void SH2::MOVBL0(const DecodedArgs &args) {
    // dbg_println("mov.b @(r0,r{}), r{}", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<8>(MemReadByte(R[args.rm] + R[0]));
}

FORCE_INLINE void SH2::MOVWL0(const DecodedArgs &args) {
    // dbg_println("mov.w @(r0,r{}), r{})", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<16>(MemReadWord(R[args.rm] + R[0]));
}

FORCE_INLINE void SH2::MOVLL0(const DecodedArgs &args) {
    // dbg_println("mov.l @(r0,r{}), r{})", args.rm, args.rn);
    R[args.rn] = MemReadLong(R[args.rm] + R[0]);
}

FORCE_INLINE void SH2::MOVBL4(const DecodedArgs &args) {
    // dbg_println("mov.b @(0x{:X},r{}), r0", args.dispImm, args.rm);
    R[0] = bit::sign_extend<8>(MemReadByte(R[args.rm] + args.dispImm));
}

FORCE_INLINE void SH2::MOVWL4(const DecodedArgs &args) {
    // dbg_println("mov.w @(0x{:X},r{}), r0", args.dispImm, args.rm);
    R[0] = bit::sign_extend<16>(MemReadWord(R[args.rm] + args.dispImm));
}

FORCE_INLINE void SH2::MOVLL4(const DecodedArgs &args) {
    // dbg_println("mov.l @(0x{:X},r{}), r{}", args.dispImm, rm, rn);
    R[args.rn] = MemReadLong(R[args.rm] + args.dispImm);
}

FORCE_INLINE void SH2::MOVBLG(const DecodedArgs &args) {
    // dbg_println("mov.b @(0x{:X},gbr), r0", disp);
    R[0] = bit::sign_extend<8>(MemReadByte(GBR + args.dispImm));
}

FORCE_INLINE void SH2::MOVWLG(const DecodedArgs &args) {
    // dbg_println("mov.w @(0x{:X},gbr), r0", args.dispImm);
    R[0] = bit::sign_extend<16>(MemReadWord(GBR + args.dispImm));
}

FORCE_INLINE void SH2::MOVLLG(const DecodedArgs &args) {
    // dbg_println("mov.l @(0x{:X},gbr), r0", args.dispImm);
    R[0] = MemReadLong(GBR + args.dispImm);
}

FORCE_INLINE void SH2::MOVBM(const DecodedArgs &args) {
    // dbg_println("mov.b r{}, @-r{}", args.rm, args.rn);
    MemWriteByte(R[args.rn] - 1, R[args.rm]);
    R[args.rn] -= 1;
}

FORCE_INLINE void SH2::MOVWM(const DecodedArgs &args) {
    // dbg_println("mov.w r{}, @-r{}", args.rm, args.rn);
    MemWriteWord(R[args.rn] - 2, R[args.rm]);
    R[args.rn] -= 2;
}

FORCE_INLINE void SH2::MOVLM(const DecodedArgs &args) {
    // dbg_println("mov.l r{}, @-r{}", args.rm, args.rn);
    MemWriteLong(R[args.rn] - 4, R[args.rm]);
    R[args.rn] -= 4;
}

FORCE_INLINE void SH2::MOVBP(const DecodedArgs &args) {
    // dbg_println("mov.b @r{}+, r{}", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<8>(MemReadByte(R[args.rm]));
    if (args.rn != args.rm) {
        R[args.rm] += 1;
    }
}

FORCE_INLINE void SH2::MOVWP(const DecodedArgs &args) {
    // dbg_println("mov.w @r{}+, r{}", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<16>(MemReadWord(R[args.rm]));
    if (args.rn != args.rm) {
        R[args.rm] += 2;
    }
}

FORCE_INLINE void SH2::MOVLP(const DecodedArgs &args) {
    // dbg_println("mov.l @r{}+, r{}", args.rm, args.rn);
    R[args.rn] = MemReadLong(R[args.rm]);
    if (args.rn != args.rm) {
        R[args.rm] += 4;
    }
}

FORCE_INLINE void SH2::MOVBS(const DecodedArgs &args) {
    // dbg_println("mov.b r{}, @r{}", args.rm, args.rn);
    MemWriteByte(R[args.rn], R[args.rm]);
}

FORCE_INLINE void SH2::MOVWS(const DecodedArgs &args) {
    // dbg_println("mov.w r{}, @r{}", args.rm, args.rn);
    MemWriteWord(R[args.rn], R[args.rm]);
}

FORCE_INLINE void SH2::MOVLS(const DecodedArgs &args) {
    // dbg_println("mov.l r{}, @r{}", args.rm, args.rn);
    MemWriteLong(R[args.rn], R[args.rm]);
}

FORCE_INLINE void SH2::MOVBS0(const DecodedArgs &args) {
    // dbg_println("mov.b r{}, @(r0,r{})", args.rm, args.rn);
    MemWriteByte(R[args.rn] + R[0], R[args.rm]);
}

FORCE_INLINE void SH2::MOVWS0(const DecodedArgs &args) {
    // dbg_println("mov.w r{}, @(r0,r{})", args.rm, args.rn);
    MemWriteWord(R[args.rn] + R[0], R[args.rm]);
}

FORCE_INLINE void SH2::MOVLS0(const DecodedArgs &args) {
    // dbg_println("mov.l r{}, @(r0,r{})", args.rm, args.rn);
    MemWriteLong(R[args.rn] + R[0], R[args.rm]);
}

FORCE_INLINE void SH2::MOVBS4(const DecodedArgs &args) {
    // dbg_println("mov.b r0, @(0x{:X},r{})", args.dispImm, args.rn);
    MemWriteByte(R[args.rn] + args.dispImm, R[0]);
}

FORCE_INLINE void SH2::MOVWS4(const DecodedArgs &args) {
    // dbg_println("mov.w r0, @(0x{:X},r{})", args.dispImm, args.rn);
    MemWriteWord(R[args.rn] + args.dispImm, R[0]);
}

FORCE_INLINE void SH2::MOVLS4(const DecodedArgs &args) {
    // dbg_println("mov.l r{}, @(0x{:X},r{})", args.rm, args.dispImm, args.rn);
    MemWriteLong(R[args.rn] + args.dispImm, R[args.rm]);
}

FORCE_INLINE void SH2::MOVBSG(const DecodedArgs &args) {
    // dbg_println("mov.b r0, @(0x{:X},gbr)", args.dispImm);
    MemWriteByte(GBR + args.dispImm, R[0]);
}

FORCE_INLINE void SH2::MOVWSG(const DecodedArgs &args) {
    // dbg_println("mov.w r0, @(0x{:X},gbr)", args.dispImm);
    MemWriteWord(GBR + args.dispImm, R[0]);
}

FORCE_INLINE void SH2::MOVLSG(const DecodedArgs &args) {
    // dbg_println("mov.l r0, @(0x{:X},gbr)", args.dispImm);
    MemWriteLong(GBR + args.dispImm, R[0]);
}

FORCE_INLINE void SH2::MOVI(const DecodedArgs &args) {
    // dbg_println("mov #{}0x{:X}, r{}", (args.dispImm < 0 ? "-" : ""), abs(args.dispImm), args.rn);
    R[args.rn] = args.dispImm;
}

FORCE_INLINE void SH2::MOVWI(const DecodedArgs &args) {
    const uint32 address = PC + 4 + args.dispImm;
    // dbg_println("mov.w @(0x{:08X},pc), r{}", address, args.rn);
    R[args.rn] = bit::sign_extend<16>(MemReadWord(address));
}

FORCE_INLINE void SH2::MOVLI(const DecodedArgs &args) {
    const uint32 address = ((PC + 4) & ~3u) + args.dispImm;
    // dbg_println("mov.l @(0x{:08X},pc), r{}", address, args.rn);
    R[args.rn] = MemReadLong(address);
}

FORCE_INLINE void SH2::MOVA(const DecodedArgs &args) {
    const uint32 address = (PC & ~3) + args.dispImm;
    // dbg_println("mova @(0x{:X},pc), r0", address);
    R[0] = address;
}

FORCE_INLINE void SH2::MOVT(const DecodedArgs &args) {
    // dbg_println("movt r{}", args.rn);
    R[args.rn] = SR.T;
}

FORCE_INLINE void SH2::CLRT() {
    // dbg_println("clrt");
    SR.T = 0;
}

FORCE_INLINE void SH2::SETT() {
    // dbg_println("sett");
    SR.T = 1;
}

FORCE_INLINE void SH2::EXTSB(const DecodedArgs &args) {
    // dbg_println("exts.b r{}, r{}", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<8>(R[args.rm]);
}

FORCE_INLINE void SH2::EXTSW(const DecodedArgs &args) {
    // dbg_println("exts.w r{}, r{}", args.rm, args.rn);
    R[args.rn] = bit::sign_extend<16>(R[args.rm]);
}

FORCE_INLINE void SH2::EXTUB(const DecodedArgs &args) {
    // dbg_println("extu.b r{}, r{}", args.rm, args.rn);
    R[args.rn] = R[args.rm] & 0xFF;
}

FORCE_INLINE void SH2::EXTUW(const DecodedArgs &args) {
    // dbg_println("extu.w r{}, r{}", args.rm, args.rn);
    R[args.rn] = R[args.rm] & 0xFFFF;
}

FORCE_INLINE void SH2::SWAPB(const DecodedArgs &args) {
    // dbg_println("swap.b r{}, r{}", args.rm, args.rn);

    const uint32 tmp0 = R[args.rm] & 0xFFFF0000;
    const uint32 tmp1 = (R[args.rm] & 0xFF) << 8u;
    R[args.rn] = ((R[args.rm] >> 8u) & 0xFF) | tmp1 | tmp0;
}

FORCE_INLINE void SH2::SWAPW(const DecodedArgs &args) {
    // dbg_println("swap.w r{}, r{}", args.rm, args.rn);

    const uint32 tmp = R[args.rm] >> 16u;
    R[args.rn] = (R[args.rm] << 16u) | tmp;
}

FORCE_INLINE void SH2::XTRCT(const DecodedArgs &args) {
    // dbg_println("xtrct r{}, r{}", args.rm, args.rn);
    R[args.rn] = (R[args.rn] >> 16u) | (R[args.rm] << 16u);
}

FORCE_INLINE void SH2::LDCGBR(const DecodedArgs &args) {
    // dbg_println("ldc r{}, gbr", args.rm);
    GBR = R[args.rm];
}

FORCE_INLINE void SH2::LDCSR(const DecodedArgs &args) {
    // dbg_println("ldc r{}, sr", args.rm);
    SR.u32 = R[args.rm] & 0x000003F3;
}

FORCE_INLINE void SH2::LDCVBR(const DecodedArgs &args) {
    // dbg_println("ldc r{}, vbr", args.rm);
    VBR = R[args.rm];
}

FORCE_INLINE void SH2::LDCMGBR(const DecodedArgs &args) {
    // dbg_println("ldc.l @r{}+, gbr", args.rm);
    GBR = MemReadLong(R[args.rm]);
    R[args.rm] += 4;
}

FORCE_INLINE void SH2::LDCMSR(const DecodedArgs &args) {
    // dbg_println("ldc.l @r{}+, sr", args.rm);
    SR.u32 = MemReadLong(R[args.rm]) & 0x000003F3;
    R[args.rm] += 4;
}

FORCE_INLINE void SH2::LDCMVBR(const DecodedArgs &args) {
    // dbg_println("ldc.l @r{}+, vbr", args.rm);
    VBR = MemReadLong(R[args.rm]);
    R[args.rm] += 4;
}

FORCE_INLINE void SH2::LDSMACH(const DecodedArgs &args) {
    // dbg_println("lds r{}, mach", args.rm);
    MAC.H = R[args.rm];
}

FORCE_INLINE void SH2::LDSMACL(const DecodedArgs &args) {
    // dbg_println("lds r{}, macl", args.rm);
    MAC.L = R[args.rm];
}

FORCE_INLINE void SH2::LDSPR(const DecodedArgs &args) {
    // dbg_println("lds r{}, pr", args.rm);
    PR = R[args.rm];
}

FORCE_INLINE void SH2::LDSMMACH(const DecodedArgs &args) {
    // dbg_println("lds.l @r{}+, mach", args.rm);
    MAC.H = MemReadLong(R[args.rm]);
    R[args.rm] += 4;
}

FORCE_INLINE void SH2::LDSMMACL(const DecodedArgs &args) {
    // dbg_println("lds.l @r{}+, macl", args.rm);
    MAC.L = MemReadLong(R[args.rm]);
    R[args.rm] += 4;
}

FORCE_INLINE void SH2::LDSMPR(const DecodedArgs &args) {
    // dbg_println("lds.l @r{}+, pr", args.rm);
    PR = MemReadLong(R[args.rm]);
    R[args.rm] += 4;
}

FORCE_INLINE void SH2::STCGBR(const DecodedArgs &args) {
    // dbg_println("stc gbr, r{}", args.rn);
    R[args.rn] = GBR;
}

FORCE_INLINE void SH2::STCSR(const DecodedArgs &args) {
    // dbg_println("stc sr, r{}", args.rn);
    R[args.rn] = SR.u32;
}

FORCE_INLINE void SH2::STCVBR(const DecodedArgs &args) {
    // dbg_println("stc vbr, r{}", args.rn);
    R[args.rn] = VBR;
}

FORCE_INLINE void SH2::STCMGBR(const DecodedArgs &args) {
    // dbg_println("stc.l gbr, @-r{}", args.rn);
    R[args.rn] -= 4;
    MemWriteLong(R[args.rn], GBR);
}

FORCE_INLINE void SH2::STCMSR(const DecodedArgs &args) {
    // dbg_println("stc.l sr, @-r{}", args.rn);
    R[args.rn] -= 4;
    MemWriteLong(R[args.rn], SR.u32);
}

FORCE_INLINE void SH2::STCMVBR(const DecodedArgs &args) {
    // dbg_println("stc.l vbr, @-r{}", args.rn);
    R[args.rn] -= 4;
    MemWriteLong(R[args.rn], VBR);
}

FORCE_INLINE void SH2::STSMACH(const DecodedArgs &args) {
    // dbg_println("sts mach, r{}", args.rn);
    R[args.rn] = MAC.H;
}

FORCE_INLINE void SH2::STSMACL(const DecodedArgs &args) {
    // dbg_println("sts macl, r{}", args.rn);
    R[args.rn] = MAC.L;
}

FORCE_INLINE void SH2::STSPR(const DecodedArgs &args) {
    // dbg_println("sts pr, r{}", args.rn);
    R[args.rn] = PR;
}

FORCE_INLINE void SH2::STSMMACH(const DecodedArgs &args) {
    // dbg_println("sts.l mach, @-r{}", args.rn);
    R[args.rn] -= 4;
    MemWriteLong(R[args.rn], MAC.H);
}

FORCE_INLINE void SH2::STSMMACL(const DecodedArgs &args) {
    // dbg_println("sts.l macl, @-r{}", args.rn);
    R[args.rn] -= 4;
    MemWriteLong(R[args.rn], MAC.L);
}

FORCE_INLINE void SH2::STSMPR(const DecodedArgs &args) {
    // dbg_println("sts.l pr, @-r{}", args.rn);
    R[args.rn] -= 4;
    MemWriteLong(R[args.rn], PR);
}

FORCE_INLINE void SH2::ADD(const DecodedArgs &args) {
    // dbg_println("add r{}, r{}", args.rm, args.rn);
    R[args.rn] += R[args.rm];
}

FORCE_INLINE void SH2::ADDI(const DecodedArgs &args) {
    // dbg_println("add #{}0x{:X}, r{}", (args.dispImm < 0 ? "-" : ""), abs(args.dispImm), args.rn);
    R[args.rn] += args.dispImm;
}

FORCE_INLINE void SH2::ADDC(const DecodedArgs &args) {
    // dbg_println("addc r{}, r{}", args.rm, args.rn);
    const uint32 tmp1 = R[args.rn] + R[args.rm];
    const uint32 tmp0 = R[args.rn];
    R[args.rn] = tmp1 + SR.T;
    SR.T = (tmp0 > tmp1) || (tmp1 > R[args.rn]);
}

FORCE_INLINE void SH2::ADDV(const DecodedArgs &args) {
    // dbg_println("addv r{}, r{}", args.rm, args.rn);

    const bool dst = static_cast<sint32>(R[args.rn]) < 0;
    const bool src = static_cast<sint32>(R[args.rm]) < 0;

    R[args.rn] += R[args.rm];

    bool ans = static_cast<sint32>(R[args.rn]) < 0;
    ans ^= dst;
    SR.T = (src == dst) & ans;
}

FORCE_INLINE void SH2::AND(const DecodedArgs &args) {
    // dbg_println("and r{}, r{}", args.rm, args.rn);
    R[args.rn] &= R[args.rm];
}

FORCE_INLINE void SH2::ANDI(const DecodedArgs &args) {
    // dbg_println("and #0x{:X}, r0", args.dispImm);
    R[0] &= args.dispImm;
}

FORCE_INLINE void SH2::ANDM(const DecodedArgs &args) {
    // dbg_println("and.b #0x{:X}, @(r0,gbr)", args.dispImm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp &= args.dispImm;
    MemWriteByte(GBR + R[0], tmp);
}

FORCE_INLINE void SH2::NEG(const DecodedArgs &args) {
    // dbg_println("neg r{}, r{}", args.rm, args.rn);
    R[args.rn] = -R[args.rm];
}

FORCE_INLINE void SH2::NEGC(const DecodedArgs &args) {
    // dbg_println("negc r{}, r{}", args.rm, args.rn);
    const uint32 tmp = -R[args.rm];
    R[args.rn] = tmp - SR.T;
    SR.T = (0 < tmp) || (tmp < R[args.rn]);
}

FORCE_INLINE void SH2::NOT(const DecodedArgs &args) {
    // dbg_println("not r{}, r{}", args.rm, args.rn);
    R[args.rn] = ~R[args.rm];
}

FORCE_INLINE void SH2::OR(const DecodedArgs &args) {
    // dbg_println("or r{}, r{}", args.rm, args.rn);
    R[args.rn] |= R[args.rm];
}

FORCE_INLINE void SH2::ORI(const DecodedArgs &args) {
    // dbg_println("or #0x{:X}, r0", args.dispImm);
    R[0] |= args.dispImm;
}

FORCE_INLINE void SH2::ORM(const DecodedArgs &args) {
    // dbg_println("or.b #0x{:X}, @(r0,gbr)", args.dispImm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp |= args.dispImm;
    MemWriteByte(GBR + R[0], tmp);
}

FORCE_INLINE void SH2::ROTCL(const DecodedArgs &args) {
    // dbg_println("rotcl r{}", args.rn);
    const bool tmp = R[args.rn] >> 31u;
    R[args.rn] = (R[args.rn] << 1u) | SR.T;
    SR.T = tmp;
}

FORCE_INLINE void SH2::ROTCR(const DecodedArgs &args) {
    // dbg_println("rotcr r{}", args.rn);
    const bool tmp = R[args.rn] & 1u;
    R[args.rn] = (R[args.rn] >> 1u) | (SR.T << 31u);
    SR.T = tmp;
}

FORCE_INLINE void SH2::ROTL(const DecodedArgs &args) {
    // dbg_println("rotl r{}", args.rn);
    SR.T = R[args.rn] >> 31u;
    R[args.rn] = (R[args.rn] << 1u) | SR.T;
}

FORCE_INLINE void SH2::ROTR(const DecodedArgs &args) {
    // dbg_println("rotr r{}", args.rn);
    SR.T = R[args.rn] & 1u;
    R[args.rn] = (R[args.rn] >> 1u) | (SR.T << 31u);
}

FORCE_INLINE void SH2::SHAL(const DecodedArgs &args) {
    // dbg_println("shal r{}", args.rn);
    SR.T = R[args.rn] >> 31u;
    R[args.rn] <<= 1u;
}

FORCE_INLINE void SH2::SHAR(const DecodedArgs &args) {
    // dbg_println("shar r{}", args.rn);
    SR.T = R[args.rn] & 1u;
    R[args.rn] = static_cast<sint32>(R[args.rn]) >> 1;
}

FORCE_INLINE void SH2::SHLL(const DecodedArgs &args) {
    // dbg_println("shll r{}", args.rn);
    SR.T = R[args.rn] >> 31u;
    R[args.rn] <<= 1u;
}

FORCE_INLINE void SH2::SHLL2(const DecodedArgs &args) {
    // dbg_println("shll2 r{}", args.rn);
    R[args.rn] <<= 2u;
}

FORCE_INLINE void SH2::SHLL8(const DecodedArgs &args) {
    // dbg_println("shll8 r{}", args.rn);
    R[args.rn] <<= 8u;
}

FORCE_INLINE void SH2::SHLL16(const DecodedArgs &args) {
    // dbg_println("shll16 r{}", args.rn);
    R[args.rn] <<= 16u;
}

FORCE_INLINE void SH2::SHLR(const DecodedArgs &args) {
    // dbg_println("shlr r{}", args.rn);
    SR.T = R[args.rn] & 1u;
    R[args.rn] >>= 1u;
}

FORCE_INLINE void SH2::SHLR2(const DecodedArgs &args) {
    // dbg_println("shlr2 r{}", args.rn);
    R[args.rn] >>= 2u;
}

FORCE_INLINE void SH2::SHLR8(const DecodedArgs &args) {
    // dbg_println("shlr8 r{}", args.rn);
    R[args.rn] >>= 8u;
}

FORCE_INLINE void SH2::SHLR16(const DecodedArgs &args) {
    // dbg_println("shlr16 r{}", args.rn);
    R[args.rn] >>= 16u;
}

FORCE_INLINE void SH2::SUB(const DecodedArgs &args) {
    // dbg_println("sub r{}, r{}", args.rm, args.rn);
    R[args.rn] -= R[args.rm];
}

FORCE_INLINE void SH2::SUBC(const DecodedArgs &args) {
    // dbg_println("subc r{}, r{}", args.rm, args.rn);
    const uint32 tmp1 = R[args.rn] - R[args.rm];
    const uint32 tmp0 = R[args.rn];
    R[args.rn] = tmp1 - SR.T;
    SR.T = (tmp0 < tmp1) || (tmp1 < R[args.rn]);
}

FORCE_INLINE void SH2::SUBV(const DecodedArgs &args) {
    // dbg_println("subv r{}, r{}", args.rm, args.rn);

    const bool dst = static_cast<sint32>(R[args.rn]) < 0;
    const bool src = static_cast<sint32>(R[args.rm]) < 0;

    R[args.rn] -= R[args.rm];

    bool ans = static_cast<sint32>(R[args.rn]) < 0;
    ans ^= dst;
    SR.T = (src != dst) & ans;
}

FORCE_INLINE void SH2::XOR(const DecodedArgs &args) {
    // dbg_println("xor r{}, r{}", args.rm, args.rn);
    R[args.rn] ^= R[args.rm];
}

FORCE_INLINE void SH2::XORI(const DecodedArgs &args) {
    // dbg_println("xor #0x{:X}, r0", args.dispImm);
    R[0] ^= args.dispImm;
}

FORCE_INLINE void SH2::XORM(const DecodedArgs &args) {
    // dbg_println("xor.b #0x{:X}, @(r0,gbr)", args.dispImm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp ^= args.dispImm;
    MemWriteByte(GBR + R[0], tmp);
}

FORCE_INLINE void SH2::DT(const DecodedArgs &args) {
    // dbg_println("dt r{}", args.rn);
    R[args.rn]--;
    SR.T = R[args.rn] == 0;
}

FORCE_INLINE void SH2::CLRMAC() {
    // dbg_println("clrmac");
    MAC.u64 = 0;
}

FORCE_INLINE void SH2::MACW(const DecodedArgs &args) {
    // dbg_println("mac.w @r{}+, @r{}+)", args.rm, args.rn);

    const sint32 op1 = static_cast<sint16>(MemReadWord(R[args.rm]));
    R[args.rm] += 2;
    const sint32 op2 = static_cast<sint16>(MemReadWord(R[args.rn]));
    R[args.rn] += 2;

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

FORCE_INLINE void SH2::MACL(const DecodedArgs &args) {
    // dbg_println("mac.l @r{}+, @r{}+)", args.rm, args.rn);

    const sint64 op1 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[args.rm])));
    R[args.rm] += 4;
    const sint64 op2 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[args.rn])));
    R[args.rn] += 4;

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

FORCE_INLINE void SH2::MULL(const DecodedArgs &args) {
    // dbg_println("mul.l r{}, r{}", args.rm, args.rn);
    MAC.L = R[args.rm] * R[args.rn];
}

FORCE_INLINE void SH2::MULS(const DecodedArgs &args) {
    // dbg_println("muls.w r{}, r{}", args.rm, args.rn);
    MAC.L = bit::sign_extend<16>(R[args.rm]) * bit::sign_extend<16>(R[args.rn]);
}

FORCE_INLINE void SH2::MULU(const DecodedArgs &args) {
    // dbg_println("mulu.w r{}, r{}", args.rm, args.rn);
    auto cast = [](uint32 val) { return static_cast<uint32>(static_cast<uint16>(val)); };
    MAC.L = cast(R[args.rm]) * cast(R[args.rn]);
}

FORCE_INLINE void SH2::DMULS(const DecodedArgs &args) {
    // dbg_println("dmuls.l r{}, r{}", args.rm, args.rn);
    auto cast = [](uint32 val) { return static_cast<sint64>(static_cast<sint32>(val)); };
    MAC.u64 = cast(R[args.rm]) * cast(R[args.rn]);
}

FORCE_INLINE void SH2::DMULU(const DecodedArgs &args) {
    // dbg_println("dmulu.l r{}, r{}", args.rm, args.rn);
    MAC.u64 = static_cast<uint64>(R[args.rm]) * static_cast<uint64>(R[args.rn]);
}

FORCE_INLINE void SH2::DIV0S(const DecodedArgs &args) {
    // dbg_println("div0s r{}, r{}", args.rm, args.rn);
    SR.M = static_cast<sint32>(R[args.rm]) < 0;
    SR.Q = static_cast<sint32>(R[args.rn]) < 0;
    SR.T = SR.M != SR.Q;
}

FORCE_INLINE void SH2::DIV0U() {
    // dbg_println("div0u");
    SR.M = 0;
    SR.Q = 0;
    SR.T = 0;
}

FORCE_INLINE void SH2::DIV1(const DecodedArgs &args) {
    // dbg_println("div1 r{}, r{}", args.rm, args.rn);

    const bool oldQ = SR.Q;
    SR.Q = static_cast<sint32>(R[args.rn]) < 0;
    R[args.rn] = (R[args.rn] << 1u) | SR.T;

    const uint32 prevVal = R[args.rn];
    if (oldQ == SR.M) {
        R[args.rn] -= R[args.rm];
    } else {
        R[args.rn] += R[args.rm];
    }

    if (oldQ) {
        if (SR.M) {
            SR.Q ^= R[args.rn] <= prevVal;
        } else {
            SR.Q ^= R[args.rn] < prevVal;
        }
    } else {
        if (SR.M) {
            SR.Q ^= R[args.rn] >= prevVal;
        } else {
            SR.Q ^= R[args.rn] > prevVal;
        }
    }

    SR.T = SR.Q == SR.M;
}

FORCE_INLINE void SH2::CMPIM(const DecodedArgs &args) {
    // dbg_println("cmp/eq #{}0x{:X}, r0", (args.dispImm < 0 ? "-" : ""), abs(args.dispImm));
    SR.T = static_cast<sint32>(R[0]) == args.dispImm;
}

FORCE_INLINE void SH2::CMPEQ(const DecodedArgs &args) {
    // dbg_println("cmp/eq r{}, r{}", args.rm, args.rn);
    SR.T = R[args.rn] == R[args.rm];
}

FORCE_INLINE void SH2::CMPGE(const DecodedArgs &args) {
    // dbg_println("cmp/ge r{}, r{}", args.rm, args.rn);
    SR.T = static_cast<sint32>(R[args.rn]) >= static_cast<sint32>(R[args.rm]);
}

FORCE_INLINE void SH2::CMPGT(const DecodedArgs &args) {
    // dbg_println("cmp/gt r{}, r{}", args.rm, args.rn);
    SR.T = static_cast<sint32>(R[args.rn]) > static_cast<sint32>(R[args.rm]);
}

FORCE_INLINE void SH2::CMPHI(const DecodedArgs &args) {
    // dbg_println("cmp/hi r{}, r{}", args.rm, args.rn);
    SR.T = R[args.rn] > R[args.rm];
}

FORCE_INLINE void SH2::CMPHS(const DecodedArgs &args) {
    // dbg_println("cmp/hs r{}, r{}", args.rm, args.rn);
    SR.T = R[args.rn] >= R[args.rm];
}

FORCE_INLINE void SH2::CMPPL(const DecodedArgs &args) {
    // dbg_println("cmp/pl r{}", args.rn);
    SR.T = static_cast<sint32>(R[args.rn]) > 0;
}

FORCE_INLINE void SH2::CMPPZ(const DecodedArgs &args) {
    // dbg_println("cmp/pz r{}", args.rn);
    SR.T = static_cast<sint32>(R[args.rn]) >= 0;
}

FORCE_INLINE void SH2::CMPSTR(const DecodedArgs &args) {
    // dbg_println("cmp/str r{}, r{}", args.rm, args.rn);
    const uint32 tmp = R[args.rm] ^ R[args.rn];
    const uint8 hh = tmp >> 24u;
    const uint8 hl = tmp >> 16u;
    const uint8 lh = tmp >> 8u;
    const uint8 ll = tmp >> 0u;
    SR.T = !(hh && hl && lh && ll);
}

FORCE_INLINE void SH2::TAS(const DecodedArgs &args) {
    // dbg_println("tas.b @r{}", args.rn);
    // dbg_println("WARNING: bus lock not implemented!");

    // TODO: enable bus lock on this read
    const uint8 tmp = MemReadByte(R[args.rn]);
    SR.T = tmp == 0;
    // TODO: disable bus lock on this write
    MemWriteByte(R[args.rn], tmp | 0x80);
}

FORCE_INLINE void SH2::TST(const DecodedArgs &args) {
    // dbg_println("tst r{}, r{}", args.rm, args.rn);
    SR.T = (R[args.rn] & R[args.rm]) == 0;
}

FORCE_INLINE void SH2::TSTI(const DecodedArgs &args) {
    // dbg_println("tst #0x{:X}, r0", args.dispImm);
    SR.T = (R[0] & args.dispImm) == 0;
}

FORCE_INLINE void SH2::TSTM(const DecodedArgs &args) {
    // dbg_println("tst.b #0x{:X}, @(r0,gbr)", args.dispImm);
    const uint8 tmp = MemReadByte(GBR + R[0]);
    SR.T = (tmp & args.dispImm) == 0;
}

FORCE_INLINE void SH2::BF(const DecodedArgs &args) {
    // dbg_println("bf 0x{:08X}", PC + args.dispImm);

    if (!SR.T) {
        PC += args.dispImm;
    } else {
        PC += 2;
    }
}

FORCE_INLINE void SH2::BFS(const DecodedArgs &args) {
    // dbg_println("bf/s 0x{:08X}", PC + args.dispImm);

    if (!SR.T) {
        SetupDelaySlot(PC + args.dispImm);
    }
    PC += 2;
}

FORCE_INLINE void SH2::BT(const DecodedArgs &args) {
    // dbg_println("bt 0x{:08X}", PC + args.dispImm);

    if (SR.T) {
        PC += args.dispImm;
    } else {
        PC += 2;
    }
}

FORCE_INLINE void SH2::BTS(const DecodedArgs &args) {
    // dbg_println("bt/s 0x{:08X}", PC + args.dispImm);

    if (SR.T) {
        SetupDelaySlot(PC + args.dispImm);
    }
    PC += 2;
}

FORCE_INLINE void SH2::BRA(const DecodedArgs &args) {
    // dbg_println("bra 0x{:08X}", PC + args.dispImm);
    SetupDelaySlot(PC + args.dispImm);
    PC += 2;
}

FORCE_INLINE void SH2::BRAF(const DecodedArgs &args) {
    // dbg_println("braf r{}", args.rm);
    SetupDelaySlot(PC + R[args.rm] + 4);
    PC += 2;
}

FORCE_INLINE void SH2::BSR(const DecodedArgs &args) {
    m_tracer.BSR({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    // dbg_println("bsr 0x{:08X}", PC + args.dispImm);

    PR = PC + 4;
    SetupDelaySlot(PC + args.dispImm);
    PC += 2;
}

FORCE_INLINE void SH2::BSRF(const DecodedArgs &args) {
    m_tracer.BSR({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    // dbg_println("bsrf r{}", args.rm);
    PR = PC + 4;
    SetupDelaySlot(PC + R[args.rm] + 4);
    PC += 2;
}

FORCE_INLINE void SH2::JMP(const DecodedArgs &args) {
    // dbg_println("jmp @r{}", args.rm);
    SetupDelaySlot(R[args.rm]);
    PC += 2;
}

FORCE_INLINE void SH2::JSR(const DecodedArgs &args) {
    // dbg_println("jsr @r{}", args.rm);
    m_tracer.JSR({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    PR = PC + 4;
    SetupDelaySlot(R[args.rm]);
    PC += 2;
}

FORCE_INLINE void SH2::TRAPA(const DecodedArgs &args) {
    m_tracer.TRAPA({R, PC, PR, SR.u32, VBR, GBR, MAC.u64});
    // dbg_println("trapa #0x{:X}", args.dispImm >> 2u);
    R[15] -= 4;
    MemWriteLong(R[15], SR.u32);
    R[15] -= 4;
    MemWriteLong(R[15], PC - 2);
    PC = MemReadLong(VBR + args.dispImm);
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

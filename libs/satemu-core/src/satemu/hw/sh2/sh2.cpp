#include <satemu/hw/sh2/sh2.hpp>

#include <satemu/hw/sh2/sh2_bus.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>
#include <satemu/util/unreachable.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>

namespace satemu::sh2 {

SH2::SH2(SH2Bus &bus, bool master)
    : m_bus(bus) {
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
    IPRB.val.u16 = 0x0000;
    VCRA.val.u16 = 0x0000;
    VCRB.val.u16 = 0x0000;
    VCRC.val.u16 = 0x0000;
    VCRD.val.u16 = 0x0000;
    ICR.val.u16 = 0x0000;
    IPRA.val.u16 = 0x0000;
    VCRWDT.val.u16 = 0x0000;
    VCRDIV = 0x0000; // undefined initial value
    BCR1.u15 = 0x03F0;
    BCR2.u16 = 0x00FC;
    WCR.u16 = 0xAAFF;
    MCR.u16 = 0x0000;

    FRT.Reset();

    for (auto &ch : dmaChannels) {
        ch.Reset();
    }
    DMAOR.u32 = 0x00000000;

    cacheEntries.fill({});
    WriteCCR(0x00);

    DVSR = 0x0;  // undefined initial value
    DVDNT = 0x0; // undefined initial value
    DVCR.u32 = 0x00000000;
    DVDNTH = 0x0; // undefined initial value
    DVDNTL = 0x0; // undefined initial value

    m_NMI = false;

    m_pendingExternalIntrLevel = 0;
    m_pendingExternalIntrVecNum = 0;
    m_pendingInterrupt.priority = 0;
    m_pendingInterrupt.vecNum = 0;

    m_delaySlot = false;
    m_delaySlotTarget = 0;
}

FLATTEN void SH2::Advance(uint64 cycles) {
    // TODO: optimize active DMA channel check
    // TODO: proper timings, cycle-stealing, etc. (suspend instructions if not cached)
    // TODO: prioritize channels based on DMAOR.PR
    for (auto &ch : dmaChannels) {
        if (!IsDMATransferActive(ch)) {
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
                continue;
            }
        }

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
            MemWriteByte(ch.dstAddress, value);
            break;
        }
        case DMATransferSize::Word: {
            const uint16 value = MemReadWord(ch.srcAddress);
            MemWriteWord(ch.dstAddress, value);
            break;
        }
        case DMATransferSize::Longword: {
            const uint32 value = MemReadLong(ch.srcAddress);
            MemWriteLong(ch.dstAddress, value);
            break;
        }
        case DMATransferSize::QuadLongword:
            for (int i = 0; i < 4; i++) {
                const uint32 value = MemReadLong(ch.srcAddress + i * sizeof(uint32));
                MemWriteLong(ch.dstAddress + i * sizeof(uint32), value);
            }
            break;
        }

        ch.srcAddress = incAddress(ch.srcAddress, ch.srcMode);
        ch.dstAddress = incAddress(ch.dstAddress, ch.dstMode);

        // Check if transfer ended
        if (ch.xferSize == DMATransferSize::QuadLongword) {
            ch.xferCount -= 4;
        } else {
            ch.xferCount--;
        }
        if (ch.xferCount == 0) {
            // Raise DEI interrupt if requested
            if (ch.irqEnable) {
                // TODO: raise DEI interrupt
            }
            ch.xferEnded = true;
        }
    }

    FRT.Advance(cycles);

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
        Execute(PC);
        // dbg_println("");
    }
}

void SH2::SetExternalInterrupt(uint8 level, uint8 vecNum) {
    assert(level < 16);
    m_pendingExternalIntrLevel = level;
    m_pendingExternalIntrVecNum = vecNum;
}

void SH2::SetNMI() {
    // HACK: should be edge-detected
    m_NMI = true;
    ICR.NMIL = 1;
}

void SH2::WriteFRTInput(uint16 value) {
    FRT.ICR = value;
    FRT.FTCSR.ICF = 1;
}

// -------------------------------------------------------------------------
// Debugging functions

/*uint64 dbg_count = 0;
static constexpr uint64 dbg_minCount = 99999999999; // 17635778; // 10489689; // 9302150; // 9547530;

template <typename... T>
void dbg_print(fmt::format_string<T...> fmt, T &&...args) {
    if (dbg_count >= dbg_minCount) {
        fmt::print(fmt, static_cast<T &&>(args)...);
    }
}

template <typename... T>
void dbg_println(fmt::format_string<T...> fmt, T &&...args) {
    if (dbg_count >= dbg_minCount) {
        fmt::println(fmt, static_cast<T &&>(args)...);
    }
}*/

// -----------------------------------------------------------------------------
// Memory accessors

template <mem_primitive T, bool instrFetch>
T SH2::MemRead(uint32 address) {
    const uint32 partition = (address >> 29u) & 0b111;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        fmt::println("{}SH2: WARNING: misaligned {}-bit read from {:08X}", (BCR1.MASTER ? "S" : "M"), sizeof(T) * 8,
                     address);
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
        fmt::println("{}SH2: unhandled {}-bit SH-2 associative purge read from {:08X}", (BCR1.MASTER ? "S" : "M"),
                     sizeof(T) * 8, address);
        return (address & 1) ? static_cast<T>(0x12231223) : static_cast<T>(0x23122312);
    case 0b011: { // cache address array
        uint32 entry = (address >> 4u) & 0x3F;
        return cacheEntries[entry].tag[CCR.Wn]; // TODO: include LRU data
    }
    case 0b100:
    case 0b110: // cache data array

        // TODO: implement
        fmt::println("{}SH2: unhandled {}-bit SH-2 cache data array read from {:08X}", (BCR1.MASTER ? "S" : "M"),
                     sizeof(T) * 8, address);
        return 0;
    case 0b111: // I/O area
        if constexpr (instrFetch) {
            // TODO: raise CPU address error due to attempt to fetch instruction from I/O area
            fmt::println("{}SH2: attempted to fetch instruction from I/O area at {:08X}", (BCR1.MASTER ? "S" : "M"),
                         address);
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
            fmt::println("{}SH2: unhandled {}-bit SH-2 I/O area read from {:08X}", (BCR1.MASTER ? "S" : "M"),
                         sizeof(T) * 8, address);
            return 0;
        }
    }

    util::unreachable();
}

template <mem_primitive T>
void SH2::MemWrite(uint32 address, T value) {
    const uint32 partition = address >> 29u;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        fmt::println("{}SH2: WARNING: misaligned {}-bit write to {:08X} = {:X}", (BCR1.MASTER ? "S" : "M"),
                     sizeof(T) * 8, address, value);
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
        fmt::println("{}SH2: unhandled {}-bit SH-2 associative purge write to {:08X} = {:X}", (BCR1.MASTER ? "S" : "M"),
                     sizeof(T) * 8, address, value);
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
        fmt::println("{}SH2: unhandled {}-bit SH-2 cache data array write to {:08X} = {:X}", (BCR1.MASTER ? "S" : "M"),
                     sizeof(T) * 8, address, value);
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
            case 0xFFFF8426: fmt::println("{}SH2: 16-bit CAS latency 1", (BCR1.MASTER ? "S" : "M")); break;
            case 0xFFFF8446: fmt::println("{}SH2: 16-bit CAS latency 2", (BCR1.MASTER ? "S" : "M")); break;
            case 0xFFFF8466: fmt::println("{}SH2: 16-bit CAS latency 3", (BCR1.MASTER ? "S" : "M")); break;
            case 0xFFFF8848: fmt::println("{}SH2: 32-bit CAS latency 1", (BCR1.MASTER ? "S" : "M")); break;
            case 0xFFFF8888: fmt::println("{}SH2: 32-bit CAS latency 2", (BCR1.MASTER ? "S" : "M")); break;
            case 0xFFFF88C8: fmt::println("{}SH2: 32-bit CAS latency 3", (BCR1.MASTER ? "S" : "M")); break;
            default:
                fmt::println("{}SH2: unhandled {}-bit SH-2 I/O area read from {:08X}", (BCR1.MASTER ? "S" : "M"),
                             sizeof(T) * 8, address);
                break;
            }
        } else {
            // TODO: implement
            fmt::println("{}SH2: unhandled {}-bit SH-2 I/O area write to {:08X} = {:X}", (BCR1.MASTER ? "S" : "M"),
                         sizeof(T) * 8, address, value);
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

FLATTEN FORCE_INLINE bool SH2::IsDMATransferActive(const DMAChannel &ch) const {
    return ch.IsEnabled() && DMAOR.DME && !DMAOR.NMIF && !DMAOR.AE;
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

    const sint32 dividend = static_cast<sint32>(DVDNT);
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
            // TODO: trigger interrupt
        }
    }
}

void SH2::DIVUBegin64() {
    static constexpr sint32 kMinValue32 = std::numeric_limits<sint32>::min();
    static constexpr sint32 kMaxValue32 = std::numeric_limits<sint32>::max();
    static constexpr sint64 kMinValue64 = std::numeric_limits<sint64>::min();

    sint64 dividend = (static_cast<sint64>(DVDNTH) << 32ll) | static_cast<sint64>(DVDNTL);
    const sint32 divisor = static_cast<sint32>(DVSR);

    bool overflow = divisor == 0;
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

        // Signal overflow
        DVCR.OVF = 1;

        // Update output registers
        if (DVCR.OVFIE) {
            DVDNTL = DVDNT = dividend;
        } else {
            // DVDNT/DVDNTL is saturated if the interrupt signal is disabled
            DVDNTL = DVDNT = dividend < 0 ? kMinValue32 : kMaxValue32;
        }
        DVDNTH = dividend >> 32ll;
    }
}

template <mem_primitive T>
T SH2::OnChipRegRead(uint32 address) {
    // Misaligned memory accesses raise an address error, meaning all accesses here are aligned.
    // Therefore:
    //   (address & 3) == 2 is only valid for 16-bit accesses
    //   (address & 1) == 1 is only valid for 8-bit accesses
    // Additionally:
    //   (address & 1) == 0 has special cases for registers 0-255:
    //     8-bit read from a 16-bit register:  r >> 8u
    //     16-bit read from a 8-bit register: (r << 8u) | r
    //     Every other access returns just r

    // Registers 0-255 do not accept 32-bit accesses
    if constexpr (std::is_same_v<T, uint32>) {
        if (address < 0x100) {
            // TODO: raise CPU address error
        }
    }

    // Registers 256-511 do not accept 8-bit accesses
    if constexpr (std::is_same_v<T, uint8>) {
        if (address >= 0x100) {
            // TODO: raise CPU address error
        }
    }

    auto readWordLower = [&](Reg16 value) -> T {
        if constexpr (std::is_same_v<T, uint8>) {
            return value.u8[(address & 1) ^ 1];
        } else {
            return value.u16;
        }
    };
    auto readByteLower = [&](uint8 value) -> T {
        if constexpr (std::is_same_v<T, uint16>) {
            return (address & 1) ? value : ((value << 8u) | value);
        } else {
            return value;
        }
    };

    // Note: Clang generates marginally faster code with case ranges here.
    // See:
    // https://quick-bench.com/q/vB2HZ3bzAIlqIazYoVxy7A0ooKg

    switch (address) {
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

    case 0x60 ... 0x61: return readWordLower(IPRB.val);
    case 0x62 ... 0x63: return readWordLower(VCRA.val);
    case 0x64 ... 0x65: return readWordLower(VCRB.val);
    case 0x66 ... 0x67: return readWordLower(VCRC.val);
    case 0x68 ... 0x69: return readWordLower(VCRD.val);

    case 0x71: return dmaChannels[0].ReadDRCR();
    case 0x72: return dmaChannels[1].ReadDRCR();

    case 0x92 ... 0x9F: return readByteLower(CCR.u8);
    case 0xE0 ... 0xE1: return readWordLower(ICR.val);
    case 0xE2 ... 0xE3: return readWordLower(IPRA.val);
    case 0xE4 ... 0xE5: return readWordLower(VCRWDT.val);

    case 0x100:
    case 0x120: return DVSR;

    case 0x104:
    case 0x124: return DVDNT;

    case 0x108:
    case 0x128: return DVCR.u32;

    case 0x10C:
    case 0x12C: return VCRDIV;

    case 0x110:
    case 0x130: return DVDNTH;

    case 0x114:
    case 0x134: return DVDNTL;

    case 0x180: return dmaChannels[0].srcAddress;
    case 0x184: return dmaChannels[0].dstAddress;
    case 0x188: return dmaChannels[0].xferCount;
    case 0x18C: return dmaChannels[0].ReadCHCR();

    case 0x190: return dmaChannels[1].srcAddress;
    case 0x194: return dmaChannels[1].dstAddress;
    case 0x198: return dmaChannels[1].xferCount;
    case 0x19C: return dmaChannels[1].ReadCHCR();

    case 0x1A0: return dmaChannels[0].vecNum;
    case 0x1A8: return dmaChannels[1].vecNum;

    case 0x1B0: return DMAOR.u32;

    case 0x1E0 ... 0x1E2: return BCR1.u16;
    case 0x1E4 ... 0x1E6: return BCR2.u16;
    case 0x1E8 ... 0x1EA: return WCR.u16;
    case 0x1EC ... 0x1EE: return MCR.u16;
    case 0x1F0 ... 0x1F2: return RTCSR.u16;
    case 0x1F4 ... 0x1F6: return RTCNT;
    case 0x1F8 ... 0x1FA: return RTCOR;

    default: //
        fmt::println("{}SH2: unhandled {}-bit on-chip register read from {:02X}", (BCR1.MASTER ? "S" : "M"),
                     sizeof(T) * 8, address);
        return 0;
    }
}

template <mem_primitive T>
void SH2::OnChipRegWrite(uint32 address, T baseValue) {
    // Misaligned memory accesses raise an address error, meaning all accesses here are aligned.
    // Therefore:
    //   (address & 3) == 2 is only valid for 16-bit accesses
    //   (address & 1) == 1 is only valid for 8-bit accesses

    // Registers 0-255 do not accept 32-bit accesses
    if constexpr (std::is_same_v<T, uint32>) {
        if (address < 0x100) {
            // TODO: raise an address error
        }
    }

    // Registers 256-511 do not accept 8-bit accesses
    uint32 value = baseValue;
    if constexpr (std::is_same_v<T, uint8>) {
        if (address >= 0x100) {
            // TODO: raise an address error
            value |= value << 8u;
        }
    }

    // For registers 0-255, 8-bit writes to 16-bit registers change the corresponding byte
    auto writeWordLower = [&](Reg16 &reg, T value, uint16 mask) {
        if constexpr (std::is_same_v<T, uint8>) {
            uint32 index = ((address & 1) ^ 1);
            mask >>= index * 8;
            if ((mask & 0xFF) != 0) {
                reg.u8[index] = value & mask;
            }
        } else {
            reg.u16 = value & mask;
        }
    };

    // Note: the repeated cases below might seem redundant, but it actually causes Clang to generate better code.
    // Case ranges (case 0x61 ... 0x62) or fallthrough (case 0x61: case 0x62) generate suboptimal code.
    // See:
    // https://quick-bench.com/q/j3XUHw-u-Y75chqoIPnxwyF3JXw
    // https://godbolt.org/z/5nbPxqd5d

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

    case 0x60: writeWordLower(IPRB.val, value, 0xFF00); break;
    case 0x61: writeWordLower(IPRB.val, value, 0xFF00); break;
    case 0x62: writeWordLower(VCRA.val, value, 0x7F7F); break;
    case 0x63: writeWordLower(VCRA.val, value, 0x7F7F); break;
    case 0x64: writeWordLower(VCRB.val, value, 0x7F7F); break;
    case 0x65: writeWordLower(VCRB.val, value, 0x7F7F); break;
    case 0x66: writeWordLower(VCRC.val, value, 0x7F7F); break;
    case 0x67: writeWordLower(VCRC.val, value, 0x7F7F); break;
    case 0x68: writeWordLower(VCRD.val, value, 0x7F00); break;
    case 0x69: writeWordLower(VCRD.val, value, 0x7F00); break;

    case 0x71: dmaChannels[0].WriteDRCR(value); break;
    case 0x72: dmaChannels[1].WriteDRCR(value); break;

    case 0x92: WriteCCR(value); break;

    case 0xE0: writeWordLower(ICR.val, value, 0x0101); break;
    case 0xE1: writeWordLower(ICR.val, value, 0x0101); break;
    case 0xE2: writeWordLower(IPRA.val, value, 0xFFF0); break;
    case 0xE3: writeWordLower(IPRA.val, value, 0xFFF0); break;
    case 0xE4: writeWordLower(VCRWDT.val, value, 0x7F7F); break;
    case 0xE5: writeWordLower(VCRWDT.val, value, 0x7F7F); break;

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
    case 0x12C: VCRDIV = value; break;

    case 0x110:
    case 0x130: DVDNTH = value; break;

    case 0x114:
    case 0x134:
        DVDNTL = value;
        DIVUBegin64();
        break;

    case 0x180: dmaChannels[0].srcAddress = value; break;
    case 0x184: dmaChannels[0].dstAddress = value; break;
    case 0x188: dmaChannels[0].xferCount = bit::extract<0, 23>(value); break;
    case 0x18C: dmaChannels[0].WriteCHCR(value); break;

    case 0x190: dmaChannels[1].srcAddress = value; break;
    case 0x194: dmaChannels[1].dstAddress = value; break;
    case 0x198: dmaChannels[1].xferCount = bit::extract<0, 23>(value); break;
    case 0x19C: dmaChannels[1].WriteCHCR(value); break;

    case 0x1A0: dmaChannels[0].vecNum = value; break;
    case 0x1A8: dmaChannels[1].vecNum = value; break;

    case 0x1B0:
        DMAOR.DME = bit::extract<0>(value);
        DMAOR.NMIF &= bit::extract<1>(value);
        DMAOR.AE &= bit::extract<2>(value);
        DMAOR.PR = bit::extract<3>(value);
        break;

    case 0x1E0: // BCR1
        // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
        if constexpr (std::is_same_v<T, uint32>) {
            if ((value >> 16u) == 0xA55A) {
                BCR1.u15 = value & 0x1FF7;
            }
        }
        break;
    case 0x1E4: // BCR2
        // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
        if constexpr (std::is_same_v<T, uint32>) {
            if ((value >> 16u) == 0xA55A) {
                BCR2.u16 = value & 0xFC;
            }
        }
        break;
    case 0x1E8: // WCR
        // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
        if constexpr (std::is_same_v<T, uint32>) {
            if ((value >> 16u) == 0xA55A) {
                WCR.u16 = value;
            }
        }
        break;
    case 0x1EC: // MCR
        // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
        if constexpr (std::is_same_v<T, uint32>) {
            if ((value >> 16u) == 0xA55A) {
                MCR.u16 = value & 0xFEFC;
            }
        }
        break;
    case 0x1F0: // RTCSR
        // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
        if constexpr (std::is_same_v<T, uint32>) {
            if ((value >> 16u) == 0xA55A) {
                // TODO: implement the set/clear rules for RTCSR.CMF
                RTCSR.u16 = (value & 0x78) | (RTCSR.u16 & 0x80);
            }
        }
        break;
    case 0x1F4: // RTCNT
        // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
        if constexpr (std::is_same_v<T, uint32>) {
            if ((value >> 16u) == 0xA55A) {
                RTCNT = value;
            }
        }
        break;
    case 0x1F8: // RTCOR
        // Only accepts 32-bit writes and the top 16 bits must be 0xA55A
        if constexpr (std::is_same_v<T, uint32>) {
            if ((value >> 16u) == 0xA55A) {
                RTCOR = value;
            }
        }
        break;
    default: //
        fmt::println("{}SH2: unhandled {}-bit on-chip register write to {:02X} = {:X}", (BCR1.MASTER ? "S" : "M"),
                     sizeof(T) * 8, address, value);
        break;
    }
}

// -----------------------------------------------------------------------------
// Interrupts

bool SH2::CheckInterrupts() {
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

    // TODO: optimize
    // potential solutions:
    // - use a sorted vector of unique interrupts
    // - precompute highest priority interrupt whenever they change
    //   - adding an interrupt is easy
    //   - removing is a problem

    m_pendingInterrupt.priority = 0;
    m_pendingInterrupt.vecNum = 0x00;

    auto update = [&](uint8 intrPriority, uint8 vecNum) {
        if (intrPriority > m_pendingInterrupt.priority) {
            m_pendingInterrupt.priority = intrPriority;
            m_pendingInterrupt.vecNum = vecNum;
        }
    };

    // HACK: should be edge-detected
    // this only works because NMI has the highest priority and can't be masked
    if (m_NMI) {
        // Set NMI interrupt: vector 0x0B, priority 16
        m_NMI = false;
        update(16, 0x0B);
        return true;
    }

    // TODO: user break

    // IRLs
    const uint8 externalIntrVecNum =
        ICR.VECMD ? m_pendingExternalIntrVecNum : 0x40 + (m_pendingExternalIntrLevel >> 1u);
    update(m_pendingExternalIntrLevel, externalIntrVecNum);

    if (DVCR.OVF && DVCR.OVFIE) {
        update(IPRA.DIVUIPn, VCRDIV);
    }

    // TODO: DMAC0, DMAC1 transfer end
    // TODO: WDT ITI, BSC REF CMI
    // TODO: SCI ERI, RXI, TXI, TEI

    // Free-running timer interrupts
    if (FRT.FTCSR.ICF && FRT.TIER.ICIE) {
        update(VCRC.FICVn, IPRB.FRTIPn);
    }
    if ((FRT.FTCSR.OCFA && FRT.TIER.OCIAE) || (FRT.FTCSR.OCFB && FRT.TIER.OCIBE)) {
        update(VCRC.FOCVn, IPRB.FRTIPn);
    }
    if (FRT.FTCSR.OVF && FRT.TIER.OVIE) {
        update(VCRD.FOVVn, IPRB.FRTIPn);
    }

    const bool result = m_pendingInterrupt.priority > SR.ILevel;
    const bool usingExternalIntr =
        m_pendingInterrupt.priority == m_pendingExternalIntrLevel && m_pendingInterrupt.vecNum == externalIntrVecNum;
    if (result && usingExternalIntr) {
        m_bus.AcknowledgeExternalInterrupt();
    }
    return result;
}

// -------------------------------------------------------------------------
// Helper functions

FORCE_INLINE void SH2::SetupDelaySlot(uint32 targetAddress) {
    m_delaySlot = true;
    m_delaySlotTarget = targetAddress;
}

FORCE_INLINE void SH2::EnterException(uint8 vectorNumber) {
    R[15] -= 4;
    MemWriteLong(R[15], SR.u32);
    R[15] -= 4;
    MemWriteLong(R[15], PC - 4);
    PC = MemReadLong(VBR + (static_cast<uint32>(vectorNumber) << 2u));
}
// -----------------------------------------------------------------------------
// Interpreter

void SH2::Execute(uint32 address) {
    if (!m_delaySlot && CheckInterrupts()) [[unlikely]] {
        // fmt::println("{}SH2: >> intr level {:02X} vec {:02X}", (BCR1.MASTER ? "S" : "M"),
        // m_pendingInterrupt.priority, m_pendingInterrupt.vecNum);
        EnterException(m_pendingInterrupt.vecNum);
        SR.ILevel = m_pendingInterrupt.priority;
        address = PC;
    }

    // TODO: emulate fetch - decode - execute - memory access - writeback pipeline
    // TODO: figure out a way to optimize delay slots for performance
    // - perhaps decoding instructions beforehand

    const uint16 instr = FetchInstruction(address);

    /*static uint64 dbg_count = 0;
    ++dbg_count;
    // dbg_print("[{:10}] {:08X}{} {:04X}  ", dbg_count, address, delaySlot ? '*' : ' ', instr);
    if (dbg_count > 14000000) {
        fmt::print("{:08X}{} {:04X}  ", address, delaySlot ? '*' : ' ', instr);
        for (int i = 0; i < 16; i++) {
            fmt::print(" {:08X}", R[i]);
        }
        fmt::println("  pr = {:08X}  sr = {:08X}  gbr = {:08X}  vbr = {:08X}", PR, SR.u32, GBR, VBR);
    }*/

    auto advancePC = [&] {
        if (m_delaySlot) {
            PC = m_delaySlotTarget;
            m_delaySlot = false;
        } else {
            PC += 2;
        }
    };

    auto nonDelaySlot = [&](auto instr, auto... params) {
        if (m_delaySlot) {
            EnterException(xvSlotIllegalInstr);
        } else {
            (this->*instr)(params...);
        }
    };

    switch (instr >> 12u) {
    case 0x0:
        switch (instr) {
        case 0x0008: CLRT(), advancePC(); break;     // 0    0000 0000 0000 1000   CLRT
        case 0x0009: NOP(), advancePC(); break;      // 0    0000 0000 0000 1001   NOP
        case 0x000B: nonDelaySlot(&SH2::RTS); break; // 0    0000 0000 0000 1011   RTS
        case 0x0018: SETT(), advancePC(); break;     // 0    0000 0000 0001 1000   SETT
        case 0x0019: DIV0U(), advancePC(); break;    // 0    0000 0000 0001 1001   DIV0U
        case 0x001B: SLEEP(), advancePC(); break;    // 0    0000 0000 0001 1011   SLEEP
        case 0x0028: CLRMAC(), advancePC(); break;   // 0    0000 0000 0010 1000   CLRMAC
        case 0x002B: nonDelaySlot(&SH2::RTE); break; // 0    0000 0000 0010 1011   RTE
        default:
            switch (instr & 0xFF) {
            case 0x02: STCSR(instr), advancePC(); break;       // n    0000 nnnn 0000 0010   STC SR, Rn
            case 0x03: nonDelaySlot(&SH2::BSRF, instr); break; // m    0000 mmmm 0000 0011   BSRF Rm
            case 0x0A: STSMACH(instr), advancePC(); break;     // n    0000 nnnn 0000 1010   STS MACH, Rn
            case 0x12: STCGBR(instr), advancePC(); break;      // n    0000 nnnn 0001 0010   STC GBR, Rn
            case 0x1A: STSMACL(instr), advancePC(); break;     // n    0000 nnnn 0001 1010   STS MACL, Rn
            case 0x22: STCVBR(instr), advancePC(); break;      // n    0000 nnnn 0010 0010   STC VBR, Rn
            case 0x23: nonDelaySlot(&SH2::BRAF, instr); break; // m    0000 mmmm 0010 0011   BRAF Rm
            case 0x29: MOVT(instr), advancePC(); break;        // n    0000 nnnn 0010 1001   MOVT Rn
            case 0x2A: STSPR(instr), advancePC(); break;       // n    0000 nnnn 0010 1010   STS PR, Rn
            default:
                switch (instr & 0xF) {
                case 0x4: MOVBS0(instr), advancePC(); break; // nm   0000 nnnn mmmm 0100   MOV.B Rm, @(R0,Rn)
                case 0x5: MOVWS0(instr), advancePC(); break; // nm   0000 nnnn mmmm 0101   MOV.W Rm, @(R0,Rn)
                case 0x6: MOVLS0(instr), advancePC(); break; // nm   0000 nnnn mmmm 0110   MOV.L Rm, @(R0,Rn)
                case 0x7: MULL(instr), advancePC(); break;   // nm   0000 nnnn mmmm 0111   MUL.L Rm, Rn
                case 0xC: MOVBL0(instr), advancePC(); break; // nm   0000 nnnn mmmm 1100   MOV.B @(R0,Rm), Rn
                case 0xD: MOVWL0(instr), advancePC(); break; // nm   0000 nnnn mmmm 1101   MOV.W @(R0,Rm), Rn
                case 0xE: MOVLL0(instr), advancePC(); break; // nm   0000 nnnn mmmm 1110   MOV.L @(R0,Rm), Rn
                case 0xF: MACL(instr), advancePC(); break;   // nm   0000 nnnn mmmm 1111   MAC.L @Rm+, @Rn+
                default: /*dbg_println("unhandled 0000 instruction");*/ __debugbreak(); break;
                }
                break;
            }
            break;
        }
        break;
    case 0x1: MOVLS4(instr), advancePC(); break; // nmd  0001 nnnn mmmm dddd   MOV.L Rm, @(disp,Rn)
    case 0x2: {
        switch (instr & 0xF) {
        case 0x0: MOVBS(instr), advancePC(); break; // nm   0010 nnnn mmmm 0000   MOV.B Rm, @Rn
        case 0x1: MOVWS(instr), advancePC(); break; // nm   0010 nnnn mmmm 0001   MOV.W Rm, @Rn
        case 0x2: MOVLS(instr), advancePC(); break; // nm   0010 nnnn mmmm 0010   MOV.L Rm, @Rn

        case 0x4: MOVBM(instr), advancePC(); break;  // nm   0010 nnnn mmmm 0100   MOV.B Rm, @-Rn
        case 0x5: MOVWM(instr), advancePC(); break;  // nm   0010 nnnn mmmm 0101   MOV.W Rm, @-Rn
        case 0x6: MOVLM(instr), advancePC(); break;  // nm   0010 nnnn mmmm 0110   MOV.L Rm, @-Rn
        case 0x7: DIV0S(instr), advancePC(); break;  // nm   0010 nnnn mmmm 0110   DIV0S Rm, Rn
        case 0x8: TST(instr), advancePC(); break;    // nm   0010 nnnn mmmm 1000   TST Rm, Rn
        case 0x9: AND(instr), advancePC(); break;    // nm   0010 nnnn mmmm 1001   AND Rm, Rn
        case 0xA: XOR(instr), advancePC(); break;    // nm   0010 nnnn mmmm 1010   XOR Rm, Rn
        case 0xB: OR(instr), advancePC(); break;     // nm   0010 nnnn mmmm 1011   OR Rm, Rn
        case 0xC: CMPSTR(instr), advancePC(); break; // nm   0010 nnnn mmmm 1100   CMP/STR Rm, Rn
        case 0xD: XTRCT(instr), advancePC(); break;  // nm   0010 nnnn mmmm 1101   XTRCT Rm, Rn
        case 0xE: MULU(instr), advancePC(); break;   // nm   0010 nnnn mmmm 1110   MULU.W Rm, Rn
        case 0xF: MULS(instr), advancePC(); break;   // nm   0010 nnnn mmmm 1111   MULS.W Rm, Rn
        default: /*dbg_println("unhandled 0010 instruction");*/ __debugbreak(); break;
        }
        break;
    }
    case 0x3:
        switch (instr & 0xF) {
        case 0x0: CMPEQ(instr), advancePC(); break; // nm   0011 nnnn mmmm 0000   CMP/EQ Rm, Rn
        case 0x2: CMPHS(instr), advancePC(); break; // nm   0011 nnnn mmmm 0010   CMP/HS Rm, Rn
        case 0x3: CMPGE(instr), advancePC(); break; // nm   0011 nnnn mmmm 0011   CMP/GE Rm, Rn
        case 0x4: DIV1(instr), advancePC(); break;  // nm   0011 nnnn mmmm 0100   DIV1 Rm, Rn
        case 0x5: DMULU(instr), advancePC(); break; // nm   0011 nnnn mmmm 0101   DMULU.L Rm, Rn
        case 0x6: CMPHI(instr), advancePC(); break; // nm   0011 nnnn mmmm 0110   CMP/HI Rm, Rn
        case 0x7: CMPGT(instr), advancePC(); break; // nm   0011 nnnn mmmm 0111   CMP/GT Rm, Rn
        case 0x8: SUB(instr), advancePC(); break;   // nm   0011 nnnn mmmm 1000   SUB Rm, Rn

        case 0xA: SUBC(instr), advancePC(); break; // nm   0011 nnnn mmmm 1010   SUBC Rm, Rn
        case 0xB: SUBV(instr), advancePC(); break; // nm   0011 nnnn mmmm 1011   SUBV Rm, Rn

        case 0xC: ADD(instr), advancePC(); break;   // nm   0011 nnnn mmmm 1100   ADD Rm, Rn
        case 0xD: DMULS(instr), advancePC(); break; // nm   0011 nnnn mmmm 1101   DMULS.L Rm, Rn
        case 0xE: ADDC(instr), advancePC(); break;  // nm   0011 nnnn mmmm 1110   ADDC Rm, Rn
        case 0xF: ADDV(instr), advancePC(); break;  // nm   0011 nnnn mmmm 1110   ADDV Rm, Rn
        default: /*dbg_println("unhandled 0011 instruction");*/ __debugbreak(); break;
        }
        break;
    case 0x4:
        if ((instr & 0xF) == 0xF) {
            // nMACW(instr),advancePC();break;m   0100 nnnn mmmm 1111   MAC.W @Rm+, @Rn+
        } else {
            switch (instr & 0xFF) {
            case 0x00: SHLL(instr), advancePC(); break;       // n    0100 nnnn 0000 0000   SHLL Rn
            case 0x01: SHLR(instr), advancePC(); break;       // n    0100 nnnn 0000 0001   SHLR Rn
            case 0x02: STSMMACH(instr), advancePC(); break;   // n    0100 nnnn 0000 0010   STS.L MACH, @-Rn
            case 0x03: STCMSR(instr), advancePC(); break;     // n    0100 nnnn 0000 0010   STC.L SR, @-Rn
            case 0x04: ROTL(instr), advancePC(); break;       // n    0100 nnnn 0000 0100   ROTL Rn
            case 0x05: ROTR(instr), advancePC(); break;       // n    0100 nnnn 0000 0101   ROTR Rn
            case 0x06: LDSMMACH(instr), advancePC(); break;   // m    0100 mmmm 0000 0110   LDS.L @Rm+, MACH
            case 0x07: LDCMSR(instr), advancePC(); break;     // m    0100 mmmm 0000 0111   LDC.L @Rm+, SR
            case 0x08: SHLL2(instr), advancePC(); break;      // n    0100 nnnn 0000 1000   SHLL2 Rn
            case 0x09: SHLR2(instr), advancePC(); break;      // n    0100 nnnn 0000 1001   SHLR2 Rn
            case 0x0A: LDSMACH(instr), advancePC(); break;    // m    0100 mmmm 0000 1010   LDS Rm, MACH
            case 0x0B: nonDelaySlot(&SH2::JSR, instr); break; // m    0100 mmmm 0000 1011   JSR @Rm

            case 0x0E: LDCSR(instr), advancePC(); break; // m    0100 mmmm 0000 1110   LDC Rm, SR

            case 0x10: DT(instr), advancePC(); break;       // n    0100 nnnn 0001 0000   DT Rn
            case 0x11: CMPPZ(instr), advancePC(); break;    // n    0100 nnnn 0001 0001   CMP/PZ Rn
            case 0x12: STSMMACL(instr), advancePC(); break; // n    0100 nnnn 0001 0010   STS.L MACL, @-Rn
            case 0x13: STCMGBR(instr), advancePC(); break;  // n    0100 nnnn 0001 0011   STC.L GBR, @-Rn

            case 0x15: CMPPL(instr), advancePC(); break;    // n    0100 nnnn 0001 0101   CMP/PL Rn
            case 0x16: LDSMMACL(instr), advancePC(); break; // m    0100 mmmm 0001 0110   LDS.L @Rm+, MACL
            case 0x17: LDCMGBR(instr), advancePC(); break;  // m    0100 mmmm 0001 0111   LDC.L @Rm+, GBR
            case 0x18: SHLL8(instr), advancePC(); break;    // n    0100 nnnn 0001 1000   SHLL8 Rn
            case 0x19: SHLR8(instr), advancePC(); break;    // n    0100 nnnn 0001 1001   SHLR8 Rn
            case 0x1A: LDSMACL(instr), advancePC(); break;  // m    0100 mmmm 0001 1010   LDS Rm, MACL
            case 0x1B: TAS(instr), advancePC(); break;      // n    0100 nnnn 0001 1011   TAS.B @Rn

            case 0x1E: LDCGBR(instr), advancePC(); break; // m    0110 mmmm 0001 1110   LDC Rm, GBR

            case 0x20: SHAL(instr), advancePC(); break;       // n    0100 nnnn 0010 0000   SHAL Rn
            case 0x21: SHAR(instr), advancePC(); break;       // n    0100 nnnn 0010 0001   SHAR Rn
            case 0x22: STSMPR(instr), advancePC(); break;     // n    0100 nnnn 0010 0010   STS.L PR, @-Rn
            case 0x23: STCMVBR(instr), advancePC(); break;    // n    0100 nnnn 0010 0011   STC.L VBR, @-Rn
            case 0x24: ROTCL(instr), advancePC(); break;      // n    0100 nnnn 0010 0100   ROTCL Rn
            case 0x25: ROTCR(instr), advancePC(); break;      // n    0100 nnnn 0010 0101   ROTCR Rn
            case 0x26: LDSMPR(instr), advancePC(); break;     // m    0100 mmmm 0010 0110   LDS.L @Rm+, PR
            case 0x27: LDCMVBR(instr), advancePC(); break;    // m    0100 mmmm 0010 0111   LDC.L @Rm+, VBR
            case 0x28: SHLL16(instr), advancePC(); break;     // n    0100 nnnn 0010 1000   SHLL16 Rn
            case 0x29: SHLR16(instr), advancePC(); break;     // n    0100 nnnn 0010 1001   SHLR16 Rn
            case 0x2A: LDSPR(instr), advancePC(); break;      // m    0100 mmmm 0010 1010   LDS Rm, PR
            case 0x2B: nonDelaySlot(&SH2::JMP, instr); break; // m    0100 mmmm 0010 1011   JMP @Rm

            case 0x2E: LDCVBR(instr), advancePC(); break; // m    0110 mmmm 0010 1110   LDC Rm, VBR

            default: /*dbg_println("unhandled 0100 instruction");*/ __debugbreak(); break;
            }
        }
        break;
    case 0x5: MOVLL4(instr), advancePC(); break; // nmd  0101 nnnn mmmm dddd   MOV.L @(disp,Rm), Rn
    case 0x6:
        switch (instr & 0xF) {
        case 0x0: MOVBL(instr), advancePC(); break; // nm   0110 nnnn mmmm 0000   MOV.B @Rm, Rn
        case 0x1: MOVWL(instr), advancePC(); break; // nm   0110 nnnn mmmm 0001   MOV.W @Rm, Rn
        case 0x2: MOVLL(instr), advancePC(); break; // nm   0110 nnnn mmmm 0010   MOV.L @Rm, Rn
        case 0x3: MOV(instr), advancePC(); break;   // nm   0110 nnnn mmmm 0010   MOV Rm, Rn
        case 0x4: MOVBP(instr), advancePC(); break; // nm   0110 nnnn mmmm 0110   MOV.B @Rm+, Rn
        case 0x5: MOVWP(instr), advancePC(); break; // nm   0110 nnnn mmmm 0110   MOV.W @Rm+, Rn
        case 0x6: MOVLP(instr), advancePC(); break; // nm   0110 nnnn mmmm 0110   MOV.L @Rm+, Rn
        case 0x7: NOT(instr), advancePC(); break;   // nm   0110 nnnn mmmm 0111   NOT Rm, Rn
        case 0x8: SWAPB(instr), advancePC(); break; // nm   0110 nnnn mmmm 1000   SWAP.B Rm, Rn
        case 0x9: SWAPW(instr), advancePC(); break; // nm   0110 nnnn mmmm 1001   SWAP.W Rm, Rn
        case 0xA: NEGC(instr), advancePC(); break;  // nm   0110 nnnn mmmm 1010   NEGC Rm, Rn
        case 0xB: NEG(instr), advancePC(); break;   // nm   0110 nnnn mmmm 1011   NEG Rm, Rn
        case 0xC: EXTUB(instr), advancePC(); break; // nm   0110 nnnn mmmm 1100   EXTU.B Rm, Rn
        case 0xD: EXTUW(instr), advancePC(); break; // nm   0110 nnnn mmmm 1101   EXTU.W Rm, Rn
        case 0xE: EXTSB(instr), advancePC(); break; // nm   0110 nnnn mmmm 1110   EXTS.B Rm, Rn
        case 0xF: EXTSW(instr), advancePC(); break; // nm   0110 nnnn mmmm 1111   EXTS.W Rm, Rn
        }
        break;
    case 0x7: ADDI(instr), advancePC(); break; // ni   0111 nnnn iiii iiii   ADD #imm, Rn
    case 0x8:
        switch ((instr >> 8u) & 0xF) {
        case 0x0: MOVBS4(instr), advancePC(); break; // nd4  1000 0000 nnnn dddd   MOV.B R0, @(disp,Rn)
        case 0x1: MOVWS4(instr), advancePC(); break; // nd4  1000 0001 nnnn dddd   MOV.W R0, @(disp,Rn)

        case 0x4: MOVBL4(instr), advancePC(); break; // md   1000 0100 mmmm dddd   MOV.B @(disp,Rm), R0
        case 0x5: MOVWL4(instr), advancePC(); break; // md   1000 0101 mmmm dddd   MOV.W @(disp,Rm), R0

        case 0x8: CMPIM(instr), advancePC(); break;     // i    1000 1000 iiii iiii   CMP/EQ #imm, R0
        case 0x9: nonDelaySlot(&SH2::BT, instr); break; // d    1000 1001 dddd dddd   BT <label>

        case 0xB: nonDelaySlot(&SH2::BF, instr); break; // d    1000 1011 dddd dddd   BF <label>

        case 0xD: nonDelaySlot(&SH2::BTS, instr); break; // d    1000 1101 dddd dddd   BT/S <label>

        case 0xF: nonDelaySlot(&SH2::BFS, instr); break; // d    1000 1111 dddd dddd   BF/S <label>

        default: /*dbg_println("unhandled 1000 instruction");*/ __debugbreak(); break;
        }
        break;
    case 0x9: MOVWI(instr), advancePC(); break;      // nd8  1001 nnnn dddd dddd   MOV.W @(disp,PC), Rn
    case 0xA: nonDelaySlot(&SH2::BRA, instr); break; // d12  1010 dddd dddd dddd   BRA <label>

    case 0xB: nonDelaySlot(&SH2::BSR, instr); break; // d12  1011 dddd dddd dddd   BSR <label>

    case 0xC:
        switch ((instr >> 8u) & 0xF) {
        case 0x0: MOVBSG(instr), advancePC(); break;       // d    1100 0000 dddd dddd   MOV.B R0, @(disp,GBR)
        case 0x1: MOVWSG(instr), advancePC(); break;       // d    1100 0001 dddd dddd   MOV.W R0, @(disp,GBR)
        case 0x2: MOVLSG(instr), advancePC(); break;       // d    1100 0010 dddd dddd   MOV.L R0, @(disp,GBR)
        case 0x3: nonDelaySlot(&SH2::TRAPA, instr); break; // i    1100 0011 iiii iiii   TRAPA #imm

        case 0x4: MOVBLG(instr), advancePC(); break; // d    1100 0100 dddd dddd   MOV.B @(disp,GBR), R0
        case 0x5: MOVWLG(instr), advancePC(); break; // d    1100 0101 dddd dddd   MOV.W @(disp,GBR), R0
        case 0x6: MOVLLG(instr), advancePC(); break; // d    1100 0110 dddd dddd   MOV.L @(disp,GBR), R0
        case 0x7: MOVA(instr), advancePC(); break;   // d    1100 0111 dddd dddd   MOVA @(disp,PC), R0
        case 0x8: TSTI(instr), advancePC(); break;   // i    1100 1000 iiii iiii   TST #imm, R0
        case 0x9: ANDI(instr), advancePC(); break;   // i    1100 1001 iiii iiii   AND #imm, R0
        case 0xA: XORI(instr), advancePC(); break;   // i    1100 1010 iiii iiii   XOR #imm, R0
        case 0xB: ORI(instr), advancePC(); break;    // i    1100 1011 iiii iiii   OR #imm, R0
        case 0xC: TSTM(instr), advancePC(); break;   // i    1100 1100 iiii iiii   TST.B #imm, @(R0,GBR)
        case 0xD: ANDM(instr), advancePC(); break;   // i    1100 1001 iiii iiii   AND #imm, @(R0,GBR)
        case 0xE: XORM(instr), advancePC(); break;   // i    1100 1001 iiii iiii   XOR #imm, @(R0,GBR)
        case 0xF: ORM(instr), advancePC(); break;    // i    1100 1001 iiii iiii   OR #imm, @(R0,GBR)
        default: /*dbg_println("unhandled 1100 instruction");*/ __debugbreak(); break;
        }
        break;
    case 0xD: MOVLI(instr), advancePC(); break; // nd8  1101 nnnn dddd dddd   MOV.L @(disp,PC), Rn
    case 0xE: MOVI(instr), advancePC(); break;  // ni   1110 nnnn iiii iiii   MOV #imm, Rn

    default: /*dbg_println("unhandled instruction");*/ __debugbreak(); break;
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
    // TODO: wait for exception
    __debugbreak();
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

    const sint32 op2 = bit::sign_extend<16, sint32>(MemReadWord(R[instr.Rn]));
    R[instr.Rn] += 2;
    const sint32 op1 = bit::sign_extend<16, sint32>(MemReadWord(R[instr.Rm]));
    R[instr.Rm] += 2;

    const sint32 mul = op1 * op2;
    if (SR.S) {
        const sint64 result = static_cast<sint64>(static_cast<sint32>(MAC.L)) + mul;
        const sint32 saturatedResult = std::clamp<sint64>(result, 0xFFFFFFFF'80000000, 0x00000000'7FFFFFFF);
        if (result == saturatedResult) {
            MAC.L = result;
        } else {
            MAC.L = saturatedResult;
            MAC.H = 1;
        }
    } else {
        MAC.u64 += mul;
    }
}

FORCE_INLINE void SH2::MACL(InstrNM instr) {
    // dbg_println("mac.l @r{}+, @r{}+)", instr.Rm, instr.Rn);

    const sint64 op2 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[instr.Rn])));
    R[instr.Rn] += 4;
    const sint64 op1 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[instr.Rm])));
    R[instr.Rm] += 4;

    const sint64 mul = op1 * op2;
    sint64 result = mul + MAC.u64;
    if (SR.S) {
        if (bit::extract<63>((result ^ MAC.u64) & (result ^ mul))) {
            if (bit::extract<63>(MAC.u64)) {
                result = 0xFFFF8000'00000000;
            } else {
                result = 0x00007FFF'FFFFFFFF;
            }
        } else {
            result = std::clamp<sint64>(result, 0xFFFF8000'00000000, 0x00007FFF'FFFFFFFF);
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
    const sint32 sdisp = (bit::sign_extend<12>(instr.disp) << 1) + 4;
    // dbg_println("bsr 0x{:08X}", PC + sdisp);

    PR = PC;
    SetupDelaySlot(PC + sdisp);
    PC += 2;
}

FORCE_INLINE void SH2::BSRF(InstrM instr) {
    // dbg_println("bsrf r{}", instr.Rm);
    PR = PC;
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
    PR = PC;
    SetupDelaySlot(R[instr.Rm]);
    PC += 2;
}

FORCE_INLINE void SH2::TRAPA(InstrI instr) {
    // dbg_println("trapa #0x{:X}", instr.imm);
    R[15] -= 4;
    MemWriteLong(R[15], SR.u32);
    R[15] -= 4;
    MemWriteLong(R[15], PC - 2);
    PC = MemReadLong(VBR + (instr.imm << 2u));
}

FORCE_INLINE void SH2::RTE() {
    // dbg_println("rte");
    SetupDelaySlot(MemReadLong(R[15]) + 4);
    PC += 2;
    R[15] += 4;
    SR.u32 = MemReadLong(R[15]) & 0x000003F3;
    R[15] += 4;
}

FORCE_INLINE void SH2::RTS() {
    // dbg_println("rts");
    SetupDelaySlot(PR + 4);
    PC += 2;
}

} // namespace satemu::sh2

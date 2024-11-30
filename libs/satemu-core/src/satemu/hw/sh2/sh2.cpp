#include <satemu/hw/sh2/sh2.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/unreachable.hpp>

#include <algorithm>
#include <limits>

namespace satemu {

uint64 dbg_count = 0;
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
}

// -----------------------------------------------------------------------------

// Returns 00 00 00 01 00 02 00 03 00 04 00 05 00 06 00 07 ... repeating
template <mem_access_type T>
T OpenBusSeqRead(uint32 address) {
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

SH2::SH2(SH2Bus &bus, bool master)
    : m_bus(bus) {
    BCR1.MASTER = !master;
    Reset(true);
}

void SH2::Reset(bool hard) {
    // Initial values:
    // - R0-R14 = undefined
    // - R15 = ReadLong(VBR + 4)

    // - SR = bits I3-I0 set, reserved bits clear, the rest is undefined
    // - GBR = undefined
    // - VBR = 0x00000000

    // - MACH, MACL = undefined
    // - PR = undefined
    // - PC = ReadLong(VBR)

    R.fill(0);
    PR = 0;

    SR.u32 = 0;
    SR.I0 = SR.I1 = SR.I2 = SR.I3 = 1;
    GBR = 0;
    VBR = 0x00000000;

    MAC.u64 = 0;

    PC = MemReadLong(VBR);
    R[15] = MemReadLong(VBR + 4);

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
    VCRDMA0 = 0x00;  // undefined initial value
    VCRDMA1 = 0x00;  // undefined initial value
    BCR1.u15 = 0x03F0;
    BCR2.u16 = 0x00FC;
    WCR.u16 = 0xAAFF;
    MCR.u16 = 0x0000;

    m_cacheEntries.fill({});
    WriteCCR(0x00);

    DVSR = 0x0;  // undefined initial value
    DVDNT = 0x0; // undefined initial value
    DVCR.u32 = 0x00000000;
    DVDNTH = 0x0; // undefined initial value
    DVDNTL = 0x0; // undefined initial value
}

void SH2::Step() {
    auto bit = [](bool value, std::string_view bit) { return value ? fmt::format(" {}", bit) : ""; };

    dbg_println(" R0 = {:08X}   R4 = {:08X}   R8 = {:08X}  R12 = {:08X}", R[0], R[4], R[8], R[12]);
    dbg_println(" R1 = {:08X}   R5 = {:08X}   R9 = {:08X}  R13 = {:08X}", R[1], R[5], R[9], R[13]);
    dbg_println(" R2 = {:08X}   R6 = {:08X}  R10 = {:08X}  R14 = {:08X}", R[2], R[6], R[10], R[14]);
    dbg_println(" R3 = {:08X}   R7 = {:08X}  R11 = {:08X}  R15 = {:08X}", R[3], R[7], R[11], R[15]);
    dbg_println("GBR = {:08X}  VBR = {:08X}  MAC = {:08X}.{:08X}", GBR, VBR, MAC.H, MAC.L);
    dbg_println(" PC = {:08X}   PR = {:08X}   SR = {:08X} {}{}{}{}{}{}{}{}", PC, PR, SR.u32, bit(SR.M, "M"),
                bit(SR.Q, "Q"), bit(SR.I3, "I3"), bit(SR.I2, "I2"), bit(SR.I1, "I1"), bit(SR.I0, "I0"), bit(SR.S, "S"),
                bit(SR.T, "T"));

    Execute<false>(PC);
    dbg_println("");
}

void SH2::SetInterruptLevel(uint8 level) {
    assert(level < 16);
    m_pendingIRL = level;
    CheckInterrupts();
}

// -------------------------------------------------------------------------
// Memory accessors

template <mem_access_type T>
T SH2::MemRead(uint32 address) {
    const uint32 partition = (address >> 29u) & 0b111;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        fmt::println("WARNING: misaligned {}-bit read from {:08X}", sizeof(T) * 8, address);
        // TODO: address error (misaligned access)
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
        fmt::println("unhandled {}-bit SH-2 associative purge read from {:08X}", sizeof(T) * 8, address);
        return (address & 1) ? static_cast<T>(0x12231223) : static_cast<T>(0x23122312);
    case 0b011: { // cache address array
        uint32 entry = (address >> 4u) & 0x3F;
        return m_cacheEntries[entry].tag[CCR.Wn]; // TODO: include LRU data
    }
    case 0b100:
    case 0b110: // cache data array

        // TODO: implement
        fmt::println("unhandled {}-bit SH-2 cache data array read from {:08X}", sizeof(T) * 8, address);
        return 0;
    case 0b111: // I/O area
        if ((address & 0xE0004000) == 0xE0004000) {
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
            fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address);
            return 0;
        }
    }

    util::unreachable();
}

template <mem_access_type T>
void SH2::MemWrite(uint32 address, T value) {
    const uint32 partition = address >> 29u;
    if (address & static_cast<uint32>(sizeof(T) - 1)) {
        fmt::println("WARNING: misaligned {}-bit write to {:08X} = {:X}", sizeof(T) * 8, address, value);
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
        fmt::println("unhandled {}-bit SH-2 associative purge write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        break;
    case 0b011: { // cache address array
        uint32 entry = (address >> 4u) & 0x3F;
        m_cacheEntries[entry].tag[CCR.Wn] = address & 0x1FFFFC04;
        // TODO: update LRU data
        break;
    }
    case 0b100:
    case 0b110: // cache data array
        // TODO: implement
        fmt::println("unhandled {}-bit SH-2 cache data array write to {:08X} = {:X}", sizeof(T) * 8, address, value);
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
            case 0xFFFF8426: fmt::println("16-bit CAS latency 1"); break;
            case 0xFFFF8446: fmt::println("16-bit CAS latency 2"); break;
            case 0xFFFF8466: fmt::println("16-bit CAS latency 3"); break;
            case 0xFFFF8848: fmt::println("32-bit CAS latency 1"); break;
            case 0xFFFF8888: fmt::println("32-bit CAS latency 2"); break;
            case 0xFFFF88C8: fmt::println("32-bit CAS latency 3"); break;
            default: fmt::println("unhandled {}-bit SH-2 I/O area read from {:08X}", sizeof(T) * 8, address); break;
            }
        } else {
            // TODO: implement
            fmt::println("unhandled {}-bit SH-2 I/O area write to {:08X} = {:X}", sizeof(T) * 8, address, value);
        }
        break;
    }
}

[[gnu::flatten]] uint8 SH2::MemReadByte(uint32 address) {
    return MemRead<uint8>(address);
}

[[gnu::flatten]] uint16 SH2::MemReadWord(uint32 address) {
    return MemRead<uint16>(address);
}

[[gnu::flatten]] uint32 SH2::MemReadLong(uint32 address) {
    return MemRead<uint32>(address);
}

[[gnu::flatten]] void SH2::MemWriteByte(uint32 address, uint8 value) {
    MemWrite<uint8>(address, value);
}

[[gnu::flatten]] void SH2::MemWriteWord(uint32 address, uint16 value) {
    MemWrite<uint16>(address, value);
}

[[gnu::flatten]] void SH2::MemWriteLong(uint32 address, uint32 value) {
    MemWrite<uint32>(address, value);
}

// -----------------------------------------------------------------------------
// On-chip peripherals

void SH2::WriteCCR(uint8 value) {
    if (CCR.u8 == value) {
        return;
    }

    // fmt::println("CCR changed from 0x{:02X} to 0x{:02X}", CCR.u8, value);
    CCR.u8 = value;
    if (CCR.CP) {
        // fmt::println("  cache purged");
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

        // Store results after 6 cycles - 3 for setting flags and 3 for calculations
        DVDNTH = dividend >> 29;
        if (DVCR.OVFIE) {
            DVDNTL = DVDNT = (dividend << 3) | ((dividend >> 31) & 7);
        } else {
            // DVDNT/DVDNTL is saturated if the interrupt signal is disabled
            DVDNTL = DVDNT = dividend < 0 ? kMinValue : kMaxValue;
        }

        // Signal overflow
        DVCR.OVF = 1;
    }
}

void SH2::DIVUBegin64() {
    // TODO: implement
}

template <mem_access_type T>
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
            // TODO: raise an address error
        }
    }

    // Registers 256-511 do not accept 8-bit accesses
    if constexpr (std::is_same_v<T, uint8>) {
        if (address >= 0x100) {
            // TODO: raise an address error
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
    case 0x60 ... 0x61: return readWordLower(IPRB.val);
    case 0x62 ... 0x63: return readWordLower(VCRA.val);
    case 0x64 ... 0x65: return readWordLower(VCRB.val);
    case 0x66 ... 0x67: return readWordLower(VCRC.val);
    case 0x68 ... 0x69: return readWordLower(VCRD.val);
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

    case 0x1A0: return VCRDMA0;
    case 0x1A8: return VCRDMA1;

    case 0x1E0 ... 0x1E2: return BCR1.u16;
    case 0x1E4 ... 0x1E6: return BCR2.u16;
    case 0x1E8 ... 0x1EA: return WCR.u16;
    case 0x1EC ... 0x1EE: return MCR.u16;
    case 0x1F0 ... 0x1F2: return RTCSR.u16;
    case 0x1F4 ... 0x1F6: return RTCNT;
    case 0x1F8 ... 0x1FA: return RTCOR;

    default: //
        fmt::println("unhandled {}-bit on-chip register read from {:02X}", sizeof(T) * 8, address);
        return 0;
    }
}

template <mem_access_type T>
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

    case 0x1A0: VCRDMA0 = value; break;
    case 0x1A8: VCRDMA1 = value; break;

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
        fmt::println("unhandled {}-bit on-chip register write to {:02X} = {:X}", sizeof(T) * 8, address, value);
        break;
    }
}

// -------------------------------------------------------------------------
// Execution

template <bool delaySlot>
void SH2::Execute(uint32 address) {
    if (m_pendingInterrupt.priority > SR.ILevel) [[unlikely]] {
        EnterException(m_pendingInterrupt.vecNum);
        SR.ILevel = m_pendingInterrupt.priority;
        CheckInterrupts();
    }

    // TODO: emulate fetch - decode - execute - memory access - writeback pipeline
    const uint16 instr = MemReadWord(address);

    ++dbg_count;
    dbg_print("[{:10}] {:08X}{} {:04X}  ", dbg_count, address, delaySlot ? '*' : ' ', instr);

    switch (instr >> 12u) {
    case 0x0:
        switch (instr) {
        case 0x0008: // 0000 0000 0000 1000   CLRT
            CLRT();
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x0009: // 0000 0000 0000 1001   NOP
            NOP();
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x000B: // 0000 0000 0000 1011   RTS
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(6);
            } else {
                RTS();
            }
            break;
        case 0x0018: // 0000 0000 0001 1000   SETT
            SETT();
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x0019: // 0000 0000 0001 1001   DIV0U
            DIV0U();
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x001B: // 0000 0000 0001 1011   SLEEP
            SLEEP();
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x0028: // 0000 0000 0010 1000   CLRMAC
            CLRMAC();
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x002B: // 0000 0000 0010 1011   RTE
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(6);
            } else {
                RTE();
            }
            break;
        default:
            switch (instr & 0xFF) {
            case 0x02: // 0000 nnnn 0000 0010   STC SR, Rn
                STCSR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x03: // 0000 mmmm 0000 0011   BSRF Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(6);
                } else {
                    BSRF(bit::extract<8, 11>(instr));
                }
                break;
            case 0x0A: // 0000 nnnn 0000 1010   STS MACH, Rn
                STSMACH(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x12: // 0000 nnnn 0001 0010   STC GBR, Rn
                STCGBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1A: // 0000 nnnn 0001 1010   STS MACL, Rn
                STSMACL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x22: // 0000 nnnn 0010 0010   STC VBR, Rn
                STCVBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x23: // 0000 mmmm 0010 0011   BRAF Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(6);
                } else {
                    BRAF(bit::extract<8, 11>(instr));
                }
                break;
            case 0x29: // 0000 nnnn 0010 1001   MOVT Rn
                MOVT(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2A: // 0000 nnnn 0010 1010   STS PR, Rn
                STSPR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            default:
                switch (instr & 0xF) {
                case 0x4: // 0000 nnnn mmmm 0100   MOV.B Rm, @(R0,Rn)
                    MOVBS0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x5: // 0000 nnnn mmmm 0101   MOV.W Rm, @(R0,Rn)
                    MOVWS0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x6: // 0000 nnnn mmmm 0110   MOV.L Rm, @(R0,Rn)
                    MOVLS0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0x7: // 0000 nnnn mmmm 0111   MUL.L Rm, Rn
                    MULL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0xC: // 0000 nnnn mmmm 1100   MOV.B @(R0,Rm), Rn
                    MOVBL0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0xD: // 0000 nnnn mmmm 1101   MOV.W @(R0,Rm), Rn
                    MOVWL0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0xE: // 0000 nnnn mmmm 1110   MOV.L @(R0,Rm), Rn
                    MOVLL0(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                case 0xF: // 0000 nnnn mmmm 1111   MAC.L @Rm+, @Rn+
                    MACL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
                    if constexpr (!delaySlot) {
                        PC += 2;
                    }
                    break;
                default: dbg_println("unhandled 0000 instruction"); break;
                }
                break;
            }
            break;
        }
        break;
    case 0x1: // 0001 nnnn mmmm dddd   MOV.L Rm, @(disp,Rn)
        MOVLS4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            PC += 2;
        }
        break;
    case 0x2: {
        const uint16 rm = bit::extract<4, 7>(instr);
        const uint16 rn = bit::extract<8, 11>(instr);
        switch (instr & 0xF) {
        case 0x0: // 0010 nnnn mmmm 0000   MOV.B Rm, @Rn
            MOVBS(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x1: // 0010 nnnn mmmm 0001   MOV.W Rm, @Rn
            MOVWS(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x2: // 0010 nnnn mmmm 0010   MOV.L Rm, @Rn
            MOVLS(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;

            // There's no case 0x3

        case 0x4: // 0010 nnnn mmmm 0100   MOV.B Rm, @-Rn
            MOVBM(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x5: // 0010 nnnn mmmm 0101   MOV.W Rm, @-Rn
            MOVWM(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x6: // 0010 nnnn mmmm 0110   MOV.L Rm, @-Rn
            MOVLM(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x7: // 0010 nnnn mmmm 0110   DIV0S Rm, Rn
            DIV0S(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x8: // 0010 nnnn mmmm 1000   TST Rm, Rn
            TST(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x9: // 0010 nnnn mmmm 1001   AND Rm, Rn
            AND(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xA: // 0010 nnnn mmmm 1010   XOR Rm, Rn
            XOR(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xB: // 0010 nnnn mmmm 1011   OR Rm, Rn
            OR(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xC: // 0010 nnnn mmmm 1100   CMP/STR Rm, Rn
            CMPSTR(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xD: // 0010 nnnn mmmm 1101   XTRCT Rm, Rn
            XTRCT(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xE: // 0010 nnnn mmmm 1110   MULU.W Rm, Rn
            MULU(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xF: // 0010 nnnn mmmm 1111   MULS.W Rm, Rn
            MULS(rm, rn);
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        default: dbg_println("unhandled 0010 instruction"); break;
        }
        break;
    }
    case 0x3:
        switch (instr & 0xF) {
        case 0x0: // 0011 nnnn mmmm 0000   CMP/EQ Rm, Rn
            CMPEQ(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x2: // 0011 nnnn mmmm 0010   CMP/HS Rm, Rn
            CMPHS(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x3: // 0011 nnnn mmmm 0011   CMP/GE Rm, Rn
            CMPGE(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x4: // 0011 nnnn mmmm 0100   DIV1 Rm, Rn
            DIV1(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x5: // 0011 nnnn mmmm 0101   DMULU.L Rm, Rn
            DMULU(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x6: // 0011 nnnn mmmm 0110   CMP/HI Rm, Rn
            CMPHI(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x7: // 0011 nnnn mmmm 0111   CMP/GT Rm, Rn
            CMPGT(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x8: // 0011 nnnn mmmm 1000   SUB Rm, Rn
            SUB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x9: // 0011 nnnn mmmm 1001   SUBC Rm, Rn
            SUBC(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xA: // 0011 nnnn mmmm 1010   SUBV Rm, Rn
            SUBV(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;

            // There's no case 0xB

        case 0xC: // 0011 nnnn mmmm 1100   ADD Rm, Rn
            ADD(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xD: // 0011 nnnn mmmm 1101   DMULS.L Rm, Rn
            DMULS(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xE: // 0011 nnnn mmmm 1110   ADDC Rm, Rn
            ADDC(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xF: // 0011 nnnn mmmm 1110   ADDV Rm, Rn
            ADDV(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        default: dbg_println("unhandled 0011 instruction"); break;
        }
        break;
    case 0x4:
        if ((instr & 0xF) == 0xF) {
            // 0100 nnnn mmmm 1111   MAC.W @Rm+, @Rn+
            MACW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        } else {
            switch (instr & 0xFF) {
            case 0x00: // 0100 nnnn 0000 0000   SHLL Rn
                SHLL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x01: // 0100 nnnn 0000 0001   SHLR Rn
                SHLR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x02: // 0100 nnnn 0000 0010   STS.L MACH, @-Rn
                STSMMACH(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x03: // 0100 nnnn 0000 0010   STC.L SR, @-Rn
                STCMSR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x04: // 0100 nnnn 0000 0100   ROTL Rn
                ROTL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x05: // 0100 nnnn 0000 0101   ROTR Rn
                ROTR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x06: // 0100 mmmm 0000 0110   LDS.L @Rm+, MACH
                LDSMMACH(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x07: // 0100 mmmm 0000 0111   LDC.L @Rm+, SR
                LDCMSR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x08: // 0100 nnnn 0000 1000   SHLL2 Rn
                SHLL2(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x09: // 0100 nnnn 0000 1001   SHLR2 Rn
                SHLR2(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0A: // 0100 mmmm 0000 1010   LDS Rm, MACH
                LDSMACH(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x0B: // 0110 mmmm 0000 1110   JSR @Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(6);
                } else {
                    JSR(bit::extract<8, 11>(instr));
                }
                break;

                // There's no case 0x0C or 0x0D

            case 0x0E: // 0110 mmmm 0000 1110   LDC Rm, SR
                LDCSR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x0F

            case 0x10: // 0100 nnnn 0001 0000   DT Rn
                DT(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x11: // 0100 nnnn 0001 0001   CMP/PZ Rn
                CMPPZ(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x12: // 0100 nnnn 0001 0010   STS.L MACL, @-Rn
                STSMMACL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x13: // 0100 nnnn 0001 0011   STC.L GBR, @-Rn
                STCMGBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x14

            case 0x15: // 0100 nnnn 0001 0101   CMP/PL Rn
                CMPPL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x16: // 0100 mmmm 0001 0110   LDS.L @Rm+, MACL
                LDSMMACL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x17: // 0100 mmmm 0001 0111   LDC.L @Rm+, GBR
                LDCMGBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x18: // 0100 nnnn 0001 1000   SHLL8 Rn
                SHLL8(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x19: // 0100 nnnn 0001 1001   SHLR8 Rn
                SHLR8(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1A: // 0100 mmmm 0001 1010   LDS Rm, MACL
                LDSMACL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x1B: // 0100 nnnn 0001 1011   TAS.B @Rn
                TAS(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x1C or 0x1D

            case 0x1E: // 0110 mmmm 0001 1110   LDC Rm, GBR
                LDCGBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x1F

            case 0x20: // 0100 nnnn 0010 0000   SHAL Rn
                SHAL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x21: // 0100 nnnn 0010 0001   SHAR Rn
                SHAR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x22: // 0100 nnnn 0010 0010   STS.L PR, @-Rn
                STSMPR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x23: // 0100 nnnn 0010 0011   STC.L VBR, @-Rn
                STCMVBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x24: // 0100 nnnn 0010 0100   ROTCL Rn
                ROTCL(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x25: // 0100 nnnn 0010 0101   ROTCR Rn
                ROTCR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x26: // 0100 mmmm 0010 0110   LDS.L @Rm+, PR
                LDSMPR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x27: // 0100 mmmm 0010 0111   LDC.L @Rm+, VBR
                LDCMVBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x28: // 0100 nnnn 0010 1000   SHLL16 Rn
                SHLL16(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x29: // 0100 nnnn 0010 1001   SHLR16 Rn
                SHLR16(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2A: // 0100 mmmm 0010 1010   LDS Rm, PR
                LDSPR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;
            case 0x2B: // 0100 mmmm 0010 1011   JMP @Rm
                if constexpr (delaySlot) {
                    // Illegal slot instruction exception
                    EnterException(6);
                } else {
                    JMP(bit::extract<8, 11>(instr));
                }
                break;

                // There's no case 0x2C or 0x2D

            case 0x2E: // 0110 mmmm 0010 1110   LDC Rm, VBR
                LDCVBR(bit::extract<8, 11>(instr));
                if constexpr (!delaySlot) {
                    PC += 2;
                }
                break;

                // There's no case 0x2F..0xFF

            default: dbg_println("unhandled 0100 instruction"); break;
            }
        }
        break;
    case 0x5: // 0101 nnnn mmmm dddd   MOV.L @(disp,Rm), Rn
        MOVLL4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            PC += 2;
        }
        break;
    case 0x6:
        switch (instr & 0xF) {
        case 0x0: // 0110 nnnn mmmm 0000   MOV.B @Rm, Rn
            MOVBL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x1: // 0110 nnnn mmmm 0001   MOV.W @Rm, Rn
            MOVWL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x2: // 0110 nnnn mmmm 0010   MOV.L @Rm, Rn
            MOVLL(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x3: // 0110 nnnn mmmm 0010   MOV Rm, Rn
            MOV(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x4: // 0110 nnnn mmmm 0110   MOV.B @Rm+, Rn
            MOVBP(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x5: // 0110 nnnn mmmm 0110   MOV.W @Rm+, Rn
            MOVWP(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x6: // 0110 nnnn mmmm 0110   MOV.L @Rm+, Rn
            MOVLP(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x7: // 0110 nnnn mmmm 0111   NOT Rm, Rn
            NOT(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x8: // 0110 nnnn mmmm 1000   SWAP.B Rm, Rn
            SWAPB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x9: // 0110 nnnn mmmm 1001   SWAP.W Rm, Rn
            SWAPW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xA: // 0110 nnnn mmmm 1010   NEGC Rm, Rn
            NEGC(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xB: // 0110 nnnn mmmm 1011   NEG Rm, Rn
            NEG(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xC: // 0110 nnnn mmmm 1100   EXTU.B Rm, Rn
            EXTUB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xD: // 0110 nnnn mmmm 1101   EXTU.W Rm, Rn
            EXTUW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xE: // 0110 nnnn mmmm 1110   EXTS.B Rm, Rn
            EXTSB(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xF: // 0110 nnnn mmmm 1111   EXTS.W Rm, Rn
            EXTSW(bit::extract<4, 7>(instr), bit::extract<8, 11>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        }
        break;
    case 0x7: // 0111 nnnn iiii iiii   ADD #imm, Rn
        ADDI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            PC += 2;
        }
        break;
    case 0x8:
        switch ((instr >> 8u) & 0xF) {
        case 0x0: // 1000 0000 nnnn dddd   MOV.B R0, @(disp,Rn)
            MOVBS4(bit::extract<0, 3>(instr), bit::extract<4, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x1: // 1000 0001 nnnn dddd   MOV.W R0, @(disp,Rn)
            MOVWS4(bit::extract<0, 3>(instr), bit::extract<4, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;

            // There's no case 0x2 or 0x3

        case 0x4: // 1000 0100 mmmm dddd   MOV.B @(disp,Rm), R0
            MOVBL4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x5: // 1000 0101 mmmm dddd   MOV.W @(disp,Rm), R0
            MOVWL4(bit::extract<4, 7>(instr), bit::extract<0, 3>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;

            // There's no case 0x6 or 0x7

        case 0x8: // 1000 1000 iiii iiii   CMP/EQ #imm, R0
            CMPIM(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x9: // 1000 1001 dddd dddd   BT <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(6);
            } else {
                BT(bit::extract<0, 7>(instr));
            }
            break;

            // There's no case 0xA

        case 0xB: // 1000 1011 dddd dddd   BF <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(6);
            } else {
                BF(bit::extract<0, 7>(instr));
            }
            break;

            // There's no case 0xC

        case 0xD: // 1000 1101 dddd dddd   BT/S <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(6);
            } else {
                BTS(bit::extract<0, 7>(instr));
            }
            break;

            // There's no case 0xE

        case 0xF: // 1000 1111 dddd dddd   BF/S <label>
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(6);
            } else {
                BFS(bit::extract<0, 7>(instr));
            }
            break;
        default: dbg_println("unhandled 1000 instruction"); break;
        }
        break;
    case 0x9: // 1001 nnnn dddd dddd   MOV.W @(disp,PC), Rn
        MOVWI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            PC += 2;
        }
        break;
    case 0xA: // 1011 dddd dddd dddd   BRA <label>
        if constexpr (delaySlot) {
            // Illegal slot instruction exception
            EnterException(6);
        } else {
            BRA(bit::extract<0, 11>(instr));
        }
        break;
    case 0xB: // 1011 dddd dddd dddd   BSR <label>
        if constexpr (delaySlot) {
            // Illegal slot instruction exception
            EnterException(6);
        } else {
            BSR(bit::extract<0, 11>(instr));
        }
        break;
    case 0xC:
        switch ((instr >> 8u) & 0xF) {
        case 0x0: // 1100 0000 dddd dddd   MOV.B R0, @(disp,GBR)
            MOVBSG(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x1: // 1100 0001 dddd dddd   MOV.W R0, @(disp,GBR)
            MOVWSG(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x2: // 1100 0010 dddd dddd   MOV.L R0, @(disp,GBR)
            MOVLSG(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x3: // 1100 0011 iiii iiii   TRAPA #imm
            if constexpr (delaySlot) {
                // Illegal slot instruction exception
                EnterException(6);
            } else {
                TRAPA(bit::extract<0, 7>(instr));
            }
            break;
        case 0x4: // 1100 0100 dddd dddd   MOV.B @(disp,GBR), R0
            MOVBLG(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x5: // 1100 0101 dddd dddd   MOV.W @(disp,GBR), R0
            MOVWLG(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x6: // 1100 0110 dddd dddd   MOV.L @(disp,GBR), R0
            MOVLLG(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x7: // 1100 0111 dddd dddd   MOVA @(disp,PC), R0
            MOVA(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x8: // 1100 1000 iiii iiii   TST #imm, R0
            TSTI(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0x9: // 1100 1001 iiii iiii   AND #imm, R0
            ANDI(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xA: // 1100 1010 iiii iiii   XOR #imm, R0
            XORI(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xB: // 1100 1011 iiii iiii   OR #imm, R0
            ORI(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xC: // 1100 1100 iiii iiii   TST.B #imm, @(R0,GBR)
            TSTM(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xD: // 1100 1001 iiii iiii   AND #imm, @(R0,GBR)
            ANDM(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xE: // 1100 1001 iiii iiii   XOR #imm, @(R0,GBR)
            XORM(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        case 0xF: // 1100 1001 iiii iiii   OR #imm, @(R0,GBR)
            ORM(bit::extract<0, 7>(instr));
            if constexpr (!delaySlot) {
                PC += 2;
            }
            break;
        default: dbg_println("unhandled 1100 instruction"); break;
        }
        break;
    case 0xD: // 1101 nnnn dddd dddd   MOV.L @(disp,PC), Rn
        MOVLI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            PC += 2;
        }
        break;
    case 0xE: // 1110 nnnn iiii iiii   MOV #imm, Rn
        MOVI(bit::extract<0, 7>(instr), bit::extract<8, 11>(instr));
        if constexpr (!delaySlot) {
            PC += 2;
        }
        break;

        // There's no case 0xF

    default: dbg_println("unhandled instruction"); break;
    }
}

void SH2::CheckInterrupts() {
    // TODO: check interrupts from these sources (in order of priority, when priority numbers are the same):
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
    // use the vector number of the exception with highest priority
    // TODO: external vector fetch

    // TODO: NMI, user break (before IRLs)

    m_pendingInterrupt.priority = m_pendingIRL;
    m_pendingInterrupt.vecNum = 0x40 + (m_pendingIRL >> 1u);

    auto update = [&](uint8 intrPriority, uint8 vecNum) {
        if (intrPriority > m_pendingInterrupt.priority) {
            m_pendingInterrupt.priority = intrPriority;
            m_pendingInterrupt.vecNum = vecNum;
        }
    };

    if (DVCR.OVF && DVCR.OVFIE) {
        update(IPRA.DIVUIPn, VCRDIV);
    }

    // TODO: DMAC0, DMAC1 transfer end
    // TODO: WDT ITI, BSC REF CMI
    // TODO: SCI ERI, RXI, TXI, TEI
    // TODO: FRT ICI, OCI, OVI

    // if level > SR.ILevel, set interrupt exception flag
    // otherwise clear interrupt exception flag
}

void SH2::EnterException(uint8 vectorNumber) {
    R[15] -= 4;
    MemWriteLong(R[15], SR.u32);
    R[15] -= 4;
    MemWriteLong(R[15], PC - 2);
    PC = MemReadLong(VBR + (static_cast<uint32>(vectorNumber) << 2u)) + 4;
}

void SH2::NOP() {
    dbg_println("nop");
}

void SH2::SLEEP() {
    dbg_println("sleep");
    PC -= 2;
    // TODO: wait for exception
    __debugbreak();
}

void SH2::MOV(uint16 rm, uint16 rn) {
    dbg_println("mov r{}, r{}", rm, rn);
    R[rn] = R[rm];
}

void SH2::MOVBL(uint16 rm, uint16 rn) {
    dbg_println("mov.b @r{}, r{}", rm, rn);
    R[rn] = bit::sign_extend<8>(MemReadByte(R[rm]));
}

void SH2::MOVWL(uint16 rm, uint16 rn) {
    dbg_println("mov.w @r{}, r{}", rm, rn);
    R[rn] = bit::sign_extend<16>(MemReadWord(R[rm]));
}

void SH2::MOVLL(uint16 rm, uint16 rn) {
    dbg_println("mov.l @r{}, r{}", rm, rn);
    R[rn] = MemReadLong(R[rm]);
}

void SH2::MOVBL0(uint16 rm, uint16 rn) {
    dbg_println("mov.b @(r0,r{}), r{}", rm, rn);
    R[rn] = bit::sign_extend<8>(MemReadByte(R[rm] + R[0]));
}

void SH2::MOVWL0(uint16 rm, uint16 rn) {
    dbg_println("mov.w @(r0,r{}), r{})", rm, rn);
    R[rn] = bit::sign_extend<16>(MemReadWord(R[rm] + R[0]));
}

void SH2::MOVLL0(uint16 rm, uint16 rn) {
    dbg_println("mov.l @(r0,r{}), r{})", rm, rn);
    R[rn] = MemReadLong(R[rm] + R[0]);
}

void SH2::MOVBL4(uint16 rm, uint16 disp) {
    dbg_println("mov.b @(0x{:X},r{}), r0", disp, rm);
    R[0] = bit::sign_extend<8>(MemReadByte(R[rm] + disp));
}

void SH2::MOVWL4(uint16 rm, uint16 disp) {
    disp <<= 1u;
    dbg_println("mov.w @(0x{:X},r{}), r0", disp, rm);
    R[0] = bit::sign_extend<16>(MemReadWord(R[rm] + disp));
}

void SH2::MOVLL4(uint16 rm, uint16 disp, uint16 rn) {
    disp <<= 2u;
    dbg_println("mov.l @(0x{:X},r{}), r{}", disp, rm, rn);
    R[rn] = MemReadLong(R[rm] + disp);
}

void SH2::MOVBLG(uint16 disp) {
    dbg_println("mov.b @(0x{:X},gbr), r0", disp);
    R[0] = bit::sign_extend<8>(MemReadByte(GBR + disp));
}

void SH2::MOVWLG(uint16 disp) {
    disp <<= 1u;
    dbg_println("mov.w @(0x{:X},gbr), r0", disp);
    R[0] = bit::sign_extend<16>(MemReadWord(GBR + disp));
}

void SH2::MOVLLG(uint16 disp) {
    disp <<= 2u;
    dbg_println("mov.l @(0x{:X},gbr), r0", disp);
    R[0] = MemReadLong(GBR + disp);
}

void SH2::MOVBM(uint16 rm, uint16 rn) {
    dbg_println("mov.b r{}, @-r{}", rm, rn);
    MemWriteByte(R[rn] - 1, R[rm]);
    R[rn] -= 1;
}

void SH2::MOVWM(uint16 rm, uint16 rn) {
    dbg_println("mov.w r{}, @-r{}", rm, rn);
    MemWriteWord(R[rn] - 2, R[rm]);
    R[rn] -= 2;
}

void SH2::MOVLM(uint16 rm, uint16 rn) {
    dbg_println("mov.l r{}, @-r{}", rm, rn);
    MemWriteLong(R[rn] - 4, R[rm]);
    R[rn] -= 4;
}

void SH2::MOVBP(uint16 rm, uint16 rn) {
    dbg_println("mov.b @r{}+, r{}", rm, rn);
    R[rn] = bit::sign_extend<8>(MemReadByte(R[rm]));
    if (rn != rm) {
        R[rm] += 1;
    }
}

void SH2::MOVWP(uint16 rm, uint16 rn) {
    dbg_println("mov.w @r{}+, r{}", rm, rn);
    R[rn] = bit::sign_extend<16>(MemReadWord(R[rm]));
    if (rn != rm) {
        R[rm] += 2;
    }
}

void SH2::MOVLP(uint16 rm, uint16 rn) {
    dbg_println("mov.l @r{}+, r{}", rm, rn);
    R[rn] = MemReadLong(R[rm]);
    if (rn != rm) {
        R[rm] += 4;
    }
}

void SH2::MOVBS(uint16 rm, uint16 rn) {
    dbg_println("mov.b r{}, @r{}", rm, rn);
    MemWriteByte(R[rn], R[rm]);
}

void SH2::MOVWS(uint16 rm, uint16 rn) {
    dbg_println("mov.w r{}, @r{}", rm, rn);
    MemWriteWord(R[rn], R[rm]);
}

void SH2::MOVLS(uint16 rm, uint16 rn) {
    dbg_println("mov.l r{}, @r{}", rm, rn);
    MemWriteLong(R[rn], R[rm]);
}

void SH2::MOVBS0(uint16 rm, uint16 rn) {
    dbg_println("mov.b r{}, @(r0,r{})", rm, rn);
    MemWriteByte(R[rn] + R[0], R[rm]);
}

void SH2::MOVWS0(uint16 rm, uint16 rn) {
    dbg_println("mov.w r{}, @(r0,r{})", rm, rn);
    MemWriteWord(R[rn] + R[0], R[rm]);
}

void SH2::MOVLS0(uint16 rm, uint16 rn) {
    dbg_println("mov.l r{}, @(r0,r{})", rm, rn);
    MemWriteLong(R[rn] + R[0], R[rm]);
}

void SH2::MOVBS4(uint16 disp, uint16 rn) {
    dbg_println("mov.b r0, @(0x{:X},r{})", disp, rn);
    MemWriteByte(R[rn] + disp, R[0]);
}

void SH2::MOVWS4(uint16 disp, uint16 rn) {
    disp <<= 1u;
    dbg_println("mov.w r0, @(0x{:X},r{})", disp, rn);
    MemWriteWord(R[rn] + disp, R[0]);
}

void SH2::MOVLS4(uint16 rm, uint16 disp, uint16 rn) {
    disp <<= 2u;
    dbg_println("mov.l r{}, @(0x{:X},r{})", rm, disp, rn);
    MemWriteLong(R[rn] + disp, R[rm]);
}

void SH2::MOVBSG(uint16 disp) {
    dbg_println("mov.b r0, @(0x{:X},gbr)", disp);
    MemWriteByte(GBR + disp, R[0]);
}

void SH2::MOVWSG(uint16 disp) {
    disp <<= 1u;
    dbg_println("mov.w r0, @(0x{:X},gbr)", disp);
    MemWriteWord(GBR + disp, R[0]);
}

void SH2::MOVLSG(uint16 disp) {
    disp <<= 2u;
    dbg_println("mov.l r0, @(0x{:X},gbr)", disp);
    MemWriteLong(GBR + disp, R[0]);
}

void SH2::MOVI(uint16 imm, uint16 rn) {
    sint32 simm = bit::sign_extend<8>(imm);
    dbg_println("mov #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
    R[rn] = simm;
}

void SH2::MOVWI(uint16 disp, uint16 rn) {
    disp <<= 1u;
    dbg_println("mov.w @(0x{:08X},pc), r{}", PC + 4 + disp, rn);
    R[rn] = bit::sign_extend<16>(MemReadWord(PC + 4 + disp));
}

void SH2::MOVLI(uint16 disp, uint16 rn) {
    disp <<= 2u;
    dbg_println("mov.l @(0x{:08X},pc), r{}", ((PC + 4) & ~3) + disp, rn);
    R[rn] = MemReadLong(((PC + 4) & ~3u) + disp);
}

void SH2::MOVA(uint16 disp) {
    disp = (disp << 2u) + 4;
    dbg_println("mova @(0x{:X},pc), r0", (PC & ~3) + disp);
    R[0] = (PC & ~3) + disp;
}

void SH2::MOVT(uint16 rn) {
    dbg_println("movt r{}", rn);
    R[rn] = SR.T;
}

void SH2::CLRT() {
    dbg_println("clrt");
    SR.T = 0;
}

void SH2::SETT() {
    dbg_println("sett");
    SR.T = 1;
}

void SH2::EXTSB(uint16 rm, uint16 rn) {
    dbg_println("exts.b r{}, r{}", rm, rn);
    R[rn] = bit::sign_extend<8>(R[rm]);
}

void SH2::EXTSW(uint16 rm, uint16 rn) {
    dbg_println("exts.w r{}, r{}", rm, rn);
    R[rn] = bit::sign_extend<16>(R[rm]);
}

void SH2::EXTUB(uint16 rm, uint16 rn) {
    dbg_println("extu.b r{}, r{}", rm, rn);
    R[rn] = R[rm] & 0xFF;
}

void SH2::EXTUW(uint16 rm, uint16 rn) {
    dbg_println("extu.w r{}, r{}", rm, rn);
    R[rn] = R[rm] & 0xFFFF;
}

void SH2::LDCGBR(uint16 rm) {
    dbg_println("ldc r{}, gbr", rm);
    GBR = R[rm];
}

void SH2::LDCSR(uint16 rm) {
    dbg_println("ldc r{}, sr", rm);
    SR.u32 = R[rm] & 0x000003F3;
}

void SH2::LDCVBR(uint16 rm) {
    dbg_println("ldc r{}, vbr", rm);
    VBR = R[rm];
}

void SH2::LDCMGBR(uint16 rm) {
    dbg_println("ldc.l @r{}+, gbr", rm);
    GBR = MemReadLong(R[rm]);
    R[rm] += 4;
}

void SH2::LDCMSR(uint16 rm) {
    dbg_println("ldc.l @r{}+, sr", rm);
    SR.u32 = MemReadLong(R[rm]) & 0x000003F3;
    R[rm] += 4;
}

void SH2::LDCMVBR(uint16 rm) {
    dbg_println("ldc.l @r{}+, vbr", rm);
    VBR = MemReadLong(R[rm]);
    R[rm] += 4;
}

void SH2::LDSMACH(uint16 rm) {
    dbg_println("lds r{}, mach", rm);
    MAC.H = R[rm];
}

void SH2::LDSMACL(uint16 rm) {
    dbg_println("lds r{}, macl", rm);
    MAC.L = R[rm];
}

void SH2::LDSPR(uint16 rm) {
    dbg_println("lds r{}, pr", rm);
    PR = R[rm];
}

void SH2::LDSMMACH(uint16 rm) {
    dbg_println("lds.l @r{}+, mach", rm);
    MAC.H = MemReadLong(R[rm]);
    R[rm] += 4;
}

void SH2::LDSMMACL(uint16 rm) {
    dbg_println("lds.l @r{}+, macl", rm);
    MAC.L = MemReadLong(R[rm]);
    R[rm] += 4;
}

void SH2::LDSMPR(uint16 rm) {
    dbg_println("lds.l @r{}+, pr", rm);
    PR = MemReadLong(R[rm]);
    R[rm] += 4;
}

void SH2::STCGBR(uint16 rn) {
    dbg_println("stc gbr, r{}", rn);
    R[rn] = GBR;
}

void SH2::STCSR(uint16 rn) {
    dbg_println("stc sr, r{}", rn);
    R[rn] = SR.u32;
}

void SH2::STCVBR(uint16 rn) {
    dbg_println("stc vbr, r{}", rn);
    R[rn] = VBR;
}

void SH2::STCMGBR(uint16 rn) {
    dbg_println("stc.l gbr, @-r{}", rn);
    R[rn] -= 4;
    MemWriteLong(R[rn], GBR);
}

void SH2::STCMSR(uint16 rn) {
    dbg_println("stc.l sr, @-r{}", rn);
    R[rn] -= 4;
    MemWriteLong(R[rn], SR.u32);
}

void SH2::STCMVBR(uint16 rn) {
    dbg_println("stc.l vbr, @-r{}", rn);
    R[rn] -= 4;
    MemWriteLong(R[rn], VBR);
}

void SH2::STSMACH(uint16 rn) {
    dbg_println("sts mach, r{}", rn);
    R[rn] = MAC.H;
}

void SH2::STSMACL(uint16 rn) {
    dbg_println("sts macl, r{}", rn);
    R[rn] = MAC.L;
}

void SH2::STSPR(uint16 rn) {
    dbg_println("sts pr, r{}", rn);
    R[rn] = PR;
}

void SH2::STSMMACH(uint16 rn) {
    dbg_println("sts.l mach, @-r{}", rn);
    R[rn] -= 4;
    MemWriteLong(R[rn], MAC.H);
}

void SH2::STSMMACL(uint16 rn) {
    dbg_println("sts.l macl, @-r{}", rn);
    R[rn] -= 4;
    MemWriteLong(R[rn], MAC.L);
}

void SH2::STSMPR(uint16 rn) {
    dbg_println("sts.l pr, @-r{}", rn);
    R[rn] -= 4;
    MemWriteLong(R[rn], PR);
}

void SH2::SWAPB(uint16 rm, uint16 rn) {
    dbg_println("swap.b r{}, r{}", rm, rn);

    const uint32 tmp0 = R[rm] & 0xFFFF0000;
    const uint32 tmp1 = (R[rm] & 0xFF) << 8u;
    R[rn] = ((R[rm] >> 8u) & 0xFF) | tmp1 | tmp0;
}

void SH2::SWAPW(uint16 rm, uint16 rn) {
    dbg_println("swap.w r{}, r{}", rm, rn);

    const uint32 tmp = R[rm] >> 16u;
    R[rn] = (R[rm] << 16u) | tmp;
}

void SH2::XTRCT(uint16 rm, uint16 rn) {
    dbg_println("xtrct r{}, r{}", rm, rn);
    R[rn] = (R[rn] >> 16u) | (R[rm] << 16u);
}

void SH2::ADD(uint16 rm, uint16 rn) {
    dbg_println("add r{}, r{}", rm, rn);
    R[rn] += R[rm];
}

void SH2::ADDI(uint16 imm, uint16 rn) {
    const sint32 simm = bit::sign_extend<8>(imm);
    dbg_println("add #{}0x{:X}, r{}", (simm < 0 ? "-" : ""), abs(simm), rn);
    R[rn] += simm;
}

void SH2::ADDC(uint16 rm, uint16 rn) {
    dbg_println("addc r{}, r{}", rm, rn);
    const uint32 tmp1 = R[rn] + R[rm];
    const uint32 tmp0 = R[rn];
    R[rn] = tmp1 + SR.T;
    SR.T = (tmp0 > tmp1) || (tmp1 > R[rn]);
}

void SH2::ADDV(uint16 rm, uint16 rn) {
    dbg_println("addv r{}, r{}", rm, rn);

    const bool dst = static_cast<sint32>(R[rn]) < 0;
    const bool src = static_cast<sint32>(R[rm]) < 0;

    R[rn] += R[rm];

    bool ans = static_cast<sint32>(R[rn]) < 0;
    ans ^= dst;
    SR.T = (src == dst) & ans;
}

void SH2::AND(uint16 rm, uint16 rn) {
    dbg_println("and r{}, r{}", rm, rn);
    R[rn] &= R[rm];
}

void SH2::ANDI(uint16 imm) {
    dbg_println("and #0x{:X}, r0", imm);
    R[0] &= imm;
}

void SH2::ANDM(uint16 imm) {
    dbg_println("and.b #0x{:X}, @(r0,gbr)", imm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp &= imm;
    MemWriteByte(GBR + R[0], tmp);
}

void SH2::NEG(uint16 rm, uint16 rn) {
    dbg_println("neg r{}, r{}", rm, rn);
    R[rn] = -R[rm];
}

void SH2::NEGC(uint16 rm, uint16 rn) {
    dbg_println("negc r{}, r{}", rm, rn);
    const uint32 tmp = -R[rm];
    R[rn] = tmp - SR.T;
    SR.T = (0 < tmp) || (tmp < R[rn]);
}

void SH2::NOT(uint16 rm, uint16 rn) {
    dbg_println("not r{}, r{}", rm, rn);
    R[rn] = ~R[rm];
}

void SH2::OR(uint16 rm, uint16 rn) {
    dbg_println("or r{}, r{}", rm, rn);
    R[rn] |= R[rm];
}

void SH2::ORI(uint16 imm) {
    dbg_println("or #0x{:X}, r0", imm);
    R[0] |= imm;
}

void SH2::ORM(uint16 imm) {
    dbg_println("or.b #0x{:X}, @(r0,gbr)", imm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp |= imm;
    MemWriteByte(GBR + R[0], tmp);
}

void SH2::ROTCL(uint16 rn) {
    dbg_println("rotcl r{}", rn);
    const bool tmp = R[rn] >> 31u;
    R[rn] = (R[rn] << 1u) | SR.T;
    SR.T = tmp;
}

void SH2::ROTCR(uint16 rn) {
    dbg_println("rotcr r{}", rn);
    const bool tmp = R[rn] & 1u;
    R[rn] = (R[rn] >> 1u) | (SR.T << 31u);
    SR.T = tmp;
}

void SH2::ROTL(uint16 rn) {
    dbg_println("rotl r{}", rn);
    SR.T = R[rn] >> 31u;
    R[rn] = (R[rn] << 1u) | SR.T;
}

void SH2::ROTR(uint16 rn) {
    dbg_println("rotr r{}", rn);
    SR.T = R[rn] & 1u;
    R[rn] = (R[rn] >> 1u) | (SR.T << 31u);
}

void SH2::SHAL(uint16 rn) {
    dbg_println("shal r{}", rn);
    SR.T = R[rn] >> 31u;
    R[rn] <<= 1u;
}

void SH2::SHAR(uint16 rn) {
    dbg_println("shar r{}", rn);
    SR.T = R[rn] & 1u;
    R[rn] = static_cast<sint32>(R[rn]) >> 1;
}

void SH2::SHLL(uint16 rn) {
    dbg_println("shll r{}", rn);
    SR.T = R[rn] >> 31u;
    R[rn] <<= 1u;
}

void SH2::SHLL2(uint16 rn) {
    dbg_println("shll2 r{}", rn);
    R[rn] <<= 2u;
}

void SH2::SHLL8(uint16 rn) {
    dbg_println("shll8 r{}", rn);
    R[rn] <<= 8u;
}

void SH2::SHLL16(uint16 rn) {
    dbg_println("shll16 r{}", rn);
    R[rn] <<= 16u;
}

void SH2::SHLR(uint16 rn) {
    dbg_println("shlr r{}", rn);
    SR.T = R[rn] & 1u;
    R[rn] >>= 1u;
}

void SH2::SHLR2(uint16 rn) {
    dbg_println("shlr2 r{}", rn);
    R[rn] >>= 2u;
}

void SH2::SHLR8(uint16 rn) {
    dbg_println("shlr8 r{}", rn);
    R[rn] >>= 8u;
}

void SH2::SHLR16(uint16 rn) {
    dbg_println("shlr16 r{}", rn);
    R[rn] >>= 16u;
}

void SH2::SUB(uint16 rm, uint16 rn) {
    dbg_println("sub r{}, r{}", rm, rn);
    R[rn] -= R[rm];
}

void SH2::SUBC(uint16 rm, uint16 rn) {
    dbg_println("subc r{}, r{}", rm, rn);
    const uint32 tmp1 = R[rn] - R[rm];
    const uint32 tmp0 = R[rn];
    R[rn] = tmp1 - SR.T;
    SR.T = (tmp0 < tmp1) || (tmp1 < R[rn]);
}

void SH2::SUBV(uint16 rm, uint16 rn) {
    dbg_println("subv r{}, r{}", rm, rn);

    const bool dst = static_cast<sint32>(R[rn]) < 0;
    const bool src = static_cast<sint32>(R[rm]) < 0;

    R[rn] -= R[rm];

    bool ans = static_cast<sint32>(R[rn]) < 0;
    ans ^= dst;
    SR.T = (src != dst) & ans;
}

void SH2::XOR(uint16 rm, uint16 rn) {
    dbg_println("xor r{}, r{}", rm, rn);
    R[rn] ^= R[rm];
}

void SH2::XORI(uint16 imm) {
    dbg_println("xor #0x{:X}, r0", imm);
    R[0] ^= imm;
}

void SH2::XORM(uint16 imm) {
    dbg_println("xor.b #0x{:X}, @(r0,gbr)", imm);
    uint8 tmp = MemReadByte(GBR + R[0]);
    tmp ^= imm;
    MemWriteByte(GBR + R[0], tmp);
}

void SH2::DT(uint16 rn) {
    dbg_println("dt r{}", rn);
    R[rn]--;
    SR.T = R[rn] == 0;
}

void SH2::CLRMAC() {
    dbg_println("clrmac");
    MAC.u64 = 0;
}

void SH2::MACW(uint16 rm, uint16 rn) {
    dbg_println("mac.w @r{}+, @r{}+)", rm, rn);

    const sint32 op2 = bit::sign_extend<16, sint32>(MemReadWord(R[rn]));
    R[rn] += 2;
    const sint32 op1 = bit::sign_extend<16, sint32>(MemReadWord(R[rm]));
    R[rm] += 2;

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

void SH2::MACL(uint16 rm, uint16 rn) {
    dbg_println("mac.l @r{}+, @r{}+)", rm, rn);

    const sint64 op2 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[rn])));
    R[rn] += 4;
    const sint64 op1 = static_cast<sint64>(static_cast<sint32>(MemReadLong(R[rm])));
    R[rm] += 4;

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

void SH2::MULL(uint16 rm, uint16 rn) {
    dbg_println("mul.l r{}, r{}", rm, rn);
    MAC.L = R[rm] * R[rn];
}

void SH2::MULS(uint16 rm, uint16 rn) {
    dbg_println("muls.w r{}, r{}", rm, rn);
    MAC.L = bit::sign_extend<16>(R[rm]) * bit::sign_extend<16>(R[rn]);
}

void SH2::MULU(uint16 rm, uint16 rn) {
    dbg_println("mulu.w r{}, r{}", rm, rn);
    auto cast = [](uint32 val) { return static_cast<uint32>(static_cast<uint16>(val)); };
    MAC.L = cast(R[rm]) * cast(R[rn]);
}

void SH2::DMULS(uint16 rm, uint16 rn) {
    dbg_println("dmuls.l r{}, r{}", rm, rn);
    auto cast = [](uint32 val) { return static_cast<sint64>(static_cast<sint32>(val)); };
    MAC.u64 = cast(R[rm]) * cast(R[rn]);
}

void SH2::DMULU(uint16 rm, uint16 rn) {
    dbg_println("dmulu.l r{}, r{}", rm, rn);
    MAC.u64 = static_cast<uint64>(R[rm]) * static_cast<uint64>(R[rn]);
}

void SH2::DIV0S(uint16 rm, uint16 rn) {
    dbg_println("div0s r{}, r{}", rm, rn);
    SR.M = static_cast<sint32>(R[rm]) < 0;
    SR.Q = static_cast<sint32>(R[rn]) < 0;
    SR.T = SR.M != SR.Q;
}

void SH2::DIV0U() {
    dbg_println("div0u");
    SR.M = 0;
    SR.Q = 0;
    SR.T = 0;
}

void SH2::DIV1(uint16 rm, uint16 rn) {
    dbg_println("div1 r{}, r{}", rm, rn);

    const bool oldQ = SR.Q;
    SR.Q = static_cast<sint32>(R[rn]) < 0;
    R[rn] = (R[rn] << 1u) | SR.T;

    const uint32 prevVal = R[rn];
    if (oldQ == SR.M) {
        R[rn] -= R[rm];
    } else {
        R[rn] += R[rm];
    }

    if (oldQ) {
        if (SR.M) {
            SR.Q ^= R[rn] <= prevVal;
        } else {
            SR.Q ^= R[rn] < prevVal;
        }
    } else {
        if (SR.M) {
            SR.Q ^= R[rn] >= prevVal;
        } else {
            SR.Q ^= R[rn] > prevVal;
        }
    }

    SR.T = SR.Q == SR.M;
}

void SH2::CMPIM(uint16 imm) {
    const sint32 simm = bit::sign_extend<8>(imm);
    dbg_println("cmp/eq #{}0x{:X}, r0", (simm < 0 ? "-" : ""), abs(simm));
    SR.T = R[0] == simm;
}

void SH2::CMPEQ(uint16 rm, uint16 rn) {
    dbg_println("cmp/eq r{}, r{}", rm, rn);
    SR.T = R[rn] == R[rm];
}

void SH2::CMPGE(uint16 rm, uint16 rn) {
    dbg_println("cmp/ge r{}, r{}", rm, rn);
    SR.T = static_cast<sint32>(R[rn]) >= static_cast<sint32>(R[rm]);
}

void SH2::CMPGT(uint16 rm, uint16 rn) {
    dbg_println("cmp/gt r{}, r{}", rm, rn);
    SR.T = static_cast<sint32>(R[rn]) > static_cast<sint32>(R[rm]);
}

void SH2::CMPHI(uint16 rm, uint16 rn) {
    dbg_println("cmp/hi r{}, r{}", rm, rn);
    SR.T = R[rn] > R[rm];
}

void SH2::CMPHS(uint16 rm, uint16 rn) {
    dbg_println("cmp/hs r{}, r{}", rm, rn);
    SR.T = R[rn] >= R[rm];
}

void SH2::CMPPL(uint16 rn) {
    dbg_println("cmp/pl r{}", rn);
    SR.T = R[rn] > 0;
}

void SH2::CMPPZ(uint16 rn) {
    dbg_println("cmp/pz r{}", rn);
    SR.T = R[rn] >= 0;
}

void SH2::CMPSTR(uint16 rm, uint16 rn) {
    dbg_println("cmp/str r{}, r{}", rm, rn);
    const uint32 tmp = R[rm] & R[rn];
    const uint8 hh = tmp >> 24u;
    const uint8 hl = tmp >> 16u;
    const uint8 lh = tmp >> 8u;
    const uint8 ll = tmp >> 0u;
    SR.T = !(hh && hl && lh && ll);
}

void SH2::TAS(uint16 rn) {
    dbg_println("tas.b @r{}", rn);
    dbg_println("WARNING: bus lock not implemented!");

    // TODO: enable bus lock on this read
    const uint8 tmp = MemReadByte(R[rn]);
    SR.T = tmp == 0;
    // TODO: disable bus lock on this write
    MemWriteByte(R[rn], tmp | 0x80);
}

void SH2::TST(uint16 rm, uint16 rn) {
    dbg_println("tst r{}, r{}", rm, rn);
    SR.T = (R[rn] & R[rm]) == 0;
}

void SH2::TSTI(uint16 imm) {
    dbg_println("tst #0x{:X}, r0", imm);
    SR.T = (R[0] & imm) == 0;
}

void SH2::TSTM(uint16 imm) {
    dbg_println("tst.b #0x{:X}, @(r0,gbr)", imm);
    const uint8 tmp = MemReadByte(GBR + R[0]);
    SR.T = (tmp & imm) == 0;
}

void SH2::BF(uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    dbg_println("bf 0x{:08X}", PC + sdisp);

    if (!SR.T) {
        PC += sdisp;
    } else {
        PC += 2;
    }
}

void SH2::BFS(uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    dbg_println("bf/s 0x{:08X}", PC + sdisp);

    if (!SR.T) {
        const uint32 delaySlot = PC + 2;
        PC += sdisp;
        Execute<true>(delaySlot);
    } else {
        PC += 2;
    }
}

void SH2::BT(uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    dbg_println("bt 0x{:08X}", PC + sdisp);

    if (SR.T) {
        PC += sdisp;
    } else {
        PC += 2;
    }
}

void SH2::BTS(uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<8>(disp) << 1) + 4;
    dbg_println("bt/s 0x{:08X}", PC + sdisp);

    if (SR.T) {
        const uint32 delaySlot = PC + 2;
        PC += sdisp;
        Execute<true>(delaySlot);
    } else {
        PC += 2;
    }
}

void SH2::BRA(uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<12>(disp) << 1) + 4;
    dbg_println("bra 0x{:08X}", PC + sdisp);
    const uint32 delaySlot = PC + 2;
    PC += sdisp;
    Execute<true>(delaySlot);
}

void SH2::BRAF(uint16 rm) {
    dbg_println("braf r{}", rm);
    const uint32 delaySlot = PC + 2;
    PC += R[rm] + 4;
    Execute<true>(delaySlot);
}

void SH2::BSR(uint16 disp) {
    const sint32 sdisp = (bit::sign_extend<12>(disp) << 1) + 4;
    dbg_println("bsr 0x{:08X}", PC + sdisp);

    PR = PC;
    PC += sdisp;
    Execute<true>(PR + 2);
}

void SH2::BSRF(uint16 rm) {
    dbg_println("bsrf r{}", rm);
    PR = PC;
    PC += R[rm] + 4;
    Execute<true>(PR + 2);
}

void SH2::JMP(uint16 rm) {
    dbg_println("jmp @r{}", rm);
    const uint32 delaySlot = PC + 2;
    PC = R[rm];
    Execute<true>(delaySlot);
}

void SH2::JSR(uint16 rm) {
    dbg_println("jsr @r{}", rm);
    PR = PC;
    PC = R[rm];
    Execute<true>(PR + 2);
}

void SH2::TRAPA(uint16 imm) {
    dbg_println("trapa #0x{:X}", imm);
    EnterException(imm);
}

void SH2::RTE() {
    dbg_println("rte");
    const uint32 delaySlot = PC + 2;
    PC = MemReadLong(R[15] + 4);
    R[15] += 4;
    SR.u32 = MemReadLong(R[15]) & 0x000003F3;
    R[15] += 4;
    Execute<true>(delaySlot);
}

void SH2::RTS() {
    dbg_println("rts");
    const uint32 delaySlot = PC + 2;
    PC = PR + 4;
    Execute<true>(delaySlot);
}

} // namespace satemu

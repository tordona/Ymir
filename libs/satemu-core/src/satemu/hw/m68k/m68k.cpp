#include <satemu/hw/m68k/m68k.hpp>

#include <satemu/hw/scsp/scsp.hpp> // because M68kBus *is* SCSP

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/unreachable.hpp>

namespace satemu::m68k {

MC68EC000::MC68EC000(M68kBus &bus)
    : m_bus(bus) {
    Reset(true);
}

void MC68EC000::Reset(bool hard) {
    // TODO: check reset values

    D.fill(0);
    A.fill(0);
    A[7] = MemReadLong(0x00000000);
    SP_swap = 0;

    PC = MemReadLong(0x00000004);

    SR.u16 = 0;
}

void MC68EC000::Step() {
    Execute();
}

template <mem_access_type T, bool instrFetch>
T MC68EC000::MemRead(uint32 address) {
    return m_bus.Read<T, instrFetch>(address);
}

template <mem_access_type T>
void MC68EC000::MemWrite(uint32 address, T value) {
    m_bus.Write<T>(address, value);
}

FLATTEN FORCE_INLINE uint16 MC68EC000::FetchInstruction() {
    uint16 instr = MemRead<uint16, true>(PC);
    PC += 2;
    return instr;
}

FLATTEN FORCE_INLINE uint8 MC68EC000::MemReadByte(uint32 address) {
    return MemRead<uint8, false>(address);
}

FLATTEN FORCE_INLINE uint16 MC68EC000::MemReadWord(uint32 address) {
    return MemRead<uint16, false>(address);
}

FLATTEN FORCE_INLINE uint32 MC68EC000::MemReadLong(uint32 address) {
    return MemRead<uint32, false>(address);
}

FLATTEN FORCE_INLINE void MC68EC000::MemWriteByte(uint32 address, uint8 value) {
    MemWrite<uint8>(address, value);
}

FLATTEN FORCE_INLINE void MC68EC000::MemWriteWord(uint32 address, uint16 value) {
    MemWrite<uint16>(address, value);
}

FLATTEN FORCE_INLINE void MC68EC000::MemWriteLong(uint32 address, uint32 value) {
    MemWrite<uint32>(address, value);
}

// M   Xn
// 000 <reg>  D<reg>               Data register
// 001 <reg>  A<reg>               Address register
// 010 <reg>  (A<reg>)             Address
// 011 <reg>  (A<reg>)+            Address with postincrement
// 100 <reg>  -(A<reg>)            Address with predecrement
// 101 <reg>  disp(A<reg>)         Address with displacement
// 110 <reg>  disp(A<reg>, <ix>)   Address with index
// 111 010    disp(PC)             Program counter with displacement
// 111 011    disp(PC, <ix>)       Program counter with index
// 111 000    (xxx).w              Absolute short
// 111 001    (xxx).l              Absolute long
// 111 100    #imm                 Immediate

template <mem_access_type T>
T MC68EC000::ReadEffectiveAddress(uint8 M, uint8 Xn) {
    switch (M) {
    case 0b000: return D[Xn];
    case 0b001: return A[Xn];
    case 0b010: return MemRead<T, false>(A[Xn]);
    case 0b011: {
        const T value = MemRead<T, false>(A[Xn]);
        A[Xn] += sizeof(T);
        return value;
    }
    case 0b100: A[Xn] -= sizeof(T); return MemRead<T, false>(A[Xn]);
    case 0b101: {
        const sint16 disp = static_cast<sint16>(FetchInstruction());
        return MemRead<T, false>(A[Xn] + disp);
    }
    case 0b110: {
        const uint16 briefExtWord = FetchInstruction();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        uint32 index = m ? A[extXn] : D[extXn];
        if (!s) {
            // Word index
            index &= 0xFFFF;
        }
        return MemRead<T, false>(A[Xn] + disp + index);
    }
    case 0b111:
        switch (Xn) {
        case 0b010: return MemRead<T, false>(PC + static_cast<sint16>(FetchInstruction()));
        case 0b011: {
            const uint16 briefExtWord = FetchInstruction();

            const uint32 address = PC + static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
            const bool s = bit::extract<11>(briefExtWord);
            const uint8 extXn = bit::extract<12, 14>(briefExtWord);
            const bool m = bit::extract<15>(briefExtWord);

            uint32 index = m ? A[extXn] : D[extXn];
            if (!s) {
                // Word index
                index &= 0xFFFF;
            }
            return MemRead<T, false>(address + index);
        }
        case 0b000: {
            const uint16 address = FetchInstruction();
            return MemRead<T, false>(address);
        }
        case 0b001: {
            const uint32 addressHigh = FetchInstruction();
            const uint32 addressLow = FetchInstruction();
            return MemRead<T, false>((addressHigh << 16u) | addressLow);
        }
        case 0b100: {
            uint32 value = FetchInstruction();
            if constexpr (std::is_same_v<T, uint32>) {
                value = (value << 16u) | FetchInstruction();
            }
            return value;
        }
        }
    }

    util::unreachable();
}

template <mem_access_type T>
void MC68EC000::WriteEffectiveAddress(uint8 M, uint8 Xn, T value) {
    static constexpr uint32 regMask = ~0u << (sizeof(T) * 8u - 1u) << 1u;

    switch (M) {
    case 0b000: D[Xn] = (D[Xn] & regMask) | value; break;
    case 0b001: A[Xn] = (A[Xn] & regMask) | value; break;
    case 0b010: MemWrite<T>(A[Xn], value); break;
    case 0b011:
        MemWrite<T>(A[Xn], value);
        A[Xn] += sizeof(T);
        break;
    case 0b100:
        A[Xn] -= sizeof(T);
        MemWrite<T>(A[Xn], value);
        break;
    case 0b101: {
        const sint16 disp = static_cast<sint16>(FetchInstruction());
        MemWrite<T>(A[Xn] + disp, value);
    } break;
    case 0b110: {
        const uint16 briefExtWord = FetchInstruction();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        uint32 index = m ? A[extXn] : D[extXn];
        if (!s) {
            // Word index
            index &= 0xFFFF;
        }
        MemWrite<T>(A[Xn] + disp + index, value);
    } break;
    }
}

template <std::integral T>
void MC68EC000::SetArithFlags(T op1, T op2, T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    SR.N = result >> shift;
    SR.Z = result == 0;
    SR.V = ((result ^ op1) & (result ^ op2)) >> shift;
    SR.C = SR.X = result < op1;
}

template <std::integral T>
void MC68EC000::SetLogicFlags(T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    SR.N = result >> shift;
    SR.Z = result == 0;
    SR.V = 0;
    SR.C = 0;
}

uint32 MC68EC000::CalcEffectiveAddress(uint8 M, uint8 Xn) {
    switch (M) {
    case 0b010: return A[Xn];
    case 0b101: {
        const sint16 disp = static_cast<sint16>(FetchInstruction());
        return A[Xn] + disp;
    }
    case 0b110: {
        const uint16 briefExtWord = FetchInstruction();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        uint32 index = m ? A[extXn] : D[extXn];
        if (!s) {
            // Word index
            index &= 0xFFFF;
        }
        return A[Xn] + disp + index;
    }
    case 0b111:
        switch (Xn) {
        case 0b010: return PC + static_cast<sint16>(FetchInstruction());
        case 0b011: {
            const uint16 briefExtWord = FetchInstruction();

            const uint32 address = PC + static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
            const bool s = bit::extract<11>(briefExtWord);
            const uint8 extXn = bit::extract<12, 14>(briefExtWord);
            const bool m = bit::extract<15>(briefExtWord);

            uint32 index = m ? A[extXn] : D[extXn];
            if (!s) {
                // Word index
                index &= 0xFFFF;
            }
            return address + index;
        }
        case 0b000: {
            const uint16 address = FetchInstruction();
            return address;
        }
        case 0b001: {
            const uint32 addressHigh = FetchInstruction();
            const uint32 addressLow = FetchInstruction();
            return (addressHigh << 16u) | addressLow;
        }
        }
    }

    util::unreachable();
}

void MC68EC000::Execute() {
    const uint16 instr = FetchInstruction();

    const OpcodeType type = g_decodeTable.opcodeTypes[instr];
    switch (type) {
    case OpcodeType::Move_EA_EA: Instr_Move_EA_EA(instr); break;
    case OpcodeType::Move_EA_SR: Instr_Move_EA_SR(instr); break;
    case OpcodeType::MoveQ: Instr_MoveQ(instr); break;
    case OpcodeType::MoveA: Instr_MoveA(instr); break;

    case OpcodeType::AddA: Instr_AddA(instr); break;
    case OpcodeType::AddI: Instr_AddI(instr); break;
    case OpcodeType::AndI_EA: Instr_AndI_EA(instr); break;

    case OpcodeType::LEA: Instr_LEA(instr); break;

    case OpcodeType::BRA: Instr_BRA(instr); break;
    case OpcodeType::BSR: Instr_BSR(instr); break;
    case OpcodeType::Bcc: Instr_Bcc(instr); break;
    case OpcodeType::DBcc: Instr_DBcc(instr); break;
    case OpcodeType::JSR: Instr_JSR(instr); break;

    case OpcodeType::RTS: Instr_RTS(instr); break;

    case OpcodeType::Illegal: Instr_Illegal(instr); break;

    case OpcodeType::Undecoded:
        fmt::println("M68K: found undecoded instruction {:04X} at {:08X} -- this is a bug!", instr, PC - 2);
        break;
    default:
        fmt::println("M68K: unexpected instruction type {} for opcode {:04X} at {:08X} -- this is a bug!",
                     static_cast<uint32>(type), instr, PC - 2);
        break;
    }
}

// -----------------------------------------------------------------------------
// Instruction interpreters

void MC68EC000::Instr_Move_EA_EA(uint16 instr) {
    const uint32 size = bit::extract<12, 13>(instr);
    const uint32 dstXn = bit::extract<9, 11>(instr);
    const uint32 dstM = bit::extract<6, 8>(instr);
    const uint32 srcXn = bit::extract<0, 2>(instr);
    const uint32 srcM = bit::extract<3, 5>(instr);

    auto move = [&]<std::integral T>() {
        const T value = ReadEffectiveAddress<T>(srcM, srcXn);
        WriteEffectiveAddress<T>(dstM, dstXn, value);
        SetLogicFlags(value);
    };

    // Note the swapped bit order between word and longword moves
    switch (size) {
    case 0b01: move.template operator()<uint8>(); break;
    case 0b11: move.template operator()<uint16>(); break;
    case 0b10: move.template operator()<uint32>(); break;
    }
}

void MC68EC000::Instr_Move_EA_SR(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    SR.u16 = ReadEffectiveAddress<uint16>(M, Xn) & 0xF71F;
}

void MC68EC000::Instr_MoveQ(uint16 instr) {
    const sint32 value = static_cast<sint8>(bit::extract<0, 7>(instr));
    const uint32 reg = bit::extract<9, 11>(instr);
    D[reg] = value;
    SetLogicFlags(value);
}

void MC68EC000::Instr_MoveA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 An = bit::extract<9, 11>(instr);
    const uint16 size = bit::extract<12, 13>(instr);

    switch (size) {
    case 0b11: A[An] = static_cast<sint16>(ReadEffectiveAddress<uint16>(M, Xn)); break;
    case 0b10: A[An] = ReadEffectiveAddress<uint32>(M, Xn); break;
    }
}

void MC68EC000::Instr_AddA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const bool sz = bit::extract<8>(instr);
    const uint16 An = bit::extract<9, 11>(instr);

    if (sz) {
        A[An] += ReadEffectiveAddress<uint32>(M, Xn);
    } else {
        A[An] += ReadEffectiveAddress<uint16>(M, Xn);
    }
}

void MC68EC000::Instr_AddI(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = FetchInstruction();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | FetchInstruction();
        }
        const T op2 = ReadEffectiveAddress<T>(M, Xn);
        const T result = op1 + op2;
        WriteEffectiveAddress<T>(M, Xn, result);
        SetArithFlags(op1, op2, result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

void MC68EC000::Instr_AndI_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = FetchInstruction();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | FetchInstruction();
        }
        const T op2 = ReadEffectiveAddress<T>(M, Xn);
        const T result = op1 & op2;
        WriteEffectiveAddress<T>(M, Xn, result);
        SetLogicFlags(result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

void MC68EC000::Instr_LEA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 An = bit::extract<9, 11>(instr);

    A[An] = CalcEffectiveAddress(M, Xn);
}

void MC68EC000::Instr_BRA(uint16 instr) {
    const uint32 currPC = PC;
    sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
    if (disp == 0x00) {
        disp = static_cast<sint16>(FetchInstruction());
    }
    PC = currPC + disp;
}

void MC68EC000::Instr_BSR(uint16 instr) {
    const uint32 currPC = PC;
    sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
    if (disp == 0x00) {
        disp = static_cast<sint16>(FetchInstruction());
    }

    A[7] -= 4;
    MemWriteLong(A[7], currPC);
    PC = currPC + disp;
}

void MC68EC000::Instr_Bcc(uint16 instr) {
    const uint32 currPC = PC;
    sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
    if (disp == 0x00) {
        disp = static_cast<sint16>(FetchInstruction());
    }
    const uint32 cond = bit::extract<8, 11>(instr);
    if (kCondTable[(cond << 4u) | SR.flags]) {
        PC = currPC + disp;
    }
}

void MC68EC000::Instr_DBcc(uint16 instr) {
    const uint32 currPC = PC;
    const uint32 Dn = bit::extract<0, 2>(instr);
    const uint32 cond = bit::extract<8, 11>(instr);
    const sint16 disp = static_cast<sint16>(FetchInstruction());

    if (!kCondTable[(cond << 4u) | SR.flags]) {
        const uint16 value = D[Dn] - 1;
        D[Dn] = (D[Dn] & 0xFFFF0000) | value;
        if (value != 0xFFFFu) {
            PC = currPC + disp;
        }
    }
}

void MC68EC000::Instr_JSR(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    const uint32 target = CalcEffectiveAddress(M, Xn);

    A[7] -= 4;
    MemWriteLong(A[7], PC);
    PC = target;
}

void MC68EC000::Instr_RTS(uint16 instr) {
    PC = MemReadLong(A[7]);
    A[7] += 4;
}

void MC68EC000::Instr_Illegal(uint16 instr) {
    // TODO: handle illegal instruction exception
}

} // namespace satemu::m68k

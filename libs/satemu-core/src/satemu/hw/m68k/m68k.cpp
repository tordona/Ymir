#define DISABLE_FORCE_INLINE

#include <satemu/hw/m68k/m68k.hpp>

#include <satemu/hw/scsp/scsp.hpp> // because M68kBus *is* SCSP

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/unreachable.hpp>

#include <cassert>

namespace satemu::m68k {

MC68EC000::MC68EC000(M68kBus &bus)
    : m_bus(bus) {
    Reset(true);
}

void MC68EC000::Reset(bool hard) {
    // TODO: check reset values
    if (hard) {
        regs.DA.fill(0);

        m_externalInterruptLevel = 0;
    }

    regs.SP = MemReadLong(0x00000000);
    SP_swap = 0;

    PC = MemReadLong(0x00000004);
    FullPrefetch();

    SR.u16 = 0;
    SR.S = 1;
    SR.T = 0;
    SR.IPM = 7;
}

FLATTEN void MC68EC000::Step() {
    Execute();
}

void MC68EC000::SetExternalInterruptLevel(uint8 level) {
    assert(level <= 7);
    m_externalInterruptLevel = level;
}

template <mem_primitive T, bool instrFetch>
T MC68EC000::MemRead(uint32 address) {
    static constexpr uint32 addrMask = ~(sizeof(T) - 1) & 0xFFFFFF;
    address &= addrMask;

    if constexpr (std::is_same_v<T, uint32>) {
        const uint32 hi = m_bus.Read<uint16, instrFetch>(address + 0);
        const uint32 lo = m_bus.Read<uint16, instrFetch>(address + 2);
        return (hi << 16u) | lo;
    } else {
        return m_bus.Read<T, instrFetch>(address);
    }
}

template <mem_primitive T>
void MC68EC000::MemWrite(uint32 address, T value) {
    static constexpr uint32 addrMask = ~(sizeof(T) - 1) & 0xFFFFFF;
    address &= addrMask;

    if constexpr (std::is_same_v<T, uint32>) {
        m_bus.Write<uint16>(address + 0, value >> 16u);
        m_bus.Write<uint16>(address + 2, value >> 0u);
    } else {
        m_bus.Write<T>(address, value);
    }
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

FORCE_INLINE void MC68EC000::SetSR(uint16 value) {
    const bool oldS = SR.S;
    SR.u16 = value & 0xA71F;

    if (SR.S != oldS) {
        std::swap(regs.SP, SP_swap);
    }
}

FORCE_INLINE void MC68EC000::EnterException(ExceptionVector vector) {
    HandleExceptionCommon(vector, SR.IPM);
}

FORCE_INLINE void MC68EC000::HandleInterrupt(ExceptionVector vector, uint8 level) {
    HandleExceptionCommon(vector, level);
}

FORCE_INLINE void MC68EC000::HandleExceptionCommon(ExceptionVector vector, uint8 intrLevel) {
    const uint16 oldSR = SR.u16;
    if (SR.S == 0) {
        std::swap(regs.SP, SP_swap);
    }
    SR.S = 1;
    SR.T = 0;
    SR.IPM = intrLevel;

    MemWrite<uint16>(regs.SP - 2, PC);
    MemWrite<uint16>(regs.SP - 6, oldSR);
    MemWrite<uint16>(regs.SP - 4, PC >> 16u);
    regs.SP -= 6;
    PC = MemRead<uint32, false>(static_cast<uint32>(vector) << 2u);
    FullPrefetch();
}

FORCE_INLINE bool MC68EC000::CheckPrivilege() {
    if (!SR.S) {
        PC -= 2;
        EnterException(ExceptionVector::PrivilegeViolation);
        return false;
    }
    return true;
}

FORCE_INLINE void MC68EC000::CheckInterrupt() {
    const uint8 level = m_externalInterruptLevel;
    if (level == 7 || level > SR.IPM) {
        ExceptionVector vector = m_bus.AcknowledgeInterrupt(level);
        if (vector == ExceptionVector::AutoVectorRequest) {
            vector = static_cast<ExceptionVector>(static_cast<uint32>(ExceptionVector::BaseAutovector) + level);
        }
        HandleInterrupt(vector, level);
    }
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

template <mem_primitive T>
FORCE_INLINE T MC68EC000::ReadEffectiveAddress(uint8 M, uint8 Xn) {
    switch (M) {
    case 0b000: return regs.D[Xn];
    case 0b001: return regs.A[Xn];
    case 0b010: return MemRead<T, false>(regs.A[Xn]);
    case 0b011: {
        const T value = MemRead<T, false>(regs.A[Xn]);
        regs.A[Xn] += sizeof(T);
        return value;
    }
    case 0b100: regs.A[Xn] -= sizeof(T); return MemRead<T, false>(regs.A[Xn]);
    case 0b101: {
        const sint16 disp = static_cast<sint16>(PrefetchNext());
        return MemRead<T, false>(regs.A[Xn] + disp);
    }
    case 0b110: {
        const uint16 briefExtWord = PrefetchNext();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        sint32 index = m ? regs.A[extXn] : regs.D[extXn];
        if (!s) {
            // Word index
            index = static_cast<sint16>(index);
        }
        return MemRead<T, false>(regs.A[Xn] + disp + index);
    }
    case 0b111:
        switch (Xn) {
        case 0b010: return MemRead<T, true>(PC - 2 + static_cast<sint16>(PrefetchNext()));
        case 0b011: {
            const uint32 pc = PC - 2;
            const uint16 extWord = PrefetchNext();

            const sint8 disp = bit::extract_signed<0, 7>(extWord);
            const bool wl = bit::extract<11>(extWord);
            const uint8 extXn = bit::extract<12, 14>(extWord);
            const bool da = bit::extract<15>(extWord);

            sint32 index = da ? regs.A[extXn] : regs.D[extXn];
            if (!wl) {
                // Word index
                index = static_cast<sint16>(index);
            }

            const uint32 address = pc + static_cast<sint8>(disp) + index;
            return MemRead<T, true>(address);
        }
        case 0b000: {
            const uint16 address = PrefetchNext();
            return MemRead<T, false>(address);
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = PrefetchNext();
            return MemRead<T, false>((addressHigh << 16u) | addressLow);
        }
        case 0b100: {
            uint32 value = PrefetchNext();
            if constexpr (std::is_same_v<T, uint32>) {
                value = (value << 16u) | PrefetchNext();
            }
            return value;
        }
        }
        break;
    }

    util::unreachable();
}

template <mem_primitive T>
FORCE_INLINE void MC68EC000::WriteEffectiveAddress(uint8 M, uint8 Xn, T value) {
    switch (M) {
    case 0b000: bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[Xn], value); break;
    case 0b001: regs.A[Xn] = value; break;
    case 0b010: MemWrite<T>(regs.A[Xn], value); break;
    case 0b011:
        MemWrite<T>(regs.A[Xn], value);
        regs.A[Xn] += sizeof(T);
        break;
    case 0b100:
        regs.A[Xn] -= sizeof(T);
        MemWrite<T>(regs.A[Xn], value);
        break;
    case 0b101: {
        const sint16 disp = static_cast<sint16>(PrefetchNext());
        MemWrite<T>(regs.A[Xn] + disp, value);
        break;
    }
    case 0b110: {
        const uint16 briefExtWord = PrefetchNext();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        sint32 index = m ? regs.A[extXn] : regs.D[extXn];
        if (!s) {
            // Word index
            index = static_cast<sint16>(index);
        }
        MemWrite<T>(regs.A[Xn] + disp + index, value);
        break;
    }
    case 0b111:
        switch (Xn) {
        case 0b000: {
            const uint16 address = PrefetchNext();
            MemWrite<T>(address, value);
            break;
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = PrefetchNext();
            MemWrite<T>((addressHigh << 16u) | addressLow, value);
            break;
        }
        }
        break;
    }
}

template <mem_primitive T, typename FnModify>
FORCE_INLINE void MC68EC000::ModifyEffectiveAddress(uint8 M, uint8 Xn, FnModify &&modify) {
    switch (M) {
    case 0b000: {
        const T value = modify(regs.D[Xn]);
        PrefetchTransfer();
        bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[Xn], value);
        break;
    }
    case 0b001: {
        const uint32 value = modify(regs.A[Xn]);
        PrefetchTransfer();
        regs.A[Xn] = value;
        break;
    }
    case 0b010: {
        const T value = MemRead<T, false>(regs.A[Xn]);
        const T result = modify(value);
        PrefetchTransfer();
        MemWrite<T>(regs.A[Xn], result);
        break;
    }
    case 0b011: {
        const T value = MemRead<T, false>(regs.A[Xn]);
        const T result = modify(value);
        PrefetchTransfer();
        MemWrite<T>(regs.A[Xn], result);
        regs.A[Xn] += sizeof(T);
        break;
    }
    case 0b100: {
        regs.A[Xn] -= sizeof(T);
        const T value = MemRead<T, false>(regs.A[Xn]);
        const T result = modify(value);
        PrefetchTransfer();
        MemWrite<T>(regs.A[Xn], result);
        break;
    }
    case 0b101: {
        const sint16 disp = static_cast<sint16>(PrefetchNext());
        const uint32 address = regs.A[Xn] + disp;
        const T value = MemRead<T, false>(address);
        const T result = modify(value);
        PrefetchTransfer();
        MemWrite<T>(address, result);
        break;
    }
    case 0b110: {
        const uint16 briefExtWord = PrefetchNext();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        sint32 index = m ? regs.A[extXn] : regs.D[extXn];
        if (!s) {
            // Word index
            index = static_cast<sint16>(index);
        }
        const uint32 address = regs.A[Xn] + disp + index;
        const T value = MemRead<T, false>(address);
        const T result = modify(value);
        PrefetchTransfer();
        MemWrite<T>(address, result);
        break;
    }
    case 0b111:
        switch (Xn) {
        case 0b000: {
            const uint16 address = PrefetchNext();
            const T value = MemRead<T, false>(address);
            const T result = modify(value);
            PrefetchTransfer();
            MemWrite<T>(address, result);
            break;
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = PrefetchNext();
            const uint32 address = (addressHigh << 16u) | addressLow;
            const T value = MemRead<T, false>(address);
            const T result = modify(value);
            PrefetchTransfer();
            MemWrite<T>(address, result);
            break;
        }
        }
        break;
    }
}

FORCE_INLINE uint32 MC68EC000::CalcEffectiveAddress(uint8 M, uint8 Xn) {
    switch (M) {
    case 0b010: return regs.A[Xn];
    case 0b101: {
        const sint16 disp = static_cast<sint16>(PrefetchNext());
        return regs.A[Xn] + disp;
    }
    case 0b110: {
        const uint16 briefExtWord = PrefetchNext();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        uint32 index = m ? regs.A[extXn] : regs.D[extXn];
        if (!s) {
            // Word index
            index &= 0xFFFF;
        }
        return regs.A[Xn] + disp + index;
    }
    case 0b111:
        switch (Xn) {
        case 0b010: return PC - 2 + static_cast<sint16>(PrefetchNext());
        case 0b011: {
            const uint16 briefExtWord = PrefetchNext();

            const uint32 address = PC - 2 + static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
            const bool s = bit::extract<11>(briefExtWord);
            const uint8 extXn = bit::extract<12, 14>(briefExtWord);
            const bool m = bit::extract<15>(briefExtWord);

            sint32 index = m ? regs.A[extXn] : regs.D[extXn];
            if (!s) {
                // Word index
                index = static_cast<sint16>(index);
            }
            return address + index;
        }
        case 0b000: {
            const uint16 address = PrefetchNext();
            return address;
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = PrefetchNext();
            return (addressHigh << 16u) | addressLow;
        }
        }
    }

    util::unreachable();
}

template <std::integral T, bool sub, bool setX>
FORCE_INLINE void MC68EC000::SetArithFlags(T op1, T op2, T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    SR.N = result >> shift;
    SR.Z = result == 0;
    if constexpr (sub) {
        SR.V = ((op1 ^ op2) & (result ^ op2)) >> shift;
    } else {
        SR.V = ((result ^ op1) & (result ^ op2)) >> shift;
    }
    SR.C = result < op1;
    if constexpr (setX) {
        SR.X = SR.C;
    }
}

template <std::integral T>
FORCE_INLINE void MC68EC000::SetLogicFlags(T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    SR.N = result >> shift;
    SR.Z = result == 0;
    SR.V = 0;
    SR.C = 0;
}

template <std::integral T>
FORCE_INLINE void MC68EC000::SetShiftFlags(T result, bool carry) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    SR.N = result >> shift;
    SR.Z = result == 0;
    SR.V = 0;
    SR.C = SR.X = carry;
}

// -----------------------------------------------------------------------------
// Prefetch queue

FLATTEN FORCE_INLINE void MC68EC000::FullPrefetch() {
    PrefetchNext();
    PrefetchTransfer();
}

FORCE_INLINE uint16 MC68EC000::PrefetchNext() {
    const uint16 prev = m_prefetchQueue[0];
    m_prefetchQueue[0] = FetchInstruction();
    return prev;
}

FORCE_INLINE void MC68EC000::PrefetchTransfer() {
    // NOTE: consolidating IRC -> IR and IR -> IRD steps here
    // technically they should be separate
    m_prefetchQueue[1] = m_prefetchQueue[0];
    PrefetchNext();
}

// -----------------------------------------------------------------------------
// Interpreter

void MC68EC000::Execute() {
    CheckInterrupt();

    const uint16 instr = m_prefetchQueue[1];

    const OpcodeType type = g_decodeTable.opcodeTypes[instr];
    switch (type) {
    case OpcodeType::Move_EA_EA: Instr_Move_EA_EA(instr); break;
    case OpcodeType::Move_EA_SR: Instr_Move_EA_SR(instr); break;
    case OpcodeType::MoveA: Instr_MoveA(instr); break;
    case OpcodeType::MoveM_EA_Rs: Instr_MoveM_EA_Rs(instr); break;
    case OpcodeType::MoveM_PI_Rs: Instr_MoveM_PI_Rs(instr); break;
    case OpcodeType::MoveM_Rs_EA: Instr_MoveM_Rs_EA(instr); break;
    case OpcodeType::MoveM_Rs_PD: Instr_MoveM_Rs_PD(instr); break;
    case OpcodeType::MoveQ: Instr_MoveQ(instr); break;

    case OpcodeType::Clr: Instr_Clr(instr); break;
    case OpcodeType::Swap: Instr_Swap(instr); break;

    case OpcodeType::Add_Dn_EA: Instr_Add_Dn_EA(instr); break;
    case OpcodeType::Add_EA_Dn: Instr_Add_EA_Dn(instr); break;
    case OpcodeType::AddA: Instr_AddA(instr); break;
    case OpcodeType::AddI: Instr_AddI(instr); break;
    case OpcodeType::AddQ_An: Instr_AddQ_An(instr); break;
    case OpcodeType::AddQ_EA: Instr_AddQ_EA(instr); break;
    case OpcodeType::AndI_EA: Instr_AndI_EA(instr); break;
    case OpcodeType::Eor_Dn_EA: Instr_Eor_Dn_EA(instr); break;
    case OpcodeType::Or_Dn_EA: Instr_Or_Dn_EA(instr); break;
    case OpcodeType::Or_EA_Dn: Instr_Or_EA_Dn(instr); break;
    case OpcodeType::OrI_EA: Instr_OrI_EA(instr); break;
    case OpcodeType::SubI: Instr_SubI(instr); break;
    case OpcodeType::SubQ_An: Instr_SubQ_An(instr); break;
    case OpcodeType::SubQ_EA: Instr_SubQ_EA(instr); break;

    case OpcodeType::LSL_I: Instr_LSL_I(instr); break;
    case OpcodeType::LSL_M: Instr_LSL_M(instr); break;
    case OpcodeType::LSL_R: Instr_LSL_R(instr); break;
    case OpcodeType::LSR_I: Instr_LSR_I(instr); break;
    case OpcodeType::LSR_M: Instr_LSR_M(instr); break;
    case OpcodeType::LSR_R: Instr_LSR_R(instr); break;

    case OpcodeType::Cmp: Instr_Cmp(instr); break;
    case OpcodeType::CmpA: Instr_CmpA(instr); break;
    case OpcodeType::CmpI: Instr_CmpI(instr); break;
    case OpcodeType::BTst_I_Dn: Instr_BTst_I_Dn(instr); break;
    case OpcodeType::BTst_I_EA: Instr_BTst_I_EA(instr); break;
    case OpcodeType::BTst_R_Dn: Instr_BTst_R_Dn(instr); break;
    case OpcodeType::BTst_R_EA: Instr_BTst_R_EA(instr); break;

    case OpcodeType::LEA: Instr_LEA(instr); break;

    case OpcodeType::BRA: Instr_BRA(instr); break;
    case OpcodeType::BSR: Instr_BSR(instr); break;
    case OpcodeType::Bcc: Instr_Bcc(instr); break;
    case OpcodeType::DBcc: Instr_DBcc(instr); break;
    case OpcodeType::JSR: Instr_JSR(instr); break;
    case OpcodeType::Jmp: Instr_Jmp(instr); break;

    case OpcodeType::RTS: Instr_RTS(instr); break;

    case OpcodeType::Trap: Instr_Trap(instr); break;
    case OpcodeType::TrapV: Instr_TrapV(instr); break;

    case OpcodeType::Noop: Instr_Noop(instr); break;

    case OpcodeType::Illegal: Instr_Illegal(instr); break;
    case OpcodeType::Illegal1010: Instr_Illegal1010(instr); break;
    case OpcodeType::Illegal1111: Instr_Illegal1111(instr); break;

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

FORCE_INLINE void MC68EC000::Instr_Move_EA_EA(uint16 instr) {
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

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Move_EA_SR(uint16 instr) {
    if (CheckPrivilege()) {
        const uint16 Xn = bit::extract<0, 2>(instr);
        const uint16 M = bit::extract<3, 5>(instr);
        SetSR(ReadEffectiveAddress<uint16>(M, Xn) & 0xF71F);

        PrefetchTransfer();
    }
}

FORCE_INLINE void MC68EC000::Instr_MoveA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 An = bit::extract<9, 11>(instr);
    const uint16 size = bit::extract<12, 13>(instr);

    switch (size) {
    case 0b11: regs.A[An] = static_cast<sint16>(ReadEffectiveAddress<uint16>(M, Xn)); break;
    case 0b10: regs.A[An] = ReadEffectiveAddress<uint32>(M, Xn); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_MoveM_EA_Rs(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const bool sz = bit::extract<6>(instr);
    const uint16 regList = PrefetchNext();
    uint32 address = CalcEffectiveAddress(M, Xn);

    auto xfer = [&]<std::integral T>(uint32 regIndex) {
        const T value = MemRead<T, false>(address);
        regs.DA[regIndex] = value;
        address += sizeof(T);
    };

    for (uint32 i = 0; i < 16; i++) {
        if (regList & (1u << i)) {
            sz ? xfer.template operator()<uint32>(i) : xfer.template operator()<uint16>(i);
        }
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_MoveM_PI_Rs(uint16 instr) {
    const uint16 An = bit::extract<0, 2>(instr);
    const bool sz = bit::extract<6>(instr);
    const uint16 regList = PrefetchNext();

    auto xfer = [&]<std::integral T>(uint32 regIndex) {
        const uint32 address = regs.A[An];
        const T value = MemRead<T, false>(address);
        regs.DA[regIndex] = value;
        regs.A[An] = address + sizeof(T);
    };

    for (uint32 i = 0; i < 16; i++) {
        if (regList & (1u << i)) {
            sz ? xfer.template operator()<uint32>(i) : xfer.template operator()<uint16>(i);
        }
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_MoveM_Rs_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const bool sz = bit::extract<6>(instr);
    const uint16 regList = PrefetchNext();
    uint32 address = CalcEffectiveAddress(M, Xn);

    auto xfer = [&]<std::integral T>(uint32 regIndex) {
        const T value = regs.DA[regIndex];
        MemWrite<T>(address, value);
        address += sizeof(T);
    };

    for (uint32 i = 0; i < 16; i++) {
        if (regList & (1u << i)) {
            sz ? xfer.template operator()<uint32>(i) : xfer.template operator()<uint16>(i);
        }
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_MoveM_Rs_PD(uint16 instr) {
    const uint16 An = bit::extract<0, 2>(instr);
    const bool sz = bit::extract<6>(instr);
    const uint16 regList = PrefetchNext();

    auto xfer = [&]<std::integral T>(uint32 regIndex) {
        const uint32 address = regs.A[An] - sizeof(T);
        const T value = regs.DA[15 - regIndex];
        MemWrite<T>(address, value);
        regs.A[An] = address;
    };

    for (uint32 i = 0; i < 16; i++) {
        if (regList & (1u << i)) {
            sz ? xfer.template operator()<uint32>(i) : xfer.template operator()<uint16>(i);
        }
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_MoveQ(uint16 instr) {
    const sint32 value = static_cast<sint8>(bit::extract<0, 7>(instr));
    const uint32 reg = bit::extract<9, 11>(instr);
    regs.D[reg] = value;
    SetLogicFlags(value);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Clr(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        ModifyEffectiveAddress<T>(M, Xn, [&](T) {
            SR.Z = 1;
            SR.N = SR.V = SR.C = 0;
            return 0;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Swap(uint16 instr) {
    const uint32 reg = bit::extract<0, 3>(instr);
    const uint32 value = (regs.D[reg] >> 16u) | (regs.D[reg] << 16u);
    regs.D[reg] = value;
    SetLogicFlags(value);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Add_Dn_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = regs.D[Dn];
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 + op1;
            SetAdditionFlags(op1, op2, result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Add_EA_Dn(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = ReadEffectiveAddress<T>(M, Xn);
        const T op2 = regs.D[Dn];
        const T result = op2 + op1;
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SetAdditionFlags(op1, op2, result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_AddA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const bool sz = bit::extract<8>(instr);
    const uint16 An = bit::extract<9, 11>(instr);

    if (sz) {
        regs.A[An] += ReadEffectiveAddress<uint32>(M, Xn);
    } else {
        regs.A[An] += ReadEffectiveAddress<uint16>(M, Xn);
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_AddI(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = PrefetchNext();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | PrefetchNext();
        }
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 + op1;
            SetAdditionFlags(op1, op2, result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_AddQ_An(uint16 instr) {
    const uint16 An = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        const uint32 op1 = static_cast<std::make_signed_t<T>>(bit::extract<9, 11>(instr));
        const uint32 op2 = regs.A[An];
        const uint32 result = op2 + op1;
        regs.A[An] = result;
    };

    switch (sz) {
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_AddQ_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 data = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = data == 0 ? 8 : data;
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 + op1;
            SetAdditionFlags(op1, op2, result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_AndI_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = PrefetchNext();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | PrefetchNext();
        }
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 & op1;
            SetLogicFlags(result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Eor_Dn_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = regs.D[Dn];
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 ^ op1;
            SetLogicFlags(result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Or_Dn_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = regs.D[Dn];
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 | op1;
            SetLogicFlags(result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Or_EA_Dn(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = ReadEffectiveAddress<T>(M, Xn);
        const T op2 = regs.D[Dn];
        const T result = op2 | op1;
        bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[Dn], result);
        SetLogicFlags(result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_OrI_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = PrefetchNext();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | PrefetchNext();
        }
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 | op1;
            SetLogicFlags(result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_SubI(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = PrefetchNext();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | PrefetchNext();
        }
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 - op1;
            SetSubtractionFlags(op1, op2, result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_SubQ_An(uint16 instr) {
    const uint16 An = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        const uint32 op1 = static_cast<std::make_signed_t<T>>(bit::extract<9, 11>(instr));
        const uint32 op2 = regs.A[An];
        const uint32 result = op2 - op1;
        regs.A[An] = result;
    };

    switch (sz) {
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_SubQ_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 data = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = data == 0 ? 8 : data;
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 - op1;
            SetSubtractionFlags(op1, op2, result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_LSL_I(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    uint32 shift = bit::extract<9, 11>(instr);
    if (shift == 0) {
        shift = 8;
    }

    auto op = [&]<std::integral T>() {
        if (sizeof(T) == sizeof(uint8) && shift == 8) {
            const T value = regs.D[Dn];
            const T result = 0;
            const bool carry = value >> 7;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SetShiftFlags(result, carry);
        } else {
            const T value = regs.D[Dn];
            const T result = value << shift;
            const bool carry = (value >> (sizeof(T) * 8 - shift)) & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SetShiftFlags(result, carry);
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_LSL_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        const uint16 result = value << 1u;
        const bool carry = value >> 15u;
        SetShiftFlags(result, carry);
        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_LSL_R(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 shiftReg = bit::extract<9, 11>(instr);
    const uint32 shift = regs.D[shiftReg] & 63;

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        T result;
        bool carry;
        if (shift > sizeof(T) * 8) {
            result = 0;
            carry = false;
        } else if (shift == sizeof(T) * 8) {
            result = 0;
            carry = value & 1;
        } else if (shift != 0) {
            result = value << shift;
            carry = (value >> (sizeof(T) * 8 - shift)) & 1;
        } else {
            result = value;
            carry = false;
        }
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SetShiftFlags(result, carry);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_LSR_I(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    uint32 shift = bit::extract<9, 11>(instr);
    if (shift == 0) {
        shift = 8;
    }

    auto op = [&]<std::integral T>() {
        if (sizeof(T) == sizeof(uint8) && shift == 8) {
            const T value = regs.D[Dn];
            const T result = 0;
            const bool carry = value & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SetShiftFlags(result, carry);
        } else {
            const T value = regs.D[Dn];
            const T result = value >> shift;
            const bool carry = (value >> (shift - 1)) & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SetShiftFlags(result, carry);
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_LSR_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        const uint16 result = value >> 1u;
        const bool carry = value & 1u;
        SetShiftFlags(result, carry);
        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_LSR_R(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 shiftReg = bit::extract<9, 11>(instr);
    const uint32 shift = regs.D[shiftReg] & 63;

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        T result;
        bool carry;
        if (shift > sizeof(T) * 8) {
            result = 0;
            carry = false;
        } else if (shift == sizeof(T) * 8) {
            result = 0;
            carry = value >> (sizeof(T) * 8 - 1);
        } else if (shift != 0) {
            result = value >> shift;
            carry = (value >> (shift - 1)) & 1;
        } else {
            result = value;
            carry = false;
        }
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SetShiftFlags(result, carry);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Cmp(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = ReadEffectiveAddress<T>(M, Xn);
        const T op2 = regs.D[Dn];
        const T result = op2 - op1;
        SetCompareFlags(op1, op2, result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_CmpA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<8>(instr);
    const uint16 An = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = static_cast<std::make_signed_t<T>>(ReadEffectiveAddress<T>(M, Xn));
        const T op2 = regs.A[An];
        const T result = op2 - op1;
        SetCompareFlags(op1, op2, result);
    };

    if (sz) {
        op.template operator()<uint32>();
    } else {
        op.template operator()<uint16>();
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_CmpI(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = PrefetchNext();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | PrefetchNext();
        }
        const T op2 = ReadEffectiveAddress<T>(M, Xn);
        const T result = op2 - op1;
        SetCompareFlags(op1, op2, result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BTst_I_Dn(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 index = PrefetchNext() & 31;

    const uint32 value = regs.D[Dn];
    SR.Z = (value >> index) & 1;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BTst_I_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 index = PrefetchNext() & 7;

    const uint8 value = ReadEffectiveAddress<uint8>(M, Xn);
    SR.Z = (value >> index) & 1;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BTst_R_Dn(uint16 instr) {
    const uint16 dstDn = bit::extract<0, 2>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 31;

    const uint32 value = regs.D[dstDn];
    SR.Z = (value >> index) & 1;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BTst_R_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 8;

    const uint8 value = ReadEffectiveAddress<uint8>(M, Xn);
    SR.Z = (value >> index) & 1;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_LEA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 An = bit::extract<9, 11>(instr);

    regs.A[An] = CalcEffectiveAddress(M, Xn);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BRA(uint16 instr) {
    const uint32 currPC = PC - 2;
    sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
    if (disp == 0x00) {
        disp = static_cast<sint16>(PrefetchNext());
    }
    PC = currPC + disp;
    FullPrefetch();
}

FORCE_INLINE void MC68EC000::Instr_BSR(uint16 instr) {
    const uint32 currPC = PC - 2;
    sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
    if (disp == 0x00) {
        disp = static_cast<sint16>(PrefetchNext());
    }

    regs.SP -= 4;
    MemWriteLong(regs.SP, PC - 2);
    PC = currPC + disp;
    FullPrefetch();
}

FORCE_INLINE void MC68EC000::Instr_Bcc(uint16 instr) {
    const uint32 currPC = PC - 2;
    sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
    if (disp == 0x00) {
        disp = static_cast<sint16>(PrefetchNext());
    }
    const uint32 cond = bit::extract<8, 11>(instr);
    if (kCondTable[(cond << 4u) | SR.flags]) {
        PC = currPC + disp;
        FullPrefetch();
        return;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_DBcc(uint16 instr) {
    const uint32 currPC = PC - 2;
    const uint32 Dn = bit::extract<0, 2>(instr);
    const uint32 cond = bit::extract<8, 11>(instr);
    const sint16 disp = static_cast<sint16>(PrefetchNext());

    if (!kCondTable[(cond << 4u) | SR.flags]) {
        const uint16 value = regs.D[Dn] - 1;
        regs.D[Dn] = (regs.D[Dn] & 0xFFFF0000) | value;
        if (value != 0xFFFFu) {
            PC = currPC + disp;
            FullPrefetch();
            return;
        }
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_JSR(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    const uint32 target = CalcEffectiveAddress(M, Xn);

    regs.SP -= 4;
    MemWriteLong(regs.SP, PC - 2);
    PC = target;
    FullPrefetch();
}

FORCE_INLINE void MC68EC000::Instr_Jmp(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    const uint32 target = CalcEffectiveAddress(M, Xn);
    PC = target;
    FullPrefetch();
}

FORCE_INLINE void MC68EC000::Instr_RTS(uint16 instr) {
    PC = MemReadLong(regs.SP);
    FullPrefetch();
    regs.SP += 4;
}

FORCE_INLINE void MC68EC000::Instr_Trap(uint16 instr) {
    const uint8 vector = bit::extract<0, 3>(instr);
    EnterException(static_cast<ExceptionVector>(0x20 + vector));
}

FORCE_INLINE void MC68EC000::Instr_TrapV(uint16 instr) {
    if (SR.V) {
        EnterException(ExceptionVector::TRAPVInstruction);
        return;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Noop(uint16 instr) {
    // TODO: synchronize integer pipeline

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Illegal(uint16 instr) {
    EnterException(ExceptionVector::IllegalInstruction);
}

FORCE_INLINE void MC68EC000::Instr_Illegal1010(uint16 instr) {
    EnterException(ExceptionVector::Line1010Emulator);
}

FORCE_INLINE void MC68EC000::Instr_Illegal1111(uint16 instr) {
    EnterException(ExceptionVector::Line1111Emulator);
}

} // namespace satemu::m68k

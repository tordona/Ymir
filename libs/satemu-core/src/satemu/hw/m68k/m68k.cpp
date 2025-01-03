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

    regs.SP = MemRead<uint32, false>(0x00000000);
    SP_swap = 0;

    PC = MemRead<uint32, false>(0x00000004);
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
    if constexpr (std::is_same_v<T, uint32>) {
        const uint32 hi = MemRead<uint16, instrFetch>(address + 0);
        const uint32 lo = MemRead<uint16, instrFetch>(address + 2);
        return (hi << 16u) | lo;
    } else {
        static constexpr uint32 addrMask = ~(sizeof(T) - 1) & 0xFFFFFF;
        address &= addrMask;

        return m_bus.Read<T, instrFetch>(address);
    }
}

template <mem_primitive T, bool instrFetch>
T MC68EC000::MemReadDesc(uint32 address) {
    if constexpr (std::is_same_v<T, uint32>) {
        T value = MemRead<uint16, instrFetch>(address + 2);
        value |= MemRead<uint16, instrFetch>(address + 0) << 16u;
        return value;
    } else {
        return MemRead<T, instrFetch>(address);
    }
}

template <mem_primitive T>
void MC68EC000::MemWrite(uint32 address, T value) {
    if constexpr (std::is_same_v<T, uint32>) {
        MemWrite<uint16>(address + 2, value >> 0u);
        MemWrite<uint16>(address + 0, value >> 16u);
    } else {
        static constexpr uint32 addrMask = ~(sizeof(T) - 1) & 0xFFFFFF;
        address &= addrMask;

        m_bus.Write<T>(address, value);
    }
}

template <mem_primitive T>
void MC68EC000::MemWriteAsc(uint32 address, T value) {
    if constexpr (std::is_same_v<T, uint32>) {
        MemWrite<uint16>(address + 0, value >> 16u);
        MemWrite<uint16>(address + 2, value >> 0u);
    } else {
        MemWrite<T>(address, value);
    }
}

FLATTEN FORCE_INLINE uint16 MC68EC000::FetchInstruction() {
    uint16 instr = MemRead<uint16, true>(PC);
    PC += 2;
    return instr;
}

FORCE_INLINE void MC68EC000::SetSR(uint16 value) {
    const bool oldS = SR.S;
    SR.u16 = value & 0xA71F;

    if (SR.S != oldS) {
        std::swap(regs.SP, SP_swap);
    }
}

FORCE_INLINE void MC68EC000::SetSSP(uint32 value) {
    if (SR.S) {
        regs.SP = value;
    } else {
        SP_swap = value;
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
        AdvanceAddress<T, true>(Xn);
        return value;
    }
    case 0b100: AdvanceAddress<T, false>(Xn); return MemRead<T, false>(regs.A[Xn]);
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
        case 0b010: {
            const sint16 disp = static_cast<sint16>(PrefetchNext());
            return MemRead<T, true>(PC - 4 + disp);
        }
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
            const sint32 address = static_cast<sint16>(PrefetchNext());
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
        AdvanceAddress<T, true>(Xn);
        break;
    case 0b100:
        AdvanceAddress<T, false>(Xn);
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
            const sint32 address = static_cast<sint16>(PrefetchNext());
            MemWrite<T>(address, value);
            break;
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = m_prefetchQueue[0];
            MemWrite<T>((addressHigh << 16u) | addressLow, value);
            PrefetchNext();
            break;
        }
        }
        break;
    }
}

template <mem_primitive T, bool prefetch, typename FnModify>
FORCE_INLINE void MC68EC000::ModifyEffectiveAddress(uint8 M, uint8 Xn, FnModify &&modify) {
    switch (M) {
    case 0b000: {
        const T value = modify(regs.D[Xn]);
        if constexpr (prefetch) {
            PrefetchTransfer();
        }
        bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[Xn], value);
        break;
    }
    case 0b001: {
        const uint32 value = modify(regs.A[Xn]);
        if constexpr (prefetch) {
            PrefetchTransfer();
        }
        regs.A[Xn] = value;
        break;
    }
    case 0b010: {
        const T value = MemRead<T, false>(regs.A[Xn]);
        const T result = modify(value);
        if constexpr (prefetch) {
            PrefetchTransfer();
        }
        MemWrite<T>(regs.A[Xn], result);
        break;
    }
    case 0b011: {
        const T value = MemRead<T, false>(regs.A[Xn]);
        const T result = modify(value);
        if constexpr (prefetch) {
            PrefetchTransfer();
        }
        MemWrite<T>(regs.A[Xn], result);
        AdvanceAddress<T, true>(Xn);
        break;
    }
    case 0b100: {
        AdvanceAddress<T, false>(Xn);
        const T value = MemRead<T, false>(regs.A[Xn]);
        const T result = modify(value);
        if constexpr (prefetch) {
            PrefetchTransfer();
        }
        MemWrite<T>(regs.A[Xn], result);
        break;
    }
    case 0b101: {
        const sint16 disp = static_cast<sint16>(PrefetchNext());
        const uint32 address = regs.A[Xn] + disp;
        const T value = MemRead<T, false>(address);
        const T result = modify(value);
        if constexpr (prefetch) {
            PrefetchTransfer();
        }
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
        if constexpr (prefetch) {
            PrefetchTransfer();
        }
        MemWrite<T>(address, result);
        break;
    }
    case 0b111:
        switch (Xn) {
        case 0b000: {
            const sint32 address = static_cast<sint16>(PrefetchNext());
            const T value = MemRead<T, false>(address);
            const T result = modify(value);
            if constexpr (prefetch) {
                PrefetchTransfer();
            }
            MemWrite<T>(address, result);
            break;
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = PrefetchNext();
            const uint32 address = (addressHigh << 16u) | addressLow;
            const T value = MemRead<T, false>(address);
            const T result = modify(value);
            if constexpr (prefetch) {
                PrefetchTransfer();
            }
            MemWrite<T>(address, result);
            break;
        }
        }
        break;
    }
}

template <mem_primitive T>
FORCE_INLINE T MC68EC000::MoveEffectiveAddress(uint8 srcM, uint8 srcXn, uint8 dstM, uint8 dstXn) {
    const T value = ReadEffectiveAddress<T>(srcM, srcXn);

    switch (dstM) {
    case 0b000:
        bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[dstXn], value);
        PrefetchTransfer();
        break;
    case 0b001:
        regs.A[dstXn] = value;
        PrefetchTransfer();
        break;
    case 0b010: {
        MemWriteAsc<T>(regs.A[dstXn], value);
        PrefetchTransfer();
        break;
    }
    case 0b011: {
        MemWriteAsc<T>(regs.A[dstXn], value);
        AdvanceAddress<T, true>(dstXn);
        PrefetchTransfer();
        break;
    }
    case 0b100: {
        AdvanceAddress<T, false>(dstXn);
        PrefetchTransfer();
        MemWrite<T>(regs.A[dstXn], value);
        break;
    }
    case 0b101: {
        const sint16 disp = static_cast<sint16>(PrefetchNext());
        const uint32 address = regs.A[dstXn] + disp;
        MemWriteAsc<T>(address, value);
        PrefetchTransfer();
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
        const uint32 address = regs.A[dstXn] + disp + index;
        MemWriteAsc<T>(address, value);
        PrefetchTransfer();
        break;
    }
    case 0b111:
        switch (dstXn) {
        case 0b000: {
            const sint32 address = static_cast<sint16>(PrefetchNext());
            MemWriteAsc<T>(address, value);
            PrefetchTransfer();
            break;
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = m_prefetchQueue[0];
            const uint32 address = (addressHigh << 16u) | addressLow;
            const bool prefetchEarly = srcM < 2 || (srcM == 7 && srcXn == 4);
            if (prefetchEarly) {
                PrefetchNext();
            }
            MemWriteAsc<T>(address, value);
            if (!prefetchEarly) {
                PrefetchNext();
            }
            PrefetchTransfer();
            break;
        }
        }
        break;
    }

    return value;
}

template <bool fetch>
FORCE_INLINE uint32 MC68EC000::CalcEffectiveAddress(uint8 M, uint8 Xn) {
    auto prefetchLast = [&]() -> uint16 {
        if constexpr (fetch) {
            return PrefetchNext();
        } else {
            return m_prefetchQueue[0];
        }
    };

    static constexpr uint32 pcOffset = fetch ? 4 : 2;

    switch (M) {
    case 0b010: return regs.A[Xn];
    case 0b101: {
        const sint16 disp = static_cast<sint16>(prefetchLast());
        return regs.A[Xn] + disp;
    }
    case 0b110: {
        const uint16 briefExtWord = prefetchLast();

        const sint8 disp = static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
        const bool s = bit::extract<11>(briefExtWord);
        const uint8 extXn = bit::extract<12, 14>(briefExtWord);
        const bool m = bit::extract<15>(briefExtWord);

        uint32 index = m ? regs.A[extXn] : regs.D[extXn];
        if (!s) {
            // Word index
            index = static_cast<sint16>(index);
        }
        return regs.A[Xn] + disp + index;
    }
    case 0b111:
        switch (Xn) {
        case 0b010: {
            const sint16 disp = static_cast<sint16>(prefetchLast());
            return PC - pcOffset + disp;
        }
        case 0b011: {
            const uint16 briefExtWord = prefetchLast();

            const uint32 address = PC - pcOffset + static_cast<sint8>(bit::extract<0, 7>(briefExtWord));
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
            const sint32 address = static_cast<sint16>(prefetchLast());
            return address;
        }
        case 0b001: {
            const uint32 addressHigh = PrefetchNext();
            const uint32 addressLow = prefetchLast();
            return (addressHigh << 16u) | addressLow;
        }
        }
    }

    util::unreachable();
}

// Determines if the value is negative
template <std::integral T>
FORCE_INLINE static bool IsNegative(T value) {
    return static_cast<std::make_signed_t<T>>(value) < 0;
}

// Determines if op2+op1 results in a carry
template <std::integral T>
FORCE_INLINE static bool IsAddCarry(T op1, T op2, T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    return ((op1 & op2) | (~result & (op1 | op2))) >> shift;
}

// Determines if op2-op1 results in a borrow
template <std::integral T>
FORCE_INLINE static bool IsSubCarry(T op1, T op2, T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    return ((op1 & result) | (~op2 & (op1 | result))) >> shift;
}

// Determines if op2+op1 results in an overflow
template <std::integral T>
FORCE_INLINE static bool IsAddOverflow(T op1, T op2, T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    return ((op1 ^ result) & (op2 ^ result)) >> shift;
}

// Determines if op2-op1 results in an overflow
template <std::integral T>
FORCE_INLINE static bool IsSubOverflow(T op1, T op2, T result) {
    static constexpr T shift = sizeof(T) * 8 - 1;
    return ((op1 ^ op2) & (result ^ op2)) >> shift;
}

template <std::integral T>
static constexpr auto kShiftTable = [] {
    std::array<T, 65> table{};
    for (T i = 0; i < 65; i++) {
        if (i == 0) {
            table[i] = 0;
        } else if (i < sizeof(T) * 8) {
            table[i] = ~static_cast<T>(0) << (sizeof(T) * 8 - i);
        } else {
            table[i] = ~static_cast<T>(0);
        }
    }
    return table;
}();

// Determines if a left shift causes the most significant bit of the value to change
template <std::integral T>
FORCE_INLINE static bool IsLeftShiftOverflow(T value, T shift) {
    if (shift < sizeof(T) * 8) {
        value &= kShiftTable<T>[shift + 1];
        return value != 0 && value != kShiftTable<T>[shift + 1];
    } else {
        return value != 0;
    }
}

template <std::integral T, bool increment>
FORCE_INLINE void MC68EC000::AdvanceAddress(uint32 An) {
    sint32 amount = sizeof(T);
    if (An == 7 && sizeof(T) == 1) {
        // Turn byte-sized increments into word-sized increments if targeting SP
        amount = sizeof(uint16);
    }
    if constexpr (increment) {
        regs.A[An] += amount;
    } else {
        regs.A[An] -= amount;
    }
}

// -----------------------------------------------------------------------------
// Helper functions for rotates through X flag

template <typename T>
struct ROXOut {
    T value;
    bool X;
};

template <std::integral T>
FORCE_INLINE T shl(T val, T shift) {
    if (shift < sizeof(T) * 8) {
        return val << shift;
    } else {
        return 0;
    }
}

template <std::integral T>
FORCE_INLINE T shr(T val, T shift) {
    if (shift < sizeof(T) * 8) {
        return val >> shift;
    } else {
        return 0;
    }
}

template <typename T>
FORCE_INLINE ROXOut<T> roxl(T val, T shift, bool X) {
    constexpr auto numBits = sizeof(T) * 8 + 1u;
    shift %= numBits;
    return {
        .value = static_cast<T>(shl<T>(val, shift) | shr<T>(val, numBits - shift) | shl<T>(X, shift - 1u)),
        .X = (shift == 0) ? X : static_cast<bool>(shr<T>(val, numBits - shift - 1u) & 1),
    };
}

template <typename T>
FORCE_INLINE ROXOut<T> roxr(T val, T shift, bool X) {
    constexpr auto numBits = sizeof(T) * 8 + 1u;
    shift %= numBits;
    return {
        .value = static_cast<T>(shr<T>(val, shift) | shl<T>(val, numBits - shift) | shl<T>(X, numBits - shift - 1u)),
        .X = (shift == 0) ? X : static_cast<bool>(shr<T>(val, shift - 1u) & 1),
    };
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
    case OpcodeType::AddX_M: Instr_AddX_M(instr); break;
    case OpcodeType::AddX_R: Instr_AddX_R(instr); break;
    case OpcodeType::And_Dn_EA: Instr_And_Dn_EA(instr); break;
    case OpcodeType::And_EA_Dn: Instr_And_EA_Dn(instr); break;
    case OpcodeType::AndI_EA: Instr_AndI_EA(instr); break;
    case OpcodeType::Eor_Dn_EA: Instr_Eor_Dn_EA(instr); break;
    case OpcodeType::EorI_EA: Instr_EorI_EA(instr); break;
    case OpcodeType::Neg: Instr_Neg(instr); break;
    case OpcodeType::NegX: Instr_NegX(instr); break;
    case OpcodeType::Not: Instr_Not(instr); break;
    case OpcodeType::Or_Dn_EA: Instr_Or_Dn_EA(instr); break;
    case OpcodeType::Or_EA_Dn: Instr_Or_EA_Dn(instr); break;
    case OpcodeType::OrI_EA: Instr_OrI_EA(instr); break;
    case OpcodeType::Sub_Dn_EA: Instr_Sub_Dn_EA(instr); break;
    case OpcodeType::Sub_EA_Dn: Instr_Sub_EA_Dn(instr); break;
    case OpcodeType::SubA: Instr_SubA(instr); break;
    case OpcodeType::SubI: Instr_SubI(instr); break;
    case OpcodeType::SubQ_An: Instr_SubQ_An(instr); break;
    case OpcodeType::SubQ_EA: Instr_SubQ_EA(instr); break;
    case OpcodeType::SubX_M: Instr_SubX_M(instr); break;
    case OpcodeType::SubX_R: Instr_SubX_R(instr); break;

    case OpcodeType::BChg_I_Dn: Instr_BChg_I_Dn(instr); break;
    case OpcodeType::BChg_I_EA: Instr_BChg_I_EA(instr); break;
    case OpcodeType::BChg_R_Dn: Instr_BChg_R_Dn(instr); break;
    case OpcodeType::BChg_R_EA: Instr_BChg_R_EA(instr); break;
    case OpcodeType::BClr_I_Dn: Instr_BClr_I_Dn(instr); break;
    case OpcodeType::BClr_I_EA: Instr_BClr_I_EA(instr); break;
    case OpcodeType::BClr_R_Dn: Instr_BClr_R_Dn(instr); break;
    case OpcodeType::BClr_R_EA: Instr_BClr_R_EA(instr); break;
    case OpcodeType::BSet_I_Dn: Instr_BSet_I_Dn(instr); break;
    case OpcodeType::BSet_I_EA: Instr_BSet_I_EA(instr); break;
    case OpcodeType::BSet_R_Dn: Instr_BSet_R_Dn(instr); break;
    case OpcodeType::BSet_R_EA: Instr_BSet_R_EA(instr); break;
    case OpcodeType::BTst_I_Dn: Instr_BTst_I_Dn(instr); break;
    case OpcodeType::BTst_I_EA: Instr_BTst_I_EA(instr); break;
    case OpcodeType::BTst_R_Dn: Instr_BTst_R_Dn(instr); break;
    case OpcodeType::BTst_R_EA: Instr_BTst_R_EA(instr); break;

    case OpcodeType::ASL_I: Instr_ASL_I(instr); break;
    case OpcodeType::ASL_M: Instr_ASL_M(instr); break;
    case OpcodeType::ASL_R: Instr_ASL_R(instr); break;
    case OpcodeType::ASR_I: Instr_ASR_I(instr); break;
    case OpcodeType::ASR_M: Instr_ASR_M(instr); break;
    case OpcodeType::ASR_R: Instr_ASR_R(instr); break;
    case OpcodeType::LSL_I: Instr_LSL_I(instr); break;
    case OpcodeType::LSL_M: Instr_LSL_M(instr); break;
    case OpcodeType::LSL_R: Instr_LSL_R(instr); break;
    case OpcodeType::LSR_I: Instr_LSR_I(instr); break;
    case OpcodeType::LSR_M: Instr_LSR_M(instr); break;
    case OpcodeType::LSR_R: Instr_LSR_R(instr); break;
    case OpcodeType::ROL_I: Instr_ROL_I(instr); break;
    case OpcodeType::ROL_M: Instr_ROL_M(instr); break;
    case OpcodeType::ROL_R: Instr_ROL_R(instr); break;
    case OpcodeType::ROR_I: Instr_ROR_I(instr); break;
    case OpcodeType::ROR_M: Instr_ROR_M(instr); break;
    case OpcodeType::ROR_R: Instr_ROR_R(instr); break;
    case OpcodeType::ROXL_I: Instr_ROXL_I(instr); break;
    case OpcodeType::ROXL_M: Instr_ROXL_M(instr); break;
    case OpcodeType::ROXL_R: Instr_ROXL_R(instr); break;
    case OpcodeType::ROXR_I: Instr_ROXR_I(instr); break;
    case OpcodeType::ROXR_M: Instr_ROXR_M(instr); break;
    case OpcodeType::ROXR_R: Instr_ROXR_R(instr); break;

    case OpcodeType::Cmp: Instr_Cmp(instr); break;
    case OpcodeType::CmpA: Instr_CmpA(instr); break;
    case OpcodeType::CmpI: Instr_CmpI(instr); break;
    case OpcodeType::CmpM: Instr_CmpM(instr); break;
    case OpcodeType::Scc: Instr_Scc(instr); break;
    case OpcodeType::TAS: Instr_TAS(instr); break;
    case OpcodeType::Tst: Instr_Tst(instr); break;

    case OpcodeType::LEA: Instr_LEA(instr); break;
    case OpcodeType::PEA: Instr_PEA(instr); break;

    case OpcodeType::Link: Instr_Link(instr); break;
    case OpcodeType::Unlink: Instr_Unlink(instr); break;

    case OpcodeType::BRA: Instr_BRA(instr); break;
    case OpcodeType::BSR: Instr_BSR(instr); break;
    case OpcodeType::Bcc: Instr_Bcc(instr); break;
    case OpcodeType::DBcc: Instr_DBcc(instr); break;
    case OpcodeType::JSR: Instr_JSR(instr); break;
    case OpcodeType::Jmp: Instr_Jmp(instr); break;

    case OpcodeType::RTE: Instr_RTE(instr); break;
    case OpcodeType::RTR: Instr_RTR(instr); break;
    case OpcodeType::RTS: Instr_RTS(instr); break;

    case OpcodeType::Chk: Instr_Chk(instr); break;
    case OpcodeType::Reset: Instr_Reset(instr); break;
    case OpcodeType::Stop: Instr_Stop(instr); break;
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

    auto move = [&]<mem_primitive T>() {
        const T value = MoveEffectiveAddress<T>(srcM, srcXn, dstM, dstXn);
        SR.N = IsNegative(value);
        SR.Z = value == 0;
        SR.V = SR.C = 0;
    };

    // Note the swapped bit order between word and longword moves
    switch (size) {
    case 0b01: move.template operator()<uint8>(); break;
    case 0b11: move.template operator()<uint16>(); break;
    case 0b10: move.template operator()<uint32>(); break;
    }
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
    const bool isProgramAccess = M == 7 && (Xn == 2 || Xn == 3);

    auto read = [&]<std::integral T>() {
        return isProgramAccess ? MemRead<T, true>(address) : MemRead<T, false>(address);
    };

    auto xfer = [&]<std::integral T>(uint32 regIndex) {
        const T value = read.template operator()<T>();
        regs.DA[regIndex] = static_cast<std::make_signed_t<T>>(value);
        address += sizeof(T);
    };

    for (uint32 i = 0; i < 16; i++) {
        if (regList & (1u << i)) {
            sz ? xfer.template operator()<uint32>(i) : xfer.template operator()<uint16>(i);
        }
    }
    // An extra memory fetch occurs after the transfers are done
    read.template operator()<uint16>();

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_MoveM_PI_Rs(uint16 instr) {
    const uint16 An = bit::extract<0, 2>(instr);
    const bool sz = bit::extract<6>(instr);
    const uint16 regList = PrefetchNext();

    auto xfer = [&]<std::integral T>(uint32 regIndex) {
        const uint32 address = regs.A[An];
        const T value = MemRead<T, false>(address);
        regs.DA[regIndex] = static_cast<std::make_signed_t<T>>(value);
        regs.A[An] = address + sizeof(T);
    };

    for (uint32 i = 0; i < 16; i++) {
        if (regList & (1u << i)) {
            sz ? xfer.template operator()<uint32>(i) : xfer.template operator()<uint16>(i);
        }
    }
    // An extra memory fetch occurs after the transfers are done
    MemRead<uint16, false>(regs.A[An]);

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
        MemWriteAsc<T>(address, value);
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

    const uint32 baseAddress = regs.A[An];

    auto xfer = [&]<std::integral T>(uint32 regIndex) {
        const uint32 address = regs.A[An] - sizeof(T);
        const T value = 7 - regIndex == An ? baseAddress : regs.DA[15 - regIndex];
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
    SR.N = IsNegative(value);
    SR.Z = value == 0;
    SR.V = SR.C = 0;

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
    SR.N = IsNegative(value);
    SR.Z = value == 0;
    SR.V = SR.C = 0;

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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsAddOverflow(op1, op2, result);
            SR.C = SR.X = IsAddCarry(op1, op2, result);
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
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = IsAddOverflow(op1, op2, result);
        SR.C = SR.X = IsAddCarry(op1, op2, result);
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
        regs.A[An] += static_cast<sint16>(ReadEffectiveAddress<uint16>(M, Xn));
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsAddOverflow(op1, op2, result);
            SR.C = SR.X = IsAddCarry(op1, op2, result);

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
        const uint32 result = op2 + (op1 == 0 ? 8 : op1);
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsAddOverflow(op1, op2, result);
            SR.C = SR.X = IsAddCarry(op1, op2, result);

            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_AddX_M(uint16 instr) {
    const uint16 Ry = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Rx = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        AdvanceAddress<T, false>(Ry);
        const T op1 = MemReadDesc<T, false>(regs.A[Ry]);
        AdvanceAddress<T, false>(Rx);
        const T op2 = MemReadDesc<T, false>(regs.A[Rx]);
        const T result = op2 + op1 + SR.X;
        SR.N = IsNegative(result);
        SR.Z &= result == 0;
        SR.V = IsAddOverflow(op1, op2, result);
        SR.C = SR.X = IsAddCarry(op1, op2, result);

        if constexpr (std::is_same_v<T, uint32>) {
            MemWrite<uint16>(regs.A[Rx] + 2, result >> 0u);
            PrefetchTransfer();
            MemWrite<uint16>(regs.A[Rx] + 0, result >> 16u);
        } else {
            PrefetchTransfer();
            MemWrite<T>(regs.A[Rx], result);
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_AddX_R(uint16 instr) {
    const uint16 Ry = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Rx = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = regs.D[Ry];
        const T op2 = regs.D[Rx];
        const T result = op2 + op1 + SR.X;
        SR.N = IsNegative(result);
        SR.Z &= result == 0;
        SR.V = IsAddOverflow(op1, op2, result);
        SR.C = SR.X = IsAddCarry(op1, op2, result);
        bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[Rx], result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_And_Dn_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = regs.D[Dn];
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 & op1;
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_And_EA_Dn(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = ReadEffectiveAddress<T>(M, Xn);
        const T op2 = regs.D[Dn];
        const T result = op2 & op1;
        bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[Dn], result);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = SR.C = 0;
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_EorI_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        T op1 = PrefetchNext();
        if constexpr (sizeof(T) == sizeof(uint32)) {
            op1 = (op1 << 16u) | PrefetchNext();
        }
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 ^ op1;
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Neg(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        ModifyEffectiveAddress<T>(M, Xn, [&](T value) {
            const T result = 0 - value;
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = static_cast<std::make_signed_t<T>>(value & result) < 0;
            SR.C = SR.X = ~SR.Z;
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_NegX(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        ModifyEffectiveAddress<T>(M, Xn, [&](T value) {
            const T result = 0 - value - SR.X;
            SR.N = result >> (sizeof(T) * 8 - 1);
            SR.Z &= result == 0;
            SR.V = static_cast<std::make_signed_t<T>>(value & result) < 0;
            SR.X = SR.C = (value | result) >> (sizeof(T) * 8 - 1);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Not(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        ModifyEffectiveAddress<T>(M, Xn, [&](T value) {
            const T result = ~value;
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
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
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = SR.C = 0;
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Sub_Dn_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = regs.D[Dn];
        ModifyEffectiveAddress<T>(M, Xn, [&](T op2) {
            const T result = op2 - op1;
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsSubOverflow(op1, op2, result);
            SR.C = SR.X = IsSubCarry(op1, op2, result);
            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_Sub_EA_Dn(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = ReadEffectiveAddress<T>(M, Xn);
        const T op2 = regs.D[Dn];
        const T result = op2 - op1;
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = IsSubOverflow(op1, op2, result);
        SR.C = SR.X = IsSubCarry(op1, op2, result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_SubA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const bool sz = bit::extract<8>(instr);
    const uint16 An = bit::extract<9, 11>(instr);

    if (sz) {
        regs.A[An] -= ReadEffectiveAddress<uint32>(M, Xn);
    } else {
        regs.A[An] -= static_cast<sint16>(ReadEffectiveAddress<uint16>(M, Xn));
    }

    PrefetchTransfer();
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsSubOverflow(op1, op2, result);
            SR.C = SR.X = IsSubCarry(op1, op2, result);

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
        const uint32 result = op2 - (op1 == 0 ? 8 : op1);
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsSubOverflow(op1, op2, result);
            SR.C = SR.X = IsSubCarry(op1, op2, result);

            return result;
        });
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_SubX_M(uint16 instr) {
    const uint16 Ry = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Rx = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        AdvanceAddress<T, false>(Ry);
        const T op1 = MemReadDesc<T, false>(regs.A[Ry]);
        AdvanceAddress<T, false>(Rx);
        const T op2 = MemReadDesc<T, false>(regs.A[Rx]);
        const T result = op2 - op1 - SR.X;
        SR.N = IsNegative(result);
        SR.Z &= result == 0;
        SR.V = IsSubOverflow(op1, op2, result);
        SR.C = SR.X = IsSubCarry(op1, op2, result);

        if constexpr (std::is_same_v<T, uint32>) {
            MemWrite<uint16>(regs.A[Rx] + 2, result >> 0u);
            PrefetchTransfer();
            MemWrite<uint16>(regs.A[Rx] + 0, result >> 16u);
        } else {
            PrefetchTransfer();
            MemWrite<T>(regs.A[Rx], result);
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }
}

FORCE_INLINE void MC68EC000::Instr_SubX_R(uint16 instr) {
    const uint16 Ry = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Rx = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = regs.D[Ry];
        const T op2 = regs.D[Rx];
        const T result = op2 - op1 - SR.X;
        SR.N = IsNegative(result);
        SR.Z &= result == 0;
        SR.V = IsSubOverflow(op1, op2, result);
        SR.C = SR.X = IsSubCarry(op1, op2, result);
        bit::deposit_into<0, sizeof(T) * 8 - 1>(regs.D[Rx], result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BChg_I_Dn(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 index = PrefetchNext() & 31;

    const uint32 bit = 1 << index;
    const uint32 value = regs.D[Dn];
    SR.Z = (value & bit) == 0;
    regs.D[Dn] ^= bit;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BChg_I_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 index = PrefetchNext() & 7;

    const uint8 bit = 1 << index;
    ModifyEffectiveAddress<uint8>(M, Xn, [&](uint8 value) {
        SR.Z = (value & bit) == 0;
        return value ^ bit;
    });
}

FORCE_INLINE void MC68EC000::Instr_BChg_R_Dn(uint16 instr) {
    const uint16 dstDn = bit::extract<0, 2>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 31;

    const uint32 bit = 1 << index;
    const uint32 value = regs.D[dstDn];
    SR.Z = (value & bit) == 0;
    regs.D[dstDn] ^= bit;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BChg_R_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 7;

    const uint8 bit = 1 << index;
    ModifyEffectiveAddress<uint8>(M, Xn, [&](uint8 value) {
        SR.Z = (value & bit) == 0;
        return value ^ bit;
    });
}

FORCE_INLINE void MC68EC000::Instr_BClr_I_Dn(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 index = PrefetchNext() & 31;

    const uint32 bit = 1 << index;
    const uint32 value = regs.D[Dn];
    SR.Z = (value & bit) == 0;
    regs.D[Dn] &= ~bit;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BClr_I_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 index = PrefetchNext() & 7;

    const uint8 bit = 1 << index;
    ModifyEffectiveAddress<uint8>(M, Xn, [&](uint8 value) {
        SR.Z = (value & bit) == 0;
        return value & ~bit;
    });
}

FORCE_INLINE void MC68EC000::Instr_BClr_R_Dn(uint16 instr) {
    const uint16 dstDn = bit::extract<0, 2>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 31;

    const uint32 bit = 1 << index;
    const uint32 value = regs.D[dstDn];
    SR.Z = (value & bit) == 0;
    regs.D[dstDn] &= ~bit;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BClr_R_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 7;

    const uint8 bit = 1 << index;
    ModifyEffectiveAddress<uint8>(M, Xn, [&](uint8 value) {
        SR.Z = (value & bit) == 0;
        return value & ~bit;
    });
}

FORCE_INLINE void MC68EC000::Instr_BSet_I_Dn(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 index = PrefetchNext() & 31;

    const uint32 bit = 1 << index;
    const uint32 value = regs.D[Dn];
    SR.Z = (value & bit) == 0;
    regs.D[Dn] |= bit;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BSet_I_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 index = PrefetchNext() & 7;

    const uint8 bit = 1 << index;
    ModifyEffectiveAddress<uint8>(M, Xn, [&](uint8 value) {
        SR.Z = (value & bit) == 0;
        return value | bit;
    });
}

FORCE_INLINE void MC68EC000::Instr_BSet_R_Dn(uint16 instr) {
    const uint16 dstDn = bit::extract<0, 2>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 31;

    const uint32 bit = 1 << index;
    const uint32 value = regs.D[dstDn];
    SR.Z = (value & bit) == 0;
    regs.D[dstDn] |= bit;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BSet_R_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 7;

    const uint8 bit = 1 << index;
    ModifyEffectiveAddress<uint8>(M, Xn, [&](uint8 value) {
        SR.Z = (value & bit) == 0;
        return value | bit;
    });
}

FORCE_INLINE void MC68EC000::Instr_BTst_I_Dn(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 index = PrefetchNext() & 31;

    const uint32 value = regs.D[Dn];
    SR.Z = !((value >> index) & 1);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BTst_I_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 index = PrefetchNext() & 7;

    const uint8 value = ReadEffectiveAddress<uint8>(M, Xn);
    SR.Z = !((value >> index) & 1);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BTst_R_Dn(uint16 instr) {
    const uint16 dstDn = bit::extract<0, 2>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 31;

    const uint32 value = regs.D[dstDn];
    SR.Z = !((value >> index) & 1);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_BTst_R_EA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 srcDn = bit::extract<9, 11>(instr);
    const uint16 index = regs.D[srcDn] & 7;

    const uint8 value = ReadEffectiveAddress<uint8>(M, Xn);
    SR.Z = !((value >> index) & 1);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ASL_I(uint16 instr) {
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
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsLeftShiftOverflow<T>(value, shift);
            SR.C = SR.X = carry;
        } else {
            const T value = regs.D[Dn];
            const T result = value << shift;
            const bool carry = (value >> (sizeof(T) * 8 - shift)) & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsLeftShiftOverflow<T>(value, shift);
            SR.C = SR.X = carry;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ASL_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        const uint16 result = value << 1u;
        const bool carry = value >> 15u;
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = IsLeftShiftOverflow<uint16>(value, 1u);
        SR.C = SR.X = carry;
        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_ASL_R(uint16 instr) {
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
        if (shift != 0) {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = IsLeftShiftOverflow<T>(value, shift);
            SR.C = SR.X = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ASR_I(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    uint32 shift = bit::extract<9, 11>(instr);
    if (shift == 0) {
        shift = 8;
    }

    auto op = [&]<std::integral T>() {
        using ST = std::make_signed_t<T>;

        if (sizeof(T) == sizeof(uint8) && shift == 8) {
            const ST value = regs.D[Dn];
            const ST result = value >> 7;
            const bool carry = value >> 7;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
        } else {
            const ST value = regs.D[Dn];
            const ST result = value >> shift;
            const bool carry = (value >> (shift - 1)) & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ASR_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        const uint16 result = static_cast<sint16>(value) >> 1u;
        const bool carry = value & 1u;
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = SR.X = carry;

        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_ASR_R(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 shiftReg = bit::extract<9, 11>(instr);
    const uint32 shift = regs.D[shiftReg] & 63;

    auto op = [&]<std::integral T>() {
        using ST = std::make_signed_t<T>;

        const ST value = regs.D[Dn];
        ST result;
        bool carry;
        if (shift >= sizeof(T) * 8) {
            result = value >> (sizeof(T) * 8 - 1);
            carry = value >> (sizeof(T) * 8 - 1);
        } else if (shift != 0) {
            result = value >> shift;
            carry = (value >> (shift - 1)) & 1;
        } else {
            result = value;
            carry = false;
        }
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        if (shift != 0) {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
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
            const bool carry = value & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;

        } else {
            const T value = regs.D[Dn];
            const T result = value << shift;
            const bool carry = (value >> (sizeof(T) * 8 - shift)) & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
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
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = SR.X = carry;

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
        if (shift != 0) {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
        }
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
            const bool carry = value >> 7;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;

        } else {
            const T value = regs.D[Dn];
            const T result = value >> shift;
            const bool carry = (value >> (shift - 1)) & 1;
            bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
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
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = SR.X = carry;

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
        if (shift != 0) {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROL_I(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    uint32 shift = bit::extract<9, 11>(instr);
    if (shift == 0) {
        shift = 8;
    }

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        const T result = std::rotl(value, shift);
        const bool carry = result & 1;
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = carry;
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROL_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        const uint16 result = std::rotl(value, 1u);
        const bool carry = result & 1;
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = carry;

        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_ROL_R(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 shiftReg = bit::extract<9, 11>(instr);
    const uint32 shift = regs.D[shiftReg] & 63;

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        const T result = std::rotl(value, shift);
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        if (shift != 0) {
            const bool carry = result & 1;
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROR_I(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    uint32 shift = bit::extract<9, 11>(instr);
    if (shift == 0) {
        shift = 8;
    }

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        const T result = std::rotr(value, shift);
        const bool carry = result >> (sizeof(T) * 8 - 1);
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = carry;
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROR_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        const uint16 result = std::rotr(value, 1u);
        const bool carry = result >> 15;
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = carry;

        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_ROR_R(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 shiftReg = bit::extract<9, 11>(instr);
    const uint32 shift = regs.D[shiftReg] & 63;

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        const T result = std::rotr(value, shift);
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        if (shift != 0) {
            const bool carry = result >> (sizeof(T) * 8 - 1);
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = SR.C = 0;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROXL_I(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    uint32 shift = bit::extract<9, 11>(instr);
    if (shift == 0) {
        shift = 8;
    }

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        auto [result, carry] = roxl<T>(value, shift, SR.X);
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = SR.X = carry;
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROXL_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        auto [result, carry] = roxl<uint16>(value, 1u, SR.X);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = SR.X = carry;

        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_ROXL_R(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 shiftReg = bit::extract<9, 11>(instr);
    const uint32 shift = regs.D[shiftReg] & 63;

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        auto [result, carry] = roxl<T>(value, shift, SR.X);
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        if (shift != 0) {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X;
        }
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROXR_I(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    uint32 shift = bit::extract<9, 11>(instr);
    if (shift == 0) {
        shift = 8;
    }

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        auto [result, carry] = roxr<T>(value, shift, SR.X);
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = SR.X = carry;
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_ROXR_M(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    ModifyEffectiveAddress<uint16>(M, Xn, [&](uint16 value) {
        auto [result, carry] = roxr<uint16>(value, 1u, SR.X);
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = 0;
        SR.C = SR.X = carry;

        return result;
    });
}

FORCE_INLINE void MC68EC000::Instr_ROXR_R(uint16 instr) {
    const uint16 Dn = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 shiftReg = bit::extract<9, 11>(instr);
    const uint32 shift = regs.D[shiftReg] & 63;

    auto op = [&]<std::integral T>() {
        const T value = regs.D[Dn];
        auto [result, carry] = roxr<T>(value, shift, SR.X);
        bit::deposit_into<0, sizeof(T) * 8 - 1, uint32>(regs.D[Dn], result);
        if (shift != 0) {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X = carry;
        } else {
            SR.N = IsNegative(result);
            SR.Z = result == 0;
            SR.V = 0;
            SR.C = SR.X;
        }
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
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = IsSubOverflow(op1, op2, result);
        SR.C = IsSubCarry(op1, op2, result);
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
        const uint32 op1 = static_cast<std::make_signed_t<T>>(ReadEffectiveAddress<T>(M, Xn));
        const uint32 op2 = regs.A[An];
        const uint32 result = op2 - op1;
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = IsSubOverflow(op1, op2, result);
        SR.C = IsSubCarry(op1, op2, result);
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
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = IsSubOverflow(op1, op2, result);
        SR.C = IsSubCarry(op1, op2, result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_CmpM(uint16 instr) {
    const uint16 Ay = bit::extract<0, 2>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);
    const uint16 Ax = bit::extract<9, 11>(instr);

    auto op = [&]<std::integral T>() {
        const T op1 = MemRead<T, false>(regs.A[Ay]);
        AdvanceAddress<T, true>(Ay);
        const T op2 = MemRead<T, false>(regs.A[Ax]);
        AdvanceAddress<T, true>(Ax);
        const T result = op2 - op1;
        SR.N = IsNegative(result);
        SR.Z = result == 0;
        SR.V = IsSubOverflow(op1, op2, result);
        SR.C = IsSubCarry(op1, op2, result);
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Scc(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint32 cond = bit::extract<8, 11>(instr);

    const uint8 value = kCondTable[(cond << 4u) | SR.flags] * 0xFF;
    ModifyEffectiveAddress<uint8>(M, Xn, [&](uint8) { return value; });
}

FORCE_INLINE void MC68EC000::Instr_TAS(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    // NOTE: this should be indivisible
    ModifyEffectiveAddress<uint8, false>(M, Xn, [&](uint8 value) {
        SR.N = IsNegative(value);
        SR.Z = value == 0;
        SR.V = SR.C = 0;
        return value | 0x80;
    });

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Tst(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 sz = bit::extract<6, 7>(instr);

    auto op = [&]<std::integral T>() {
        const T value = ReadEffectiveAddress<T>(M, Xn);
        SR.N = IsNegative(value);
        SR.Z = value == 0;
        SR.V = SR.C = 0;
    };

    switch (sz) {
    case 0b00: op.template operator()<uint8>(); break;
    case 0b01: op.template operator()<uint16>(); break;
    case 0b10: op.template operator()<uint32>(); break;
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_LEA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 An = bit::extract<9, 11>(instr);

    regs.A[An] = CalcEffectiveAddress(M, Xn);

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_PEA(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    const uint32 address = CalcEffectiveAddress(M, Xn);
    AdvanceAddress<uint32, false>(7);
    if (M == 7 && Xn <= 1) {
        MemWriteAsc<uint32>(regs.SP, address);
        PrefetchTransfer();
    } else {
        PrefetchTransfer();
        MemWriteAsc<uint32>(regs.SP, address);
    }
}

FORCE_INLINE void MC68EC000::Instr_Link(uint16 instr) {
    const uint16 An = bit::extract<0, 2>(instr);
    const sint16 disp = PrefetchNext();

    MemWriteAsc<uint32>(regs.SP - 4, regs.A[An]);
    regs.SP -= 4;
    regs.A[An] = regs.SP;
    regs.SP += disp;

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Unlink(uint16 instr) {
    const uint16 An = bit::extract<0, 2>(instr);

    regs.SP = regs.A[An];
    regs.A[An] = MemRead<uint32, false>(regs.SP);
    if (An != 7) {
        regs.SP += 4;
    }

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
    const bool longDisp = disp == 0;
    if (longDisp) {
        disp = static_cast<sint16>(m_prefetchQueue[0]);
        PC += 2;
    }

    regs.SP -= 4;
    MemWrite<uint16>(regs.SP + 0, (PC - 2) >> 16u);
    MemWrite<uint16>(regs.SP + 2, (PC - 2) >> 0u);
    PC = currPC + disp;
    FullPrefetch();
}

FORCE_INLINE void MC68EC000::Instr_Bcc(uint16 instr) {
    const uint32 currPC = PC - 2;
    sint16 disp = static_cast<sint8>(bit::extract<0, 7>(instr));
    const bool longDisp = disp == 0;
    if (longDisp) {
        disp = static_cast<sint16>(m_prefetchQueue[0]);
    }
    const uint32 cond = bit::extract<8, 11>(instr);
    if (kCondTable[(cond << 4u) | SR.flags]) {
        PC = currPC + disp;
        FullPrefetch();
        return;
    } else if (longDisp) {
        PrefetchNext();
    }

    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_DBcc(uint16 instr) {
    const uint32 currPC = PC - 2;
    const uint32 Dn = bit::extract<0, 2>(instr);
    const uint32 cond = bit::extract<8, 11>(instr);
    const sint16 disp = static_cast<sint16>(m_prefetchQueue[0]);

    if (!kCondTable[(cond << 4u) | SR.flags]) {
        const uint16 value = regs.D[Dn] - 1;
        regs.D[Dn] = (regs.D[Dn] & 0xFFFF0000) | value;
        if (value != 0xFFFFu) {
            PC = currPC + disp;
        }
    }

    FullPrefetch();
}

FORCE_INLINE void MC68EC000::Instr_JSR(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    const uint32 target = CalcEffectiveAddress<false>(M, Xn);
    const uint32 currPC = M == 2 ? PC - 2 : PC;

    regs.SP -= 4;
    PC = target;
    PrefetchNext();
    MemWrite<uint16>(regs.SP + 0, currPC >> 16u);
    MemWrite<uint16>(regs.SP + 2, currPC >> 0u);
    PrefetchTransfer();
}

FORCE_INLINE void MC68EC000::Instr_Jmp(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);

    const uint32 target = CalcEffectiveAddress<false>(M, Xn);
    PC = target;
    FullPrefetch();
}

FORCE_INLINE void MC68EC000::Instr_RTE(uint16 instr) {
    PC -= 2;
    if (CheckPrivilege()) {
        uint32 SP = regs.SP;
        SetSR(MemRead<uint16, false>(SP));
        SP += 2;
        PC = MemRead<uint32, false>(SP);
        FullPrefetch();
        SP += 4;
        SetSSP(SP);
    }
}

FORCE_INLINE void MC68EC000::Instr_RTR(uint16 instr) {
    SR.xflags = MemRead<uint16, false>(regs.SP);
    regs.SP += 2;
    PC = MemRead<uint32, false>(regs.SP);
    FullPrefetch();
    regs.SP += 4;
}

FORCE_INLINE void MC68EC000::Instr_RTS(uint16 instr) {
    PC = MemRead<uint32, false>(regs.SP);
    FullPrefetch();
    regs.SP += 4;
}

FORCE_INLINE void MC68EC000::Instr_Chk(uint16 instr) {
    const uint16 Xn = bit::extract<0, 2>(instr);
    const uint16 M = bit::extract<3, 5>(instr);
    const uint16 Dn = bit::extract<9, 11>(instr);

    const sint16 upperBound = ReadEffectiveAddress<uint16>(M, Xn);
    const sint16 value = regs.D[Dn];
    SR.Z = value == 0; // undocumented
    SR.V = 0;          // undefined
    SR.C = 0;          // undefined
    if (value < 0 || value > upperBound) {
        SR.N = (value < 0);
        PC -= 2;
        EnterException(ExceptionVector::CHKInstruction);
    } else {
        SR.N = 0;
        PrefetchTransfer();
    }
}

FORCE_INLINE void MC68EC000::Instr_Reset(uint16 instr) {
    PC -= 2;
    if (CheckPrivilege()) {
        PC += 2;
        // TODO: assert RESET signal, causing all external devices to be reset
        PrefetchTransfer();
    }
}

FORCE_INLINE void MC68EC000::Instr_Stop(uint16 instr) {
    PC -= 2;
    if (CheckPrivilege()) {
        PC += 2;
        SetSR(m_prefetchQueue[0]);
        // TODO: stop CPU; should resume when a trace, interrupt or reset exception occurs
    }
}

FORCE_INLINE void MC68EC000::Instr_Trap(uint16 instr) {
    PC -= 2;
    const uint8 vector = bit::extract<0, 3>(instr);
    EnterException(static_cast<ExceptionVector>(0x20 + vector));
}

FORCE_INLINE void MC68EC000::Instr_TrapV(uint16 instr) {
    if (SR.V) {
        PrefetchNext();
        PC -= 4;
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
    PC -= 4;
    EnterException(ExceptionVector::Line1010Emulator);
}

FORCE_INLINE void MC68EC000::Instr_Illegal1111(uint16 instr) {
    PC -= 4;
    EnterException(ExceptionVector::Line1111Emulator);
}

} // namespace satemu::m68k

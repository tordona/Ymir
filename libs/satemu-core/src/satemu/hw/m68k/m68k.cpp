#include <satemu/hw/m68k/m68k.hpp>

#include <satemu/hw/scsp/scsp.hpp>

namespace satemu::m68k {

MC68EC000::MC68EC000(scsp::SCSP &bus)
    : m_bus(bus) {
    Reset(true);
}

void MC68EC000::Reset(bool hard) {
    // TODO: check reset values

    D.fill(0);
    A.fill(0);
    A[7] = m_bus.Read<uint32>(0x00000000);
    SP_swap = 0;

    PC = m_bus.Read<uint32>(0x00000004);

    CCR.u16 = 0;
}

void MC68EC000::Step() {
    Execute(PC);
}

template <mem_access_type T, bool instrFetch>
T MC68EC000::MemRead(uint32 address) {
    return m_bus.Read<T>(address);
}

template <mem_access_type T>
void MC68EC000::MemWrite(uint32 address, T value) {
    m_bus.Write<T>(address, value);
}

FLATTEN FORCE_INLINE uint16 MC68EC000::FetchInstruction(uint32 address) {
    return MemRead<uint16, true>(address);
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

void MC68EC000::Execute(uint32 address) {
    auto fetch = [&] {
        const uint16 instr = FetchInstruction(address);
        address += 2;
        return instr;
    };

    const uint16 instr = fetch();
    switch (instr >> 12u) {
    case 0x0: break;
    case 0x1: break;
    case 0x2: break;
    case 0x3: break;
    case 0x4: break;
    case 0x5: break;
    case 0x6: break;
    case 0x7: break;
    case 0x8: break;
    case 0x9: break;
    case 0xA: break;
    case 0xB: break;
    case 0xC: break;
    case 0xD: break;
    case 0xE: break;
    case 0xF: break;
    }

    PC += 2;
}

} // namespace satemu::m68k

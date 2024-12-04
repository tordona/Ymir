#include <satemu/hw/m68k/m68k.hpp>

#include <satemu/hw/m68k/m68k_bus.hpp>

namespace satemu::m68k {

MC68EC000::MC68EC000(M68kBus &bus)
    : m_bus(bus) {
    Reset(true);
}

void MC68EC000::Reset(bool hard) {
    // TODO: check reset values

    D.fill(0);
    A.fill(0);
    SP_swap = 0;

    PC = 0;

    CCR.u16 = 0;
}

void MC68EC000::Step() {}

} // namespace satemu::m68k

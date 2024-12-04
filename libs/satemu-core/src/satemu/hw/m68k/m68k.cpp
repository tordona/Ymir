#include <satemu/hw/m68k/m68k.hpp>

#include <satemu/hw/m68k/m68k_bus.hpp>

namespace satemu::m68k {

M68EC000::M68EC000(M68kBus &bus)
    : m_bus(bus) {
    Reset(true);
}

void M68EC000::Reset(bool hard) {}

void M68EC000::Step() {}

} // namespace satemu::m68k

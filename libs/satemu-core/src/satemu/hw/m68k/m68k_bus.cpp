#include <satemu/hw/m68k/m68k_bus.hpp>

namespace satemu::m68k {

M68kBus::M68kBus() {
    Reset();
}

void M68kBus::Reset() {
    m_WRAM.fill(0);
}

} // namespace satemu::m68k

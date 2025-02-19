#include <satemu/debug/debug_probe_sh2.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::debug {

SH2DebugProbe::SH2DebugProbe(sh2::SH2 &sh2)
    : m_sh2(sh2) {}

const std::array<uint32, 16> &SH2DebugProbe::GetGPRs() const {
    return m_sh2.R;
}

uint32 SH2DebugProbe::GetPC() const {
    return m_sh2.PC;
}

} // namespace satemu::debug

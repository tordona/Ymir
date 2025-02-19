#include <satemu/debug/debug_probe.hpp>

#include <satemu/debug/debug_probe_sh2.hpp>

#include <satemu/sys/saturn.hpp>

namespace satemu::debug {

struct DebugProbe::Impl {
    Impl(Saturn &saturn)
        : masterSH2(saturn.SH2.master)
        , slaveSH2(saturn.SH2.slave) {}

    SH2DebugProbe masterSH2;
    SH2DebugProbe slaveSH2;
};

DebugProbe::DebugProbe(Saturn &saturn)
    : m_impl(std::make_unique<Impl>(saturn)) {}

DebugProbe::~DebugProbe() = default;

SH2DebugProbe &DebugProbe::GetMasterSH2() {
    return m_impl->masterSH2;
}

SH2DebugProbe &DebugProbe::GetSlaveSH2() {
    return m_impl->slaveSH2;
}

} // namespace satemu::debug

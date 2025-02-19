#include <satemu/debug/debug_probe.hpp>

#include <satemu/sys/saturn.hpp>

namespace satemu::debug {

DebugProbe::DebugProbe(Saturn &saturn)
    : masterSH2(saturn.SH2.master)
    , slaveSH2(saturn.SH2.slave) {}

} // namespace satemu::debug

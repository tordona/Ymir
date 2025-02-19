#pragma once

#include <memory>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu {

struct Saturn;

} // namespace satemu

// -----------------------------------------------------------------------------

namespace satemu::debug {

struct SH2DebugProbe;

// Grants controlled access to the internal state of the emulator.
struct DebugProbe {
    DebugProbe(Saturn &saturn);
    ~DebugProbe();

    SH2DebugProbe &GetMasterSH2();
    SH2DebugProbe &GetSlaveSH2();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace satemu::debug

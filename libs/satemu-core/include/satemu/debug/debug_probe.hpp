#pragma once

#include "debug_probe_sh2.hpp"

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu {

struct Saturn;

} // namespace satemu

// -----------------------------------------------------------------------------

namespace satemu::debug {

// Grants controlled access to the internal state of the emulator.
struct DebugProbe {
    DebugProbe(Saturn &saturn);

    SH2DebugProbe masterSH2;
    SH2DebugProbe slaveSH2;
};

} // namespace satemu::debug

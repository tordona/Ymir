#pragma once

#include <satemu/core/types.hpp>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::sh2 {

class SH2;

} // namespace satemu::sh2

// -----------------------------------------------------------------------------

namespace satemu::debug {

// Grants access to the internal state of an SH-2 processor.
struct SH2DebugProbe {
    SH2DebugProbe(sh2::SH2 &sh2);

    const std::array<uint32, 16> &GetGPRs() const;

    uint32 GetPC() const;

private:
    sh2::SH2 &m_sh2;
};

} // namespace satemu::debug

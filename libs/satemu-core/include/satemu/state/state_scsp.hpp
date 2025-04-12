#pragma once

#include "state_m68k.hpp"

#include <satemu/core/types.hpp>

namespace satemu::state {

inline namespace v1 {

    struct SCSPState {
        // TODO

        M68KState m68k;
    };

} // namespace v1

} // namespace satemu::state

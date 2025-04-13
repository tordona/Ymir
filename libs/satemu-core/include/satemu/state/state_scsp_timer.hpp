#pragma once

#include <satemu/core/types.hpp>

namespace satemu::state {

inline namespace v1 {

    struct SCSPTimer {
        uint8 incrementInterval;
        uint8 reload;

        bool doReload;
        uint8 counter;
    };

} // namespace v1

} // namespace satemu::state

#pragma once

#include <ymir/core/types.hpp>

namespace ymir::state {

struct SCSPTimer {
    uint8 incrementInterval;
    uint8 reload;

    bool doReload;
    uint8 counter;
};

} // namespace ymir::state

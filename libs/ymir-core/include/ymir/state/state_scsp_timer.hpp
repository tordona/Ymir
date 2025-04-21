#pragma once

#include <ymir/core/types.hpp>

namespace ymir::state {

namespace v1 {

    struct SCSPTimer {
        uint8 incrementInterval;
        uint8 reload;

        bool doReload;
        uint8 counter;
    };

} // namespace v1

inline namespace v2 {

    using v1::SCSPTimer;

} // namespace v2

} // namespace ymir::state

#pragma once

#include <ymir/core/types.hpp>

#include <array>

namespace ymir::state {

namespace v1 {

    struct M68KState {
        alignas(16) std::array<uint32, 8 + 8> DA;
        uint32 SP_swap;
        uint32 PC;
        uint16 SR;

        std::array<uint16, 2> prefetchQueue;
        uint8 extIntrLevel;
    };

} // namespace v1

namespace v2 {

    using v1::M68KState;

} // namespace v2

inline namespace v3 {

    using v2::M68KState;

} // namespace v3

} // namespace ymir::state

#pragma once

#include <satemu/core/types.hpp>

#include <array>

namespace satemu::state {

inline namespace v1 {

    struct M68KState {
        std::array<uint32, 8 + 8> DA;
        uint32 SP_swap;
        uint32 PC;
        uint16 SR;

        std::array<uint16, 2> prefetchQueue;
        uint8 extIntrLevel;
    };

} // namespace v1

} // namespace satemu::state

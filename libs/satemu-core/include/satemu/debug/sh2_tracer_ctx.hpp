#pragma once

#include "sh2_tracer.hpp"

#include <concepts>
#include <memory>

namespace satemu::debug {

// Holds an SH2 tracer and simplifies tracer usage.
struct SH2TracerContext {
    // Instantiates the specified tracer with the arguments passed to its constructor.
    template <std::derived_from<debug::ISH2Tracer> T, typename... Args>
    void Use(Args &&...args) {
        m_tracer = std::make_unique<T>(std::forward<Args>(args)...);
    }

    // Frees the tracer.
    void Clear() {
        m_tracer.reset();
    }

    template <bool debug>
    void Interrupt(uint8 vecNum, uint8 level) {
        if constexpr (debug) {
            if (m_tracer) {
                return m_tracer->Interrupt(vecNum, level);
            }
        }
    }

private:
    std::unique_ptr<ISH2Tracer> m_tracer;
};

} // namespace satemu::debug

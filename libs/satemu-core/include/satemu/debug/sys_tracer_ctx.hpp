#pragma once

#include "sys_tracer.hpp"

#include <concepts>
#include <memory>

namespace satemu::debug {

// Holds a system tracer and simplifies tracer usage.
struct SystemTracerContext {
    // Instantiates the specified tracer with the arguments passed to its constructor.
    template <std::derived_from<debug::ISystemTracer> T, typename... Args>
    void Use(Args &&...args) {
        m_tracer = std::make_unique<T>(std::forward<Args>(args)...);
    }

    // Frees the tracer.
    void Clear() {
        m_tracer.reset();
    }

private:
    std::unique_ptr<ISystemTracer> m_tracer;
};

} // namespace satemu::debug

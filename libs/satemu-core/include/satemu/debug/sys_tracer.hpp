#pragma once

namespace satemu::debug {

// Interface for system tracers.
//
// Must be implemented by users of the core library and instantiated with the `Use` method of the `SystemTracerContext`
// instance in `satemu::Saturn`.
struct ISystemTracer {
    virtual ~ISystemTracer() = default;
};

} // namespace satemu::debug

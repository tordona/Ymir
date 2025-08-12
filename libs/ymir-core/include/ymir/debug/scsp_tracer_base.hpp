#pragma once

/**
@file
@brief Defines `ymir::debug::ISCSPTracer`, the SCSP tracer interface.
*/

#include <ymir/core/types.hpp>

namespace ymir::debug {

/// @brief Interface for SCSP tracers.
///
/// Must be implemented by users of the core library.
///
/// Attach to an instance of `ymir::cdblock::CDBlock` with its `UseTracer(ISCSPTracer *)` method.
struct ISCSPTracer {
    /// @brief Default virtual destructor. Required for inheritance.
    virtual ~ISCSPTracer() = default;

    /// @brief Invoked when the SCSP outputs a sample.
    /// @param[in] left the left channel of the final output sample
    /// @param[in] right the right channel of the final output sample
    virtual void Sample(sint16 left, sint16 right) {}
};

} // namespace ymir::debug

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

    /// @brief Invoked for each slot when the SCSP outputs a sample.
    /// @param[in] index the slot index
    /// @param[in] left the left channel of the final output sample
    /// @param[in] right the right channel of the final output sample
    // TODO: void SlotSample(uint32 index, sint16 left, sint16 right) final;
};

} // namespace ymir::debug

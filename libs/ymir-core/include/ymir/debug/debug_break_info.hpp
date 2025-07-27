#pragma once

/**
@file
@brief Debug break information.
*/

#include <ymir/core/types.hpp>

namespace ymir::debug {

/// @brief Describes a debug break event.
///
/// This is a tagged union -- `event` is the tag, and `details` is the union. Each event indicates its valid field.
struct DebugBreakInfo {
    /// @brief The event that raised the debug break signal.
    enum class Event {
        /// @brief An SH-2 CPU hit a breakpoint.
        /// The `DebugBreakInfo::details::sh2Breakpoint` field has details about this event.
        SH2Breakpoint,
    } event;

    /// @brief Details about the event.
    union Details {
        /// @brief Details about the `Event::SH2Breakpoint` event.
        struct SH2Breakpoint {
            bool master; ///< Triggered from the MSH2 (`true`) or SSH2 (`false`)
            uint32 pc;   ///< PC address of the breakpoint
        } sh2Breakpoint;
    } details;

    /// @brief Constructs a `DebugBreakInfo` for an SH-2 breakpoint hit.
    /// @param[in] master whether the event was triggered from the MSH2 (`true`) or SSH2 (`false`) CPU
    /// @param[in] pc the PC address of the breakpoint
    /// @return a `DebugBreakInfo` struct with the `Event::SH2Breakpoint` event
    static DebugBreakInfo SH2Breakpoint(bool master, uint32 pc) {
        return DebugBreakInfo{.event = Event::SH2Breakpoint,
                              .details = {.sh2Breakpoint = {.master = master, .pc = pc}}};
    }
};

} // namespace ymir::debug

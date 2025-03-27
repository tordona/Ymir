#pragma once

#include <satemu/util/dev_log.hpp>

namespace satemu::scu::grp {

// -----------------------------------------------------------------------------
// Dev log groups

// Hierarchy:
//
// base
//   regs
//   dma
//   debug
//   dsp

struct base {
    static constexpr bool enabled = true;
    static constexpr devlog::Level level = devlog::level::debug;
    static constexpr std::string_view name = "SCU";
};

struct regs : public base {
    static constexpr std::string_view name = "SCU-Regs";
};

struct dma : public base {
    static constexpr std::string_view name = "SCU-DMA";
};

struct debug : public base {
    static constexpr std::string_view name = "SCU-Debug";
};

struct dsp : public base {
    static constexpr std::string_view name = "SCU-DSP";
};

} // namespace satemu::scu::grp

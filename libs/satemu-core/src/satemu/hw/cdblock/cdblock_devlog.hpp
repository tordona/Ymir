#pragma once

#include <satemu/util/dev_log.hpp>

namespace satemu::cdblock::grp {

// -----------------------------------------------------------------------------
// Dev log groups

// Hierarchy:
//
// base
//   regs
//   play_init
//   play
//   xfer
//   part_mgr

struct base {
    static constexpr bool enabled = true;
    static constexpr devlog::Level level = devlog::level::debug;
    static constexpr std::string_view name = "CDBlock";
};

struct regs : public base {
    static constexpr std::string_view name = "CDBlock-Regs";
};

struct play_init : public base {
    static constexpr std::string_view name = "CDBlock-PlayInit";
};

struct play : public base {
    static constexpr std::string_view name = "CDBlock-Play";
};

struct xfer : public base {
    static constexpr std::string_view name = "CDBlock-Transfer";
};

struct part_mgr : public base {
    static constexpr std::string_view name = "CDBlock-PartMgr";
};

} // namespace satemu::cdblock::grp

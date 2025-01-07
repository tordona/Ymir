#pragma once

#include <satemu/core_types.hpp>

#include <fmt/format.h>

#include <string>

namespace dbg {

// -----------------------------------------------------------------------------
// Debug levels

using Level = uint32;

namespace level {
    inline constexpr Level trace = 1;
    inline constexpr Level debug = 2;
    inline constexpr Level info = 3;
    inline constexpr Level warn = 4;
    inline constexpr Level error = 5;

    template <Level level>
    inline constexpr const char *name = "unk";

    template <>
    inline constexpr const char *name<trace> = "trace";
    template <>
    inline constexpr const char *name<debug> = "debug";
    template <>
    inline constexpr const char *name<info> = "info";
    template <>
    inline constexpr const char *name<warn> = "warn";
    template <>
    inline constexpr const char *name<error> = "error";
} // namespace level

// -----------------------------------------------------------------------------
// Configuration

// Global debug level
// Log messages with level >= debugLevel will be printed.
inline constexpr Level debugLevel = level::debug;

// -----------------------------------------------------------------------------
// Debug functions

namespace detail {

    template <Level level, typename... T>
    constexpr void print_raw(fmt::format_string<T...> fmt, T &&...args) {
        if constexpr (level >= debugLevel) {
            fmt::print(fmt, static_cast<T &&>(args)...);
        }
    }

    template <Level level>
    constexpr void print_level() {
        if constexpr (level >= debugLevel) {
            fmt::print("{:5s} | ", level::name<level>);
        }
    }

} // namespace detail

template <Level level, typename... T>
constexpr void print(fmt::format_string<T...> fmt, T &&...args) {
    if constexpr (level >= debugLevel) {
        detail::print_level<level>();
        fmt::println(fmt, static_cast<T &&>(args)...);
    }
}

// Convenience functions that log with a specific level

template <typename... T>
constexpr void trace(fmt::format_string<T...> fmt, T &&...args) {
    print<level::trace>(fmt, static_cast<T &&>(args)...);
}

template <typename... T>
constexpr void debug(fmt::format_string<T...> fmt, T &&...args) {
    print<level::debug>(fmt, static_cast<T &&>(args)...);
}

template <typename... T>
constexpr void info(fmt::format_string<T...> fmt, T &&...args) {
    print<level::info>(fmt, static_cast<T &&>(args)...);
}

template <typename... T>
constexpr void warn(fmt::format_string<T...> fmt, T &&...args) {
    print<level::warn>(fmt, static_cast<T &&>(args)...);
}

template <typename... T>
constexpr void error(fmt::format_string<T...> fmt, T &&...args) {
    print<level::error>(fmt, static_cast<T &&>(args)...);
}

// -----------------------------------------------------------------------------
// Debug categories

struct Category {
    constexpr Category(const char *name, bool enabled = true)
        : name(name)
        , enabled(enabled) {}

    constexpr Category(const Category &parent, const char *name, bool enabled = true)
        : name(name)
        , parent(&parent)
        , enabled(enabled) {}

    const char *const name;
    const Category *const parent = nullptr;

    bool enabled = true;

    bool Enabled() const {
        if (parent != nullptr && !parent->Enabled()) {
            return false;
        }
        return enabled;
    }

    std::string Name() const {
        if (parent == nullptr) {
            return name;
        } else {
            return fmt::format("{}-{}", parent->Name(), name);
        }
    }

    template <Level level, typename... T>
    constexpr void print(fmt::format_string<T...> fmt, T &&...args) const {
        if constexpr (level >= debugLevel) {
            if (Enabled()) {
                detail::print_level<level>();
                detail::print_raw<level>("{:16s} | ", Name());
                fmt::println(fmt, static_cast<T &&>(args)...);
            }
        }
    }

    // Convenience functions that log with a specific level

    template <typename... T>
    constexpr void trace(fmt::format_string<T...> fmt, T &&...args) const {
        print<level::trace>(fmt, static_cast<T &&>(args)...);
    }

    template <typename... T>
    constexpr void debug(fmt::format_string<T...> fmt, T &&...args) const {
        print<level::debug>(fmt, static_cast<T &&>(args)...);
    }

    template <typename... T>
    constexpr void info(fmt::format_string<T...> fmt, T &&...args) const {
        print<level::info>(fmt, static_cast<T &&>(args)...);
    }

    template <typename... T>
    constexpr void warn(fmt::format_string<T...> fmt, T &&...args) const {
        print<level::warn>(fmt, static_cast<T &&>(args)...);
    }

    template <typename... T>
    constexpr void error(fmt::format_string<T...> fmt, T &&...args) const {
        print<level::error>(fmt, static_cast<T &&>(args)...);
    }
};
// TODO: subcategories? tree of categories?

namespace cat {
    inline constexpr Category MSH2{"SH2-M"};
    inline constexpr Category SSH2{"SH2-S"};
    inline constexpr Category SCU{"SCU"};
    inline constexpr Category SCSP{"SCSP"};
    inline constexpr Category M68K{"M68K"};
    inline constexpr Category VDP1{"VDP1"};
    inline constexpr Category VDP2{"VDP2"};

    inline constexpr Category CDBlock{"CDBlock"};
    inline constexpr Category CDBlockRegs{CDBlock, "Regs"};
    inline constexpr Category CDBlockPlayInit{CDBlock, "PlayInit"};
    inline constexpr Category CDBlockPlay{CDBlock, "Play"};
    inline constexpr Category CDBlockXfer{CDBlock, "Transfer"};
    inline constexpr Category CDBlockPartMgr{CDBlock, "PartMgr"};
} // namespace cat

} // namespace dbg

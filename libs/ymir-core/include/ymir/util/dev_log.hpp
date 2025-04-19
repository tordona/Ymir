#pragma once

// A simple logging mechanism to aid development.
// Compile-time enable/disable flags to ensure optimal performance when these logs are disabled.
//
// Not meant to be used for user logs.
//
// Usage:
// 1. Define groups
//
//  namespace grp {
//      // Simple group
//      struct base {
//          static constexpr bool enabled = true;
//          static constexpr std::string_view name = "Group";
//      };
//
//      // Inherit rules from another group
//      struct child : public base {
//          static constexpr std::string_view name = "Child"; // override what you need
//      };
//
//      // Dynamically named group
//      struct dynamic_name {
//          static constexpr bool enabled = true;
//          static constexpr std::string_view Name(std::string_view arg) {
//              return fmt::format("{}-Group", arg);
//          }
//      };
//  }
//
// 2. Log
//     devlog::debug<grp::base>("Executing");
//     devlog::info<grp::child>("Uses {{fmt}} formatting: {} {:X}", 123, 0x456);
//     devlog::warn<grp::dynamic_name>("X", "This logs as X-Group");
//
// - Log values with complex computation if category is enabled
//     if constexpr (devlog::trace_enabled<grp::base>) {
//         auto result = ...; // do complex calculation
//         dev::trace<grp::base>("Complex result: {}", result);
//     }

#include <fmt/format.h>

#include <concepts>
#include <string>

namespace devlog {

// Globally enable or disable dev logging.
inline constexpr bool globalEnable = Ymir_ENABLE_DEVLOG;

// -----------------------------------------------------------------------------
// Log levels

using Level = uint32;

namespace level {
    inline constexpr Level trace = 1;
    inline constexpr Level debug = 2;
    inline constexpr Level info = 3;
    inline constexpr Level warn = 4;
    inline constexpr Level error = 5;
    inline constexpr Level off = 6; // should never be used to log things

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

namespace detail {

    template <typename T>
    concept Group = requires() {
        requires std::same_as<std::decay_t<decltype(T::enabled)>, bool>;
        requires std::same_as<std::decay_t<decltype(T::level)>, Level>;
    };

    template <typename T>
    concept StaticNameGroup = requires() {
        requires Group<T>;
        requires std::same_as<std::decay_t<decltype(T::name)>, std::string_view>;
    };

    template <typename T>
    concept DynamicNameGroup = requires() {
        requires Group<T>;
        { T::Name(std::declval<std::string_view>()) } -> std::same_as<std::string>;
    };

    template <Level level, detail::Group TGroup>
    inline constexpr bool enabled = globalEnable && TGroup::enabled && level >= TGroup::level;

    template <Level level, StaticNameGroup TGroup, typename... TArgs>
    constexpr void log(fmt::format_string<TArgs...> fmt, TArgs &&...args) {
        static_assert(level < level::off);
        if constexpr (enabled<level, TGroup>) {
            fmt::print("{:5s} | {:16s} | ", level::name<level>, TGroup::name);
            fmt::println(fmt, static_cast<TArgs &&>(args)...);
        }
    }

    template <Level level, DynamicNameGroup TGroup, typename... TArgs>
    constexpr void log(std::string_view nameArgs, fmt::format_string<TArgs...> fmt, TArgs &&...args) {
        static_assert(level < level::off);
        if constexpr (enabled<level, TGroup>) {
            fmt::print("{:5s} | {:16s} | ", level::name<level>, TGroup::Name(nameArgs));
            fmt::println(fmt, static_cast<TArgs &&>(args)...);
        }
    }

} // namespace detail

template <detail::Group TGroup>
inline constexpr bool trace_enabled = detail::enabled<level::trace, TGroup>;

template <detail::Group TGroup>
inline constexpr bool debug_enabled = detail::enabled<level::debug, TGroup>;

template <detail::Group TGroup>
inline constexpr bool info_enabled = detail::enabled<level::info, TGroup>;

template <detail::Group TGroup>
inline constexpr bool warn_enabled = detail::enabled<level::warn, TGroup>;

template <detail::Group TGroup>
inline constexpr bool error_enabled = detail::enabled<level::error, TGroup>;

template <detail::StaticNameGroup TGroup, typename... TArgs>
constexpr void trace(fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::trace, TGroup, TArgs...>(fmt, static_cast<TArgs &&>(args)...);
}

template <detail::StaticNameGroup TGroup, typename... TArgs>
constexpr void debug(fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::debug, TGroup, TArgs...>(fmt, static_cast<TArgs &&>(args)...);
}

template <detail::StaticNameGroup TGroup, typename... TArgs>
constexpr void info(fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::info, TGroup, TArgs...>(fmt, static_cast<TArgs &&>(args)...);
}

template <detail::StaticNameGroup TGroup, typename... TArgs>
constexpr void warn(fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::warn, TGroup, TArgs...>(fmt, static_cast<TArgs &&>(args)...);
}

template <detail::StaticNameGroup TGroup, typename... TArgs>
constexpr void error(fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::error, TGroup, TArgs...>(fmt, static_cast<TArgs &&>(args)...);
}

template <detail::DynamicNameGroup TGroup, typename... TArgs>
constexpr void trace(std::string_view nameArg, fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::trace, TGroup, TArgs...>(nameArg, fmt, static_cast<TArgs &&>(args)...);
}

template <detail::DynamicNameGroup TGroup, typename... TArgs>
constexpr void debug(std::string_view nameArg, fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::debug, TGroup, TArgs...>(nameArg, fmt, static_cast<TArgs &&>(args)...);
}

template <detail::DynamicNameGroup TGroup, typename... TArgs>
constexpr void info(std::string_view nameArg, fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::info, TGroup, TArgs...>(nameArg, fmt, static_cast<TArgs &&>(args)...);
}

template <detail::DynamicNameGroup TGroup, typename... TArgs>
constexpr void warn(std::string_view nameArg, fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::warn, TGroup, TArgs...>(nameArg, fmt, static_cast<TArgs &&>(args)...);
}

template <detail::DynamicNameGroup TGroup, typename... TArgs>
constexpr void error(std::string_view nameArg, fmt::format_string<TArgs...> fmt, TArgs &&...args) {
    detail::log<level::error, TGroup, TArgs...>(nameArg, fmt, static_cast<TArgs &&>(args)...);
}

} // namespace devlog

#pragma once

#include <satemu/core_types.hpp>
#include <satemu/sys/sys.hpp>

#include <satemu/export.h>

namespace satemu {

namespace version {

    constexpr auto string = satemu_VERSION;
    constexpr auto major = satemu_VERSION_MAJOR;
    constexpr auto minor = satemu_VERSION_MINOR;
    constexpr auto patch = satemu_VERSION_PATCH;

} // namespace version

// LIB_EXPORT void func();

} // namespace satemu

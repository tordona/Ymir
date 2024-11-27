#pragma once

#include <satemu/sys/sys.hpp>

namespace satemu {

namespace version {

    constexpr auto string = satemu_VERSION;
    constexpr auto major = satemu_VERSION_MAJOR;
    constexpr auto minor = satemu_VERSION_MINOR;
    constexpr auto patch = satemu_VERSION_PATCH;

} // namespace version

} // namespace satemu

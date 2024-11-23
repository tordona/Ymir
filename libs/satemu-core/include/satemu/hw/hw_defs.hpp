#pragma once

#include <satemu/core_types.hpp>

#include <concepts>

namespace satemu {

// Specifies valid types for memory accesses
template <typename T>
concept mem_access_type = std::same_as<T, uint8> || std::same_as<T, uint16> || std::same_as<T, uint32>;

} // namespace satemu

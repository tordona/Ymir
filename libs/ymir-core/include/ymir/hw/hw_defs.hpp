#pragma once

#include <ymir/core/types.hpp>

#include <concepts>

namespace ymir {

// Specifies valid types for memory accesses
template <typename T>
concept mem_primitive = std::same_as<T, uint8> || std::same_as<T, uint16> || std::same_as<T, uint32>;

} // namespace ymir

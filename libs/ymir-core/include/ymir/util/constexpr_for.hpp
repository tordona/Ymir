#pragma once

#include <utility>

namespace util {

namespace detail {

    template <typename F, std::size_t... S>
    inline constexpr void constexpr_for_impl(F &&function, std::index_sequence<S...>) {
        (function(std::integral_constant<std::size_t, S>{}), ...);
    }

} // namespace detail

template <std::size_t iterations, typename F>
inline constexpr void constexpr_for(F &&function) {
    detail::constexpr_for_impl(std::forward<F>(function), std::make_index_sequence<iterations>());
}

} // namespace util

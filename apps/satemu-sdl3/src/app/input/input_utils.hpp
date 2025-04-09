#pragma once

#include "input_context.hpp"

#include <fmt/format.h>

#include <type_traits>

namespace app::input {

template <typename T>
    requires(std::is_same_v<T, SingleShotAction> || std::is_same_v<T, BinaryAction>)
inline std::string ToShortcut(InputContext &ctx, T action) {
    fmt::memory_buffer buf{};
    auto inserter = std::back_inserter(buf);
    bool first = true;
    for (auto &bind : ctx.GetMappedInputs(action)) {
        if (first == true) {
            first = false;
        } else {
            fmt::format_to(inserter, ", ");
        }
        fmt::format_to(inserter, "{}", input::ToHumanString(bind.event));
    }
    return fmt::to_string(buf);
};

} // namespace app::input

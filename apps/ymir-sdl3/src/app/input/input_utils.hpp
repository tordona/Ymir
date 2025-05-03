#pragma once

#include "input_context.hpp"

#include <fmt/format.h>

namespace app::input {

inline std::string ToShortcut(InputContext &ctx, ActionID action) {
    fmt::memory_buffer buf{};
    auto inserter = std::back_inserter(buf);
    bool first = true;
    for (auto &bind : ctx.GetMappedInputs(action)) {
        if (first == true) {
            first = false;
        } else {
            fmt::format_to(inserter, ", ");
        }
        fmt::format_to(inserter, "{}", input::ToHumanString(bind.element));
    }
    return fmt::to_string(buf);
};

} // namespace app::input

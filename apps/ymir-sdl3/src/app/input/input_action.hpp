#pragma once

#include <ymir/core/types.hpp>

namespace app::input {

// Distinguishes actions of a given kind within an input context. Apps can use any mapping scheme they wish for actions.
// 4 billion different actions should be more than enough for any kind of app.
//
// Any number of input elements can be mapped to a given action.
using ActionID = uint32;

} // namespace app::input

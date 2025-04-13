#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/callback.hpp>

namespace satemu::vdp {

// Invoked when the VDP1 finishes drawing a frame.
using CBVDP1FrameComplete = util::OptionalCallback<void()>;

// Invoked when the VDP2 renderer finishes rendering a frame.
// Framebuffer data is in little-endian XRGB8888 format.
using CBFrameComplete = util::OptionalCallback<void(uint32 *fb, uint32 width, uint32 height)>;

} // namespace satemu::vdp

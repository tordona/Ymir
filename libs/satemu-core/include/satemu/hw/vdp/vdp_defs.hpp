#pragma once

#include "vdp1_defs.hpp"
#include "vdp2_defs.hpp"

#include <satemu/core_types.hpp>

#include <satemu/util/callback.hpp>
#include <satemu/util/inline.hpp>
#include <satemu/util/size_ops.hpp>

#include <array>

namespace satemu::vdp {

// -----------------------------------------------------------------------------
// Memory chip sizes

constexpr size_t kVDP1VRAMSize = 512_KiB;
constexpr size_t kVDP1FramebufferRAMSize = 256_KiB;
constexpr size_t kVDP2VRAMSize = 512_KiB;
constexpr size_t kVDP2CRAMSize = 4_KiB;

// -----------------------------------------------------------------------------
// Basic types

union Color555 {
    uint16 u16;
    struct {
        uint16 r : 5;
        uint16 g : 5;
        uint16 b : 5;
        uint16 msb : 1; // CC in CRAM, transparency in cells when using RGB format
    };
};

union Color888 {
    uint32 u32;
    struct {
        uint32 r : 8;
        uint32 g : 8;
        uint32 b : 8;
        uint32 : 7;
        uint32 msb : 1; // CC in CRAM, transparency in cells when using RGB format
    };
};

FORCE_INLINE Color888 ConvertRGB555to888(Color555 color) {
    return Color888{
        .r = static_cast<uint32>(color.r) << 3u,
        .g = static_cast<uint32>(color.g) << 3u,
        .b = static_cast<uint32>(color.b) << 3u,
        .msb = color.msb,
    };
}

// TODO: move this to a "renderer defs" header
// Framebuffer color is in little-endian XRGB8888 format
using FramebufferColor = uint32;

// TODO: move these to a "renderer defs" header
using CBRequestFramebuffer = util::Callback<FramebufferColor *(uint32 width, uint32 height)>;
using CBFrameComplete = util::Callback<void(FramebufferColor *fb, uint32 width, uint32 height)>;

} // namespace satemu::vdp

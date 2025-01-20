#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/hw/scu/scu.hpp>

#include "slope.hpp"

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/constexpr_for.hpp>
#include <satemu/util/unreachable.hpp>

#include <fmt/format.h>

#include <cassert>

namespace satemu::vdp {

VDP::VDP(scu::SCU &scu)
    : m_SCU(scu) {

    m_framebuffer = nullptr;

    // TODO: set PAL flag
    Reset(true);
}

void VDP::Reset(bool hard) {
    if (hard) {
        for (uint32 addr = 0; addr < m_VRAM1.size(); addr++) {
            if ((addr & 0x1F) == 0) {
                m_VRAM1[addr] = 0x80;
            } else if ((addr & 0x1F) == 1) {
                m_VRAM1[addr] = 0x00;
            } else if ((addr & 2) == 2) {
                m_VRAM1[addr] = 0x55;
            } else {
                m_VRAM1[addr] = 0xAA;
            }
        }

        m_VRAM2.fill(0);
        m_CRAM.fill(0);
        for (auto &fb : m_spriteFB) {
            fb.fill(0);
        }
        m_drawFB = 0;
    }

    m_VDP1.Reset();
    m_VDP2.Reset();

    m_HPhase = HorizontalPhase::Active;
    m_VPhase = VerticalPhase::Active;
    m_currCycles = 0;
    m_dotClockMult = 2;
    m_VCounter = 0;
    m_HRes = 320;
    m_VRes = 224;

    m_VDP1RenderContext.Reset();

    for (auto &state : m_layerStates) {
        state.Reset();
    }
    m_spriteLayerState.Reset();
    for (auto &state : m_normBGLayerStates) {
        state.Reset();
    }
    for (auto &state : m_rotParamStates) {
        state.Reset();
    }
    m_lineBackLayerState.Reset();

    BeginHPhaseActiveDisplay();
    BeginVPhaseActiveDisplay();

    UpdateResolution();
}

void VDP::Advance(uint64 cycles) {
    // Update timings and fire events
    // TODO: use scheduler events

    for (uint64 cy = 0; cy < cycles; cy++) {
        VDP1ProcessCommands();
    }

    m_currCycles += cycles;
    while (m_currCycles >= m_HTimings[static_cast<uint32>(m_HPhase)]) {
        auto nextPhase = static_cast<uint32>(m_HPhase) + 1;
        if (nextPhase == 4) {
            m_currCycles -= m_HTimings[3];
            nextPhase = 0;
        }

        m_HPhase = static_cast<HorizontalPhase>(nextPhase);
        switch (m_HPhase) {
        case HorizontalPhase::Active: BeginHPhaseActiveDisplay(); break;
        case HorizontalPhase::RightBorder: BeginHPhaseRightBorder(); break;
        case HorizontalPhase::HorizontalSync: BeginHPhaseHorizontalSync(); break;
        case HorizontalPhase::LeftBorder: BeginHPhaseLeftBorder(); break;
        }
    }
}

void VDP::DumpVDP1VRAM(std::ostream &out) {
    out.write((const char *)m_VRAM1.data(), m_VRAM1.size());
}

void VDP::DumpVDP2VRAM(std::ostream &out) {
    out.write((const char *)m_VRAM2.data(), m_VRAM2.size());
}

void VDP::DumpVDP2CRAM(std::ostream &out) {
    out.write((const char *)m_CRAM.data(), m_CRAM.size());
}

void VDP::DumpVDP1Framebuffers(std::ostream &out) {
    out.write((const char *)m_spriteFB[m_drawFB].data(), m_spriteFB[m_drawFB].size());
    out.write((const char *)m_spriteFB[m_drawFB ^ 1].data(), m_spriteFB[m_drawFB ^ 1].size());
}

void VDP::UpdateResolution() {
    if (!m_VDP2.TVMDDirty) {
        return;
    }

    m_VDP2.TVMDDirty = false;

    // Horizontal/vertical resolution tables
    // NTSC uses the first two vRes entries, PAL uses the full table, and exclusive monitors use 480 lines
    static constexpr uint32 hRes[] = {320, 352, 640, 704};
    static constexpr uint32 vRes[] = {224, 240, 256, 256};

    // TODO: check for NTSC, PAL or exclusive monitor; assuming NTSC for now
    // TODO: exclusive monitor: even hRes entries are valid for 31 KHz monitors, odd are for Hi-Vision
    m_HRes = hRes[m_VDP2.TVMD.HRESOn];
    m_VRes = vRes[m_VDP2.TVMD.VRESOn & (m_VDP2.TVSTAT.PAL ? 3 : 1)];
    if (m_VDP2.TVMD.LSMDn == 3) {
        // Double-density interlace doubles the vertical resolution
        m_VRes *= 2;
    }

    rootLog2.info("Screen resolution set to {}x{}", m_HRes, m_VRes);
    switch (m_VDP2.TVMD.LSMDn) {
    case 0: rootLog2.info("Non-interlace mode"); break;
    case 1: rootLog2.info("Invalid interlace mode"); break;
    case 2: rootLog2.info("Single-density interlace mode"); break;
    case 3: rootLog2.info("Double-density interlace mode"); break;
    }

    m_framebuffer = m_cbRequestFramebuffer(m_HRes, m_VRes);

    // Timing tables
    // NOTE: the timings indicate when the specified phase begins

    // Horizontal phase timings
    //   RBd = Right Border
    //   HSy = Horizontal Sync
    //   LBd = Left Border
    //   ADp = Active Display
    static constexpr std::array<std::array<uint32, 4>, 8> hTimings{{
        // RBd, HSy, LBd, ADp
        {320, 347, 400, 427},
        {352, 375, 432, 455},
        {640, 694, 800, 854},
        {704, 375, 864, 910},
    }};
    m_HTimings = hTimings[m_VDP2.TVMD.HRESOn & 3]; // TODO: check exclusive monitor timings

    // Vertical phase timings
    //   BBd = Bottom Border
    //   BBl = Bottom Blanking
    //   VSy = Vertical Sync
    //   TBl = Top Blanking
    //   TBd = Top Border
    //   LLn = Last Line
    //   ADp = Active Display
    static constexpr std::array<std::array<std::array<uint32, 7>, 4>, 2> vTimings{{
        // NTSC
        {{
            // BBd, BBl, VSy, TBl, TBd, LLn, ADp
            {224, 232, 237, 240, 255, 262, 263},
            {240, 240, 245, 248, 263, 262, 263},
            {224, 232, 237, 240, 255, 262, 263},
            {240, 240, 245, 248, 263, 262, 263},
        }},
        // PAL
        {{
            // BBd, BBl, VSy, TBl, TBd, ADp
            {224, 256, 259, 262, 281, 312, 313},
            {240, 264, 267, 270, 289, 312, 313},
            {256, 272, 275, 278, 297, 312, 313},
            {256, 272, 275, 278, 297, 312, 313},
        }},
    }};
    m_VTimings = vTimings[m_VDP2.TVSTAT.PAL][m_VDP2.TVMD.VRESOn];
    if (m_VDP2.TVMD.LSMDn == 3) {
        for (uint32 &timing : m_VTimings) {
            timing *= 2;
        }
    }

    // Adjust for dot clock
    const uint32 dotClockMult = (m_VDP2.TVMD.HRESOn & 2) ? 2 : 4;
    for (auto &timing : m_HTimings) {
        timing *= dotClockMult;
    }
    m_dotClockMult = dotClockMult;

    rootLog2.info("Dot clock mult = {}, display {}", dotClockMult, (m_VDP2.TVMD.DISP ? "ON" : "OFF"));
}

FORCE_INLINE void VDP::IncrementVCounter() {
    ++m_VCounter;
    while (m_VCounter >= m_VTimings[static_cast<uint32>(m_VPhase)]) {
        auto nextPhase = static_cast<uint32>(m_VPhase) + 1;
        if (nextPhase == 7) {
            m_VCounter = 0;
            nextPhase = 0;
        }

        m_VPhase = static_cast<VerticalPhase>(nextPhase);
        switch (m_VPhase) {
        case VerticalPhase::Active: BeginVPhaseActiveDisplay(); break;
        case VerticalPhase::BottomBorder: BeginVPhaseBottomBorder(); break;
        case VerticalPhase::BottomBlanking: BeginVPhaseBottomBlanking(); break;
        case VerticalPhase::VerticalSync: BeginVPhaseVerticalSync(); break;
        case VerticalPhase::TopBlanking: BeginVPhaseTopBlanking(); break;
        case VerticalPhase::TopBorder: BeginVPhaseTopBorder(); break;
        case VerticalPhase::LastLine: BeginVPhaseLastLine(); break;
        }
    }
}

// ----

void VDP::BeginHPhaseActiveDisplay() {
    rootLog2.trace("(VCNT = {:3d})  Entering horizontal active display phase", m_VCounter);
    if (m_VPhase == VerticalPhase::Active) {
        if (m_VCounter == 0) {
            rootLog2.trace("Begin frame, VDP1 framebuffer {}", m_drawFB ^ 1);
            rootLog2.trace("VBE={:d} FCM={:d} FCT={:d} PTM={:d} mswap={:d} merase={:d}", m_VDP1.vblankErase,
                           m_VDP1.fbSwapMode, m_VDP1.fbSwapTrigger, m_VDP1.plotTrigger, m_VDP1.fbManualSwap,
                           m_VDP1.fbManualErase);

            bool swapFB = false;
            if (m_VDP1.fbManualSwap) {
                m_VDP1.fbManualSwap = false;
                swapFB = true;
            }

            if (!m_VDP1.fbSwapMode) {
                swapFB = true;
            }

            // Swap framebuffers and trigger:
            // - Manual erase
            // - VDP1 draw if PMTR.PTM == 0b10
            if (swapFB) {
                if (m_VDP1.fbManualErase) {
                    m_VDP1.fbManualErase = false;
                    VDP1EraseFramebuffer();
                }
                VDP1SwapFramebuffer();
                if (m_VDP1.plotTrigger == 0b10) {
                    VDP1BeginFrame();
                }
            }

            VDP2InitFrame();
        }
        VDP2DrawLine();
    }
}

void VDP::BeginHPhaseRightBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering right border phase", m_VCounter);
}

void VDP::BeginHPhaseHorizontalSync() {
    IncrementVCounter();
    rootLog2.trace("(VCNT = {:3d})  Entering horizontal sync phase", m_VCounter);

    m_VDP2.TVSTAT.HBLANK = 1;
    m_SCU.TriggerHBlankIN();
}

void VDP::BeginHPhaseLeftBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering left border phase", m_VCounter);
    m_VDP2.TVSTAT.HBLANK = 0;
}

// ----

void VDP::BeginVPhaseActiveDisplay() {
    rootLog2.trace("(VCNT = {:3d})  Entering vertical active display phase", m_VCounter);
    if (m_VDP2.TVMD.LSMDn != 0) {
        m_VDP2.TVSTAT.ODD ^= 1;
    } else {
        m_VDP2.TVSTAT.ODD = 1;
    }
}

void VDP::BeginVPhaseBottomBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering bottom border phase", m_VCounter);
}

void VDP::BeginVPhaseBottomBlanking() {
    rootLog2.trace("(VCNT = {:3d})  Entering bottom blanking phase", m_VCounter);
}

void VDP::BeginVPhaseVerticalSync() {
    rootLog2.trace("(VCNT = {:3d})  Entering vertical sync phase", m_VCounter);
    m_VDP2.TVSTAT.VBLANK = 1;
    m_SCU.TriggerVBlankIN();
}

void VDP::BeginVPhaseTopBlanking() {
    rootLog2.trace("(VCNT = {:3d})  Entering top blanking phase", m_VCounter);

    // End frame
    rootLog2.trace("Ending frame");
    m_cbFrameComplete(m_framebuffer, m_HRes, m_VRes);

    UpdateResolution();
}

void VDP::BeginVPhaseTopBorder() {
    rootLog2.trace("(VCNT = {:3d})  Entering top border phase", m_VCounter);
}

void VDP::BeginVPhaseLastLine() {
    rootLog2.trace("(VCNT = {:3d})  Entering last line phase", m_VCounter);

    m_VDP2.TVSTAT.VBLANK = 0;
    m_SCU.TriggerVBlankOUT();

    rootLog2.trace("VBlank OUT  VBE={:d} FCM={:d}", m_VDP1.vblankErase, m_VDP1.fbSwapMode);

    // VBlank erase or 1-cycle mode
    if (m_VDP1.vblankErase || !m_VDP1.fbSwapMode) {
        VDP1EraseFramebuffer();
    }
}

// -----------------------------------------------------------------------------
// VDP1

FORCE_INLINE std::array<uint8, kVDP1FramebufferRAMSize> &VDP::VDP1GetDrawFB() {
    return m_spriteFB[m_drawFB];
}

FORCE_INLINE std::array<uint8, kVDP1FramebufferRAMSize> &VDP::VDP1GetDisplayFB() {
    return m_spriteFB[m_drawFB ^ 1];
}

FORCE_INLINE void VDP::VDP1EraseFramebuffer() {
    renderLog1.trace("Erasing framebuffer {}", m_drawFB ^ 1);
    // TODO: erase only the specified region
    VDP1GetDisplayFB().fill(m_VDP1.eraseWriteValue);
}

FORCE_INLINE void VDP::VDP1SwapFramebuffer() {
    renderLog1.trace("Swapping framebuffers - draw {}, display {}", m_drawFB ^ 1, m_drawFB);
    m_drawFB ^= 1;
}

void VDP::VDP1BeginFrame() {
    renderLog1.trace("Starting frame on framebuffer {} - VBE={:d} FCT={:d} FCM={:d}", m_drawFB, m_VDP1.vblankErase,
                     m_VDP1.fbSwapTrigger, m_VDP1.fbSwapMode);

    // TODO: setup rendering
    // TODO: figure out VDP1 timings

    m_VDP1.prevCommandAddress = m_VDP1.currCommandAddress;
    m_VDP1.currCommandAddress = 0;
    m_VDP1.returnAddress = ~0;
    m_VDP1.prevFrameEnded = m_VDP1.currFrameEnded;
    m_VDP1.currFrameEnded = false;

    m_VDP1RenderContext.rendering = true;

    VDP1ProcessCommands();
}

void VDP::VDP1EndFrame() {
    renderLog1.trace("Ending frame");
    m_VDP1RenderContext.rendering = false;
    m_VDP1.currFrameEnded = true;
}

void VDP::VDP1ProcessCommands() {
    static constexpr uint16 kNoReturn = ~0;

    if (!m_VDP1RenderContext.rendering) {
        return;
    }

    auto &cmdAddress = m_VDP1.currCommandAddress;

    const VDP1Command::Control control{.u16 = VDP1ReadVRAM<uint16>(cmdAddress)};
    renderLog1.trace("Processing command: {:04X}", control.u16);
    if (control.end) [[unlikely]] {
        renderLog1.trace("End of command list");
        VDP1EndFrame();
        m_SCU.TriggerSpriteDrawEnd();
    } else if (!control.skip) {
        // Process command
        using enum VDP1Command::CommandType;

        switch (control.command) {
        case DrawNormalSprite: VDP1Cmd_DrawNormalSprite(cmdAddress, control); break;
        case DrawScaledSprite: VDP1Cmd_DrawScaledSprite(cmdAddress, control); break;
        case DrawDistortedSprite: // fallthrough
        case DrawDistortedSpriteAlt: VDP1Cmd_DrawDistortedSprite(cmdAddress, control); break;

        case DrawPolygon: VDP1Cmd_DrawPolygon(cmdAddress); break;
        case DrawPolylines: // fallthrough
        case DrawPolylinesAlt: VDP1Cmd_DrawPolylines(cmdAddress); break;
        case DrawLine: VDP1Cmd_DrawLine(cmdAddress); break;

        case UserClipping: // fallthrough
        case UserClippingAlt: VDP1Cmd_SetUserClipping(cmdAddress); break;
        case SystemClipping: VDP1Cmd_SetSystemClipping(cmdAddress); break;
        case SetLocalCoordinates: VDP1Cmd_SetLocalCoordinates(cmdAddress); break;

        default:
            renderLog1.debug("Unexpected command type {:X}", static_cast<uint16>(control.command));
            VDP1EndFrame();
            return;
        }
    }

    // Go to the next command
    {
        using enum VDP1Command::JumpType;

        switch (control.jumpMode) {
        case Next: cmdAddress += 0x20; break;
        case Assign: {
            cmdAddress = VDP1ReadVRAM<uint16>(cmdAddress + 0x02) << 3u;
            break;
        }
        case Call: {
            // Nested calls seem to not update the return address
            if (m_VDP1.returnAddress == kNoReturn) {
                m_VDP1.returnAddress = cmdAddress + 0x20;
            }
            cmdAddress = VDP1ReadVRAM<uint16>(cmdAddress + 0x02) << 3u;
            break;
        }
        case Return: {
            // Return seems to only return if there was a previous Call
            if (m_VDP1.returnAddress != kNoReturn) {
                cmdAddress = m_VDP1.returnAddress;
                m_VDP1.returnAddress = kNoReturn;
            } else {
                cmdAddress += 0x20;
            }
            break;
        }
        }
        cmdAddress &= 0x7FFFF;
    }
}

FORCE_INLINE bool VDP::VDP1IsPixelUserClipped(CoordS32 coord) const {
    auto [x, y] = coord;
    const auto &ctx = m_VDP1RenderContext;
    if (x < ctx.userClipX0 || x > ctx.userClipX1) {
        return true;
    }
    if (y < ctx.userClipY0 || y > ctx.userClipY1) {
        return true;
    }
    return false;
}

FORCE_INLINE bool VDP::VDP1IsPixelSystemClipped(CoordS32 coord) const {
    auto [x, y] = coord;
    const auto &ctx = m_VDP1RenderContext;
    if (x < 0 || x > ctx.sysClipH) {
        return true;
    }
    if (y < 0 || y > ctx.sysClipV) {
        return true;
    }
    return false;
}

FORCE_INLINE bool VDP::VDP1IsLineSystemClipped(CoordS32 coord1, CoordS32 coord2) const {
    auto [x1, y1] = coord1;
    auto [x2, y2] = coord2;
    const auto &ctx = m_VDP1RenderContext;
    if (x1 < 0 && x2 < 0) {
        return true;
    }
    if (y1 < 0 && y2 < 0) {
        return true;
    }
    if (x1 > ctx.sysClipH && x2 > ctx.sysClipH) {
        return true;
    }
    if (y1 > ctx.sysClipV && y2 > ctx.sysClipV) {
        return true;
    }
    return false;
}

bool VDP::VDP1IsQuadSystemClipped(CoordS32 coord1, CoordS32 coord2, CoordS32 coord3, CoordS32 coord4) const {
    auto [x1, y1] = coord1;
    auto [x2, y2] = coord2;
    auto [x3, y3] = coord3;
    auto [x4, y4] = coord4;
    const auto &ctx = m_VDP1RenderContext;
    if (x1 < 0 && x2 < 0 && x3 < 0 && x4 < 0) {
        return true;
    }
    if (y1 < 0 && y2 < 0 && y3 < 0 && y4 < 0) {
        return true;
    }
    if (x1 > ctx.sysClipH && x2 > ctx.sysClipH && x3 > ctx.sysClipH && x4 > ctx.sysClipH) {
        return true;
    }
    if (y1 > ctx.sysClipV && y2 > ctx.sysClipV && y3 > ctx.sysClipV && y4 > ctx.sysClipV) {
        return true;
    }
    return false;
}

FORCE_INLINE void VDP::VDP1PlotPixel(CoordS32 coord, const VDP1PixelParams &pixelParams,
                                     const VDP1GouraudParams &gouraudParams) {
    auto [x, y] = coord;
    if (pixelParams.mode.meshEnable && ((x ^ y) & 1)) {
        return;
    }

    // Reject pixels outside of clipping area
    if (VDP1IsPixelSystemClipped(coord)) {
        return;
    }
    if (pixelParams.mode.userClippingEnable) {
        // clippingMode = false -> draw inside, reject outside
        // clippingMode = true -> draw outside, reject inside
        // The function returns true if the pixel is clipped, therefore we want to reject pixels that return the
        // opposite of clippingMode on that function.
        if (VDP1IsPixelUserClipped(coord) != pixelParams.mode.clippingMode) {
            return;
        }
    }

    // TODO: pixelParams.mode.preClippingDisable

    const uint32 fbOffset = y * m_VDP1.fbSizeH + x;
    auto &drawFB = VDP1GetDrawFB();
    if (m_VDP1.pixel8Bits) {
        // TODO: what happens if pixelParams.mode.colorCalcBits/gouraudEnable != 0?
        if (pixelParams.mode.msbOn) {
            drawFB[fbOffset & 0x3FFFF] |= 0x80;
        } else {
            drawFB[fbOffset & 0x3FFFF] = pixelParams.color;
        }
    } else {
        uint8 *pixel = &drawFB[(fbOffset * sizeof(uint16)) & 0x3FFFE];

        if (pixelParams.mode.msbOn) {
            drawFB[(fbOffset * sizeof(uint16)) & 0x3FFFE] |= 0x80;
        } else {
            Color555 srcColor{.u16 = pixelParams.color};
            Color555 dstColor{.u16 = util::ReadBE<uint16>(pixel)};

            // Apply color calculations
            //
            // In all cases where calculation is done, the raw color data to be drawn ("original graphic") or from
            // the background are interpreted as 5:5:5 RGB.

            if (pixelParams.mode.gouraudEnable) {
                // Calculate gouraud shading on source color
                // Interpolate between A, B, C and D (ordered in the standard Saturn quad orientation) using U and V
                // Gouraud channel values are offset by -16

                auto lerp = [](sint64 x, sint64 y, uint64 t) -> sint16 {
                    static constexpr uint64 shift = Slope::kFracBits;
                    return ((x << shift) + (y - x) * t) >> shift;
                };

                const Color555 A = gouraudParams.colorA;
                const Color555 B = gouraudParams.colorB;
                const Color555 C = gouraudParams.colorC;
                const Color555 D = gouraudParams.colorD;
                const uint64 U = gouraudParams.U;
                const uint64 V = gouraudParams.V;

                const sint16 ABr = lerp(static_cast<sint16>(A.r), static_cast<sint16>(B.r), U);
                const sint16 ABg = lerp(static_cast<sint16>(A.g), static_cast<sint16>(B.g), U);
                const sint16 ABb = lerp(static_cast<sint16>(A.b), static_cast<sint16>(B.b), U);

                const sint16 DCr = lerp(static_cast<sint16>(D.r), static_cast<sint16>(C.r), U);
                const sint16 DCg = lerp(static_cast<sint16>(D.g), static_cast<sint16>(C.g), U);
                const sint16 DCb = lerp(static_cast<sint16>(D.b), static_cast<sint16>(C.b), U);

                srcColor.r = std::clamp(srcColor.r + lerp(ABr, DCr, V) - 0x10, 0, 31);
                srcColor.g = std::clamp(srcColor.g + lerp(ABg, DCg, V) - 0x10, 0, 31);
                srcColor.b = std::clamp(srcColor.b + lerp(ABb, DCb, V) - 0x10, 0, 31);

                // HACK: replace with U/V coordinates
                // srcColor.r = U >> (Slope::kFracBits - 5);
                // srcColor.g = V >> (Slope::kFracBits - 5);

                // HACK: replace with computed gouraud gradient
                // srcColor.r = lerp(ABr, DCr, V);
                // srcColor.g = lerp(ABg, DCg, V);
                // srcColor.b = lerp(ABb, DCb, V);
            }

            switch (pixelParams.mode.colorCalcBits) {
            case 0: // Replace
                util::WriteBE<uint16>(pixel, srcColor.u16);
                break;
            case 1: // Shadow
                // Halve destination luminosity if it's not transparent
                if (dstColor.msb) {
                    dstColor.r >>= 1u;
                    dstColor.g >>= 1u;
                    dstColor.b >>= 1u;
                    util::WriteBE<uint16>(pixel, dstColor.u16);
                }
                break;
            case 2: // Half-luminance
                // Draw original graphic with halved luminance
                srcColor.r >>= 1u;
                srcColor.g >>= 1u;
                srcColor.b >>= 1u;
                util::WriteBE<uint16>(pixel, srcColor.u16);
                break;
            case 3: // Half-transparency
                // If background is not transparent, blend half of original graphic and half of background
                // Otherwise, draw original graphic as is
                if (dstColor.msb) {
                    srcColor.r = (srcColor.r + dstColor.r) >> 1u;
                    srcColor.g = (srcColor.g + dstColor.g) >> 1u;
                    srcColor.b = (srcColor.b + dstColor.b) >> 1u;
                }
                util::WriteBE<uint16>(pixel, srcColor.u16);
                break;
            }
        }
    }
}

FORCE_INLINE void VDP::VDP1PlotLine(CoordS32 coord1, CoordS32 coord2, const VDP1PixelParams &pixelParams,
                                    VDP1GouraudParams &gouraudParams) {
    for (LineStepper line{coord1, coord2}; line.CanStep(); line.Step()) {
        gouraudParams.U = line.FracPos();
        VDP1PlotPixel(line.Coord(), pixelParams, gouraudParams);
        if (line.NeedsAntiAliasing()) {
            VDP1PlotPixel(line.AACoord(), pixelParams, gouraudParams);
        }
    }
}

void VDP::VDP1PlotTexturedLine(CoordS32 coord1, CoordS32 coord2, const VDP1TexturedLineParams &lineParams,
                               VDP1GouraudParams &gouraudParams) {
    const uint32 charSizeH = lineParams.charSizeH;
    const uint32 charSizeV = lineParams.charSizeV;
    const auto mode = lineParams.mode;
    const auto control = lineParams.control;

    const uint32 v = lineParams.texFracV >> Slope::kFracBits;
    // Bail out if V coordinate is out of range
    if (v >= charSizeV) {
        return;
    }
    gouraudParams.V = lineParams.texFracV / charSizeV;

    uint16 color = 0;
    bool transparent = true;
    const bool flipU = control.flipH;
    for (TexturedLineStepper line{coord1, coord2, charSizeH, flipU}; line.CanStep(); line.Step()) {
        // Load new texel if U coordinate changed.
        // Note that the very first pixel in the line always passes the check.
        if (line.UChanged()) {
            const uint32 u = line.U();
            // Bail out if U coordinate is out of range
            if (u >= charSizeH) {
                break;
            }

            // TODO: process end codes, unless mode.endCodeDisable || mode.highSpeedShrink
            // TODO: handle mode.highSpeedShrink

            const uint32 charIndex = u + v * charSizeH;

            // Read next texel
            switch (mode.colorMode) {
            case 0: // 4 bpp, 16 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + (charIndex >> 1));
                color = (color >> (((u ^ 1) & 1) * 4)) & 0xF;
                transparent = color == 0x0;
                color |= lineParams.colorBank;
                break;
            case 1: // 4 bpp, 16 colors, lookup table mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + (charIndex >> 1));
                color = (color >> (((u ^ 1) & 1) * 4)) & 0xF;
                transparent = color == 0x0;
                color = VDP1ReadVRAM<uint16>(color * sizeof(uint16) + lineParams.colorBank * 8);
                break;
            case 2: // 8 bpp, 64 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + charIndex) & 0x3F;
                transparent = color == 0x0;
                color |= lineParams.colorBank & 0xFFC0;
                break;
            case 3: // 8 bpp, 128 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + charIndex) & 0x7F;
                transparent = color == 0x00;
                color |= lineParams.colorBank & 0xFF80;
                break;
            case 4: // 8 bpp, 256 colors, bank mode
                color = VDP1ReadVRAM<uint8>(lineParams.charAddr + charIndex);
                transparent = color == 0x00;
                color |= lineParams.colorBank & 0xFF00;
                break;
            case 5: // 16 bpp, 32768 colors, RGB mode
                color = VDP1ReadVRAM<uint16>(lineParams.charAddr + charIndex * sizeof(uint16));
                transparent = color == 0x0000;
                break;
            }
        }

        if (transparent && !mode.transparentPixelDisable) {
            continue;
        }

        VDP1PixelParams pixelParams{
            .mode = mode,
            .color = color,
        };

        gouraudParams.U = line.FracU() / charSizeH;

        VDP1PlotPixel(line.Coord(), pixelParams, gouraudParams);
        if (line.NeedsAntiAliasing()) {
            VDP1PlotPixel(line.AACoord(), pixelParams, gouraudParams);
        }
    }
}

void VDP::VDP1Cmd_DrawNormalSprite(uint32 cmdAddress, VDP1Command::Control control) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};
    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const uint32 charAddr = VDP1ReadVRAM<uint16>(cmdAddress + 0x08) * 8u;
    const VDP1Command::Size size{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x0A)};
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const uint32 charSizeH = size.H * 8;
    const uint32 charSizeV = size.V;

    const sint32 lx = xa;             // left X
    const sint32 ty = ya;             // top Y
    const sint32 rx = xa + charSizeH; // right X
    const sint32 by = ya + charSizeV; // bottom Y

    const CoordS32 coordA{lx, ty};
    const CoordS32 coordB{rx, ty};
    const CoordS32 coordC{rx, by};
    const CoordS32 coordD{lx, by};

    renderLog1.trace("Draw normal sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
                     "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
                     lx, ty, rx, ty, rx, by, lx, by, color, gouraudTable, mode.u16, charSizeH, charSizeV, charAddr);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    VDP1TexturedLineParams lineParams{
        .control = control,
        .mode = mode,
        .colorBank = color,
        .charAddr = charAddr,
        .charSizeH = charSizeH,
        .charSizeV = charSizeV,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };

    // Interpolate linearly over edges A-D and B-C
    const bool flipV = control.flipV;
    for (TexturedQuadEdgesStepper edge{coordA, coordB, coordC, coordD, charSizeV, flipV}; edge.CanStep(); edge.Step()) {
        // Plot lines between the interpolated points
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};
        lineParams.texFracV = edge.FracV();
        VDP1PlotTexturedLine(coordL, coordR, lineParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawScaledSprite(uint32 cmdAddress, VDP1Command::Control control) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};
    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const uint32 charAddr = VDP1ReadVRAM<uint16>(cmdAddress + 0x08) * 8u;
    const VDP1Command::Size size{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x0A)};
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const uint32 charSizeH = size.H * 8;
    const uint32 charSizeV = size.V;

    // Calculated quad coordinates
    sint32 qxa;
    sint32 qya;
    sint32 qxb;
    sint32 qyb;
    sint32 qxc;
    sint32 qyc;
    sint32 qxd;
    sint32 qyd;

    const uint8 zoomPointH = bit::extract<0, 1>(control.zoomPoint);
    const uint8 zoomPointV = bit::extract<2, 3>(control.zoomPoint);
    if (zoomPointH == 0 || zoomPointV == 0) {
        const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
        const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));

        // Top-left coordinates on vertex A
        // Bottom-right coordinates on vertex C
        qxa = xa;
        qya = ya;
        qxb = xc;
        qyb = ya;
        qxc = xc;
        qyc = yc;
        qxd = xa;
        qyd = yc;
    } else {
        const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10));
        const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12));

        // Zoom origin on vertex A
        // Zoom dimensions on vertex B

        // X axis
        switch (zoomPointH) {
        case 1: // left
            qxa = xa;
            qxb = xa + xb;
            qxc = xa + xb;
            qxd = xa;
            break;
        case 2: // center
            qxa = xa - xb / 2;
            qxb = xa + (xb + 1) / 2;
            qxc = xa + (xb + 1) / 2;
            qxd = xa - xb / 2;
            break;
        case 3: // right
            qxa = xa - xb;
            qxb = xa;
            qxc = xa;
            qxd = xa - xb;
            break;
        }

        // Y axis
        switch (zoomPointV) {
        case 1: // upper
            qya = ya;
            qyb = ya;
            qyc = ya + yb;
            qyd = ya + yb;
            break;
        case 2: // center
            qya = ya - yb / 2;
            qyb = ya - yb / 2;
            qyc = ya + (yb + 1) / 2;
            qyd = ya + (yb + 1) / 2;
            break;
        case 3: // lower
            qya = ya - yb;
            qyb = ya - yb;
            qyc = ya;
            qyd = ya;
            break;
        }
    }

    qxa += ctx.localCoordX;
    qya += ctx.localCoordY;
    qxb += ctx.localCoordX;
    qyb += ctx.localCoordY;
    qxc += ctx.localCoordX;
    qyc += ctx.localCoordY;
    qxd += ctx.localCoordX;
    qyd += ctx.localCoordY;

    const CoordS32 coordA{qxa, qya};
    const CoordS32 coordB{qxb, qyb};
    const CoordS32 coordC{qxc, qyc};
    const CoordS32 coordD{qxd, qyd};

    renderLog1.trace("Draw scaled sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
                     "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
                     qxa, qya, qxb, qyb, qxc, qyc, qxd, qyd, color, gouraudTable, mode.u16, charSizeH, charSizeV,
                     charAddr);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    VDP1TexturedLineParams lineParams{
        .control = control,
        .mode = mode,
        .colorBank = color,
        .charAddr = charAddr,
        .charSizeH = charSizeH,
        .charSizeV = charSizeV,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };

    // Interpolate linearly over edges A-D and B-C
    const bool flipV = control.flipV;
    for (TexturedQuadEdgesStepper edge{coordA, coordB, coordC, coordD, charSizeV, flipV}; edge.CanStep(); edge.Step()) {
        // Plot lines between the interpolated points
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};
        lineParams.texFracV = edge.FracV();
        VDP1PlotTexturedLine(coordL, coordR, lineParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawDistortedSprite(uint32 cmdAddress, VDP1Command::Control control) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};
    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const uint32 charAddr = VDP1ReadVRAM<uint16>(cmdAddress + 0x08) * 8u;
    const VDP1Command::Size size{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x0A)};
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14)) + ctx.localCoordX;
    const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16)) + ctx.localCoordY;
    const sint32 xd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x18)) + ctx.localCoordX;
    const sint32 yd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1A)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const uint32 charSizeH = size.H * 8;
    const uint32 charSizeV = size.V;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};
    const CoordS32 coordC{xc, yc};
    const CoordS32 coordD{xd, yd};

    renderLog1.trace("Draw distorted sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
                     "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
                     xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable, mode.u16, charSizeH, charSizeV, charAddr);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    VDP1TexturedLineParams lineParams{
        .control = control,
        .mode = mode,
        .colorBank = color,
        .charAddr = charAddr,
        .charSizeH = charSizeH,
        .charSizeV = charSizeV,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };

    // Interpolate linearly over edges A-D and B-C
    const bool flipV = control.flipV;
    for (TexturedQuadEdgesStepper edge{coordA, coordB, coordC, coordD, charSizeV, flipV}; edge.CanStep(); edge.Step()) {
        // Plot lines between the interpolated points
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};
        lineParams.texFracV = edge.FracV();
        VDP1PlotTexturedLine(coordL, coordR, lineParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawPolygon(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};

    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14)) + ctx.localCoordX;
    const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16)) + ctx.localCoordY;
    const sint32 xd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x18)) + ctx.localCoordX;
    const sint32 yd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1A)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};
    const CoordS32 coordC{xc, yc};
    const CoordS32 coordD{xd, yd};

    renderLog1.trace(
        "VDP1: Draw polygon: {}x{} - {}x{} - {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}", xa, ya,
        xb, yb, xc, yc, xd, yd, color, gouraudTable, mode.u16);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    const VDP1PixelParams pixelParams{
        .mode = mode,
        .color = color,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .colorC{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)},
        .colorD{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)},
    };

    // Interpolate linearly over edges A-D and B-C
    for (QuadEdgesStepper edge{coordA, coordB, coordC, coordD}; edge.CanStep(); edge.Step()) {
        const CoordS32 coordL{edge.LX(), edge.LY()};
        const CoordS32 coordR{edge.RX(), edge.RY()};

        gouraudParams.V = edge.FracPos();

        // Plot lines between the interpolated points
        VDP1PlotLine(coordL, coordR, pixelParams, gouraudParams);
    }
}

void VDP::VDP1Cmd_DrawPolylines(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};

    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const sint32 xc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14)) + ctx.localCoordX;
    const sint32 yc = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16)) + ctx.localCoordY;
    const sint32 xd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x18)) + ctx.localCoordX;
    const sint32 yd = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1A)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};
    const CoordS32 coordC{xc, yc};
    const CoordS32 coordD{xd, yd};

    renderLog1.trace("Draw polylines: {}x{} - {}x{} - {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}",
                     xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable >> 3u, mode.u16);

    if (VDP1IsQuadSystemClipped(coordA, coordB, coordC, coordD)) {
        return;
    }

    const VDP1PixelParams pixelParams{
        .mode = mode,
        .color = color,
    };

    const Color555 A{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)};
    const Color555 B{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)};
    const Color555 C{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 4u)};
    const Color555 D{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 6u)};

    VDP1GouraudParams gouraudParamsAB{.colorA = A, .colorB = B, .V = 0};
    VDP1GouraudParams gouraudParamsBC{.colorA = B, .colorB = C, .V = 0};
    VDP1GouraudParams gouraudParamsCD{.colorA = C, .colorB = D, .V = 0};
    VDP1GouraudParams gouraudParamsDA{.colorA = D, .colorB = A, .V = 0};

    VDP1PlotLine(coordA, coordB, pixelParams, gouraudParamsAB);
    VDP1PlotLine(coordB, coordC, pixelParams, gouraudParamsBC);
    VDP1PlotLine(coordC, coordD, pixelParams, gouraudParamsCD);
    VDP1PlotLine(coordD, coordA, pixelParams, gouraudParamsDA);
}

void VDP::VDP1Cmd_DrawLine(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    const VDP1Command::DrawMode mode{.u16 = VDP1ReadVRAM<uint16>(cmdAddress + 0x04)};

    const uint16 color = VDP1ReadVRAM<uint16>(cmdAddress + 0x06);
    const sint32 xa = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C)) + ctx.localCoordX;
    const sint32 ya = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E)) + ctx.localCoordY;
    const sint32 xb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x10)) + ctx.localCoordX;
    const sint32 yb = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x12)) + ctx.localCoordY;
    const uint32 gouraudTable = static_cast<uint32>(VDP1ReadVRAM<uint16>(cmdAddress + 0x1C)) << 3u;

    const CoordS32 coordA{xa, ya};
    const CoordS32 coordB{xb, yb};

    renderLog1.trace("Draw line: {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}", xa, ya, xb, yb,
                     color, gouraudTable, mode.u16);

    if (VDP1IsLineSystemClipped(coordA, coordB)) {
        return;
    }

    const VDP1PixelParams pixelParams{
        .mode = mode,
        .color = color,
    };

    VDP1GouraudParams gouraudParams{
        .colorA{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 0u)},
        .colorB{.u16 = VDP1ReadVRAM<uint16>(gouraudTable + 2u)},
        .V = 0,
    };

    VDP1PlotLine(coordA, coordB, pixelParams, gouraudParams);
}

void VDP::VDP1Cmd_SetSystemClipping(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.sysClipH = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
    ctx.sysClipV = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));
    renderLog1.trace("Set system clipping: {}x{}", ctx.sysClipH, ctx.sysClipV);
}

void VDP::VDP1Cmd_SetUserClipping(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.userClipX0 = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    ctx.userClipY0 = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    ctx.userClipX1 = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
    ctx.userClipY1 = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));
    renderLog1.trace("Set user clipping: {}x{} - {}x{}", ctx.userClipX0, ctx.userClipY0, ctx.userClipX1,
                     ctx.userClipY1);
}

void VDP::VDP1Cmd_SetLocalCoordinates(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.localCoordX = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    ctx.localCoordY = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    renderLog1.trace("Set local coordinates: {}x{}", ctx.localCoordX, ctx.localCoordY);
}

// -----------------------------------------------------------------------------
// VDP2

void VDP::VDP2InitFrame() {
    if (m_VDP2.bgEnabled[5]) {
        VDP2InitRotationBG<0>();
        VDP2InitRotationBG<1>();
    } else {
        VDP2InitRotationBG<0>();
        VDP2InitNormalBG<0>();
        VDP2InitNormalBG<1>();
        VDP2InitNormalBG<2>();
        VDP2InitNormalBG<3>();
    }
}

template <uint32 index>
FORCE_INLINE void VDP::VDP2InitNormalBG() {
    static_assert(index < 4, "Invalid NBG index");

    if (!m_VDP2.bgEnabled[index]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[index + 1];
    NormBGLayerState &bgState = m_normBGLayerStates[index];
    bgState.fracScrollX = bgParams.scrollAmountH;
    bgState.fracScrollY = bgParams.scrollAmountV;
    bgState.scrollIncH = bgParams.scrollIncH;
    bgState.mosaicCounterY = 0;
    if constexpr (index < 2) {
        bgState.lineScrollTableAddress = bgParams.lineScrollTableAddress;
    }
}

template <uint32 index>
FORCE_INLINE void VDP::VDP2InitRotationBG() {
    static_assert(index < 2, "Invalid RBG index");

    if (!m_VDP2.bgEnabled[index + 4]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[index];
    const bool cellSizeShift = bgParams.cellSizeShift;
    const bool twoWordChar = bgParams.twoWordChar;

    for (int param = 0; param < 2; param++) {
        const RotationParams &rotParam = m_VDP2.rotParams[param];
        auto &pageBaseAddresses = m_rotParamStates[param].pageBaseAddresses;
        const uint16 plsz = rotParam.plsz;
        for (int plane = 0; plane < 16; plane++) {
            const uint32 mapIndex = rotParam.mapIndices[plane];
            pageBaseAddresses[plane] = CalcPageBaseAddress(cellSizeShift, twoWordChar, plsz, mapIndex);
        }
    }
}

void VDP::VDP2UpdateEnabledBGs() {
    // Sprite layer is always enabled
    m_layerStates[0].enabled = true;

    if (m_VDP2.bgEnabled[5]) {
        m_layerStates[1].enabled = true;  // RBG0
        m_layerStates[2].enabled = true;  // RBG1
        m_layerStates[3].enabled = false; // EXBG
        m_layerStates[4].enabled = false; // not used
        m_layerStates[5].enabled = false; // not used
    } else {
        m_layerStates[1].enabled = m_VDP2.bgEnabled[4]; // RBG0
        m_layerStates[2].enabled = m_VDP2.bgEnabled[0]; // NBG0
        m_layerStates[3].enabled = m_VDP2.bgEnabled[1]; // NBG1/EXBG
        m_layerStates[4].enabled = m_VDP2.bgEnabled[2]; // NBG2
        m_layerStates[5].enabled = m_VDP2.bgEnabled[3]; // NBG3
    }
}

void VDP::VDP2UpdateLineScreenScroll(const BGParams &bgParams, NormBGLayerState &bgState) {
    auto read = [&] {
        const uint32 address = bgState.lineScrollTableAddress & 0x7FFFF;
        const uint32 value = VDP2ReadVRAM<uint32>(address);
        bgState.lineScrollTableAddress += sizeof(uint32);
        return value;
    };

    if ((m_VCounter & ~(~0 << bgParams.lineScrollInterval)) == 0) {
        if (bgParams.lineScrollXEnable) {
            bgState.fracScrollX = bit::extract<8, 26>(read());
        }
        if (bgParams.lineScrollYEnable) {
            bgState.fracScrollY = bit::extract<8, 26>(read());
        }
        if (bgParams.lineZoomEnable) {
            bgState.scrollIncH = bit::extract<8, 18>(read());
        }
    }
}

void VDP::VDP2LoadRotationParameterTables() {
    const uint32 baseAddress = m_VDP2.commonRotParams.baseAddress;
    const bool readAll = m_VCounter == 0;

    for (int i = 0; i < 2; i++) {
        RotationParams &params = m_VDP2.rotParams[i];
        RotationParamState &state = m_rotParamStates[i];

        const bool readXst = readAll || params.readXst;
        const bool readYst = readAll || params.readYst;
        const bool readKAst = readAll || params.readKAst;

        // Tables are located at the base address 0x80 bytes apart
        RotationParamTable t{};
        const uint32 address = baseAddress + i * 0x80;
        t.ReadFrom(&m_VRAM2[address & 0x7FFFF]);

        // Calculate parameters

        // Transformed starting screen coordinates
        // 16*(16-16) + 16*(16-16) + 16*(16-16) = 32 frac bits
        // reduce to 16 frac bits
        const sint64 Xsp = (t.A * (t.Xst - t.Px) + t.B * (t.Yst - t.Py) + t.C * (t.Zst - t.Pz)) >> 16ll;
        const sint64 Ysp = (t.D * (t.Xst - t.Px) + t.E * (t.Yst - t.Py) + t.F * (t.Zst - t.Pz)) >> 16ll;

        // Transformed view coordinates
        // 16*(16-16) + 16*(16-16) + 16*(16-16) + 16 + 16 = 32+32+32 + 16+16
        // reduce 32 to 16 frac bits, result is 16 frac bits
        /***/ sint64 Xp = ((t.A * (t.Px - t.Cx) + t.B * (t.Py - t.Cy) + t.C * (t.Pz - t.Cz)) >> 16ll) + t.Cx + t.Mx;
        const sint64 Yp = ((t.D * (t.Px - t.Cx) + t.E * (t.Py - t.Cy) + t.F * (t.Pz - t.Cz)) >> 16ll) + t.Cy + t.My;

        // Screen coordinate increments per Vcnt
        // 16*16 + 16*16 = 32
        // reduce to 16 frac bits
        const sint64 scrXIncV = (t.A * t.deltaXst + t.B * t.deltaYst) >> 16ll;
        const sint64 scrYIncV = (t.D * t.deltaXst + t.E * t.deltaYst) >> 16ll;

        // Screen coordinate increments per Hcnt
        // 16*16 + 16*16 = 32 frac bits
        // reduce to 16 frac bits
        const sint64 scrXIncH = (t.A * t.deltaX + t.B * t.deltaY) >> 16ll;
        const sint64 scrYIncH = (t.D * t.deltaX + t.E * t.deltaY) >> 16ll;

        // Scaling factors
        // 16 frac bits
        sint64 kx = t.kx;
        sint64 ky = t.ky;

        // Current screen coordinates (16 frac bits) and coefficient address (10 frac bits)
        sint32 scrX = state.scrX;
        sint32 scrY = state.scrY;
        uint32 KA = state.KA;

        // Precompute whole line
        for (uint32 x = 0; x < m_HRes; x++) {
            if (x == 0) {
                if (readKAst) {
                    KA = state.KA = t.KAst;
                }
            }

            // Replace parameters with those obtained from the coefficient table if enabled
            if (params.coeffTableEnable) {
                const Coefficient coeff = VDP2FetchRotationCoefficient(params, KA);
                state.lineColorData[x] = coeff.lineColorData;
                state.transparent[x] = coeff.transparent;

                using enum CoefficientDataMode;
                switch (params.coeffDataMode) {
                case ScaleCoeffXY: kx = ky = coeff.value; break;
                case ScaleCoeffX: kx = coeff.value; break;
                case ScaleCoeffY: ky = coeff.value; break;
                case ViewpointX: Xp = coeff.value; break;
                }

                // Increment coefficient table address by Hcnt
                KA += t.dKAx;
            }

            if (x == 0) {
                if (readXst) {
                    scrX = state.scrX = Xsp;
                }
                if (readYst) {
                    scrY = state.scrY = Ysp;
                }
            }

            // Store screen coordinates
            state.screenCoords[x].x = ((kx * scrX) >> 16ll) + Xp;
            state.screenCoords[x].y = ((ky * scrY) >> 16ll) + Yp;

            // Increment screen coordinates and coefficient table address by Hcnt
            scrX += scrXIncH;
            scrY += scrYIncH;
        }

        // Increment screen coordinates and coefficient table address by Vcnt for the next iteration
        state.scrX += scrXIncV;
        state.scrY += scrYIncV;
        state.KA += t.dKAst;

        // Disable read flags now that we've dealt with them
        params.readXst = false;
        params.readYst = false;
        params.readKAst = false;
    }
}

void VDP::VDP2DrawLine() {
    renderLog2.trace("Drawing line {}", m_VCounter);

    using FnDrawLayer = void (VDP::*)();

    // Lookup table of sprite drawing functions
    // Indexing: [colorMode]
    static constexpr auto fnDrawSprite = [] {
        std::array<FnDrawLayer, 4> arr{};

        util::constexpr_for<4>([&](auto index) {
            const uint32 cmIndex = bit::extract<0, 1>(index());

            const uint32 colorMode = cmIndex <= 2 ? cmIndex : 2;
            arr[cmIndex] = &VDP::VDP2DrawSpriteLayer<colorMode>;
        });

        return arr;
    }();

    const uint32 colorMode = m_VDP2.RAMCTL.CRMDn;

    // Load rotation parameters if requested
    VDP2LoadRotationParameterTables();

    // Draw line color and back screen layers
    VDP2DrawLineColorAndBackScreens();

    // Draw sprite layer
    (this->*fnDrawSprite[colorMode])();

    // Draw background layers
    if (m_VDP2.bgEnabled[5]) {
        VDP2DrawRotationBG<0>(colorMode); // RBG0
        VDP2DrawRotationBG<1>(colorMode); // RBG1
    } else {
        VDP2DrawRotationBG<0>(colorMode); // RBG0
        VDP2DrawNormalBG<0>(colorMode);   // NBG0
        VDP2DrawNormalBG<1>(colorMode);   // NBG1
        VDP2DrawNormalBG<2>(colorMode);   // NBG2
        VDP2DrawNormalBG<3>(colorMode);   // NBG3
    }

    // Compose image
    VDP2ComposeLine();
}

void VDP::VDP2DrawLineColorAndBackScreens() {
    const LineBackScreenParams &lineParams = m_VDP2.lineScreenParams;
    const LineBackScreenParams &backParams = m_VDP2.backScreenParams;

    const uint32 y = m_VCounter;

    // Read line color screen color
    {
        const uint32 line = lineParams.perLine ? y : 0;
        const uint32 address = lineParams.baseAddress + line * sizeof(uint16);
        const uint32 cramAddress = VDP2ReadVRAM<uint16>(address) * sizeof(uint16);
        const Color555 color555{.u16 = VDP2ReadCRAM<uint16>(cramAddress)};
        m_lineBackLayerState.lineColor = ConvertRGB555to888(color555);
    }

    // Read back screen color
    {
        const uint32 line = backParams.perLine ? y : 0;
        const uint32 address = backParams.baseAddress + line * sizeof(Color555);
        const Color555 color555{.u16 = VDP2ReadVRAM<uint16>(address)};
        m_lineBackLayerState.backColor = ConvertRGB555to888(color555);
    }
}

template <uint32 colorMode>
NO_INLINE void VDP::VDP2DrawSpriteLayer() {
    const uint32 y = m_VCounter;

    for (uint32 x = 0; x < m_HRes; x++) {
        const auto &spriteFB = VDP1GetDisplayFB();
        const uint32 spriteFBOffset = x + y * m_VDP1.fbSizeH;

        const SpriteParams &params = m_VDP2.spriteParams;
        auto &pixel = m_layerStates[0].pixels[x];
        auto &attr = m_spriteLayerState.attrs[x];

        bool isPaletteData = true;
        if (params.mixedFormat) {
            const uint16 spriteDataValue = util::ReadBE<uint16>(&spriteFB[(spriteFBOffset * sizeof(uint16)) & 0x3FFFE]);
            if (bit::extract<15>(spriteDataValue)) {
                // RGB data
                pixel.color = ConvertRGB555to888(Color555{spriteDataValue});
                pixel.transparent = false;
                pixel.priority = params.priorities[0];
                attr.msbSet = pixel.color.msb;
                attr.colorCalcRatio = params.colorCalcRatios[0];
                attr.shadowOrWindow = false;
                attr.normalShadow = false;
                isPaletteData = false;
            }
        }

        if (isPaletteData) {
            // Palette data
            const SpriteData spriteData = VDP2FetchSpriteData(spriteFBOffset);
            const uint32 colorIndex = params.colorDataOffset + spriteData.colorData;
            pixel.color = VDP2FetchCRAMColor<colorMode>(0, colorIndex);
            pixel.transparent = spriteData.colorData == 0;
            pixel.priority = params.priorities[spriteData.priority];
            attr.msbSet = spriteData.colorDataMSB;
            attr.colorCalcRatio = params.colorCalcRatios[spriteData.colorCalcRatio];
            attr.shadowOrWindow = spriteData.shadowOrWindow;
            attr.normalShadow = spriteData.normalShadow;
        }
    }
}

template <uint32 bgIndex>
FORCE_INLINE void VDP::VDP2DrawNormalBG(uint32 colorMode) {
    static_assert(bgIndex < 4, "Invalid NBG index");

    using FnDraw = void (VDP::*)(const BGParams &, LayerState &, NormBGLayerState &);

    // Lookup table of scroll BG drawing functions
    // Indexing: [charMode][fourCellChar][colorFormat][colorMode]
    static constexpr auto fnDrawScroll = [] {
        std::array<std::array<std::array<std::array<FnDraw, 4>, 8>, 2>, 3> arr{};

        util::constexpr_for<3 * 2 * 8 * 4>([&](auto index) {
            const uint32 fcc = bit::extract<0>(index());
            const uint32 cf = bit::extract<1, 3>(index());
            const uint32 clm = bit::extract<4, 5>(index());
            const uint32 chm = bit::extract<6, 7>(index());

            const CharacterMode chmEnum = static_cast<CharacterMode>(chm);
            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = clm <= 2 ? clm : 2;
            arr[chm][fcc][cf][clm] = &VDP::VDP2DrawNormalScrollBG<chmEnum, fcc, cfEnum, colorMode>;
        });

        return arr;
    }();

    // Lookup table of bitmap BG drawing functions
    // Indexing: [colorFormat]
    static constexpr auto fnDrawBitmap = [] {
        std::array<std::array<FnDraw, 4>, 8> arr{};

        util::constexpr_for<8 * 4>([&](auto index) {
            const uint32 cf = bit::extract<0, 2>(index());
            const uint32 cm = bit::extract<3, 4>(index());

            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = cm <= 2 ? cm : 2;
            arr[cf][cm] = &VDP::VDP2DrawNormalBitmapBG<cfEnum, colorMode>;
        });

        return arr;
    }();

    if (!m_VDP2.bgEnabled[bgIndex]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[bgIndex + 1];
    LayerState &layerState = m_layerStates[bgIndex + 2];
    NormBGLayerState &bgState = m_normBGLayerStates[bgIndex];

    if constexpr (bgIndex < 2) {
        VDP2UpdateLineScreenScroll(bgParams, bgState);
    }

    const uint32 cf = static_cast<uint32>(bgParams.colorFormat);
    if (bgParams.bitmap) {
        (this->*fnDrawBitmap[cf][colorMode])(bgParams, layerState, bgState);
    } else {
        const bool twc = bgParams.twoWordChar;
        const bool fcc = bgParams.cellSizeShift;
        const bool exc = bgParams.extChar;
        const uint32 chm = static_cast<uint32>(twc   ? CharacterMode::TwoWord
                                               : exc ? CharacterMode::OneWordExtended
                                                     : CharacterMode::OneWordStandard);
        (this->*fnDrawScroll[chm][fcc][cf][colorMode])(bgParams, layerState, bgState);
    }

    bgState.mosaicCounterY++;
    if (bgState.mosaicCounterY >= m_VDP2.mosaicV) {
        bgState.mosaicCounterY = 0;
    }
}

template <uint32 bgIndex>
FORCE_INLINE void VDP::VDP2DrawRotationBG(uint32 colorMode) {
    static_assert(bgIndex < 2, "Invalid RBG index");

    using FnDraw = void (VDP::*)(const BGParams &, LayerState &);

    // Lookup table of scroll BG drawing functions
    // Indexing: [charMode][fourCellChar][colorFormat][colorMode]
    static constexpr auto fnDrawScroll = [] {
        std::array<std::array<std::array<std::array<FnDraw, 4>, 8>, 2>, 3> arr{};

        util::constexpr_for<3 * 2 * 8 * 4>([&](auto index) {
            const uint32 fcc = bit::extract<0>(index());
            const uint32 cf = bit::extract<1, 3>(index());
            const uint32 clm = bit::extract<4, 5>(index());
            const uint32 chm = bit::extract<6, 7>(index());

            const CharacterMode chmEnum = static_cast<CharacterMode>(chm);
            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = clm <= 2 ? clm : 2;
            arr[chm][fcc][cf][clm] = &VDP::VDP2DrawRotationScrollBG<bgIndex == 0, chmEnum, fcc, cfEnum, colorMode>;
        });

        return arr;
    }();

    // Lookup table of bitmap BG drawing functions
    // Indexing: [colorFormat][colorMode]
    static constexpr auto fnDrawBitmap = [] {
        std::array<std::array<FnDraw, 4>, 8> arr{};

        util::constexpr_for<8 * 4>([&](auto index) {
            const uint32 cf = bit::extract<0, 2>(index());
            const uint32 cm = bit::extract<3, 4>(index());

            const ColorFormat cfEnum = static_cast<ColorFormat>(cf <= 4 ? cf : 4);
            const uint32 colorMode = cm <= 2 ? cm : 2;
            arr[cf][cm] = &VDP::VDP2DrawRotationBitmapBG<bgIndex == 0, cfEnum, colorMode>;
        });

        return arr;
    }();

    if (!m_VDP2.bgEnabled[bgIndex + 4]) {
        return;
    }

    const BGParams &bgParams = m_VDP2.bgParams[bgIndex];
    LayerState &layerState = m_layerStates[bgIndex + 1];

    const uint32 cf = static_cast<uint32>(bgParams.colorFormat);
    if (bgParams.bitmap) {
        (this->*fnDrawBitmap[cf][colorMode])(bgParams, layerState);
    } else {
        const bool twc = bgParams.twoWordChar;
        const bool fcc = bgParams.cellSizeShift;
        const bool exc = bgParams.extChar;
        const uint32 chm = static_cast<uint32>(twc   ? CharacterMode::TwoWord
                                               : exc ? CharacterMode::OneWordExtended
                                                     : CharacterMode::OneWordStandard);
        (this->*fnDrawScroll[chm][fcc][cf][colorMode])(bgParams, layerState);
    }
}

FORCE_INLINE void VDP::VDP2ComposeLine() {
    if (m_framebuffer == nullptr) {
        return;
    }

    const uint32 y = m_VCounter;

    if (!m_VDP2.TVMD.DISP) {
        std::fill_n(&m_framebuffer[y * m_HRes], m_HRes, 0);
        return;
    }

    for (uint32 x = 0; x < m_HRes; x++) {
        std::array<Layer, 3> layers = {LYR_Back, LYR_Back, LYR_Back};
        std::array<uint8, 3> layerPrios = {0, 0, 0};

        // Determine layer order
        for (int layer = 0; layer < m_layerStates.size(); layer++) {
            const LayerState &state = m_layerStates[layer];
            if (!state.enabled) {
                continue;
            }

            const Pixel &pixel = state.pixels[x];
            if (pixel.transparent) {
                continue;
            }
            if (pixel.priority == 0) {
                continue;
            }
            if (layer == LYR_Sprite) {
                const auto &attr = m_spriteLayerState.attrs[x];
                if (attr.normalShadow) {
                    continue;
                }
            }

            // Insert the layer into the appropriate position in the stack
            // - Higher priority beats lower priority
            // - If same priority, lower Layer index beats higher Layer index
            // - layers[0] is topmost (first) layer
            for (int i = 0; i < 3; i++) {
                if (pixel.priority > layerPrios[i] || (pixel.priority == layerPrios[i] && layer < layers[i])) {
                    // Push layers back
                    for (int j = 2; j > i; j--) {
                        layers[j] = layers[j - 1];
                        layerPrios[j] = layerPrios[j - 1];
                    }
                    layers[i] = static_cast<Layer>(layer);
                    layerPrios[i] = pixel.priority;
                    break;
                }
            }
        }

        // Retrieves the color of the given layer and applies color offset
        auto getLayerColor = [&](Layer layer) -> Color888 {
            bool colorOffsetEnable{};
            bool colorOffsetSelect{};
            Color888 color{};

            if (layer == LYR_Back) {
                const LineBackScreenParams &backParams = m_VDP2.backScreenParams;
                color = m_lineBackLayerState.backColor;
                colorOffsetEnable = backParams.colorOffsetEnable;
                colorOffsetSelect = backParams.colorOffsetSelect;
            } else {
                const LayerState &state = m_layerStates[layer];
                const Pixel &pixel = state.pixels[x];
                color = pixel.color;
                if (layer == LYR_Sprite) {
                    const SpriteParams &spriteParams = m_VDP2.spriteParams;
                    colorOffsetEnable = spriteParams.colorOffsetEnable;
                    colorOffsetSelect = spriteParams.colorOffsetSelect;
                } else {
                    const BGParams &bgParams = m_VDP2.bgParams[layer - LYR_RBG0];
                    colorOffsetEnable = bgParams.colorOffsetEnable;
                    colorOffsetSelect = bgParams.colorOffsetSelect;
                }
            }

            // Apply color offset if enabled
            if (colorOffsetEnable) {
                const auto &colorOffset = m_VDP2.colorOffsetParams[colorOffsetSelect];
                color.r = std::clamp(color.r + colorOffset.r, 0, 255);
                color.g = std::clamp(color.g + colorOffset.g, 0, 255);
                color.b = std::clamp(color.b + colorOffset.b, 0, 255);
            }
            return color;
        };

        auto isColorCalcEnabled = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                const SpriteParams &spriteParams = m_VDP2.spriteParams;
                if (!spriteParams.colorCalcEnable) {
                    return false;
                }

                const Pixel &pixel = m_layerStates[LYR_Sprite].pixels[x];

                using enum SpriteColorCalculationCondition;
                switch (spriteParams.colorCalcCond) {
                case PriorityLessThanOrEqual: return pixel.priority <= spriteParams.colorCalcValue;
                case PriorityEqual: return pixel.priority == spriteParams.colorCalcValue;
                case PriorityGreaterThanOrEqual: return pixel.priority >= spriteParams.colorCalcValue;
                case MsbEqualsOne: return m_spriteLayerState.attrs[x].msbSet;
                default: util::unreachable();
                }
            } else if (layer == LYR_Back) {
                return m_VDP2.backScreenParams.colorCalcEnable;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].colorCalcEnable;
            }
        };

        auto getColorCalcRatio = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                return m_spriteLayerState.attrs[x].colorCalcRatio;
            } else if (layer == LYR_Back) {
                return m_VDP2.backScreenParams.colorCalcRatio;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].colorCalcRatio;
            }
        };

        auto isLineColorEnabled = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                return m_VDP2.spriteParams.lineColorScreenEnable;
            } else if (layer == LYR_Back) {
                return false;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].lineColorScreenEnable;
            }
        };

        auto isShadowEnabled = [&](Layer layer) {
            if (layer == LYR_Sprite) {
                return m_spriteLayerState.attrs[x].shadowOrWindow;
            } else if (layer == LYR_Back) {
                return m_VDP2.backScreenParams.shadowEnable;
            } else {
                return m_VDP2.bgParams[layer - LYR_RBG0].shadowEnable;
            }
        };

        const bool isTopLayerColorCalcEnabled = [&] {
            if (!isColorCalcEnabled(layers[0])) {
                return false;
            }
            if (VDP2IsInsideWindow(m_VDP2.colorCalcParams.windowSet, x)) {
                return false;
            }
            if (layers[0] == LYR_Back || layers[0] == LYR_Sprite) {
                return true;
            }
            return m_layerStates[layers[0]].pixels[x].specialColorCalc;
        }();

        const auto &colorCalcParams = m_VDP2.colorCalcParams;

        // Calculate color
        Color888 outputColor{};
        if (isTopLayerColorCalcEnabled) {
            const Color888 topColor = getLayerColor(layers[0]);
            Color888 btmColor = getLayerColor(layers[1]);

            // Apply extended color calculations (only in normal TV modes)
            const bool useExtendedColorCalc = colorCalcParams.extendedColorCalcEnable && m_VDP2.TVMD.HRESOn < 2;
            if (useExtendedColorCalc) {
                // TODO: honor color RAM mode + palette/RGB format restrictions
                // - modes 1 and 2 don't blend layers if the bottom layer uses palette color

                // HACK: assuming color RAM mode 0 for now (aka no restrictions)
                if (isColorCalcEnabled(layers[1])) {
                    const Color888 l2Color = getLayerColor(layers[2]);
                    btmColor.r = (btmColor.r + l2Color.r) / 2;
                    btmColor.g = (btmColor.g + l2Color.g) / 2;
                    btmColor.b = (btmColor.b + l2Color.b) / 2;
                }
            }

            // Insert and blend line color screen if top layer uses it
            if (isLineColorEnabled(layers[0])) {
                const Color888 lineColor = m_lineBackLayerState.lineColor;
                if (useExtendedColorCalc) {
                    btmColor.r = (lineColor.r + btmColor.r) / 2;
                    btmColor.g = (lineColor.g + btmColor.g) / 2;
                    btmColor.b = (lineColor.b + btmColor.b) / 2;
                } else {
                    const uint8 ratio = m_VDP2.lineScreenParams.colorCalcRatio;
                    const uint8 complRatio = 32 - ratio;
                    btmColor.r = (lineColor.r * complRatio + btmColor.r * ratio) / 32;
                    btmColor.g = (lineColor.g * complRatio + btmColor.g * ratio) / 32;
                    btmColor.b = (lineColor.b * complRatio + btmColor.b * ratio) / 32;
                }
            }

            // Blend top and blended bottom layers
            if (colorCalcParams.useAdditiveBlend) {
                outputColor.r = std::min(topColor.r + btmColor.r, 255);
                outputColor.g = std::min(topColor.g + btmColor.g, 255);
                outputColor.b = std::min(topColor.b + btmColor.b, 255);
            } else {
                const uint8 ratio = getColorCalcRatio(colorCalcParams.useSecondScreenRatio ? layers[1] : layers[0]);
                const uint8 complRatio = 32 - ratio;
                outputColor.r = (topColor.r * complRatio + btmColor.r * ratio) / 32;
                outputColor.g = (topColor.g * complRatio + btmColor.g * ratio) / 32;
                outputColor.b = (topColor.b * complRatio + btmColor.b * ratio) / 32;
            }
        } else {
            outputColor = getLayerColor(layers[0]);
        }

        // Apply sprite shadow
        if (isShadowEnabled(layers[0])) {
            const bool isNormalShadow = m_spriteLayerState.attrs[x].normalShadow;
            const bool isMSBShadow =
                !m_VDP2.spriteParams.spriteWindowEnable && m_spriteLayerState.attrs[x].shadowOrWindow;
            if (isNormalShadow || isMSBShadow) {
                outputColor.r >>= 1u;
                outputColor.g >>= 1u;
                outputColor.b >>= 1u;
            }
        }

        m_framebuffer[x + y * m_HRes] = outputColor.u32;
    }
}

template <VDP::CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawNormalScrollBG(const BGParams &bgParams, LayerState &layerState,
                                           NormBGLayerState &bgState) {
    uint32 fracScrollX = bgState.fracScrollX;
    const uint32 fracScrollY = bgState.fracScrollY;
    bgState.fracScrollY += bgParams.scrollIncV;

    uint32 cellScrollTableAddress = m_VDP2.verticalCellScrollTableAddress;

    auto readCellScrollY = [&] {
        const uint32 value = VDP2ReadVRAM<uint32>(cellScrollTableAddress);
        cellScrollTableAddress += sizeof(uint32);
        return bit::extract<8, 26>(value);
    };

    uint8 mosaicCounterX = 0;
    uint32 cellScrollY = 0;

    if (bgParams.verticalCellScrollEnable) {
        // Read first vertical scroll amount if scrolled partway through a cell at the start of the line
        if (((fracScrollX >> 8u) & 7) != 0) {
            cellScrollY = readCellScrollY();
        }
    }

    for (uint32 x = 0; x < m_HRes; x++) {
        // Apply vertical cell-scrolling or horizontal mosaic
        if (bgParams.verticalCellScrollEnable) {
            // Update vertical cell scroll amount
            if (((fracScrollX >> 8u) & 7) == 0) {
                cellScrollY = readCellScrollY();
            }
        } else if (bgParams.mosaicEnable) {
            // Apply horizontal mosaic
            // TODO: should mosaic have priority over vertical cell scroll?
            const uint8 currMosaicCounterX = mosaicCounterX;
            mosaicCounterX++;
            if (mosaicCounterX >= m_VDP2.mosaicH) {
                mosaicCounterX = 0;
            }
            if (currMosaicCounterX > 0) {
                // Simply copy over the data from the previous pixel
                layerState.pixels[x] = layerState.pixels[x - 1];

                // Increment horizontal coordinate
                fracScrollX += bgState.scrollIncH;
                continue;
            }
        }

        if (VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside active window area
            layerState.pixels[x].transparent = true;
        } else {
            // Compute integer scroll screen coordinates
            const uint32 scrollX = fracScrollX >> 8u;
            const uint32 scrollY = ((fracScrollY + cellScrollY) >> 8u) - bgState.mosaicCounterY;
            const CoordU32 scrollCoord{scrollX, scrollY};

            // Plot pixel
            layerState.pixels[x] = VDPFetchScrollBGPixel<false, charMode, fourCellChar, colorFormat, colorMode>(
                bgParams, bgParams.pageBaseAddresses, scrollCoord);
        }

        // Increment horizontal coordinate
        fracScrollX += bgState.scrollIncH;
    }
}

template <ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawNormalBitmapBG(const BGParams &bgParams, LayerState &layerState,
                                           NormBGLayerState &bgState) {
    uint32 fracScrollX = bgState.fracScrollX;
    const uint32 fracScrollY = bgState.fracScrollY;
    bgState.fracScrollY += bgParams.scrollIncV;

    uint32 cellScrollTableAddress = m_VDP2.verticalCellScrollTableAddress;

    auto readCellScrollY = [&] {
        const uint32 value = VDP2ReadVRAM<uint32>(cellScrollTableAddress);
        cellScrollTableAddress += sizeof(uint32);
        return bit::extract<8, 26>(value);
    };

    uint32 mosaicCounterX = 0;
    uint32 cellScrollY = 0;

    for (uint32 x = 0; x < m_HRes; x++) {
        // Update vertical cell scroll amount
        if (bgParams.verticalCellScrollEnable) {
            if (((fracScrollX >> 8u) & 7) == 0) {
                cellScrollY = readCellScrollY();
            }
        } else if (bgParams.mosaicEnable) {
            // Apply horizontal mosaic
            // TODO: should mosaic have priority over vertical cell scroll?
            const uint8 currMosaicCounterX = mosaicCounterX;
            mosaicCounterX++;
            if (mosaicCounterX >= m_VDP2.mosaicH) {
                mosaicCounterX = 0;
            }
            if (currMosaicCounterX > 0) {
                // Simply copy over the data from the previous pixel
                layerState.pixels[x] = layerState.pixels[x - 1];

                // Increment horizontal coordinate
                fracScrollX += bgState.scrollIncH;
                continue;
            }
        }

        if (VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside active window area
            layerState.pixels[x].transparent = true;
        } else {
            // Compute integer scroll screen coordinates
            const uint32 scrollX = fracScrollX >> 8u;
            const uint32 scrollY = ((fracScrollY + cellScrollY) >> 8u) - bgState.mosaicCounterY;
            const CoordU32 scrollCoord{scrollX, scrollY};

            // Plot pixel
            layerState.pixels[x] = VDP2FetchBitmapPixel<colorFormat, colorMode>(bgParams, scrollCoord);
        }

        // Increment horizontal coordinate
        fracScrollX += bgState.scrollIncH;
    }
}

template <bool selRotParam, VDP::CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawRotationScrollBG(const BGParams &bgParams, LayerState &layerState) {
    const CommonRotationParams &commonRotParams = m_VDP2.commonRotParams;

    for (uint32 x = 0; x < m_HRes; x++) {
        auto &pixel = layerState.pixels[x];

        const RotParamSelector rotParamSelector = selRotParam ? VDP2SelectRotationParameter(bgParams, x) : RotParamA;

        const RotationParams &rotParams = m_VDP2.rotParams[rotParamSelector];
        const RotationParamState &rotParamState = m_rotParamStates[rotParamSelector];

        // Handle transparent pixels in coefficient table
        if ((!selRotParam || commonRotParams.rotParamMode != RotationParamMode::Coefficient) &&
            rotParams.coeffTableEnable && rotParamState.transparent[x]) {
            pixel.transparent = true;
            continue;
        }

        const sint32 fracScrollX = rotParamState.screenCoords[x].x;
        const sint32 fracScrollY = rotParamState.screenCoords[x].y;

        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 16u;
        const uint32 scrollY = fracScrollY >> 16u;
        const CoordU32 scrollCoord{scrollX, scrollY};

        if (commonRotParams.rotParamMode != RotationParamMode::Window && VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside a window and not using window-based rotation parameter selection
            pixel.transparent = true;
        } else {
            // Plot pixel
            pixel = VDPFetchScrollBGPixel<true, charMode, fourCellChar, colorFormat, colorMode>(
                bgParams, rotParamState.pageBaseAddresses, scrollCoord);
        }
    }
}

template <bool selRotParam, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawRotationBitmapBG(const BGParams &bgParams, LayerState &layerState) {
    const CommonRotationParams &commonRotParams = m_VDP2.commonRotParams;

    for (uint32 x = 0; x < m_HRes; x++) {
        auto &pixel = layerState.pixels[x];

        const RotParamSelector rotParamSelector = selRotParam ? VDP2SelectRotationParameter(bgParams, x) : RotParamA;

        const RotationParams &rotParams = m_VDP2.rotParams[rotParamSelector];
        const RotationParamState &rotParamState = m_rotParamStates[rotParamSelector];

        // Handle transparent pixels in coefficient table
        if ((!selRotParam || commonRotParams.rotParamMode != RotationParamMode::Coefficient) &&
            rotParams.coeffTableEnable && rotParamState.transparent[x]) {
            pixel.transparent = true;
            continue;
        }

        const sint32 fracScrollX = rotParamState.screenCoords[x].x;
        const sint32 fracScrollY = rotParamState.screenCoords[x].y;

        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 16u;
        const uint32 scrollY = fracScrollY >> 16u;
        const CoordU32 scrollCoord{scrollX, scrollY};

        if (commonRotParams.rotParamMode != RotationParamMode::Window && VDP2IsInsideWindow(bgParams.windowSet, x)) {
            // Make pixel transparent if inside a window and not using window-based rotation parameter selection
            pixel.transparent = true;
        } else {
            // Plot pixel
            pixel = VDP2FetchBitmapPixel<colorFormat, colorMode>(bgParams, scrollCoord);
        }
    }
}

VDP::RotParamSelector VDP::VDP2SelectRotationParameter(const BGParams &bgParams, uint32 x) {
    const CommonRotationParams &commonRotParams = m_VDP2.commonRotParams;

    using enum RotationParamMode;
    switch (commonRotParams.rotParamMode) {
    case RotationParamA: return RotParamA;
    case RotationParamB: return RotParamB;
    case Coefficient:
        return m_VDP2.rotParams[0].coeffTableEnable && m_rotParamStates[0].transparent[x] ? RotParamB : RotParamA;
    case Window: return VDP2IsInsideWindow(bgParams.windowSet, x) ? RotParamB : RotParamA;
    }
}

Coefficient VDP::VDP2FetchRotationCoefficient(const RotationParams &params, uint32 coeffAddress) {
    Coefficient coeff{};

    // Coefficient data formats:
    //
    // 1 word   15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // kx/ky   |TP|SN|Coeff. IP  | Coefficient fractional part |
    // Px      |TP|SN|Coefficient integer part            | FP |
    //
    // 2 words  31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // kx/ky   |TP| Line color data    |SN|Coeff. integer part |Coefficient fractional part                    |
    // Px      |TP| Line color data    |SN|Coefficient integer part                    |Coeff. fractional part |
    //
    // TP=transparent bit   SN=coefficient sign bit   IP=coefficient integer part   FP=coefficient fractional part

    const uint32 baseAddress = params.coeffTableAddressOffset;
    const uint32 offset = coeffAddress >> 10u;

    if (params.coeffDataSize == 1) {
        // One-word coefficient data
        const uint32 address = (baseAddress + offset) * sizeof(uint16);
        const uint16 data = m_VDP2.RAMCTL.CRKTE ? VDP2ReadCRAM<uint16>(address | 0x800) : VDP2ReadVRAM<uint16>(address);
        coeff.value = bit::extract_signed<0, 14>(data);
        coeff.lineColorData = 0;
        coeff.transparent = bit::extract<15>(data);

        if (params.coeffDataMode == CoefficientDataMode::ViewpointX) {
            coeff.value <<= 14;
        } else {
            coeff.value <<= 6;
        }
    } else {
        // Two-word coefficient data
        const uint32 address = (baseAddress + offset) * sizeof(uint32);
        const uint32 data = m_VDP2.RAMCTL.CRKTE ? VDP2ReadCRAM<uint32>(address | 0x800) : VDP2ReadVRAM<uint32>(address);
        coeff.value = bit::extract_signed<0, 23>(data);
        coeff.lineColorData = bit::extract<24, 30>(data);
        coeff.transparent = bit::extract<31>(data);

        if (params.coeffDataMode == CoefficientDataMode::ViewpointX) {
            coeff.value <<= 8;
        }
    }

    return coeff;
}

template <bool hasSpriteWindow>
bool VDP::VDP2IsInsideWindow(const WindowSet<hasSpriteWindow> &windowSet, uint32 x) {
    // If no windows are enabled, consider the pixel outside of windows
    if (!std::any_of(windowSet.enabled.begin(), windowSet.enabled.end(), std::identity())) {
        return false;
    }

    // Check normal windows
    for (int i = 0; i < 2; i++) {
        // Skip if disabled
        if (!windowSet.enabled[i]) {
            continue;
        }

        const WindowParams &windowParam = m_VDP2.windowParams[i];
        const bool inverted = windowSet.inverted[i];

        // Truth table: (state: false=outside, true=inside)
        // state  inverted  result   st != ao
        // false  false     outside  false
        // true   false     inside   true
        // false  true      inside   true
        // true   true      outside  false
        auto isInside = [&](bool state) { return state != inverted; };

        // Check vertical coordinate
        const bool insideY = isInside(m_VCounter >= windowParam.startY && m_VCounter <= windowParam.endY);

        uint16 startX = windowParam.startX;
        uint16 endX = windowParam.endX;

        // Read line window if enabled
        if (windowParam.lineWindowTableEnable) {
            const uint32 address = windowParam.lineWindowTableAddress + m_VCounter * sizeof(uint16) * 2;
            startX = bit::extract<0, 9>(VDP2ReadVRAM<uint16>(address + 0));
            endX = bit::extract<0, 9>(VDP2ReadVRAM<uint16>(address + 2));
        }

        // For normal screen modes, X coordinates don't use bit 0
        if (m_VDP2.TVMD.HRESOn < 2) {
            startX >>= 1;
            endX >>= 1;
        }

        // Check horizontal coordinate
        const bool insideX = isInside(x >= startX && x <= endX);

        // Short-circuit the output if the logic allows for it
        // true short-circuits OR logic
        // false short-circuits AND logic
        const bool inside = insideX && insideY;
        if (inside == (windowSet.logic == WindowLogic::Or)) {
            return inside;
        }
    }

    // Check sprite window
    if constexpr (hasSpriteWindow) {
        if (windowSet.enabled[2]) {
            const bool inverted = windowSet.inverted[2];
            return m_spriteLayerState.attrs[x].shadowOrWindow != inverted;
        }
    }

    // Return the appropriate value for the given logic mode.
    // If we got to this point using OR logic, then the pixel is outside all enabled windows.
    // If we got to this point using AND logic, then the pixel is inside all enabled windows.
    return windowSet.logic == WindowLogic::And;
}

template <bool rot, VDP::CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE VDP::Pixel VDP::VDPFetchScrollBGPixel(const BGParams &bgParams, std::span<const uint32> pageBaseAddresses,
                                                   CoordU32 scrollCoord) {
    //      Map (NBGs)              Map (RBGs)
    // +---------+---------+   +----+----+----+----+
    // |         |         |   | A  | B  | C  | D  |
    // | Plane A | Plane B |   +----+----+----+----+
    // |         |         |   | E  | F  | G  | H  |
    // +---------+---------+   +----+----+----+----+
    // |         |         |   | I  | J  | K  | L  |
    // | Plane C | Plane D |   +----+----+----+----+
    // |         |         |   | M  | N  | O  | P  |
    // +---------+---------+   +----+----+----+----+
    //
    // Normal and rotation BGs are divided into planes in the exact configurations illustrated above.
    // The BG's Map Offset Register is combined with the BG plane's Map Register (MPxxN#) to produce a base address for
    // each plane:
    //   Address bits  Source
    //            8-6  Map Offset Register (MPOFN)
    //            5-0  Map Register (MPxxN#)
    //
    // These addresses are precomputed in pageBaseAddresses.
    //
    //       2x2 Plane               2x1 Plane          1x1 Plane
    //        PLSZ=3                  PLSZ=1             PLSZ=0
    // +---------+---------+   +---------+---------+   +---------+
    // |         |         |   |         |         |   |         |
    // | Page 1  | Page 2  |   | Page 1  | Page 2  |   | Page 1  |
    // |         |         |   |         |         |   |         |
    // +---------+---------+   +---------+---------+   +---------+
    // |         |         |
    // | Page 3  | Page 4  |
    // |         |         |
    // +---------+---------+
    //
    // Each plane is composed of 1x1, 2x1 or 2x2 pages, determined by Plane Size in the Plane Size Register (PLSZ).
    // Pages are stored sequentially in VRAM left to right, top to bottom, as shown.
    //
    // The size is stored as a bit shift in bgParams.pageShiftH and bgParams.pageShiftV.
    //
    //        64x64 Page                 32x32 Page
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |CP 1|CP 2|  |CP63|CP64|   |CP 1|CP 2|  |CP31|CP32|
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |  65|  66|  | 127| 128|   |  33|  34|  |  63|  64|
    // +----+----+..+----+----+   +----+----+..+----+----+
    // :    :    :  :    :    :   :    :    :  :    :    :
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |3969|3970|  |4031|4032|   | 961| 962|  | 991| 992|
    // +----+----+..+----+----+   +----+----+..+----+----+
    // |4033|4034|  |4095|4096|   | 993| 994|  |1023|1024|
    // +----+----+..+----+----+   +----+----+..+----+----+
    //
    // Pages contain 32x32 or 64x64 character patterns, which are groups of 1x1 or 2x2 cells, determined by Character
    // Size in the Character Control Register (CHCTLA-B).
    //
    // Pages always contain a total of 64x64 cells - a grid of 64x64 1x1 character patterns or 32x32 2x2 character
    // patterns. Because of this, pages always have 512x512 dots.
    //
    // Character patterns in a page are stored sequentially in VRAM left to right, top to bottom, as shown above.
    //
    // fourCellChar specifies the size of the character patterns (1x1 when false, 2x2 when true) and, by extension, the
    // dimensions of the page (32x32 or 64x64 respectively).
    //
    // 2x2 Character Pattern     1x1 C.P.
    // +---------+---------+   +---------+
    // |         |         |   |         |
    // | Cell 1  | Cell 2  |   | Cell 1  |
    // |         |         |   |         |
    // +---------+---------+   +---------+
    // |         |         |
    // | Cell 3  | Cell 4  |
    // |         |         |
    // +---------+---------+
    //
    // Character patterns are groups of 1x1 or 2x2 cells, determined by Character Size in the Character Control Register
    // (CHCTLA-B).
    //
    // Cells are stored sequentially in VRAM left to right, top to bottom, as shown above.
    //
    // Character patterns contain a character number (15 bits), a palette number (7 bits, only used with 16 or 256 color
    // palette modes), two special function bits (Special Priority and Special Color Calculation) and two flip bits
    // (horizontal and vertical).
    //
    // Character patterns can be one or two words long, as defined by Pattern Name Data Size in the Pattern Name Control
    // Register (PNCN0-3, PNCR). When using one word characters, some of the data comes from supplementary registers.
    //
    // fourCellChar stores the character pattern size (1x1 when false, 2x2 when true).
    // twoWordChar determines if characters are one (false) or two (true) words long.
    // extChar determines the length of the character data field in one word characters --
    // when true, they're extended by two bits, taking over the two flip bits.
    //
    //           Cell
    // +--+--+--+--+--+--+--+--+
    // | 1| 2| 3| 4| 5| 6| 7| 8|
    // +--+--+--+--+--+--+--+--+
    // | 9|10|11|12|13|14|15|16|
    // +--+--+--+--+--+--+--+--+
    // |17|18|19|20|21|22|23|24|
    // +--+--+--+--+--+--+--+--+
    // |25|26|27|28|29|30|31|32|
    // +--+--+--+--+--+--+--+--+
    // |33|34|35|36|37|38|39|40|
    // +--+--+--+--+--+--+--+--+
    // |41|42|43|44|45|46|47|48|
    // +--+--+--+--+--+--+--+--+
    // |49|50|51|52|53|54|55|56|
    // +--+--+--+--+--+--+--+--+
    // |57|58|59|60|61|62|63|64|
    // +--+--+--+--+--+--+--+--+
    //
    // Cells contain 8x8 dots (pixels) in one of the following color formats:
    //   - 16 color palette
    //   - 256 color palette
    //   - 1024 or 2048 color palette (depending on Color Mode)
    //   - 5:5:5 RGB (32768 colors)
    //   - 8:8:8 RGB (16777216 colors)
    //
    // colorFormat specifies one of the color formats above.
    // colorMode determines the palette color format in CRAM, one of:
    //   - 16-bit 5:5:5 RGB, 1024 words
    //   - 16-bit 5:5:5 RGB, 2048 words
    //   - 32-bit 8:8:8 RGB, 1024 longwords

    static constexpr std::size_t planeMSB = rot ? 12 : 11;
    static constexpr uint32 planeWidth = rot ? 4u : 2u;
    static constexpr uint32 planeMask = planeWidth - 1;

    static constexpr bool twoWordChar = charMode == CharacterMode::TwoWord;
    static constexpr bool extChar = charMode == CharacterMode::OneWordExtended;

    auto [scrollX, scrollY] = scrollCoord;

    // Determine plane index from the scroll coordinates
    const uint32 planeX = (bit::extract<9, planeMSB>(scrollX) >> bgParams.pageShiftH) & planeMask;
    const uint32 planeY = (bit::extract<9, planeMSB>(scrollY) >> bgParams.pageShiftV) & planeMask;
    const uint32 plane = planeX + planeY * planeWidth;

    // Determine page index from the scroll coordinates
    const uint32 pageX = bit::extract<9>(scrollX) & bgParams.pageShiftH;
    const uint32 pageY = bit::extract<9>(scrollY) & bgParams.pageShiftV;
    const uint32 page = pageX + pageY * 2u;

    // Determine character pattern from the scroll coordinates
    const uint32 charPatX = bit::extract<3, 8>(scrollX) >> fourCellChar;
    const uint32 charPatY = bit::extract<3, 8>(scrollY) >> fourCellChar;
    const uint32 charIndex = charPatX + charPatY * (64u >> fourCellChar);

    // Determine cell index from the scroll coordinates
    const uint32 cellX = bit::extract<3>(scrollX) & fourCellChar;
    const uint32 cellY = bit::extract<3>(scrollY) & fourCellChar;
    const uint32 cellIndex = cellX + cellY * 2u;

    // Determine dot coordinates
    const uint32 dotX = bit::extract<0, 2>(scrollX);
    const uint32 dotY = bit::extract<0, 2>(scrollY);
    const CoordU32 dotCoord{dotX, dotY};

    // Fetch character
    const uint32 pageBaseAddress = pageBaseAddresses[plane];
    const uint32 pageOffset = page << kPageSizes[fourCellChar][twoWordChar];
    const uint32 pageAddress = pageBaseAddress + pageOffset;
    constexpr bool largePalette = colorFormat != ColorFormat::Palette16;
    const Character ch =
        twoWordChar ? VDP2FetchTwoWordCharacter(pageAddress, charIndex)
                    : VDP2FetchOneWordCharacter<fourCellChar, largePalette, extChar>(bgParams, pageAddress, charIndex);

    // Fetch pixel using character data
    return VDP2FetchCharacterPixel<colorFormat, colorMode>(bgParams, ch, dotCoord, cellIndex);
}

FORCE_INLINE VDP::Character VDP::VDP2FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex) {
    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint32);
    const uint32 charData = VDP2ReadVRAM<uint32>(charAddress);

    Character ch{};
    ch.charNum = bit::extract<0, 14>(charData);
    ch.palNum = bit::extract<16, 22>(charData);
    ch.specColorCalc = bit::extract<28>(charData);
    ch.specPriority = bit::extract<29>(charData);
    ch.flipH = bit::extract<30>(charData);
    ch.flipV = bit::extract<31>(charData);
    return ch;
}

template <bool fourCellChar, bool largePalette, bool extChar>
FORCE_INLINE VDP::Character VDP::VDP2FetchOneWordCharacter(const BGParams &bgParams, uint32 pageBaseAddress,
                                                           uint32 charIndex) {
    // Contents of 1 word character patterns vary based on Character Size, Character Color Count and Auxiliary Mode:
    //     Character Size        = CHCTLA/CHCTLB.xxCHSZ  = !fourCellChar = !FCC
    //     Character Color Count = CHCTLA/CHCTLB.xxCHCNn = largePalette  = LP
    //     Auxiliary Mode        = PNCN0/PNCR.xxCNSM     = extChar      = EC
    //             ---------------- Character data ----------------    Supplement in Pattern Name Control Register
    // FCC LP  EC  |15 14 13 12 11 10 9  8  7  6  5  4  3  2  1  0|    | 9  8  7  6  5  4  3  2  1  0|
    //  F   F   F  |palnum 3-0 |VF|HF| character number 9-0       |    |PR|CC| PN 6-4 |charnum 14-10 |
    //  F   T   F  |--| PN 6-4 |VF|HF| character number 9-0       |    |PR|CC|--------|charnum 14-10 |
    //  T   F   F  |palnum 3-0 |VF|HF| character number 11-2      |    |PR|CC| PN 6-4 |CN 14-12|CN1-0|
    //  T   T   F  |--| PN 6-4 |VF|HF| character number 11-2      |    |PR|CC|--------|CN 14-12|CN1-0|
    //  F   F   T  |palnum 3-0 |       character number 11-0      |    |PR|CC| PN 6-4 |CN 14-12|-----|
    //  F   T   T  |--| PN 6-4 |       character number 11-0      |    |PR|CC|--------|CN 14-12|-----|
    //  T   F   T  |palnum 3-0 |       character number 13-2      |    |PR|CC| PN 6-4 |cn|-----|CN1-0|   cn=CN14
    //  T   T   T  |--| PN 6-4 |       character number 13-2      |    |PR|CC|--------|cn|-----|CN1-0|   cn=CN14

    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint16);
    const uint16 charData = VDP2ReadVRAM<uint16>(charAddress);

    // Character number bit range from the 1-word character pattern data (charData)
    static constexpr uint32 baseCharNumStart = 0;
    static constexpr uint32 baseCharNumEnd = 9 + 2 * extChar;
    static constexpr uint32 baseCharNumPos = 2 * fourCellChar;

    // Upper character number bit range from the supplementary character number (bgParams.supplCharNum)
    static constexpr uint32 supplCharNumStart = 2 * fourCellChar + 2 * extChar;
    static constexpr uint32 supplCharNumEnd = 4;
    static constexpr uint32 supplCharNumPos = 10 + supplCharNumStart;
    // The lower bits are always in range 0..1 and only used if fourCellChar == true

    const uint32 baseCharNum = bit::extract<baseCharNumStart, baseCharNumEnd>(charData);
    const uint32 supplCharNum = bit::extract<supplCharNumStart, supplCharNumEnd>(bgParams.supplScrollCharNum);

    Character ch{};
    ch.charNum = (baseCharNum << baseCharNumPos) | (supplCharNum << supplCharNumPos);
    if constexpr (fourCellChar) {
        ch.charNum |= bit::extract<0, 1>(bgParams.supplScrollCharNum);
    }
    if constexpr (largePalette) {
        ch.palNum = bit::extract<12, 14>(charData) << 4u;
    } else {
        ch.palNum = bit::extract<12, 15>(charData) | bgParams.supplScrollPalNum;
    }
    ch.specColorCalc = bgParams.supplScrollSpecialColorCalc;
    ch.specPriority = bgParams.supplScrollSpecialPriority;
    ch.flipH = !extChar && bit::extract<10>(charData);
    ch.flipV = !extChar && bit::extract<11>(charData);
    return ch;
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE VDP::Pixel VDP::VDP2FetchCharacterPixel(const BGParams &bgParams, Character ch, CoordU32 dotCoord,
                                                     uint32 cellIndex) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");

    Pixel pixel{};

    auto [dotX, dotY] = dotCoord;

    assert(dotX < 8);
    assert(dotY < 8);

    // Flip dot coordinates if requested
    if (ch.flipH) {
        dotX ^= 7;
        cellIndex ^= 1;
    }
    if (ch.flipV) {
        dotY ^= 7;
        cellIndex ^= 2;
    }

    // Adjust cell index based on color format
    if constexpr (!IsPaletteColorFormat(colorFormat)) {
        cellIndex <<= 2;
    } else if constexpr (colorFormat != ColorFormat::Palette16) {
        cellIndex <<= 1;
    }

    // Cell addressing uses a fixed offset of 32 bytes
    const uint32 cellAddress = (ch.charNum + cellIndex) * 0x20;
    const uint32 dotOffset = dotX + dotY * 8;

    // Determine special color calculation flag
    const auto &specFuncCode = m_VDP2.specialFunctionCodes[bgParams.specialFunctionSelect];
    auto getSpecialColorCalcFlag = [&](uint8 colorData) {
        using enum SpecialColorCalcMode;
        switch (bgParams.specialColorCalcMode) {
        case PerScreen: return bgParams.colorCalcEnable;
        case PerCharacter: return bgParams.colorCalcEnable && ch.specColorCalc;
        case PerDot: return bgParams.colorCalcEnable && ch.specColorCalc && specFuncCode.colorMatches[colorData];
        case ColorDataMSB: return bgParams.colorCalcEnable && bit::extract<2>(colorData);
        }
    };

    // Fetch color and determine transparency.
    // Also determine special color calculation flag if using per-dot or color data MSB.
    uint8 colorData = 0;
    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = cellAddress + (dotOffset >> 1u);
        const uint8 dotData = (VDP2ReadVRAM<uint8>(dotAddress) >> (((dotX & 1) ^ 1) * 4)) & 0xF;
        const uint32 colorIndex = (ch.palNum << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(colorData);
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = cellAddress + dotOffset;
        const uint8 dotData = VDP2ReadVRAM<uint8>(dotAddress);
        const uint32 colorIndex = ((ch.palNum & 0x70) << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(colorData);
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        const uint32 colorIndex = dotData & 0x7FF;
        colorData = bit::extract<1, 3>(dotData);
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && (dotData & 0x7FF) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(colorData);
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        pixel.color = ConvertRGB555to888(Color555{.u16 = dotData});
        pixel.transparent = bgParams.enableTransparency && bit::extract<15>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(0b111);
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint32);
        const uint32 dotData = VDP2ReadVRAM<uint32>(dotAddress);
        pixel.color.u32 = dotData;
        pixel.transparent = bgParams.enableTransparency && bit::extract<31>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(0b111);
    }

    // Compute priority
    pixel.priority = bgParams.priorityNumber;
    if (bgParams.priorityMode == PriorityMode::PerCharacter) {
        pixel.priority &= ~1;
        pixel.priority |= ch.specPriority;
    } else if (bgParams.priorityMode == PriorityMode::PerDot) {
        if constexpr (IsPaletteColorFormat(colorFormat)) {
            pixel.priority &= ~1;
            if (ch.specPriority && specFuncCode.colorMatches[colorData]) {
                pixel.priority |= 1;
            }
        }
    }

    return pixel;
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE VDP::Pixel VDP::VDP2FetchBitmapPixel(const BGParams &bgParams, CoordU32 dotCoord) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");

    Pixel pixel{};

    auto [dotX, dotY] = dotCoord;

    // Bitmap data wraps around infinitely
    dotX &= bgParams.bitmapSizeH - 1;
    dotY &= bgParams.bitmapSizeV - 1;

    // Bitmap addressing uses a fixed offset of 0x20000 bytes which is precalculated when MPOFN/MPOFR is written to
    const uint32 bitmapBaseAddress = bgParams.bitmapBaseAddress;
    const uint32 dotOffset = dotX + dotY * bgParams.bitmapSizeH;
    const uint32 palNum = bgParams.supplBitmapPalNum;

    // Determine special color calculation flag
    auto getSpecialColorCalcFlag = [&](bool colorDataMSB) {
        using enum SpecialColorCalcMode;
        switch (bgParams.specialColorCalcMode) {
        case PerScreen: return bgParams.colorCalcEnable;
        case PerCharacter: return bgParams.colorCalcEnable && bgParams.supplBitmapSpecialColorCalc;
        case PerDot: return bgParams.colorCalcEnable && bgParams.supplBitmapSpecialColorCalc;
        case ColorDataMSB: return bgParams.colorCalcEnable && colorDataMSB;
        }
    };

    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = bitmapBaseAddress + (dotOffset >> 1u);
        const uint8 dotData = (VDP2ReadVRAM<uint8>(dotAddress) >> (((dotX & 1) ^ 1) * 4)) & 0xF;
        const uint32 colorIndex = palNum | dotData;
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(bit::extract<3>(dotData));
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset;
        const uint8 dotData = VDP2ReadVRAM<uint8>(dotAddress);
        const uint32 colorIndex = palNum | dotData;
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && dotData == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(bit::extract<3>(dotData));
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        const uint32 colorIndex = dotData & 0x7FF;
        pixel.color = VDP2FetchCRAMColor<colorMode>(bgParams.cramOffset, colorIndex);
        pixel.transparent = bgParams.enableTransparency && (dotData & 0x7FF) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(bit::extract<3>(dotData));
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = VDP2ReadVRAM<uint16>(dotAddress);
        pixel.color = ConvertRGB555to888(Color555{.u16 = dotData});
        pixel.transparent = bgParams.enableTransparency && bit::extract<15>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(true);
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint32);
        const uint32 dotData = VDP2ReadVRAM<uint32>(dotAddress);
        pixel.color = Color888{.u32 = dotData};
        pixel.transparent = bgParams.enableTransparency && bit::extract<31>(dotData) == 0;
        pixel.specialColorCalc = getSpecialColorCalcFlag(true);
    }

    // Compute priority
    pixel.priority = bgParams.priorityNumber;
    if (bgParams.priorityMode == PriorityMode::PerCharacter || bgParams.priorityMode == PriorityMode::PerDot) {
        pixel.priority &= ~1;
        pixel.priority |= bgParams.supplBitmapSpecialPriority;
    }

    return pixel;
}

template <uint32 colorMode>
FORCE_INLINE Color888 VDP::VDP2FetchCRAMColor(uint32 cramOffset, uint32 colorIndex) {
    static_assert(colorMode <= 2, "Invalid CRMD value");

    if constexpr (colorMode == 0) {
        // RGB 5:5:5, 1024 words
        const uint32 address = (cramOffset + colorIndex) * sizeof(uint16);
        const uint16 data = VDP2ReadCRAM<uint16>(address & 0x7FF);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else if constexpr (colorMode == 1) {
        // RGB 5:5:5, 2048 words
        const uint32 address = (cramOffset + colorIndex) * sizeof(uint16);
        const uint16 data = VDP2ReadCRAM<uint16>(address);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else { // colorMode == 2
        // RGB 8:8:8, 1024 words
        const uint32 address = (cramOffset + colorIndex) * sizeof(uint32);
        const uint32 data = VDP2ReadCRAM<uint32>(address);
        return Color888{.u32 = data};
    }
}

FLATTEN FORCE_INLINE SpriteData VDP::VDP2FetchSpriteData(uint32 fbOffset) {
    const uint8 type = m_VDP2.spriteParams.type;
    if (type < 8) {
        return VDP2FetchWordSpriteData(fbOffset * sizeof(uint16), type);
    } else {
        return VDP2FetchByteSpriteData(fbOffset, type);
    }
}

FORCE_INLINE SpriteData VDP::VDP2FetchWordSpriteData(uint32 fbOffset, uint8 type) {
    assert(type < 8);

    const uint16 rawData = util::ReadBE<uint16>(&VDP1GetDisplayFB()[fbOffset & 0x3FFFE]);

    SpriteData data{};
    switch (m_VDP2.spriteParams.type) {
    case 0x0:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 13>(rawData);
        data.priority = bit::extract<14, 15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x1:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 12>(rawData);
        data.priority = bit::extract<13, 15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x2:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 13>(rawData);
        data.priority = bit::extract<14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x3:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11, 12>(rawData);
        data.priority = bit::extract<13, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x4:
        data.colorData = bit::extract<0, 9>(rawData);
        data.colorDataMSB = bit::extract<9>(rawData);
        data.colorCalcRatio = bit::extract<10, 12>(rawData);
        data.priority = bit::extract<13, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<9>(data.colorData);
        break;
    case 0x5:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorDataMSB = bit::extract<10>(rawData);
        data.colorCalcRatio = bit::extract<11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<10>(data.colorData);
        break;
    case 0x6:
        data.colorData = bit::extract<0, 9>(rawData);
        data.colorDataMSB = bit::extract<9>(rawData);
        data.colorCalcRatio = bit::extract<10, 11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<9>(data.colorData);
        break;
    case 0x7:
        data.colorData = bit::extract<0, 8>(rawData);
        data.colorDataMSB = bit::extract<8>(rawData);
        data.colorCalcRatio = bit::extract<9, 11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        data.normalShadow = VDP2IsNormalShadow<8>(data.colorData);
        break;
    }
    return data;
}

FORCE_INLINE SpriteData VDP::VDP2FetchByteSpriteData(uint32 fbOffset, uint8 type) {
    assert(type >= 8);

    const uint8 rawData = VDP1GetDisplayFB()[fbOffset & 0x3FFFF];

    SpriteData data{};
    switch (m_VDP2.spriteParams.type) {
    case 0x8:
        data.colorData = bit::extract<0, 6>(rawData);
        data.colorDataMSB = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<6>(data.colorData);
        break;
    case 0x9:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorDataMSB = bit::extract<5>(rawData);
        data.colorCalcRatio = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<5>(data.colorData);
        break;
    case 0xA:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorDataMSB = bit::extract<5>(rawData);
        data.priority = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<5>(data.colorData);
        break;
    case 0xB:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorDataMSB = bit::extract<5>(rawData);
        data.colorCalcRatio = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<5>(data.colorData);
        break;
    case 0xC:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    case 0xD:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.colorCalcRatio = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    case 0xE:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.priority = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    case 0xF:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorDataMSB = bit::extract<7>(rawData);
        data.colorCalcRatio = bit::extract<6, 7>(rawData);
        data.normalShadow = VDP2IsNormalShadow<7>(data.colorData);
        break;
    }
    return data;
}

template <uint32 colorDataBits>
FORCE_INLINE bool VDP::VDP2IsNormalShadow(uint16 colorData) {
    // Check against normal shadow pattern (LSB = 0, rest of the bits = 1)
    static constexpr uint16 kNormalShadowValue = ~(~0 << (colorDataBits + 1)) & ~1;
    return (colorData == kNormalShadowValue);
}

} // namespace satemu::vdp

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
    // TODO: set PAL flag
    Reset(true);
}

void VDP::Reset(bool hard) {
    m_VRAM1.fill(0);
    m_VRAM2.fill(0);
    m_CRAM.fill(0);
    for (auto &fb : m_spriteFB) {
        fb.fill(0);
    }
    m_drawFB = 0;

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

    m_spriteLayer.Reset();
    for (auto &bgLayer : m_normBGLayers) {
        bgLayer.Reset();
    }
    for (auto &bgLayer : m_rotBGLayers) {
        bgLayer.Reset();
    }

    BeginHPhaseActiveDisplay();
    BeginVPhaseActiveDisplay();

    UpdateResolution();
}

void VDP::Advance(uint64 cycles) {
    // Update timings and fire events
    // TODO: use scheduler events

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

void VDP::UpdateResolution() {
    if (!m_VDP2.TVMDDirty) {
        return;
    }

    m_VDP2.TVMDDirty = false;

    // Horizontal/vertical resolution tables
    // NTSC uses the first two vRes entries, PAL uses the full table, and exclusive monitors use 480 lines
    static constexpr uint32 hRes[] = {320, 352, 640, 704};
    static constexpr uint32 vRes[] = {224, 240, 256, 256};

    // TODO: wait until next frame to update timings and etc.
    // - not sure how the hardware behaves if TVMODE is changed mid-frame
    // TODO: check for NTSC, PAL or exclusive monitor; assuming NTSC for now
    // TODO: exclusive monitor: even hRes entries are valid for 31 KHz monitors, odd are for Hi-Vision
    m_HRes = hRes[m_VDP2.TVMD.HRESOn];
    m_VRes = vRes[m_VDP2.TVMD.VRESOn & (m_VDP2.TVSTAT.PAL ? 3 : 1)];
    if (m_VDP2.TVMD.LSMDn == 3) {
        // Double-density interlace doubles the vertical resolution
        m_VRes *= 2;
    }

    fmt::println("VDP2: screen resolution set to {}x{}", m_HRes, m_VRes);
    switch (m_VDP2.TVMD.LSMDn) {
    case 0: fmt::println("VDP2: non-interlace mode"); break;
    case 1: fmt::println("VDP2: invalid interlace mode"); break;
    case 2: fmt::println("VDP2: single interlace mode"); break;
    case 3: fmt::println("VDP2: double interlace mode"); break;
    }

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

    // Adjust for dot clock
    const uint32 dotClockMult = (m_VDP2.TVMD.HRESOn & 2) ? 2 : 4;
    for (auto &timing : m_HTimings) {
        timing *= dotClockMult;
    }
    m_dotClockMult = dotClockMult;

    fmt::println("VDP2: dot clock mult = {}", dotClockMult);
    fmt::println("VDP2: display {}", m_VDP2.TVMD.DISP ? "ON" : "OFF");
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
    // fmt::println("VDP2: (VCNT = {:3d})  entering horizontal active display phase", m_VCounter);
    if (m_VPhase == VerticalPhase::Active) {
        if (m_VCounter == 0) {
            m_framebuffer = m_cbRequestFramebuffer(m_HRes, m_VRes);
            // fmt::println("VDP: begin frame, VDP1 fb {}", m_drawFB ^ 1);

            for (int i = 0; i < 4; i++) {
                const auto &bgParams = m_VDP2.normBGParams[i];
                auto &bgLayer = m_normBGLayers[i];
                bgLayer.fracScrollX = bgParams.scrollAmountH;
                bgLayer.fracScrollY = bgParams.scrollAmountV;
                bgLayer.scrollIncH = bgParams.scrollIncH;
                bgLayer.mosaicCounterY = 0;
                if (i < 2) {
                    bgLayer.lineScrollTableAddress = bgParams.lineScrollTableAddress;
                }
            }
        }
        VDP2DrawLine();
    }
}

void VDP::BeginHPhaseRightBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering right border phase", m_VCounter);
}

void VDP::BeginHPhaseHorizontalSync() {
    IncrementVCounter();
    // fmt::println("VDP2: (VCNT = {:3d})  entering horizontal sync phase", m_VCounter);

    m_VDP2.TVSTAT.HBLANK = 1;
    m_SCU.TriggerHBlankIN();
}

void VDP::BeginHPhaseLeftBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering left border phase", m_VCounter);
    m_VDP2.TVSTAT.HBLANK = 0;
}

// ----

void VDP::BeginVPhaseActiveDisplay() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering vertical active display phase", m_VCounter);
    if (m_VDP2.TVMD.LSMDn != 0) {
        m_VDP2.TVSTAT.ODD ^= 1;
    } else {
        m_VDP2.TVSTAT.ODD = 1;
    }
}

void VDP::BeginVPhaseBottomBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering bottom border phase", m_VCounter);
}

void VDP::BeginVPhaseBottomBlanking() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering bottom blanking phase", m_VCounter);
}

void VDP::BeginVPhaseVerticalSync() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering vertical sync phase", m_VCounter);
    m_VDP2.TVSTAT.VBLANK = 1;
    m_SCU.TriggerVBlankIN();
}

void VDP::BeginVPhaseTopBlanking() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering top blanking phase", m_VCounter);

    // TODO: end frame
    m_cbFrameComplete(m_framebuffer, m_HRes, m_VRes);

    UpdateResolution();
}

void VDP::BeginVPhaseTopBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering top border phase", m_VCounter);
}

void VDP::BeginVPhaseLastLine() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering last line phase", m_VCounter);

    m_VDP2.TVSTAT.VBLANK = 0;
    m_SCU.TriggerVBlankOUT();

    // fmt::println("VDP: VBlank OUT  VBE={:d} FCM={:d} FCT={:d} PTM={:d} mswap={:d} merase={:d}", m_VDP1.vblankErase,
    //              m_VDP1.fbSwapMode, m_VDP1.fbSwapTrigger, m_VDP1.plotTrigger, m_VDP1.fbManualSwap,
    //              m_VDP1.fbManualErase);

    bool swapFB = false;
    if (m_VDP1.fbManualSwap) {
        m_VDP1.fbManualSwap = false;
        swapFB = true;
    }

    if (!m_VDP1.fbSwapMode) {
        swapFB = true;
    }

    // VBlank erase or 1-cycle mode
    if (m_VDP1.vblankErase || !m_VDP1.fbSwapMode) {
        VDP1EraseFramebuffer();
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
    // fmt::println("VDP1: Erasing framebuffer {}", m_drawFB ^ 1);
    // TODO: erase only the specified region
    VDP1GetDisplayFB().fill(m_VDP1.eraseWriteValue);
}

FORCE_INLINE void VDP::VDP1SwapFramebuffer() {
    // fmt::println("VDP1: Swapping framebuffers - draw {}, display {}", m_drawFB ^ 1, m_drawFB);
    m_drawFB ^= 1;
}

void VDP::VDP1BeginFrame() {
    // fmt::println("VDP1: starting frame on framebuffer {} - VBE={:d} FCT={:d} FCM={:d}", m_drawFB, m_VDP1.vblankErase,
    //              m_VDP1.fbSwapTrigger, m_VDP1.fbSwapMode);

    // TODO: setup rendering
    // TODO: figure out VDP1 timings

    m_VDP1.prevCommandAddress = m_VDP1.currCommandAddress;
    m_VDP1.currCommandAddress = 0;
    m_VDP1.returnAddress = ~0;
    m_VDP1.prevFrameEnded = m_VDP1.currFrameEnded;
    m_VDP1.currFrameEnded = false;

    // TODO: process while advancing cycles
    VDP1ProcessCommands();
}

void VDP::VDP1EndFrame() {
    m_VDP1.currFrameEnded = true;
}

void VDP::VDP1ProcessCommands() {
    static constexpr uint16 kNoReturn = ~0;

    auto &cmdAddress = m_VDP1.currCommandAddress;

    // Run up to 10000 commands to avoid infinite loops
    // TODO: cycle counting
    for (int i = 0; i < 10000; i++) {
        const VDP1Command::Control control{.u16 = VDP1ReadVRAM<uint16>(cmdAddress)};
        if (control.end) {
            // fmt::println("VDP1: End of command list");
            VDP1EndFrame();
            m_SCU.TriggerSpriteDrawEnd();
            return;
        }

        // Process command
        if (!control.skip) {
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
                fmt::println("VDP1: Unexpected command type {:X}", static_cast<uint16>(control.command));
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
                cmdAddress = util::ReadBE<uint16>(&m_VRAM1[cmdAddress + 0x02]) << 3u;
                break;
            }
            case Call: {
                // Nested calls seem to not update the return address
                if (m_VDP1.returnAddress == kNoReturn) {
                    m_VDP1.returnAddress = cmdAddress + 0x20;
                }
                cmdAddress = util::ReadBE<uint16>(&m_VRAM1[cmdAddress + 0x02]) << 3u;
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
        }

        cmdAddress &= 0x7FFFF;
    }
}

bool VDP::VDP1IsPixelUserClipped(sint32 x, sint32 y) const {
    const auto &ctx = m_VDP1RenderContext;
    if (x < ctx.userClipX0 || x > ctx.userClipX1) {
        return true;
    }
    if (y < ctx.userClipY0 || y > ctx.userClipY1) {
        return true;
    }
    return false;
}

bool VDP::VDP1IsPixelSystemClipped(sint32 x, sint32 y) const {
    const auto &ctx = m_VDP1RenderContext;
    if (x < 0 || x > ctx.sysClipH) {
        return true;
    }
    if (y < 0 || y > ctx.sysClipV) {
        return true;
    }
    return false;
}

bool VDP::VDP1IsLineSystemClipped(sint32 x1, sint32 y1, sint32 x2, sint32 y2) const {
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

bool VDP::VDP1IsQuadSystemClipped(sint32 x1, sint32 y1, sint32 x2, sint32 y2, sint32 x3, sint32 y3, sint32 x4,
                                  sint32 y4) const {
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

FORCE_INLINE void VDP::VDP1PlotPixel(sint32 x, sint32 y, uint16 color, VDP1Command::DrawMode mode,
                                     uint32 gouraudTable) {
    if (mode.meshEnable && ((x ^ y) & 1)) {
        return;
    }

    // Reject pixels outside of clipping area
    if (VDP1IsPixelSystemClipped(x, y)) {
        return;
    }
    if (mode.userClippingEnable) {
        // clippingMode = false -> draw inside, reject outside
        // clippingMode = true -> draw outside, reject inside
        // The function returns true if the pixel is clipped, therefore we want to reject pixels that return the
        // opposite of clippingMode on that function.
        if (VDP1IsPixelUserClipped(x, y) != mode.clippingMode) {
            return;
        }
    }

    // TODO: use gouraud table
    // TODO: color calculations? (mode.colorCalc)
    // TODO: mode.preClippingDisable
    // TODO: mode.msbOn?

    const uint32 fbOffset = y * m_VDP1.fbSizeH + x;
    auto &drawFB = VDP1GetDrawFB();
    if (m_VDP1.pixel8Bits) {
        drawFB[fbOffset & 0x3FFFF] = color;
    } else {
        util::WriteBE<uint16>(&drawFB[(fbOffset * sizeof(uint16)) & 0x3FFFE], color);
    }
}

FORCE_INLINE void VDP::VDP1PlotLine(sint32 x1, sint32 y1, sint32 x2, sint32 y2, uint16 color,
                                    VDP1Command::DrawMode mode, uint32 gouraudTable) {
    for (LineStepper line{x1, y1, x2, y2}; line.CanStep(); line.Step()) {
        VDP1PlotPixel(line.X(), line.Y(), color, mode, gouraudTable);
        if (line.NeedsAntiAliasing()) {
            VDP1PlotPixel(line.AAX(), line.AAY(), color, mode, gouraudTable);
        }
    }
}

void VDP::VDP1PlotTexturedLine(sint32 x1, sint32 y1, sint32 x2, sint32 y2, uint32 colorBank,
                               VDP1Command::Control control, VDP1Command::DrawMode mode, uint32 gouraudTable,
                               uint32 charAddr, uint32 charSizeH, uint32 charSizeV, uint32 v, bool swapped) {
    if (control.flipV) {
        v = charSizeV - 1 - v;
    }

    for (TexturedLineStepper line{x1, y1, x2, y2, charSizeH, swapped}; line.CanStep(); line.Step()) {
        uint32 u = line.U();
        if (control.flipH) {
            u = charSizeH - 1 - u;
        }
        if (line.UChanged()) {
            // TODO: optimization: load new texel and compute U here
            // TODO: process end codes
        }

        // TODO: calculate color properly

        const uint32 charIndex = u + v * charSizeH;

        uint16 color = 0;
        bool transparent = true;
        switch (mode.colorMode) {
        case 0: // 4 bpp, 16 colors, bank mode
            color = m_VRAM1[(charAddr + (charIndex >> 1)) & 0x7FFFF];
            color = (color >> (((u ^ 1) & 1) * 4)) & 0xF;
            transparent = color == 0x0;
            color |= colorBank;
            break;
        case 1: // 4 bpp, 16 colors, lookup table mode
            color = m_VRAM1[(charAddr + (charIndex >> 1)) & 0x7FFFF];
            color = (color >> (((u ^ 1) & 1) * 4)) & 0xF;
            transparent = color == 0x0;
            color = util::ReadBE<uint16>(&m_VRAM1[(color * sizeof(uint16) + colorBank * 8) & 0x7FFFF]);
            break;
        case 2: // 8 bpp, 64 colors, bank mode
            color = m_VRAM1[(charAddr + charIndex) & 0x7FFFF] & 0x3F;
            transparent = color == 0x0;
            color |= colorBank & 0xFFC0;
            break;
        case 3: // 8 bpp, 128 colors, bank mode
            color = m_VRAM1[(charAddr + charIndex) & 0x7FFFF] & 0x7F;
            transparent = color == 0x00;
            color |= colorBank & 0xFF80;
            break;
        case 4: // 8 bpp, 256 colors, bank mode
            color = m_VRAM1[(charAddr + charIndex) & 0x7FFFF];
            transparent = color == 0x00;
            color |= colorBank & 0xFF00;
            break;
        case 5: // 16 bpp, 32768 colors, RGB mode
            color = util::ReadBE<uint16>(&m_VRAM1[(charAddr + charIndex * sizeof(uint16)) & 0x7FFFF]);
            transparent = color == 0x0000;
            break;
        }

        if (transparent && !mode.transparentPixelDisable) {
            continue;
        }

        VDP1PlotPixel(line.X(), line.Y(), color, mode, gouraudTable);
        if (line.NeedsAntiAliasing()) {
            VDP1PlotPixel(line.AAX(), line.AAY(), color, mode, gouraudTable);
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

    const sint32 xb = rx;
    const sint32 yb = ty;
    const sint32 xc = rx;
    const sint32 yc = by;
    const sint32 xd = lx;
    const sint32 yd = by;

    // fmt::println("VDP1: Draw normal sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
    //              "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
    //              xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable, mode.u16, charSizeH, charSizeV, charAddr);

    if (VDP1IsQuadSystemClipped(xa, ya, xb, yb, xc, yc, xd, yd)) {
        return;
    }

    // Interpolate linearly over edges A-D and B-C
    for (TexturedQuadEdgesStepper edge{xa, ya, xb, yb, xc, yc, xd, yd, charSizeV}; edge.CanStep(); edge.Step()) {
        const sint32 lx = edge.XMaj();
        const sint32 ly = edge.YMaj();
        const sint32 rx = edge.XMin();
        const sint32 ry = edge.YMin();
        const uint32 v = edge.V();
        const bool swapped = edge.Swapped();

        // Plot lines between the interpolated points
        VDP1PlotTexturedLine(lx, ly, rx, ry, color, control, mode, gouraudTable, charAddr, charSizeH, charSizeV, v,
                             swapped);
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

    // fmt::println("VDP1: Draw scaled sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
    //              "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
    //              qxa, qya, qxb, qyb, qxc, qyc, qxd, qyd, color, gouraudTable, mode.u16, charSizeH, charSizeV,
    //              charAddr);

    if (VDP1IsQuadSystemClipped(qxa, qya, qxb, qyb, qxc, qyc, qxd, qyd)) {
        return;
    }

    // Interpolate linearly over edges A-D and B-C
    for (TexturedQuadEdgesStepper edge{qxa, qya, qxb, qyb, qxc, qyc, qxd, qyd, charSizeV}; edge.CanStep();
         edge.Step()) {
        const sint32 lx = edge.XMaj();
        const sint32 ly = edge.YMaj();
        const sint32 rx = edge.XMin();
        const sint32 ry = edge.YMin();
        const uint32 v = edge.V();
        const bool swapped = edge.Swapped();

        // Plot lines between the interpolated points
        VDP1PlotTexturedLine(lx, ly, rx, ry, color, control, mode, gouraudTable, charAddr, charSizeH, charSizeV, v,
                             swapped);
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

    // fmt::println("VDP1: Draw distorted sprite: {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} {:3d}x{:<3d} color={:04X} "
    //              "gouraud={:04X} mode={:04X} size={:2d}x{:<2d} char={:X}",
    //              xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable, mode.u16, charSizeH, charSizeV, charAddr);

    if (VDP1IsQuadSystemClipped(xa, ya, xb, yb, xc, yc, xd, yd)) {
        return;
    }

    // Interpolate linearly over edges A-D and B-C
    for (TexturedQuadEdgesStepper edge{xa, ya, xb, yb, xc, yc, xd, yd, charSizeV}; edge.CanStep(); edge.Step()) {
        const sint32 lx = edge.XMaj();
        const sint32 ly = edge.YMaj();
        const sint32 rx = edge.XMin();
        const sint32 ry = edge.YMin();
        const uint32 v = edge.V();
        const bool swapped = edge.Swapped();

        // Plot lines between the interpolated points
        VDP1PlotTexturedLine(lx, ly, rx, ry, color, control, mode, gouraudTable, charAddr, charSizeH, charSizeV, v,
                             swapped);
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

    // fmt::println(
    // "VDP1: Draw polygon: {}x{} - {}x{} - {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}",
    //              xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable, mode.u16);

    if (VDP1IsQuadSystemClipped(xa, ya, xb, yb, xc, yc, xd, yd)) {
        return;
    }

    // Interpolate linearly over edges A-D and B-C
    for (QuadEdgesStepper edge{xa, ya, xb, yb, xc, yc, xd, yd}; edge.CanStep(); edge.Step()) {
        const sint32 lx = edge.XMaj();
        const sint32 ly = edge.YMaj();
        const sint32 rx = edge.XMin();
        const sint32 ry = edge.YMin();

        // Plot lines between the interpolated points
        VDP1PlotLine(lx, ly, rx, ry, color, mode, gouraudTable);
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

    // fmt::println(
    // "VDP1: Draw polylines: {}x{} - {}x{} - {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}",
    //              xa, ya, xb, yb, xc, yc, xd, yd, color, gouraudTable >> 3u, mode.u16);

    if (VDP1IsQuadSystemClipped(xa, ya, xb, yb, xc, yc, xd, yd)) {
        return;
    }

    VDP1PlotLine(xa, ya, xb, yb, color, mode, gouraudTable);
    VDP1PlotLine(xb, yb, xc, yc, color, mode, gouraudTable);
    VDP1PlotLine(xc, yc, xd, yd, color, mode, gouraudTable);
    VDP1PlotLine(xd, yd, xa, ya, color, mode, gouraudTable);
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

    // fmt::println("VDP1: Draw line: {}x{} - {}x{}, color {:04X}, gouraud table {}, CMDPMOD = {:04X}", xa, ya, xb, yb,
    //              color, gouraudTable, mode.u16);

    if (VDP1IsLineSystemClipped(xa, ya, xb, yb)) {
        return;
    }

    VDP1PlotLine(xa, ya, xb, yb, color, mode, gouraudTable);
}

void VDP::VDP1Cmd_SetSystemClipping(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.sysClipH = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
    ctx.sysClipV = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));
    // fmt::println("VDP1: Set system clipping: {}x{}", ctx.sysClipH, ctx.sysClipV);
}

void VDP::VDP1Cmd_SetUserClipping(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.userClipX0 = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    ctx.userClipY0 = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    ctx.userClipX1 = bit::extract<0, 9>(VDP1ReadVRAM<uint16>(cmdAddress + 0x14));
    ctx.userClipY1 = bit::extract<0, 8>(VDP1ReadVRAM<uint16>(cmdAddress + 0x16));
    // fmt::println("VDP1: Set user clipping: {}x{} - {}x{}", ctx.userClipX0, ctx.userClipY0, ctx.userClipX1,
    //              ctx.userClipY1);
}

void VDP::VDP1Cmd_SetLocalCoordinates(uint32 cmdAddress) {
    auto &ctx = m_VDP1RenderContext;
    ctx.localCoordX = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0C));
    ctx.localCoordY = bit::sign_extend<16>(VDP1ReadVRAM<uint16>(cmdAddress + 0x0E));
    // fmt::println("VDP1: Set local coordinates: {}x{}", ctx.localCoordX, ctx.localCoordY);
}

void VDP::VDP2ClearDisabledBGs() {
    for (int i = 0; i < 4; i++) {
        if (!m_VDP2.normBGParams[i].enabled) {
            m_normBGLayers[i].pixels.fill({});
        }
    }
    for (int i = 0; i < 2; i++) {
        if (!m_VDP2.rotBGParams[i].enabled) {
            m_rotBGLayers[i].pixels.fill({});
        }
    }
}

// -----------------------------------------------------------------------------
// VDP2

void VDP::VDP2UpdateLineScreenScroll(const NormBGParams &bgParams, NormBGLayer &layer) {
    auto read = [&] {
        const uint32 address = layer.lineScrollTableAddress & 0x7FFFF;
        const uint32 value = util::ReadBE<uint32>(&m_VRAM2[address]);
        layer.lineScrollTableAddress += sizeof(uint32);
        return value;
    };

    if ((m_VCounter & ~(~0 << bgParams.lineScrollInterval)) == 0) {
        if (bgParams.lineScrollXEnable) {
            layer.fracScrollX = bit::extract<8, 26>(read());
        }
        if (bgParams.lineScrollYEnable) {
            layer.fracScrollY = bit::extract<8, 26>(read());
        }
        if (bgParams.lineZoomEnable) {
            layer.scrollIncH = bit::extract<8, 18>(read());
        }
    }
}

void VDP::VDP2DrawLine() {
    // fmt::println("VDP2: drawing line {}", m_VCounter);

    using FnDrawSprite = void (VDP::*)();
    using FnDrawScrollNBG = void (VDP::*)(const NormBGParams &, NormBGLayer &);
    using FnDrawBitmapNBG = void (VDP::*)(const NormBGParams &, NormBGLayer &);
    // using FnDrawRotBG = void (VDP::*)(const RotBGParams &, BGLayer &);

    // Lookup table of VDP2DrawSpriteLayer functions
    // [colorMode]
    static constexpr auto fnDrawSprite = [] {
        std::array<FnDrawSprite, 4> arr{};

        util::constexpr_for<4>([&](auto index) {
            const uint32 cmIndex = bit::extract<0, 1>(index());

            const uint32 colorMode = cmIndex <= 2 ? cmIndex : 2;
            arr[cmIndex] = &VDP::VDP2DrawSpriteLayer<colorMode>;
        });

        return arr;
    }();

    // Lookup table of VDP2DrawNormalScrollBG functions
    // [twoWordChar][fourCellChar][wideChar][colorFormat][colorMode]
    static constexpr auto fnDrawScrollNBGs = [] {
        std::array<std::array<std::array<std::array<std::array<FnDrawScrollNBG, 4>, 8>, 2>, 2>, 2> arr{};

        util::constexpr_for<2 * 2 * 2 * 8 * 4>([&](auto index) {
            const uint32 twcIndex = bit::extract<0>(index());
            const uint32 fccIndex = bit::extract<1>(index());
            const uint32 wcIndex = bit::extract<2>(index());
            const uint32 cfIndex = bit::extract<3, 5>(index());
            const uint32 cmIndex = bit::extract<6, 7>(index());

            const bool twoWordChar = twcIndex;
            const bool fourCellChar = fccIndex;
            const bool wideChar = wcIndex;
            const ColorFormat colorFormat = static_cast<ColorFormat>(cfIndex <= 4 ? cfIndex : 4);
            const uint32 colorMode = cmIndex <= 2 ? cmIndex : 2;
            arr[twcIndex][fccIndex][wcIndex][cfIndex][cmIndex] =
                &VDP::VDP2DrawNormalScrollBG<twoWordChar, fourCellChar, wideChar, colorFormat, colorMode>;
        });

        return arr;
    }();

    // Lookup table of VDP2DrawNormalBitmapBG functions
    // [colorFormat][colorMode]
    static constexpr auto fnDrawBitmapNBGs = [] {
        std::array<std::array<FnDrawBitmapNBG, 4>, 8> arr{};

        util::constexpr_for<8 * 4>([&](auto index) {
            const uint32 cfIndex = bit::extract<0, 2>(index());
            const uint32 cmIndex = bit::extract<3, 4>(index());

            const ColorFormat colorFormat = static_cast<ColorFormat>(cfIndex <= 4 ? cfIndex : 4);
            const size_t colorMode = cmIndex <= 2 ? cmIndex : 2;
            arr[cfIndex][cmIndex] = &VDP::VDP2DrawNormalBitmapBG<colorFormat, colorMode>;
        });

        return arr;
    }();

    // TODO: optimize
    // - RBG1 replaces NBG0 if enabled
    // - when RBG0 and RBG1 are both enabled, NBG0-3 are disabled
    // - this means we can use less layers (4 max it seems)

    const uint32 colorMode = m_VDP2.RAMCTL.CRMDn;

    // Draw sprite layer
    (this->*fnDrawSprite[colorMode])();

    // Draw normal BGs
    for (int i = 0; i < 4; i++) {
        const auto &bg = m_VDP2.normBGParams[i];
        auto &layer = m_normBGLayers[i];
        if (bg.enabled) {
            layer.cramOffset = bg.caos << (colorMode == 1 ? 10 : 9);
            layer.mosaicCounterY++;
            if (layer.mosaicCounterY >= m_VDP2.mosaicV) {
                layer.mosaicCounterY = 0;
            }

            if (i < 2) {
                VDP2UpdateLineScreenScroll(bg, layer);
            }

            const uint32 colorFormat = static_cast<uint32>(bg.colorFormat);
            if (bg.bitmap) {
                (this->*fnDrawBitmapNBGs[colorFormat][colorMode])(bg, layer);
            } else {
                const bool twoWordChar = bg.twoWordChar;
                const bool fourCellChar = bg.cellSizeShift;
                const bool wideChar = bg.wideChar;
                (this->*fnDrawScrollNBGs[twoWordChar][fourCellChar][wideChar][colorFormat][colorMode])(bg, layer);
            }
        }
    }

    // Draw rotation BGs
    for (int i = 0; i < 2; i++) {
        const auto &bg = m_VDP2.rotBGParams[i];
        auto &layer = m_rotBGLayers[i];
        if (bg.enabled) {
            // layer.cramOffset = bg.caos << (colorMode == 1 ? 10 : 9);

            // TODO: implement
            if (bg.bitmap) {
                // (this->*fnDrawBitmapRBGs[...])(bg, layer);
            } else {
                // (this->*fnDrawScrollRBGs[...])(bg, layer);
            }
        }
    }

    // Compose image
    if (m_framebuffer != nullptr) {
        const bool usesRBG1 = m_VDP2.rotBGParams[1].enabled;
        const uint32 y = m_VCounter;
        for (uint32 x = 0; x < m_HRes; x++) {
            // Get references to pixels of all layers
            // TODO: merge BG layers NBG0 and RBG1
            const auto &spritePixel = m_spriteLayer.pixels[x];
            const auto &nbg0rbg1Pixel = usesRBG1 ? m_normBGLayers[5].pixels[x] : m_normBGLayers[0].pixels[x];
            const auto &nbg1exbgPixel = m_normBGLayers[1].pixels[x];
            const auto &nbg2Pixel = m_normBGLayers[2].pixels[x];
            const auto &nbg3Pixel = m_normBGLayers[3].pixels[x];
            const auto &rbg0Pixel = m_normBGLayers[4].pixels[x];

            // TODO: refactor this mess
            // - too many switch-cases to get parameters/data
            enum Layer { LYR_Back, LYR_NBG3, LYR_NBG2, LYR_NBG1_EXBG, LYR_NBG0_RBG1, LYR_RBG0, LYR_Sprite };
            std::array<Layer, 3> layers = {LYR_Back, LYR_Back, LYR_Back};
            std::array<uint8, 3> layerPrios = {0, 0, 0};

            // Inserts a layer into the appropriate position in the stack
            // - Higher priority beats lower priority
            // - If same priority, higher Layer index beats lower Layer index
            // - layers[0] is topmost (first) layer
            auto insertLayer = [&](Layer layer, uint8 priority, bool transparent) {
                if (transparent) {
                    return;
                }
                if (priority == 0) {
                    return;
                }

                for (int i = 0; i < 3; i++) {
                    if (priority > layerPrios[i] || (priority == layerPrios[i] && layer > layers[i])) {
                        // Push layers back
                        for (int j = 2; j > i; j--) {
                            layers[j] = layers[j - 1];
                            layerPrios[j] = layerPrios[j - 1];
                        }
                        layers[i] = layer;
                        layerPrios[i] = priority;
                        break;
                    }
                }
            };

            // Determine layer order
            insertLayer(LYR_Sprite, spritePixel.priority, spritePixel.transparent);
            insertLayer(LYR_NBG0_RBG1, nbg0rbg1Pixel.priority, nbg0rbg1Pixel.transparent);
            insertLayer(LYR_NBG1_EXBG, nbg1exbgPixel.priority, nbg1exbgPixel.transparent);
            insertLayer(LYR_NBG2, nbg2Pixel.priority, nbg2Pixel.transparent);
            insertLayer(LYR_NBG3, nbg3Pixel.priority, nbg3Pixel.transparent);
            insertLayer(LYR_RBG0, rbg0Pixel.priority, rbg0Pixel.transparent);

            // Get references to all layer parameters
            // TODO: merge BG layers NBG0 and RBG1
            const auto &spriteParams = m_VDP2.spriteParams;
            const auto &nbg0Params = m_VDP2.normBGParams[0];
            const auto &nbg1Params = m_VDP2.normBGParams[1];
            const auto &nbg2Params = m_VDP2.normBGParams[2];
            const auto &nbg3Params = m_VDP2.normBGParams[3];
            const auto &rbg0Params = m_VDP2.rotBGParams[0];
            const auto &rbg1Params = m_VDP2.rotBGParams[1];
            const auto &backParams = m_VDP2.backScreenParams;

            // Retrieves the color of the given layer
            auto getLayerColor = [&](Layer layer) -> Color888 {
                bool colorOffsetEnable{};
                bool colorOffsetSelect{};
                Color888 color{};

                switch (layer) {
                case LYR_Sprite:
                    color = spritePixel.color;
                    colorOffsetEnable = spriteParams.colorOffsetEnable;
                    colorOffsetSelect = spriteParams.colorOffsetSelect;
                    break;
                case LYR_RBG0:
                    color = rbg0Pixel.color;
                    colorOffsetEnable = rbg0Params.colorOffsetEnable;
                    colorOffsetSelect = rbg0Params.colorOffsetSelect;
                    break;
                case LYR_NBG0_RBG1:
                    color = nbg0rbg1Pixel.color;
                    colorOffsetEnable = usesRBG1 ? rbg1Params.colorOffsetEnable : nbg0Params.colorOffsetEnable;
                    colorOffsetSelect = usesRBG1 ? rbg1Params.colorOffsetSelect : nbg0Params.colorOffsetSelect;
                    break;
                case LYR_NBG1_EXBG:
                    color = nbg1exbgPixel.color;
                    colorOffsetEnable = nbg1Params.colorOffsetEnable;
                    colorOffsetSelect = nbg1Params.colorOffsetSelect;
                    break;
                case LYR_NBG2:
                    color = nbg2Pixel.color;
                    colorOffsetEnable = nbg2Params.colorOffsetEnable;
                    colorOffsetSelect = nbg2Params.colorOffsetSelect;
                    break;
                case LYR_NBG3:
                    color = nbg3Pixel.color;
                    colorOffsetEnable = nbg3Params.colorOffsetEnable;
                    colorOffsetSelect = nbg3Params.colorOffsetSelect;
                    break;
                case LYR_Back: {
                    const uint32 line = backParams.perLine ? y : 0;
                    const uint32 address = backParams.baseAddress + line * sizeof(Color555);
                    const Color555 color555{.u16 = util::ReadBE<uint16>(&m_VRAM2[address & 0x7FFFF])};
                    color = ConvertRGB555to888(color555);
                    colorOffsetEnable = backParams.colorOffsetEnable;
                    colorOffsetSelect = backParams.colorOffsetSelect;
                    break;
                }
                default: util::unreachable();
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
                switch (layer) {
                case LYR_Sprite: return spriteParams.colorCalcEnable;
                case LYR_RBG0: return rbg0Params.colorCalcEnable;
                case LYR_NBG0_RBG1: return usesRBG1 ? rbg1Params.colorCalcEnable : nbg0Params.colorCalcEnable;
                case LYR_NBG1_EXBG: return nbg1Params.colorCalcEnable;
                case LYR_NBG2: return nbg2Params.colorCalcEnable;
                case LYR_NBG3: return nbg3Params.colorCalcEnable;
                case LYR_Back: return backParams.colorCalcEnable;
                default: util::unreachable();
                }
            };

            auto getColorCalcRatio = [&](Layer layer) {
                switch (layer) {
                case LYR_Sprite: return spritePixel.colorCalcRatio;
                case LYR_RBG0: return rbg0Params.colorCalcRatio;
                case LYR_NBG0_RBG1: return usesRBG1 ? rbg1Params.colorCalcRatio : nbg0Params.colorCalcRatio;
                case LYR_NBG1_EXBG: return nbg1Params.colorCalcRatio;
                case LYR_NBG2: return nbg2Params.colorCalcRatio;
                case LYR_NBG3: return nbg3Params.colorCalcRatio;
                case LYR_Back: return backParams.colorCalcRatio;
                default: util::unreachable();
                }
            };

            const auto &colorCalcParams = m_VDP2.colorCalcParams;

            // Calculate color
            // TODO: handle LNCL insertion
            // - inserted behind the topmost layer if it has line color insertion enabled
            // TODO: handle specialColorCalcMode directly in rendering code
            Color888 outputColor{};
            if (isColorCalcEnabled(layers[0])) {
                const Color888 topColor = getLayerColor(layers[0]);
                Color888 btmColor = getLayerColor(layers[1]);

                // Apply extended color calculations (only in normal TV modes)
                if (colorCalcParams.extendedColorCalcEnable && m_VDP2.TVMD.HRESOn < 2) {
                    // TODO: honor color RAM mode + palette/RGB format restrictions
                    // - modes 1 and 2 don't blend layers if the bottom layer uses palette color
                    //   - LNCL is considered RGB for that matter (i.e. it blends if requested)
                    // TODO: honor LNCL insertion

                    // HACK: assuming color RAM mode 0 for now
                    if (isColorCalcEnabled(layers[1])) {
                        const Color888 l2Color = getLayerColor(layers[2]);
                        btmColor.r = (btmColor.r + l2Color.r) / 2;
                        btmColor.g = (btmColor.g + l2Color.g) / 2;
                        btmColor.b = (btmColor.b + l2Color.b) / 2;
                    }
                }

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

            m_framebuffer[x + y * m_HRes] = outputColor.u32;
        }
    }
}

template <uint32 colorMode>
NO_INLINE void VDP::VDP2DrawSpriteLayer() {
    const uint32 y = m_VCounter;

    for (uint32 x = 0; x < m_HRes; x++) {
        const auto &spriteFB = VDP1GetDisplayFB();
        const uint32 spriteFBOffset = x + y * m_VDP1.fbSizeH;

        const auto &params = m_VDP2.spriteParams;
        auto &pixel = m_spriteLayer.pixels[x];

        bool isPaletteData = true;
        if (params.mixedFormat) {
            const uint16 spriteDataValue = util::ReadBE<uint16>(&spriteFB[spriteFBOffset * sizeof(uint16)]);
            if (bit::extract<15>(spriteDataValue)) {
                // RGB data
                pixel.color = ConvertRGB555to888(Color555{spriteDataValue});
                pixel.transparent = false;
                pixel.priority = params.priorities[0];
                pixel.colorCalcRatio = params.colorCalcRatios[0];
                pixel.shadowOrWindow = false;
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
            pixel.colorCalcRatio = params.colorCalcRatios[spriteData.colorCalcRatio];
            pixel.shadowOrWindow = spriteData.shadowOrWindow;
        }
    }
}

template <bool twoWordChar, bool fourCellChar, bool wideChar, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawNormalScrollBG(const NormBGParams &bgParams, NormBGLayer &layer) {
    //          Map
    // +---------+---------+
    // |         |         |   Normal BGs always have 4 planes named A,B,C,D in this exact configuration.
    // | Plane A | Plane B |   Each plane can point to different portions of VRAM through a combination of bits
    // |         |         |   from the Map Register (per BG) and the Map Offset Register (per BG and plane):
    // +---------+---------+     bits 5-0 of the address come from the Map Register (MPxxN#)
    // |         |         |     bits 8-6 of the address come from the Map Offset Register (MPOFN)
    // | Plane C | Plane D |
    // |         |         |
    // +---------+---------+
    //
    //         Plane
    // +---------+---------+   Each plane is composed of 1x1, 2x1 or 2x2 pages, determined by Plane Size in the
    // |         |         |   Plane Size Register (PLSZ).
    // | Page 1  | Page 2  |   Pages are stored sequentially in VRAM left to right, top to bottom, as shown.
    // |         |         |
    // +---------+---------+
    // |         |         |
    // | Page 3  | Page 4  |
    // |         |         |
    // +---------+---------+
    //
    //           Page
    // +----+----+..+----+----+   Pages contain 32x32 or 64x64 character patterns, which are groups of 1x1 or 2x2 cells,
    // |CP 1|CP 2|  |CP63|CP64|   determined by Character Size in the Character Control Register (CHCTLA-B).
    // +----+----+..+----+----+   Pages always contain a total of 64x64 cells - a grid of 64x64 1x1 character patterns
    // |  65|  66|  | 127| 128|   or 32x32 2x2 character patterns. Because of this, pages always have 512x512 dots.
    // +----+----+..+----+----+
    // :    :    :  :    :    :   Character patterns in a page are stored sequentially in VRAM left to right, top to
    // +----+----+..+----+----+   bottom, as shown. The figure to the left illustrates a 64x64 page; a 32x32 page would
    // |3969|3970|  |4031|4032|   have 1024 character patterns in total instead of 4096.
    // +----+----+..+----+----+
    // |4033|4034|  |4095|4096|
    // +----+----+..+----+----+
    //
    //   Character Pattern
    // +---------+---------+   Character patterns are groups of 1x1 or 2x2 cells, determined by Character Size in the
    // |         |         |   Character Control Register (CHCTLA-B).
    // | Cell 1  | Cell 2  |   Cells are stored sequentially in VRAM left to right, top to bottom, as shown.
    // |         |         |   Character patterns contain a character number (15 bits), a palette number (7 bits, only
    // +---------+---------+   used with 16 or 256 color palette modes), two special function bits (Special Priority and
    // |         |         |   Special Color Calculation) and two flip bits (horizontal and vertical).
    // | Cell 3  | Cell 4  |   Character patterns can be one or two words long, as defined by Pattern Name Data Size
    // |         |         |   in the Pattern Name Control Register (PNCN0-3, PNCR).
    // +---------+---------+   When using one word characters, some of the data comes from supplementary registers.
    //
    //           Cell
    // +--+--+--+--+--+--+--+--+   Cells contain 8x8 dots (pixels) in one of the following color formats:
    // | 1| 2| 3| 4| 5| 6| 7| 8|     - 16 color palette
    // +--+--+--+--+--+--+--+--+     - 256 color palette
    // | 9|10|11|12|13|14|15|16|     - 1024 or 2048 color palette (depending on Color Mode)
    // +--+--+--+--+--+--+--+--+     - 5:5:5 RGB (32768 colors)
    // |17|18|19|20|21|22|23|24|     - 8:8:8 RGB (16777216 colors)
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

    const auto &specialFunctionCodes = m_VDP2.specialFunctionCodes[bgParams.specialFunctionSelect];

    uint32 fracScrollX = layer.fracScrollX;
    const uint32 fracScrollY = layer.fracScrollY;
    layer.fracScrollY += bgParams.scrollIncV;

    uint32 cellScrollTableAddress = m_VDP2.verticalCellScrollTableAddress;

    auto readCellScrollY = [&] {
        const uint32 address = cellScrollTableAddress & 0x7FFFF;
        const uint32 value = util::ReadBE<uint32>(&m_VRAM2[address]);
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
                layer.pixels[x] = layer.pixels[x - 1];

                // Increment horizontal coordinate
                fracScrollX += layer.scrollIncH;
                continue;
            }
        }

        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 8u;
        const uint32 scrollY = ((fracScrollY + cellScrollY) >> 8u) - layer.mosaicCounterY;

        // Determine plane index from the scroll coordinate
        const uint32 planeX = bit::extract<9>(scrollX) >> bgParams.pageShiftH;
        const uint32 planeY = bit::extract<9>(scrollY) >> bgParams.pageShiftV;
        const uint32 plane = planeX + planeY * 2u;

        // Determine page index from the scroll coordinate
        const uint32 pageX = bit::extract<9>(scrollX) & bgParams.pageShiftH;
        const uint32 pageY = bit::extract<9>(scrollY) & bgParams.pageShiftV;
        const uint32 page = pageX + pageY * 2u;

        // Determine character pattern from the scroll coordinate
        const uint32 charPatX = bit::extract<3, 8>(scrollX) >> fourCellChar;
        const uint32 charPatY = bit::extract<3, 8>(scrollY) >> fourCellChar;
        const uint32 charIndex = charPatX + charPatY * (64u >> fourCellChar);

        // Determine cell index from the scroll coordinate
        const uint32 cellX = bit::extract<3>(scrollX) & fourCellChar;
        const uint32 cellY = bit::extract<3>(scrollY) & fourCellChar;
        const uint32 cellIndex = cellX + cellY * 2u;

        // Determine dot coordinates
        const uint32 dotX = bit::extract<0, 2>(scrollX);
        const uint32 dotY = bit::extract<0, 2>(scrollY);

        // Fetch character
        const uint32 pageBaseAddress = bgParams.pageBaseAddresses[plane];
        const uint32 pageOffset = page * kPageSizes[fourCellChar][twoWordChar];
        const uint32 pageAddress = pageBaseAddress + pageOffset;
        constexpr bool largePalette = colorFormat != ColorFormat::Palette16;
        const Character ch =
            twoWordChar
                ? VDP2FetchTwoWordCharacter(pageAddress, charIndex)
                : VDP2FetchOneWordCharacter<fourCellChar, largePalette, wideChar>(bgParams, pageAddress, charIndex);

        // Fetch dot color using character data
        auto &pixel = layer.pixels[x];
        uint8 colorData{};
        pixel.color = VDP2FetchCharacterColor<colorFormat, colorMode>(layer.cramOffset, colorData, pixel.transparent,
                                                                      ch, dotX, dotY, cellIndex);
        pixel.transparent &= bgParams.enableTransparency;

        // Compute priority
        pixel.priority = bgParams.priorityNumber;
        if (bgParams.priorityMode == PriorityMode::PerCharacter) {
            pixel.priority &= ~1;
            pixel.priority |= ch.specPriority;
        } else if (bgParams.priorityMode == PriorityMode::PerDot) {
            if constexpr (IsPaletteColorFormat(colorFormat)) {
                pixel.priority &= ~1;
                if (ch.specPriority && specialFunctionCodes.colorMatches[colorData]) {
                    pixel.priority |= 1;
                }
            }
        }

        // Increment horizontal coordinate
        fracScrollX += layer.scrollIncH;
    }
}

template <ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::VDP2DrawNormalBitmapBG(const NormBGParams &bgParams, NormBGLayer &layer) {
    uint32 fracScrollX = layer.fracScrollX;
    const uint32 fracScrollY = layer.fracScrollY;
    layer.fracScrollY += bgParams.scrollIncV;

    uint32 cellScrollTableAddress = m_VDP2.verticalCellScrollTableAddress;

    auto readCellScrollY = [&] {
        const uint32 address = cellScrollTableAddress & 0x7FFFF;
        const uint32 value = util::ReadBE<uint32>(&m_VRAM2[address]);
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
                layer.pixels[x] = layer.pixels[x - 1];

                // Increment horizontal coordinate
                fracScrollX += layer.scrollIncH;
                continue;
            }
        }

        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 8u;
        const uint32 scrollY = ((fracScrollY + cellScrollY) >> 8u) - layer.mosaicCounterY;

        // Fetch dot color from bitmap
        auto &pixel = layer.pixels[x];
        pixel.color = VDP2FetchBitmapColor<colorFormat, colorMode>(bgParams, pixel.transparent, layer.cramOffset,
                                                                   scrollX, scrollY);
        pixel.transparent &= bgParams.enableTransparency;

        // Compute priority
        pixel.priority = bgParams.priorityNumber;
        if (bgParams.priorityMode == PriorityMode::PerCharacter || bgParams.priorityMode == PriorityMode::PerDot) {
            pixel.priority &= ~1;
            pixel.priority |= bgParams.supplBitmapSpecialPriority;
        }

        // Increment horizontal coordinate
        fracScrollX += layer.scrollIncH;
    }
}

FORCE_INLINE VDP::Character VDP::VDP2FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex) {
    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint32);
    const uint32 charData = util::ReadBE<uint32>(&m_VRAM2[charAddress & 0x7FFFF]);

    Character ch{};
    ch.charNum = bit::extract<0, 14>(charData);
    ch.palNum = bit::extract<16, 22>(charData);
    ch.specColorCalc = bit::extract<28>(charData);
    ch.specPriority = bit::extract<29>(charData);
    ch.flipH = bit::extract<30>(charData);
    ch.flipV = bit::extract<31>(charData);
    return ch;
}

template <bool fourCellChar, bool largePalette, bool wideChar>
FORCE_INLINE VDP::Character VDP::VDP2FetchOneWordCharacter(const NormBGParams &bgParams, uint32 pageBaseAddress,
                                                           uint32 charIndex) {
    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint16);
    const uint16 charData = util::ReadBE<uint16>(&m_VRAM2[charAddress & 0x7FFFF]);

    /*
    Contents of 1 word character patterns vary based on Character Size, Character Color Count and Auxiliary Mode:
        Character Size        = CHCTLA/CHCTLB.xxCHSZ  = !fourCellChar = !FCC
        Character Color Count = CHCTLA/CHCTLB.xxCHCNn = largePalette  = LP
        Auxiliary Mode        = PNCN0/PNCR.xxCNSM     = wideChar      = WC
                ---------------- Character data ----------------    Supplement in Pattern Name Control Register
    FCC LP  WC  |15 14 13 12 11 10 9  8  7  6  5  4  3  2  1  0|    | 9  8  7  6  5  4  3  2  1  0|
     F   F   F  |palnum 3-0 |VF|HF| character number 9-0       |    |PR|CC| PN 6-4 |charnum 14-10 |
     F   T   F  |--| PN 6-4 |VF|HF| character number 9-0       |    |PR|CC|--------|charnum 14-10 |
     T   F   F  |palnum 3-0 |VF|HF| character number 11-2      |    |PR|CC| PN 6-4 |CN 14-12|CN1-0|
     T   T   F  |--| PN 6-4 |VF|HF| character number 11-2      |    |PR|CC|--------|CN 14-12|CN1-0|
     F   F   T  |palnum 3-0 |       character number 11-0      |    |PR|CC| PN 6-4 |CN 14-12|-----|
     F   T   T  |--| PN 6-4 |       character number 11-0      |    |PR|CC|--------|CN 14-12|-----|
     T   F   T  |palnum 3-0 |       character number 13-2      |    |PR|CC| PN 6-4 |cn|-----|CN1-0|   cn=CN14
     T   T   T  |--| PN 6-4 |       character number 13-2      |    |PR|CC|--------|cn|-----|CN1-0|   cn=CN14
    */

    // Character number bit range from the 1-word character pattern data (charData)
    static constexpr uint32 baseCharNumStart = 0;
    static constexpr uint32 baseCharNumEnd = 9 + 2 * wideChar;
    static constexpr uint32 baseCharNumPos = 2 * fourCellChar;

    // Upper character number bit range from the supplementary character number (bgParams.supplCharNum)
    static constexpr uint32 supplCharNumStart = 2 * fourCellChar + 2 * wideChar;
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
    ch.flipH = !wideChar && bit::extract<10>(charData);
    ch.flipV = !wideChar && bit::extract<11>(charData);
    return ch;
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE Color888 VDP::VDP2FetchCharacterColor(uint32 cramOffset, uint8 &colorData, bool &transparent, Character ch,
                                                   uint8 dotX, uint8 dotY, uint32 cellIndex) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");
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

    // Cell addressing uses a fixed offset of 32 bytes
    const uint32 cellAddress = (ch.charNum + cellIndex * 2) * 0x20;
    const uint32 dotOffset = dotX + dotY * 8;

    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = cellAddress + (dotOffset >> 1u);
        const uint8 dotData = (m_VRAM2[dotAddress & 0x7FFFF] >> (((dotX & 1) ^ 1) * 4)) & 0xF;
        const uint32 colorIndex = (ch.palNum << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        transparent = dotData == 0;
        return VDP2FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = cellAddress + dotOffset;
        const uint8 dotData = m_VRAM2[dotAddress & 0x7FFFF];
        const uint32 colorIndex = ((ch.palNum & 0x70) << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        transparent = dotData == 0;
        return VDP2FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        const uint32 colorIndex = dotData & 0x7FF;
        colorData = bit::extract<1, 3>(dotData);
        transparent = (dotData & 0x7FF) == 0;
        return VDP2FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        transparent = bit::extract<15>(dotData) == 0;
        return ConvertRGB555to888(Color555{.u16 = dotData});
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = cellAddress + dotOffset * sizeof(uint32);
        const uint32 dotData = util::ReadBE<uint32>(&m_VRAM2[dotAddress & 0x7FFFF]);
        transparent = bit::extract<31>(dotData) == 0;
        return Color888{.u32 = dotData};
    }
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE Color888 VDP::VDP2FetchBitmapColor(const NormBGParams &bgParams, bool &transparent, uint32 cramOffset,
                                                uint32 dotX, uint32 dotY) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");

    // Bitmap data wraps around infinitely
    dotX &= bgParams.bitmapSizeH - 1;
    dotY &= bgParams.bitmapSizeV - 1;

    // Bitmap addressing uses a fixed offset of 0x20000 bytes which is precalculated when MPOFN/MPOFR is written to
    const uint32 bitmapBaseAddress = bgParams.bitmapBaseAddress;
    const uint32 dotOffset = dotX + dotY * bgParams.bitmapSizeH;
    const uint32 palNum = bgParams.supplBitmapPalNum;

    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = bitmapBaseAddress + (dotOffset >> 1u);
        const uint8 dotData = (m_VRAM2[dotAddress & 0x7FFFF] >> (((dotX & 1) ^ 1) * 4)) & 0xF;
        const uint32 colorIndex = palNum | dotData;
        transparent = dotData == 0;
        return VDP2FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset;
        const uint8 dotData = m_VRAM2[dotAddress & 0x7FFFF];
        const uint32 colorIndex = palNum | dotData;
        transparent = dotData == 0;
        return VDP2FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        const uint32 colorIndex = dotData & 0x7FF;
        transparent = (dotData & 0x7FF) == 0;
        return VDP2FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        transparent = bit::extract<15>(dotData) == 0;
        return ConvertRGB555to888(Color555{.u16 = dotData});
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = bitmapBaseAddress + dotOffset * sizeof(uint32);
        const uint32 dotData = util::ReadBE<uint32>(&m_VRAM2[dotAddress & 0x7FFFF]);
        transparent = bit::extract<31>(dotData) == 0;
        return Color888{.u32 = dotData};
    }
}

template <uint32 colorMode>
FORCE_INLINE Color888 VDP::VDP2FetchCRAMColor(uint32 cramOffset, uint32 colorIndex) {
    static_assert(colorMode <= 2, "Invalid CRMD value");

    if constexpr (colorMode == 0) {
        // RGB 5:5:5, 1024 words
        const uint32 address = (cramOffset + colorIndex * sizeof(uint16)) & 0x7FF;
        const uint16 data = util::ReadBE<uint16>(&m_CRAM[address]);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else if constexpr (colorMode == 1) {
        // RGB 5:5:5, 2048 words
        const uint32 address = (cramOffset + colorIndex * sizeof(uint16)) & 0xFFF;
        const uint16 data = util::ReadBE<uint16>(&m_CRAM[address]);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else { // colorMode == 2
        // RGB 8:8:8, 1024 words
        const uint32 address = (cramOffset + colorIndex * sizeof(uint32)) & 0xFFF;
        const uint32 data = util::ReadBE<uint32>(&m_CRAM[address]);
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

FORCE_INLINE SpriteData VDP::VDP2FetchByteSpriteData(uint32 fbOffset, uint8 type) {
    assert(type >= 8);

    const uint8 rawData = VDP1GetDisplayFB()[fbOffset & 0x3FFFF];

    SpriteData data{};
    switch (m_VDP2.spriteParams.type) {
    case 0x8:
        data.colorData = bit::extract<0, 6>(rawData);
        data.priority = bit::extract<7>(rawData);
        break;
    case 0x9:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorCalcRatio = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        break;
    case 0xA:
        data.colorData = bit::extract<0, 5>(rawData);
        data.priority = bit::extract<6, 7>(rawData);
        break;
    case 0xB:
        data.colorData = bit::extract<0, 5>(rawData);
        data.colorCalcRatio = bit::extract<6, 7>(rawData);
        break;
    case 0xC:
        data.colorData = bit::extract<0, 7>(rawData);
        data.priority = bit::extract<7>(rawData);
        break;
    case 0xD:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorCalcRatio = bit::extract<6>(rawData);
        data.priority = bit::extract<7>(rawData);
        break;
    case 0xE:
        data.colorData = bit::extract<0, 7>(rawData);
        data.priority = bit::extract<6, 7>(rawData);
        break;
    case 0xF:
        data.colorData = bit::extract<0, 7>(rawData);
        data.colorCalcRatio = bit::extract<6, 7>(rawData);
        break;
    }
    return data;
}

FORCE_INLINE SpriteData VDP::VDP2FetchWordSpriteData(uint32 fbOffset, uint8 type) {
    assert(type < 8);

    const uint16 rawData = util::ReadBE<uint16>(&VDP1GetDisplayFB()[fbOffset & 0x3FFFE]);

    SpriteData data{};
    switch (m_VDP2.spriteParams.type) {
    case 0x0:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorCalcRatio = bit::extract<11, 13>(rawData);
        data.priority = bit::extract<14, 15>(rawData);
        break;
    case 0x1:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorCalcRatio = bit::extract<11, 12>(rawData);
        data.priority = bit::extract<13, 15>(rawData);
        break;
    case 0x2:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorCalcRatio = bit::extract<11, 13>(rawData);
        data.priority = bit::extract<14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        break;
    case 0x3:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorCalcRatio = bit::extract<11, 12>(rawData);
        data.priority = bit::extract<13, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        break;
    case 0x4:
        data.colorData = bit::extract<0, 9>(rawData);
        data.colorCalcRatio = bit::extract<10, 12>(rawData);
        data.priority = bit::extract<13, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        break;
    case 0x5:
        data.colorData = bit::extract<0, 10>(rawData);
        data.colorCalcRatio = bit::extract<11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        break;
    case 0x6:
        data.colorData = bit::extract<0, 9>(rawData);
        data.colorCalcRatio = bit::extract<10, 11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        break;
    case 0x7:
        data.colorData = bit::extract<0, 8>(rawData);
        data.colorCalcRatio = bit::extract<9, 11>(rawData);
        data.priority = bit::extract<12, 14>(rawData);
        data.shadowOrWindow = bit::extract<15>(rawData);
        break;
    }
    return data;
}

} // namespace satemu::vdp

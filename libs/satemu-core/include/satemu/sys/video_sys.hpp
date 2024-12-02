#pragma once

#include <satemu/hw/vdp/vdp1.hpp>
#include <satemu/hw/vdp/vdp2.hpp>

#include <satemu/util/inline.hpp>

// Forward declarations
namespace satemu::scu {

class SCU;

} // namespace satemu::scu

namespace satemu::sys {

// Framebuffer color is in little-endian XRGB8888 format
using FramebufferColor = uint32;

using CBRequestFramebuffer = util::Callback<FramebufferColor *(uint32 width, uint32 height)>;
using CBFrameComplete = util::Callback<void(FramebufferColor *fb, uint32 width, uint32 height)>;

class VideoSystem {
public:
    VideoSystem(scu::SCU &scu);

    void Reset(bool hard);

    FORCE_INLINE void SetCallbacks(CBRequestFramebuffer cbRequestFramebuffer, CBFrameComplete cbFrameComplete) {
        m_cbRequestFramebuffer = cbRequestFramebuffer;
        m_cbFrameComplete = cbFrameComplete;
    }

    // TODO: replace with scheduler events
    void Advance(uint64 cycles);

    vdp1::VDP1 VDP1;
    vdp2::VDP2 VDP2;

private:
    scu::SCU &m_SCU;

    // -------------------------------------------------------------------------
    // Frontend callbacks

    // Invoked when the renderer is about to start a new frame, to retrieve a buffer from the frontend in which to
    // render the screen. The frame will contain <width> x <height> pixels in XBGR8888 little-endian format.
    CBRequestFramebuffer m_cbRequestFramebuffer;

    // Invoked whne the renderer finishes drawing a frame.
    CBFrameComplete m_cbFrameComplete;

    // -------------------------------------------------------------------------
    // Timings and signals

    // Horizontal display phases:
    // NOTE: dots listed are for NTSC/PAL modes
    // NOTE: each dot takes 4 system (SH-2) cycles
    //
    // 0             320/352        347/375     400/432    427/455 dots
    // +----------------+--------------+-----------+-------------+
    // | Active display | Right border | Horz sync | Left border | (no blanking intervals?)
    // +-+--------------+-+------------+-----------+-------------+
    //   |                |
    //   |                +-- Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    //   |                    The border is optional.
    //   |
    //   +-- Graphics data is shown here
    //
    // Vertical display phases:
    // (from https://wiki.yabause.org/index.php5?title=VDP2, with extra notes by StrikerX3)
    // NOTE: scanlines listed are for NTSC/PAL modes
    //
    // +----------------+ Scanline 0
    // |                |
    // | Active display |   Graphics data is shown here.
    // |                |
    // +----------------+ Scanline 224, 240 or 256
    // |                |   Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    // | Bottom border  |   The bottom border is optional.
    // |                |
    // +----------------+ Scanline 232, 240, 256, 264 or 272
    // |                |
    // | Bottom blanking|   Appears as light black.
    // |                |
    // +----------------+ Scanline 237, 245, 259, 267 or 275
    // |                |
    // | Vertical sync  |   Appears as pure black.
    // |                |
    // +----------------+ Scanline 240, 248, 262, 270 or 278
    // |                |
    // | Top blanking   |   Appears as light black.
    // |                |
    // +----------------+ Scanline 255, 263, 281, 289 or 297
    // |                |   Either black (BDCLMD=0) or set to the border color as defined by the back screen.
    // | Top border     |   The top border is optional.
    // |                |
    // +----------------+ Scanline 262 or 313

    enum class HorizontalPhase { Active, RightBorder, HorizontalSync, LeftBorder };
    HorizontalPhase m_HPhase; // Current horizontal display phase

    enum class VerticalPhase { Active, BottomBorder, BottomBlanking, VerticalSync, TopBlanking, TopBorder, LastLine };
    VerticalPhase m_VPhase; // Current vertical display phase

    // Current cycles (for phase timing) measured in system cycles.
    // HCNT is derived from this.
    // TODO: replace with scheduler
    uint64 m_currCycles;
    uint32 m_dotClockMult;
    uint16 m_VCounter;

    // Display resolution (derived from TVMODE)
    uint32 m_HRes; // Horizontal display resolution
    uint32 m_VRes; // Vertical display resolution

    // Display timings
    std::array<uint32, 4> m_HTimings;
    std::array<uint32, 7> m_VTimings;

    // Updates the display resolution and timings based on TVMODE if it is dirty
    void UpdateResolution();

    void IncrementVCounter();

    // Phase handlers
    void BeginHPhaseActiveDisplay();
    void BeginHPhaseRightBorder();
    void BeginHPhaseHorizontalSync();
    void BeginHPhaseLeftBorder();

    void BeginVPhaseActiveDisplay();
    void BeginVPhaseBottomBorder();
    void BeginVPhaseBottomBlanking();
    void BeginVPhaseVerticalSync();
    void BeginVPhaseTopBlanking();
    void BeginVPhaseTopBorder();
    void BeginVPhaseLastLine();

    // -------------------------------------------------------------------------
    // Rendering
    // TODO: move to a class or namespace

    // Pattern Name Data, contains parameters for a character
    struct Character {
        uint16 charNum;     // Character number, 15 bits
        uint8 palNum;       // Palette number, 7 bits
        bool specColorCalc; // Special color calculation
        bool specPriority;  // Special priority
        bool flipH;         // Horizontal flip
        bool flipV;         // Vertical flip
    };

    struct BGRenderContext {
        // CRAM base offset for color fetching.
        // Derived from RAMCTL.CRMDn and CRAOFA/CRAOFB.xxCAOSn
        uint32 cramOffset;

        // Bits 3-1 of the color data retrieved from VRAM per pixel.
        // Used by special priority function.
        std::array<uint8, 704> colorData;

        // Colors per pixel
        std::array<vdp::Color888, 704> colors;

        // Priorities per pixel
        std::array<uint8, 704> priorities;
    };

    // Render contexts for NBGs 0-3 then RBGs 0-1
    std::array<BGRenderContext, 4 + 2> m_renderContexts;

    // Framebuffer provided by the frontend to render the current frame into
    FramebufferColor *m_framebuffer;

    // Draws the scanline at m_VCounter.
    void DrawLine();

    // Draws a normal scroll BG scanline.
    // bgParams contains the parameters for the BG to draw.
    // rctx contains additional context for the renderer.
    // twoWordChar indicates if character patterns use one word (false) or two words (true).
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <bool twoWordChar, bool fourCellChar, bool wideChar, vdp2::ColorFormat colorFormat, uint32 colorMode>
    void DrawNormalScrollBG(const vdp2::NormBGParams &bgParams, BGRenderContext &rctx);

    // Draws a normal bitmap BG scanline.
    // bgParams contains the parameters for the BG to draw.
    // rctx contains additional context for the renderer.
    // colorFormat is the color format for bitmap data.
    // colorMode is the CRAM color mode.
    template <vdp2::ColorFormat colorFormat, uint32 colorMode>
    void DrawNormalBitmapBG(const vdp2::NormBGParams &bgParams, BGRenderContext &rctx);

    // Fetches a two-word character from VRAM.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    Character FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a one-word character from VRAM.
    // bgParams contains the parameters for the BG to draw.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // largePalette indicates if the color format uses 16 colors (false) or more (true).
    // wideChar indicates if the flip bits are available (false) or used to extend the character number (true).
    template <bool fourCellChar, bool largePalette, bool wideChar>
    Character FetchOneWordCharacter(const vdp2::NormBGParams &bgParams, uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a color from a pixel in the specified cell in a 2x2 character pattern.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorData is an output variable where bits 3-1 of the palette color data from VRAM is stored.
    // ch contains character parameters.
    // dotX and dotY specify the coordinates of the pixel within the cell, both ranging from 0 to 7.
    // cellIndex is the index of the cell in the character pattern, ranging from 0 to 3.
    // colorFormat is the value of CHCTLA/CHCTLB.xxCHCNn.
    // colorMode is the CRAM color mode.
    template <vdp2::ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 FetchCharacterColor(uint32 cramOffset, uint8 &colorData, Character ch, uint8 dotX, uint8 dotY,
                                      uint32 cellIndex);

    // Fetches a color from a bitmap pixel.
    // bgParams contains the bitmap parameters.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // dotX and dotY specify the coordinates of the pixel within the bitmap.
    // colorFormat is the color format for pixel data.
    // colorMode is the CRAM color mode.
    template <vdp2::ColorFormat colorFormat, uint32 colorMode>
    vdp::Color888 FetchBitmapColor(const vdp2::NormBGParams &bgParams, uint32 cramOffset, uint8 dotX, uint8 dotY);

    // Fetches a color from CRAM using the current color mode specified by RAMCTL.CRMDn.
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorIndex specifies the color index.
    // colorMode is the CRAM color mode.
    template <uint32 colorMode>
    vdp::Color888 FetchCRAMColor(uint32 cramOffset, uint32 colorIndex);
};

} // namespace satemu::sys

#pragma once

#include "vdp_defs.hpp"

#include "vdp1_regs.hpp"
#include "vdp2_regs.hpp"

#include <satemu/core/scheduler.hpp>
#include <satemu/sys/system.hpp>

#include <satemu/hw/hw_defs.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>
#include <satemu/util/debug_print.hpp>
#include <satemu/util/inline.hpp>

#include <array>
#include <iosfwd>
#include <span>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu {

struct Saturn;

} // namespace satemu

namespace satemu::sh2 {

class SH2Bus;

} // namespace satemu::sh2

namespace satemu::scu {

class SCU;

} // namespace satemu::scu

namespace satemu::smpc {

class SMPC;

} // namespace satemu::smpc

// -----------------------------------------------------------------------------

namespace satemu::vdp {

// Contains both VDP1 and VDP2
class VDP {
    static constexpr dbg::Category rootLog1{"VDP1"};
    static constexpr dbg::Category regsLog1{rootLog1, "Regs"};
    static constexpr dbg::Category renderLog1{rootLog1, "Render"};

    static constexpr dbg::Category rootLog2{"VDP2"};
    static constexpr dbg::Category regsLog2{rootLog2, "Regs"};
    static constexpr dbg::Category renderLog2{rootLog2, "Render"};

public:
    VDP(core::Scheduler &scheduler, scu::SCU &scu, smpc::SMPC &smpc);

    void Reset(bool hard);

    FORCE_INLINE void SetCallbacks(CBRequestFramebuffer cbRequestFramebuffer, CBFrameComplete cbFrameComplete) {
        m_cbRequestFramebuffer = cbRequestFramebuffer;
        m_cbFrameComplete = cbFrameComplete;
    }

    // TODO: replace with scheduler events
    template <bool debug>
    void Advance(uint64 cycles);

    void DumpVDP1VRAM(std::ostream &out) const;
    void DumpVDP2VRAM(std::ostream &out) const;
    void DumpVDP2CRAM(std::ostream &out) const;

    // Dumps draw then display
    void DumpVDP1Framebuffers(std::ostream &out) const;

    bool InLastLinePhase() const {
        return m_VPhase == VerticalPhase::LastLine;
    }

private:
    alignas(16) std::array<uint8, kVDP1VRAMSize> m_VRAM1;
    alignas(16) std::array<uint8, kVDP2VRAMSize> m_VRAM2; // 4x 128 KiB banks: A0, A1, B0, B1
    alignas(16) std::array<uint8, kVDP2CRAMSize> m_CRAM;
    alignas(16) std::array<std::array<uint8, kVDP1FramebufferRAMSize>, 2> m_spriteFB;
    std::size_t m_drawFB; // index of current sprite draw buffer; opposite buffer is CPU-accessible

    scu::SCU &m_SCU;
    smpc::SMPC &m_SMPC;

    core::Scheduler &m_scheduler;
    core::EventID m_phaseUpdateEvent;

    template <bool debug>
    static void OnPhaseUpdateEvent(core::EventContext &eventContext, void *userContext, uint64 cyclesLate);

    friend struct satemu::Saturn;
    void SetVideoStandard(sys::VideoStandard videoStandard);

    // -------------------------------------------------------------------------
    // Memory accessors

    void MapMemory(sh2::SH2Bus &bus);

    // -------------------------------------------------------------------------
    // VDP1 memory/register access

    template <mem_primitive T>
    T VDP1ReadVRAM(uint32 address);

    template <mem_primitive T>
    void VDP1WriteVRAM(uint32 address, T value);

    template <mem_primitive T>
    T VDP1ReadFB(uint32 address);

    template <mem_primitive T>
    void VDP1WriteFB(uint32 address, T value);

    template <mem_primitive T>
    T VDP1ReadReg(uint32 address);

    template <mem_primitive T>
    void VDP1WriteReg(uint32 address, T value);

    // -------------------------------------------------------------------------
    // VDP2 memory/register access

    template <mem_primitive T>
    T VDP2ReadVRAM(uint32 address);

    template <mem_primitive T>
    void VDP2WriteVRAM(uint32 address, T value);

    template <mem_primitive T>
    T VDP2ReadCRAM(uint32 address);

    template <mem_primitive T>
    void VDP2WriteCRAM(uint32 address, T value);

    template <mem_primitive T>
    T VDP2ReadReg(uint32 address);

    template <mem_primitive T>
    void VDP2WriteReg(uint32 address, T value);

    // -------------------------------------------------------------------------
    // Frontend callbacks

    // Invoked when the renderer is about to start a new frame, to retrieve a buffer from the frontend in which to
    // render the screen. The frame will contain <width> x <height> pixels in XBGR8888 little-endian format.
    CBRequestFramebuffer m_cbRequestFramebuffer;

    // Invoked whne the renderer finishes drawing a frame.
    CBFrameComplete m_cbFrameComplete;

    // -------------------------------------------------------------------------
    // Registers

    VDP1Regs m_VDP1;
    VDP2Regs m_VDP2;

    // -------------------------------------------------------------------------

    // RAMCTL.CRMD modes 2 and 3 shuffle address bits as follows:
    //   10 09 08 07 06 05 04 03 02 01 11 00
    //   in short, bits 10-01 are shifted left and bit 11 takes place of bit 01
    FORCE_INLINE uint32 MapCRAMAddress(uint32 address) const {
        static constexpr auto kMapping = [] {
            std::array<std::array<uint32, 4096>, 2> addrs{};
            for (uint32 addr = 0; addr < 4096; addr++) {
                addrs[0][addr] = addr;
                addrs[1][addr] =
                    bit::extract<0>(addr) | (bit::extract<11>(addr) << 1u) | (bit::extract<1, 10>(addr) << 2u);
            }
            return addrs;
        }();

        return kMapping[m_VDP2.RAMCTL.CRMDn >> 1][address & 0xFFF];

        /*if (m_VDP2.RAMCTL.CRMDn == 2 || m_VDP2.RAMCTL.CRMDn == 3) {
            address =
                bit::extract<0>(address) | (bit::extract<11>(address) << 1u) | (bit::extract<1, 10>(address) << 2u);
        }
        return address & 0xFFF;*/
    }

    // -------------------------------------------------------------------------
    // Timings and signals

    // Based on https://github.com/srg320/Saturn_hw/blob/main/VDP2/VDP2.xlsx

    // Horizontal display phases:
    // NOTE: each dot takes 4 system (SH-2) cycles on standard resolutions, 2 system cycles on hi-res modes
    // NOTE: hi-res modes doubles all HCNTs
    //
    //   320 352  dots
    // --------------------------------
    //     0   0  Active display area
    //   320 352  Right border
    //   347 375  Horizontal sync
    //   374 403  VBlank OUT
    //   400 432  Left border
    //   426 454  Last dot
    //   427 455  Total HCNT
    //
    // Vertical display phases:
    // NOTE: bottom blanking, vertical sync and top blanking are consolidated into a single phase since no important
    // events happen other than not drawing the border
    //
    //    NTSC    --  PAL  --
    //   224 240  224 240 256  lines
    // ---------------------------------------------
    //     0   0    0   0   0  Active display area
    //   224 240  224 240 256  Bottom border
    //   232 240  256 264 272  Bottom blanking | these are
    //   237 245  259 267 275  Vertical sync   | merged into
    //   240 248  262 270 278  Top blanking    | one phase
    //   255 255  281 289 297  Top border
    //   262 262  312 312 312  Last line
    //   263 263  313 313 313  Total VCNT
    //
    // Events:
    //   VBLANK signal is raised when entering bottom border V phase
    //   VBLANK signal is lowered when entering VBlank clear H phase during last line V phase
    //
    //   HBLANK signal is raised when entering right border H phase (closest match, 4 cycles early)
    //   HBLANK signal is lowered when entering left border H phase (closest match, 10 cycles early)
    //
    //   Even/odd field flag is flipped when entering last dot H phase during first line of bottom border V phase
    //
    //   VBlank IN/OUT interrupts are raised when the VBLANK signal is raised/lowered
    //   HBlank IN interrupt is raised when the HBLANK signal is raised
    //
    //   Drawing happens when in both active display area phases
    //   Border drawing happens when in any of the border phases

    enum class HorizontalPhase { Active, RightBorder, Sync, VBlankOut, LeftBorder, LastDot };
    HorizontalPhase m_HPhase; // Current horizontal display phase

    enum class VerticalPhase { Active, BottomBorder, BlankingAndSync, TopBorder, LastLine };
    VerticalPhase m_VPhase; // Current vertical display phase

    // 180008   HCNT    H Counter
    //
    //   bits   r/w  code          description
    //  15-10        -             Reserved, must be zero
    //    9-0   R    HCT9-0        H Counter Value
    //
    // Notes
    // - Counter layout depends on screen mode:
    //     Normal: bits 8-0 shifted left by 1; HCT0 is invalid
    //     Hi-Res: bits 9-0
    //     Excl. Normal: bits 8-0 (no shift); HCT9 is invalid
    //     Excl. Hi-Res: bits 9-1 shifted right by 1; HCT9 is invalid
    //
    // 18000A   VCNT    V Counter
    //
    //   bits   r/w  code          description
    //  15-10        -             Reserved, must be zero
    //    9-0   R    VCT9-0        V Counter Value
    //
    // Notes
    // - Counter layout depends on screen mode:
    //     Exclusive Monitor: bits 9-0
    //     Normal Hi-Res double-density interlace:
    //       bits 8-0 shifted left by 1
    //       bit 0 contains interlaced field (0=odd, 1=even)
    //     All other modes: bits 8-0 shifted left by 1; VCT0 is invalid

    // TODO: store latched HCounter
    uint16 m_VCounter;

    // Display resolution (derived from TVMODE)
    uint32 m_HRes; // Horizontal display resolution
    uint32 m_VRes; // Vertical display resolution

    // Display timings
    std::array<uint32, 6> m_HTimings;
    std::array<uint32, 5> m_VTimings;

    // Moves to the next phase.
    template <bool debug>
    void UpdatePhase();

    // Returns the number of cycles between the current and the next phase.
    uint64 GetPhaseCycles() const;

    // Updates the display resolution and timings based on TVMODE if it is dirty
    void UpdateResolution();

    void IncrementVCounter();

    // Phase handlers
    void BeginHPhaseActiveDisplay();
    void BeginHPhaseRightBorder();
    void BeginHPhaseSync();
    void BeginHPhaseVBlankOut();
    void BeginHPhaseLeftBorder();
    void BeginHPhaseLastDot();

    void BeginVPhaseActiveDisplay();
    void BeginVPhaseBottomBorder();
    void BeginVPhaseBlankingAndSync();
    void BeginVPhaseTopBorder();
    void BeginVPhaseLastLine();

    // -------------------------------------------------------------------------
    // VDP1

    // TODO: split out rendering code

    // VDP1 renderer parameters and state
    struct VDP1RenderContext {
        VDP1RenderContext() {
            Reset();
        }

        void Reset() {
            sysClipH = 512;
            sysClipV = 256;

            userClipX0 = 0;
            userClipY0 = 0;

            userClipX1 = 512;
            userClipY1 = 256;

            localCoordX = 0;
            localCoordY = 0;

            rendering = false;
        }

        // System clipping dimensions
        uint16 sysClipH;
        uint16 sysClipV;

        // User clipping area
        // Top-left
        uint16 userClipX0;
        uint16 userClipY0;
        // Bottom-right
        uint16 userClipX1;
        uint16 userClipY1;

        // Local coordinates offset
        sint32 localCoordX;
        sint32 localCoordY;

        bool rendering;
    } m_VDP1RenderContext;

    struct VDP1GouraudParams {
        Color555 colorA;
        Color555 colorB;
        Color555 colorC;
        Color555 colorD;
        uint32 U; // 16 fractional bits, from 0.0 to 1.0
        uint32 V; // 16 fractional bits, from 0.0 to 1.0
    };

    struct VDP1PixelParams {
        VDP1Command::DrawMode mode;
        uint16 color;
    };

    struct VDP1TexturedLineParams {
        VDP1Command::Control control;
        VDP1Command::DrawMode mode;
        uint32 colorBank;
        uint32 charAddr;
        uint32 charSizeH;
        uint32 charSizeV;
        uint64 texFracV;
    };

    // Character modes, a combination of Character Size from the Character Control Register (CHCTLA-B) and Character
    // Number Supplement from the Pattern Name Control Register (PNCN0-3/PNCR)
    enum class CharacterMode {
        TwoWord,         // 2 word characters
        OneWordStandard, // 1 word characters with standard character data, H/V flip available
        OneWordExtended, // 1 word characters with extended character data; H/V flip unavailable
    };

    // Pattern Name Data, contains parameters for a character
    struct Character {
        uint16 charNum;     // Character number, 15 bits
        uint8 palNum;       // Palette number, 7 bits
        bool specColorCalc; // Special color calculation
        bool specPriority;  // Special priority
        bool flipH;         // Horizontal flip
        bool flipV;         // Vertical flip
    };

    // Common pixel data: color, transparency, priority and special color calculation flag.
    struct Pixel {
        Color888 color = {.u32 = 0};
        bool transparent = true;
        uint8 priority = 0;
        bool specialColorCalc = false;
    };

    // Layer state, containing the pixel output for the current scanline.
    struct LayerState {
        LayerState() {
            Reset();
        }

        void Reset() {
            pixels.fill({});
            enabled = false;
        }

        alignas(16) std::array<Pixel, kMaxResH> pixels;

        bool enabled;
    };

    // Layer state specific to the sprite layer.
    // Includes additional pixel attributes for each pixel in the scanline.
    struct SpriteLayerState {
        SpriteLayerState() {
            Reset();
        }

        void Reset() {
            attrs.fill({});
        }

        struct Attributes {
            uint8 colorCalcRatio = 0;
            bool shadowOrWindow = false;
            bool normalShadow = false;
            bool msbSet = false;
        };

        alignas(16) std::array<Attributes, kMaxResH> attrs;
    };

    // NBG layer state, including coordinate counters, increments and addresses.
    struct NormBGLayerState {
        NormBGLayerState() {
            Reset();
        }

        void Reset() {
            fracScrollX = 0;
            fracScrollY = 0;
            scrollIncH = 0x100;
            lineScrollTableAddress = 0;
            mosaicCounterY = 0;
        }

        // Initial fractional X scroll coordinate.
        uint32 fracScrollX;

        // Fractional Y scroll coordinate.
        // Reset at the start of every frame and updated every scanline.
        uint32 fracScrollY;

        // Fractional X scroll coordinate increment.
        // Applied every scanline.
        uint32 scrollIncH;

        // Current line scroll table address.
        // Reset at the start of every frame and incremented every 1/2/4/8/16 lines.
        uint32 lineScrollTableAddress;

        // Vertical mosaic counter.
        // Reset at the start of every frame and incremented every line.
        // The value is mod mosaicV.
        uint8 mosaicCounterY;
    };

    // State for Rotation Parameters A and B.
    struct RotationParamState {
        RotationParamState() {
            Reset();
        }

        void Reset() {
            pageBaseAddresses.fill(0);
            screenCoords.fill({});
            lineColor.fill({.u32 = 0});
            transparent.fill(0);
            scrX = scrY = 0;
            KA = 0;
        }

        // Page base addresses for RBG planes A-P using Rotation Parameters A and B.
        // Indexing: [Plane A-P]
        // Derived from mapIndices, CHCTLA/CHCTLB.xxCHSZ, PNCR.xxPNB and PLSZ.xxPLSZn
        std::array<uint32, 16> pageBaseAddresses;

        // Precomputed screen coordinates (with 16 fractional bits).
        alignas(16) std::array<CoordS32, kMaxResH> screenCoords;

        // Precomputed coefficient table line color.
        // Filled in only if the coefficient table is enabled and using line color data.
        alignas(16) std::array<Color888, kMaxResH> lineColor;

        // Prefetched coefficient table transparency bits.
        // Filled in only if the coefficient table is enabled.
        alignas(16) std::array<bool, kMaxResH> transparent;

        // Current base screen coordinates, updated every scanline.
        sint32 scrX, scrY;

        // Current base coefficient address, updated every scanline.
        uint32 KA;
    };

    enum RotParamSelector { RotParamA, RotParamB };

    // State of the LNCL and BACK screens, including the current color and address.
    struct LineBackLayerState {
        LineBackLayerState() {
            Reset();
        }

        void Reset() {
            lineColor.u32 = 0;
            backColor.u32 = 0;
        }

        Color888 lineColor;
        Color888 backColor;
    };

    // Layer state indices
    enum Layer { LYR_Sprite, LYR_RBG0, LYR_NBG0_RBG1, LYR_NBG1_EXBG, LYR_NBG2, LYR_NBG3, LYR_Back, LYR_LineColor };

    // Common layer states
    //     RBG0+RBG1   RBG0        no RBGs
    // [0] Sprite      Sprite      Sprite
    // [1] RBG0        RBG0        -
    // [2] RBG1        NBG0        NBG0
    // [3] EXBG        NBG1/EXBG   NBG1/EXBG
    // [4] -           NBG2        NBG2
    // [5] -           NBG3        NBG3
    std::array<LayerState, 6> m_layerStates;

    // Sprite layer state
    SpriteLayerState m_spriteLayerState;

    // Layer state for NBGs 0-3
    std::array<NormBGLayerState, 4> m_normBGLayerStates;

    // States for Rotation Parameters A and B.
    std::array<RotationParamState, 2> m_rotParamStates;

    // State for the line color and back screens.
    LineBackLayerState m_lineBackLayerState;

    // Framebuffer provided by the frontend to render the current frame into
    FramebufferColor *m_framebuffer;

    // Gets the current VDP1 draw framebuffer.
    std::array<uint8, kVDP1FramebufferRAMSize> &VDP1GetDrawFB();

    // Gets the current VDP1 display framebuffer.
    std::array<uint8, kVDP1FramebufferRAMSize> &VDP1GetDisplayFB();

    // Erases the current VDP1 display framebuffer.
    void VDP1EraseFramebuffer();

    // Swaps VDP1 framebuffers.
    void VDP1SwapFramebuffer();

    // Begins the next VDP1 frame.
    void VDP1BeginFrame();

    // Ends the current VDP1 frame.
    void VDP1EndFrame();

    // Processes the VDP1 command table.
    void VDP1ProcessCommands();

    bool VDP1IsPixelUserClipped(CoordS32 coord) const;
    bool VDP1IsPixelSystemClipped(CoordS32 coord) const;
    bool VDP1IsLineSystemClipped(CoordS32 coord1, CoordS32 coord2) const;
    bool VDP1IsQuadSystemClipped(CoordS32 coord1, CoordS32 coord2, CoordS32 coord3, CoordS32 coord4) const;

    void VDP1PlotPixel(CoordS32 coord, const VDP1PixelParams &pixelParams, const VDP1GouraudParams &gouraudParams);
    void VDP1PlotLine(CoordS32 coord1, CoordS32 coord2, const VDP1PixelParams &pixelParams,
                      VDP1GouraudParams &gouraudParams);
    void VDP1PlotTexturedLine(CoordS32 coord1, CoordS32 coord2, const VDP1TexturedLineParams &lineParams,
                              VDP1GouraudParams &gouraudParams);

    // Individual VDP1 command processors

    void VDP1Cmd_DrawNormalSprite(uint32 cmdAddress, VDP1Command::Control control);
    void VDP1Cmd_DrawScaledSprite(uint32 cmdAddress, VDP1Command::Control control);
    void VDP1Cmd_DrawDistortedSprite(uint32 cmdAddress, VDP1Command::Control control);

    void VDP1Cmd_DrawPolygon(uint32 cmdAddress);
    void VDP1Cmd_DrawPolylines(uint32 cmdAddress);
    void VDP1Cmd_DrawLine(uint32 cmdAddress);

    void VDP1Cmd_SetSystemClipping(uint32 cmdAddress);
    void VDP1Cmd_SetUserClipping(uint32 cmdAddress);
    void VDP1Cmd_SetLocalCoordinates(uint32 cmdAddress);

    // -------------------------------------------------------------------------
    // VDP2

    // Initializes renderer state for a new frame.
    void VDP2InitFrame();

    // Initializes the specified NBG.
    template <uint32 index>
    void VDP2InitNormalBG();

    // Initializes the specified RBG.
    template <uint32 index>
    void VDP2InitRotationBG();

    // Updates the enabled backgrounds.
    void VDP2UpdateEnabledBGs();

    // Updates the line screen scroll parameters for the given background.
    // Only valid for NBG0 and NBG1.
    //
    // bgParams contains the parameters for the BG to draw.
    // bgState is a reference to the background layer state for the background.
    void VDP2UpdateLineScreenScroll(const BGParams &bgParams, NormBGLayerState &bgState);

    // Loads rotation parameter tables and calculates coefficients and increments.
    void VDP2CalcRotationParameterTables();

    // Draws the current VDP2 scanline.
    void VDP2DrawLine();

    // Draws the line color and back screens.
    void VDP2DrawLineColorAndBackScreens();

    // Draws the current VDP2 scanline of the sprite layer.
    //
    // colorMode is the CRAM color mode.
    // rotate determines if Rotation Parameter A coordinates should be used to draw the sprite layer.
    template <uint32 colorMode, bool rotate>
    void VDP2DrawSpriteLayer();

    // Draws the current VDP2 scanline of the specified normal background layer.
    //
    // colorMode is the CRAM color mode.
    //
    // bgIndex specifies the normal background index, from 0 to 3.
    template <uint32 bgIndex>
    void VDP2DrawNormalBG(uint32 colorMode);

    // Draws the current VDP2 scanline of the specified rotation background layer.
    //
    // colorMode is the CRAM color mode.
    //
    // bgIndex specifies the rotation background index, from 0 to 1.
    template <uint32 bgIndex>
    void VDP2DrawRotationBG(uint32 colorMode);

    // Composes the current VDP2 scanline out of the rendered lines.
    void VDP2ComposeLine();

    // Draws a normal scroll BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    // bgState is a reference to the background layer state for the background.
    //
    // charMode indicates if character patterns use two words or one word with standard or extended character data.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawNormalScrollBG(const BGParams &bgParams, LayerState &layerState, NormBGLayerState &bgState);

    // Draws a normal bitmap BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    // bgState is a reference to the background layer state for the background.
    //
    // colorFormat is the color format for bitmap data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawNormalBitmapBG(const BGParams &bgParams, LayerState &layerState, NormBGLayerState &bgState);

    // Draws a rotation scroll BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    //
    // selRotParam enables dynamic rotation parameter selection (for RBG0).
    // charMode indicates if character patterns use two words or one word with standard or extended character data.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <bool selRotParam, CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawRotationScrollBG(const BGParams &bgParams, LayerState &layerState);

    // Draws a rotation bitmap BG scanline.
    //
    // bgParams contains the parameters for the BG to draw.
    // layerState is a reference to the common layer state for the background.
    //
    // selRotParam enables dynamic rotation parameter selection (for RBG0).
    // colorFormat is the color format for bitmap data.
    // colorMode is the CRAM color mode.
    template <bool selRotParam, ColorFormat colorFormat, uint32 colorMode>
    void VDP2DrawRotationBitmapBG(const BGParams &bgParams, LayerState &layerState);

    // Selects a rotation parameter set based on the current parameter selection mode.
    //
    // x is the horizontal coordinate of the pixel
    RotParamSelector VDP2SelectRotationParameter(uint32 x);

    // Determines if a rotation coefficient entry can be fetched from the specified address.
    // Coefficients can always be fetched from CRAM.
    // Coefficients can only be fetched from VRAM if the corresponding bank is designated for coefficient data.
    //
    // params is the rotation parameter from which to retrieve the base address and coefficient data size.
    // coeffAddress is the calculated coefficient address (KA).
    bool VDP2CanFetchCoefficient(const RotationParams &params, uint32 coeffAddress) const;

    // Fetches a rotation coefficient entry from VRAM or CRAM (depending on RAMCTL.CRKTE) using the specified rotation
    // parameters.
    //
    // params is the rotation parameter from which to retrieve the base address and coefficient data size.
    // coeffAddress is the calculated coefficient address (KA).
    Coefficient VDP2FetchRotationCoefficient(const RotationParams &params, uint32 coeffAddress);

    // Checks if the pixel at the given (X, VCounter) coordinate is inside the specified windows.
    // Retrusn true if the pixel is inside a window.
    // Returns false if the pixel is outside all windows or no windows are enabled.
    //
    // windowSet contains the window set to check.
    // x is the horizontal coordinate of the pixel.
    //
    // hasSpriteWindow determines if the window set contains the sprite window (SW).
    template <bool hasSpriteWindow>
    bool VDP2IsInsideWindow(const WindowSet<hasSpriteWindow> &windowSet, uint32 x);

    // Fetches a scroll background pixel at the given coordinates.
    //
    // bgParams contains the parameters for the BG to draw.
    // pageBaseAddresses is a reference to the table containing the planes' pages' base addresses.
    // pageShiftH and pageShiftV are address shifts derived from PLSZ to determine the plane and page indices.
    // scrollCoord has the coordinates of the scroll screen.
    //
    // charMode indicates if character patterns use two words or one word with standard or extended character data.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // colorFormat is the color format for cell data.
    // colorMode is the CRAM color mode.
    template <bool rot, CharacterMode charMode, bool fourCellChar, ColorFormat colorFormat, uint32 colorMode>
    Pixel VDP2FetchScrollBGPixel(const BGParams &bgParams, std::span<const uint32> pageBaseAddresses, uint32 pageShiftH,
                                 uint32 pageShiftV, CoordU32 scrollCoord);

    // Fetches a two-word character from VRAM.
    //
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    Character VDP2FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a one-word character from VRAM.
    //
    // bgParams contains the parameters for the BG to draw.
    // pageBaseAddress specifies the base address of the page of character patterns.
    // charIndex is the index of the character to fetch.
    // fourCellChar indicates if character patterns are 1x1 cells (false) or 2x2 cells (true).
    // largePalette indicates if the color format uses 16 colors (false) or more (true).
    // extChar indicates if the flip bits are available (false) or used to extend the character number (true).
    template <bool fourCellChar, bool largePalette, bool extChar>
    Character VDP2FetchOneWordCharacter(const BGParams &bgParams, uint32 pageBaseAddress, uint32 charIndex);

    // Fetches a pixel in the specified cell in a 2x2 character pattern.
    //
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // ch is the character's parameters.
    // dotCoord specify the coordinates of the pixel within the cell, ranging from 0 to 7.
    // cellIndex is the index of the cell in the character pattern, ranging from 0 to 3.
    //
    // colorFormat is the value of CHCTLA/CHCTLB.xxCHCNn.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    Pixel VDP2FetchCharacterPixel(const BGParams &bgParams, Character ch, CoordU32 dotCoord, uint32 cellIndex);

    // Fetches a bitmap pixel at the given coordinates.
    //
    // bgParams contains the parameters for the BG to draw.
    // dotCoord specify the coordinates of the pixel within the bitmap.
    //
    // colorFormat is the color format for pixel data.
    // colorMode is the CRAM color mode.
    template <ColorFormat colorFormat, uint32 colorMode>
    Pixel VDP2FetchBitmapPixel(const BGParams &bgParams, CoordU32 dotCoord);

    // Fetches a color from CRAM using the current color mode specified by RAMCTL.CRMDn.
    //
    // cramOffset is the base CRAM offset computed from CRAOFA/CRAOFB.xxCAOSn and RAMCTL.CRMDn.
    // colorIndex specifies the color index.
    // colorMode is the CRAM color mode.
    template <uint32 colorMode>
    Color888 VDP2FetchCRAMColor(uint32 cramOffset, uint32 colorIndex);

    // Fetches sprite data based on the current sprite mode.
    //
    // fbOffset is the offset into the framebuffer (in bytes) where the sprite data is located.
    SpriteData VDP2FetchSpriteData(uint32 fbOffset);

    // Fetches 16-bit sprite data based on the current sprite mode.
    //
    // fbOffset is the offset into the framebuffer (in bytes) where the sprite data is located.
    // type is the sprite type (between 0 and 7).
    SpriteData VDP2FetchWordSpriteData(uint32 fbOffset, uint8 type);

    // Fetches 8-bit sprite data based on the current sprite mode.
    //
    // fbOffset is the offset into the framebuffer (in bytes) where the sprite data is located.
    // type is the sprite type (between 8 and 15).
    SpriteData VDP2FetchByteSpriteData(uint32 fbOffset, uint8 type);

    // Determines the type of sprite shadow (if any) based on color data.
    //
    // colorData is the raw color data.
    //
    // colorDataBits specifies the bit width of the color data.
    template <uint32 colorDataBits>
    static bool VDP2IsNormalShadow(uint16 colorData);

    // Retrieves the Y display coordinate based on the current interlace mode.
    //
    // y is the Y coordinate to translate
    uint32 VDP2GetY(uint32 y) const;
};

} // namespace satemu::vdp

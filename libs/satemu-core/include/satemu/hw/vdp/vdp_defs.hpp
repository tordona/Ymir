#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/callback.hpp>
#include <satemu/util/data_ops.hpp>
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

// -----------------------------------------------------------------------------
// VDP1 registers

// VDP1 command structure in VRAM
//   00  CMDCTRL  Control Words
//   02  CMDLINK  Link Specification
//   04  CMDPMOD  Draw Mode Word
//   06  CMDCOLR  Color Control Word
//   08  CMDSRCA  Character Address
//   0A  CMDSIZE  Character Size
//   0C  CMDXA    Vertex A X Coordinate
//   0E  CMDYA    Vertex A Y Coordinate
//   10  CMDXB    Vertex B X Coordinate
//   12  CMDYB    Vertex B Y Coordinate
//   14  CMDXC    Vertex C X Coordinate
//   16  CMDYC    Vertex C Y Coordinate
//   18  CMDXD    Vertex D X Coordinate
//   1A  CMDYD    Vertex D Y Coordinate
//   1C  CMDGRDA  Gouraud Shading Table
struct VDP1Command {
    enum class CommandType : uint16 {
        // Textured drawing
        DrawNormalSprite = 0x0,
        DrawScaledSprite = 0x1,
        DrawDistortedSprite = 0x2,
        DrawDistortedSpriteAlt = 0x3,

        // Untextured drawing
        DrawPolygon = 0x4,
        DrawPolylines = 0x5,
        DrawPolylinesAlt = 0x7,
        DrawLine = 0x6,

        // Clipping coordinate setting
        UserClipping = 0x8,
        UserClippingAlt = 0xB,
        SystemClipping = 0x9,

        // Local coordinate setting
        SetLocalCoordinates = 0xA,
    };

    enum class JumpType : uint16 {
        Next = 0x0,
        Assign = 0x1,
        Call = 0x2,
        Return = 0x3,
    };

    // CMDCTRL
    union Control {
        uint16 u16;
        struct {
            CommandType command : 4;
            uint16 direction : 2;
            uint16 : 2;
            uint16 zoomPoint : 4;
            JumpType jumpMode : 2;
            uint16 skip : 1;
            uint16 end : 1;
        };
    };

    // CMDPMOD
    //   15   MSB On
    //   12   High-Speed Shrink
    //   11   Pre-clipping Disable
    //   10   User Clipping Enable
    //    9   User Clipping Mode
    //    8   Mesh Enable
    //    7   End Code Disable
    //    6   Transparent Pixel Disable
    //  5-3   Color Mode
    //          000 (0) = 4 bpp, 16 colors, bank mode
    //          001 (1) = 4 bpp, 16 colors, lookup table mode
    //          010 (2) = 8 bpp, 64 colors, bank mode
    //          011 (3) = 8 bpp, 128 colors, bank mode
    //          100 (4) = 8 bpp, 256 colors, bank mode
    //          101 (5) = 16 bpp, 32768 colors, RGB mode
    //  2-0   Color Calculation Bits
    union DrawMode {
        uint16 u16;
        struct {
            uint16 colorCalc : 3;
            uint16 colorMode : 3;
            uint16 transparentPixelDisable : 1;
            uint16 endCodeDisable : 1;
            uint16 meshEnable : 1;
            uint16 clippingMode : 1;
            uint16 userClippingEnable : 1;
            uint16 preClippingDisable : 1;
            uint16 highSpeedShrink : 1;
            uint16 : 2;
            uint16 msbOn : 1;
        };
    };

    // CMDSIZE
    //  13-8   Character Size X / 8
    //   7-0   Character Size Y
    union Size {
        uint16 u16;
        struct {
            uint16 V : 8;
            uint16 H : 6;
        };
    };
};

struct VDP1Regs {
    void Reset() {
        vblankErase = false;
        hdtvEnable = false;
        fbRotEnable = false;
        pixel8Bits = false;

        fbSwapTrigger = false;
        fbSwapMode = false;
        dblInterlaceDrawLine = false;
        dblInterlaceEnable = false;
        evenOddCoordSelect = false;

        plotTrigger = 0;

        eraseWriteValue = 0;

        eraseX1 = 0;
        eraseY1 = 0;
        eraseX3 = 0;
        eraseY3 = 0;

        // HACK: should be false
        currFrameEnded = true;
        prevFrameEnded = true;

        currCommandAddress = 0;
        prevCommandAddress = 0;

        returnAddress = ~0;

        fbManualErase = false;
        fbManualSwap = false;

        UpdateTVMR();
    }

    // Erase the framebuffer on VBlank.
    // Derived from TVMR.VBE
    bool vblankErase;

    // HDTV mode enable.
    // Derived from TVMR.TVM bit 2
    bool hdtvEnable;

    // Frame buffer rotation enable.
    // Derived from TVMR.TVM bit 1
    bool fbRotEnable;

    // Pixel data width - 8 bits (true) or 16 bits (false)
    // Derived from TVMR.TVM bit 0
    bool pixel8Bits;

    // Frame buffer dimensions.
    // Derived from TVMR.TVM
    uint32 fbSizeH;
    uint32 fbSizeV;

    // Frame buffer swap trigger: enabled (true) or disabled (false).
    // Exact behavior depends on TVMR.VBE, FBCR.FCM and FBCR.FCT.
    // Derived from FBCR.FCT
    bool fbSwapTrigger;

    // Frame buffer swap mode: manual (true) or 1-cycle mode (false).
    // Exact behavior depends on TVMR.VBE, FBCR.FCM and FBCR.FCT.
    // Derived from FBCR.FCM
    bool fbSwapMode;

    // Double interlace draw line (even/odd lines).
    // Behavior depends on FBCR.DIE.
    // Derived from FBCR.DIL
    bool dblInterlaceDrawLine;

    // Double interlace enable.
    // Derived from FBCR.DIE
    bool dblInterlaceEnable;

    // Even (false)/odd (true) coordinate select.
    // Affects High Speed Shrink drawing.
    // Derived from FBCR.EOS
    bool evenOddCoordSelect;

    // Frame drawing trigger.
    // Derived from PTMR.PTM
    uint8 plotTrigger;

    // Value written to erased parts of the framebuffer.
    // Derived from EWDR
    uint16 eraseWriteValue;

    // Erase window coordinates
    // Derived from EWLR and EWRR
    uint16 eraseX1; // Erase window top-left X coordinate
    uint16 eraseY1; // Erase window top-left Y coordinate
    uint16 eraseX3; // Erase window bottom-right X coordinate
    uint16 eraseY3; // Erase window bottom-right Y coordinate

    // Whether the drawing end command was fetched on the current and previous frames.
    // Used in EDSR
    bool currFrameEnded; // Drawing end bit fetched on current frame
    bool prevFrameEnded; // Drawing end bit fetched on previous frame

    // Addresses of the last executed commands in the current and previous frames.
    // Used in LOPR and COPR
    uint16 currCommandAddress; // Address of the last executed command in the current frame
    uint16 prevCommandAddress; // Address of the last executed command in the previous frame

    // Return address in the command table.
    // Used by commands that use the jump types Call and Return.
    uint16 returnAddress;

    bool fbManualErase; // Manual framebuffer erase requested
    bool fbManualSwap;  // Manual framebuffer swap requested

    void UpdateTVMR() {
        static constexpr uint32 kSizesH[] = {512, 1024, 512, 512, 512, 512, 512, 512};
        static constexpr uint32 kSizesV[] = {256, 256, 256, 512, 512, 512, 512, 512};
        const uint8 tvm = (hdtvEnable << 2) | (fbRotEnable << 1) | pixel8Bits;
        fbSizeH = kSizesH[tvm];
        fbSizeV = kSizesV[tvm];
    }

    // 100000   TVMR  TV Mode Selection
    //
    //   bits   r/w  code  description
    //   15-4        -     Reserved, must be zero
    //      3     W  VBE   V-Blank Erase/Write Enable
    //                       0 = do not erase/write during VBlank
    //                       1 = perform erase/write during VBlank
    //    2-0     W  TVM   TV Mode Select
    //                       bit 2: HDTV Enable (0=NTSC/PAL, 1=HDTV/31KC)
    //                       bit 1: Frame Buffer Rotation Enable (0=disable, 1=enable)
    //                       bit 0: Bit Depth Selection (0=16bpp, 1=8bpp)
    //
    // Notes:
    // - When using frame buffer rotation, interlace cannot be set to double density mode.
    // - When using HDTV modes, rotation must be disabled and the bit depth must be set to 16bpp
    // - TVM changes must be done between the 2nd HBlank IN from VBlank IN and the 1st HBlank IN after VBlank OUT.
    // - The frame buffer screen size varies based on TVM:
    //     TVM   Frame buffer screen size
    //     000    512x256
    //     001   1024x256
    //     010    512x256
    //     011    512x512
    //     100    512x256

    FORCE_INLINE void WriteTVMR(uint16 value) {
        vblankErase = bit::extract<3>(value);
        hdtvEnable = bit::extract<2>(value);
        fbRotEnable = bit::extract<1>(value);
        pixel8Bits = bit::extract<0>(value);
        UpdateTVMR();
    }

    // -------------------------------------------------------------------------

    // 100002   FBCR  Frame Buffer Change Mode
    //
    //   bits   r/w  code  description
    //   15-5        -     Reserved, must be zero
    //      4     W  EOS   Even/Odd Coordinate Select (sample pixels at: 0=even coordinates, 1=odd coordinates)
    //                       Related to High Speed Shrink option
    //      3     W  DIE   Double Interlace Enable (0=non-interlace/single interlace, 1=double interlace)
    //      2     W  DIL   Double Interlace Draw Line
    //                       If DIE = 0:
    //                         0 = draws even and odd lines
    //                         1 = (prohibited)
    //                       If DIE = 1:
    //                         0 = draws even lines only
    //                         1 = draws odd lines only
    //      1     W  FCM   Frame Buffer Change Mode
    //      0     W  FCT   Frame Buffer Change Trigger
    //
    // Notes:
    // TVMR.VBE, FCM and FCT specify when frame buffer swaps happen and whether they are cleared on swap.
    //   TVMR.VBE  FCM  FCT  Mode                          Timing
    //         0    0    0   1-cycle mode                  Swap every field (60 Hz)
    //         0    1    0   Manual mode (erase)           Erase in next field
    //         0    1    1   Manual mode (swap)            Swap in next field
    //         1    1    1   Manual mode (erase and swap)  Erase at VBlank IN and swap in next field
    // Unlisted combinations are prohibited.
    // For manual erase and swap, the program should write VBE,FCM,FCT = 011, then wait until the HBlank IN of the
    // last visible scanline immediately before VBlank (224 or 240) to issue another write to set VBE,FCM,FCT = 111,
    // and finally restore VBE = 0 after VBlank OUT to stop VDP1 from clearing the next frame buffer.

    FORCE_INLINE void WriteFBCR(uint16 value) {
        fbSwapTrigger = bit::extract<0>(value);
        fbSwapMode = bit::extract<1>(value);
        dblInterlaceDrawLine = bit::extract<2>(value);
        dblInterlaceEnable = bit::extract<3>(value);
        evenOddCoordSelect = bit::extract<4>(value);

        if (fbSwapMode) {
            if (fbSwapTrigger) {
                fbManualSwap = true;
            } else {
                fbManualErase = true;
            }
        }
    }

    // 100004   PTMR  Draw Trigger
    //
    //   bits   r/w  code  description
    //   15-2        -     Reserved, must be zero
    //    1-0     W  PTM   Plot Trigger Mode
    //                       00 (0) = No trigger
    //                       01 (1) = Trigger immediately upon writing this value to PTMR
    //                       10 (2) = Trigger on frame buffer swap
    //                       11 (3) = (prohibited)

    FORCE_INLINE void WritePTMR(uint16 value) {
        plotTrigger = bit::extract<0, 1>(value);
    }

    // 100006   EWDR  Erase/write Data
    //
    //   bits   r/w  code  description
    //   15-0     W  -     Erase/Write Data Value
    //
    // Notes:
    // - The entire register value is used to clear the frame buffer
    // - Writes 16-bit values at a time
    // - For 8-bit modes:
    //   - Bits 15-8 specify the values for even X coordinates
    //   - Bits 7-0 specify the values for odd X coordinates

    FORCE_INLINE void WriteEWDR(uint16 value) {
        eraseWriteValue = value;
    }

    // 100008   EWLR  Erase/write Upper-left coordinate
    //
    //   bits   r/w  code  description
    //     15        -     Reserved, must be zero
    //   14-9     W  -     Upper-left Coordinate X1
    //    8-0     W  -     Upper-left Coordinate Y1

    FORCE_INLINE void WriteEWLR(uint16 value) {
        eraseY1 = bit::extract<0, 8>(value);
        eraseX1 = bit::extract<9, 14>(value);
    }

    // 10000A   EWRR  Erase/write Bottom-right Coordinate
    //
    //   bits   r/w  code  description
    //   15-9     W  -     Lower-right Coordinate X3
    //    8-0     W  -     Lower-right Coordinate Y3

    FORCE_INLINE void WriteEWRR(uint16 value) {
        eraseY3 = bit::extract<0, 8>(value);
        eraseX3 = bit::extract<9, 15>(value);
    }

    // 10000C   ENDR  Draw Forced Termination
    //
    // (all bits are reserved and must be zero)
    //
    // Notes:
    // - Stops drawing ~30 clock cycles after the write is issued to this register

    // 100010   EDSR  Transfer End Status
    //
    //   bits   r/w  code  description
    //   15-2        -     Reserved, must be zero
    //      1   R    CEF   Current End Bit Fetch Status
    //                       0 = drawing in progress (end bit not yet fetched)
    //                       1 = drawing finished (end bit fetched)
    //      0   R    BEF   Before End Bit Fetch Status
    //                       0 = previous drawing end bit not fetched
    //                       1 = previous drawing end bit fetched

    FORCE_INLINE uint16 ReadEDSR() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, prevFrameEnded);
        bit::deposit_into<1>(value, currFrameEnded);
        return value;
    }

    // 100012   LOPR  Last Operation Command Address
    //
    //   bits   r/w  code  description
    //   15-0   R    -     Last Operation Command Address (divided by 8)

    FORCE_INLINE uint16 ReadLOPR() const {
        return prevCommandAddress >> 3u;
    }

    // 100014   COPR  Current Operation Command Address
    //
    //   bits   r/w  code  description
    //   15-0   R    -     Current Operation Command Address (divided by 8)

    FORCE_INLINE uint16 ReadCOPR() const {
        return currCommandAddress >> 3u;
    }

    // 100016   MODR  Mode Status
    //
    //   bits   r/w  code  description
    //  15-12   R    VER   Version Number (0b0001)
    //   11-9        -     Reserved, must be zero
    //      8   R    PTM1  Plot Trigger Mode (read-only view of PTMR.PTM bit 1)
    //      7   R    EOS   Even/Odd Coordinate Select (read-only view of FBCR.EOS)
    //      6   R    DIE   Double Interlace Enable (read-only view of FBCR.DIE)
    //      5   R    DIL   Double Interlace Draw Line (read-only view of FBCR.DIL)
    //      4   R    FCM   Frame Buffer Change Mode (read-only view of FBCR.FCM)
    //      3   R    VBE   V-Blank Erase/Write Enable (read-only view of TVMR.VBE)
    //    2-0   R    TVM   TV Mode Selection (read-only view of TVMR.TVM)

    FORCE_INLINE uint16 ReadMODR() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, pixel8Bits);
        bit::deposit_into<1>(value, fbRotEnable);
        bit::deposit_into<2>(value, hdtvEnable);
        bit::deposit_into<3>(value, vblankErase);
        bit::deposit_into<4>(value, fbSwapMode);
        bit::deposit_into<5>(value, dblInterlaceDrawLine);
        bit::deposit_into<6>(value, dblInterlaceEnable);
        bit::deposit_into<7>(value, evenOddCoordSelect);
        bit::deposit_into<12, 15>(value, 0b0001);
        return value;
    }
};

// -----------------------------------------------------------------------------
// VDP2 registers

// Character color formats
enum class ColorFormat : uint8 {
    Palette16,
    Palette256,
    Palette2048,
    RGB555,
    RGB888,
};

// Rotation BG screen-over process
enum class ScreenOverProcess : uint8 {
    Repeat,
    RepeatChar,
    Transparent,
    Fixed512,
};

// Special priority modes
enum class PriorityMode : uint8 {
    PerScreen,
    PerCharacter,
    PerDot,
};

// Map index mask lookup table
// [Character Size][Pattern Name Data Size ^ 1][Plane Size]
static constexpr uint32 kMapIndexMasks[2][2][4] = {
    {{0x7F, 0x7E, 0x7E, 0x7C}, {0x3F, 0x3E, 0x3E, 0x3C}},
    {{0x1FF, 0x1FE, 0x1FE, 0x1FC}, {0xFF, 0xFE, 0xFE, 0xFC}},
};

// Page sizes lookup table
// [Character Size][Pattern Name Data Size ^ 1]
static constexpr uint32 kPageSizes[2][2] = {
    {13, 14},
    {11, 12},
};

// NBG (rot=false) and RBG (rot=true) parameters.
template <bool rot>
struct BGParams {
    BGParams() {
        Reset();
    }

    void Reset() {
        enabled = false;
        transparent = false;
        bitmap = false;

        lineColorScreenEnable = false;

        priorityNumber = 0;
        priorityMode = PriorityMode::PerScreen;
        specialFunctionSelect = 0;

        cellSizeShift = 0;

        pageShiftH = 0;
        pageShiftV = 0;
        bitmapSizeH = 512;
        bitmapSizeV = 256;

        scrollAmountH = 0;
        scrollAmountV = 0;

        scrollIncH = 1u << 8u;
        scrollIncV = 1u << 8u;

        mapIndices.fill(0);
        pageBaseAddresses.fill(0);
        bitmapBaseAddress = 0;

        colorFormat = ColorFormat::Palette16;
        screenOverProcess = ScreenOverProcess::Repeat;

        supplScrollCharNum = 0;
        supplScrollPalNum = 0;
        supplScrollSpecialColorCalc = false;
        supplScrollSpecialPriority = false;

        supplBitmapPalNum = 0;
        supplBitmapSpecialColorCalc = false;
        supplBitmapSpecialPriority = false;

        wideChar = false;
        twoWordChar = false;

        plsz = 0;
        bmsz = 0;
        caos = 0;
    }

    // Whether to display this background.
    // Derived from BGON.xxON
    bool enabled;

    // If true, honor transparency bit in color data.
    // Derived from BGON.xxTPON
    bool transparent;

    // Whether the background uses cells (false) or a bitmap (true).
    // Derived CHCTLA/CHCTLB.xxBMEN
    bool bitmap;

    // Enables LNCL screen insertion if this BG is the topmost layer.
    // Derived from LNCLEN.xxLCEN
    bool lineColorScreenEnable;

    // Priority number from 0 (transparent) to 7 (highest).
    // Derived from PRINA/PRINB/PRIR.xxPRINn
    uint8 priorityNumber;

    // Special priority mode.
    // Derived from SFPRMD.xxSPRMn
    PriorityMode priorityMode;

    // Special function select (0=A, 1=B).
    // Derived from SFSEL.xxSFCS
    uint8 specialFunctionSelect;

    // Cell size shift corresponding to the dimensions of a character pattern (0=1x1, 1=2x2).
    // Derived from CHCTLA/CHCTLB.xxCHSZ
    uint32 cellSizeShift;

    // Page shifts are either 0 or 1, used when determining which plane a particular (x,y) coordinate belongs to.
    // A shift of 0 corresponds to 1 page per plane dimension.
    // A shift of 1 corresponds to 2 pages per plane dimension.
    uint32 pageShiftH; // Horizontal page shift, derived from PLSZ.xxPLSZn
    uint32 pageShiftV; // Vertical page shift, derived from PLSZ.xxPLSZn

    // Bitmap dimensions, when the screen is in bitmap mode.
    uint32 bitmapSizeH; // Horizontal bitmap dots, derived from CHCTLA/CHCTLB.xxBMSZ
    uint32 bitmapSizeV; // Vertical bitmap dots, derived from CHCTLA/CHCTLB.xxBMSZ

    // Screen scroll amount, in 11.8 fixed-point format.
    // Used in scroll NBGs.
    // Scroll amounts for NBGs 2 and 3 do not have a fractional part, but the values are still stored with 8 fractional
    // bits here for consistency and ease of implementation.
    uint32 scrollAmountH; // Horizontal scroll amount with 8 fractional bits, derived from SCXINn and SCXDNn
    uint32 scrollAmountV; // Vertical scroll amount with 8 fractional bits, derived from SCYINn and SCYDNn

    // Screen scroll increment per pixel, in 11.8 fixed-point format.
    // NBGs 2 and 3 do not have increment registers; they always increment each coordinate by 1.0, which is stored here
    // for consistency and ease of implementation.
    uint32 scrollIncH; // Horizontal scroll increment with 8 fractional bits, derived from ZMXINn and ZMXDNn
    uint32 scrollIncV; // Vertical scroll increment with 8 fractional bits, derived from ZMYINn and ZMYDNn

    // Indices for NBG planes A-D, derived from MPOFN and MPABN0-MPCDN3.
    // Indices for RBG planes A-P, derived from MPOFR and MPABRA-MPOPRB.
    std::array<uint16, rot ? 16 : 4> mapIndices;

    // Page base addresses for NBG planes A-D or RBG planes A-P.
    // Derived from mapIndices, CHCTLA/CHCTLB.xxCHSZ, PNCNn/PNCR.xxPNB and PLSZ.xxPLSZn
    std::array<uint32, rot ? 16 : 4> pageBaseAddresses;

    // Base address of bitmap data.
    // Derived from MPOFN/MPOFR
    uint32 bitmapBaseAddress;

    // Character color format.
    // Derived from CHCTLA/CHCTLB.xxCHCNn
    ColorFormat colorFormat;

    // Rotation BG screen-over process.
    // Derived from PLSZ.RxOVRn
    ScreenOverProcess screenOverProcess;

    // Supplementary bits 4-0 for scroll screen character number, when using 1-word characters.
    // Derived from PNCN0/PNCR.xxSCNn
    uint32 supplScrollCharNum;

    // Supplementary bits 6-4 for scroll screen palette number, when using 1-word characters.
    // The value is already shifted in place to optimize rendering calculations.
    // Derived from PNCN0/PNCR.xxSPLTn
    uint32 supplScrollPalNum;

    // Bits 6-4 for bitmap palette number.
    // The value is already shifted in place to optimize rendering calculations.
    // Derived from BMPNA/BMPNB.xxBMPn
    uint32 supplBitmapPalNum;

    // Supplementary Special Color Calculation bit for scroll BGs.
    // Derived from PNCN0/PNCR.xxSCC
    bool supplScrollSpecialColorCalc;

    // Supplementary Special Priority bit for scroll BGs.
    // Derived from PNCN0/PNCR.xxSPR
    bool supplScrollSpecialPriority;

    // Supplementary Special Color Calculation bit for bitmap BGs.
    // Derived from BMPNA/BMPNB.xxBMCC
    bool supplBitmapSpecialColorCalc;

    // Supplementary Special Priority bit for bitmap BGs.
    // Derived from BMPNA/BMPNB.xxBMPR
    bool supplBitmapSpecialPriority;

    // Character number width: 10 bits (false) or 12 bits (true).
    // When true, disables the horizontal and vertical flip bits in the character.
    // Derived from PNCN0/PNCR.xxCNSM
    bool wideChar;

    // Whether characters use one (false) or two (true) words.
    // Derived from PNCNn/PNCR.xxPNB
    bool twoWordChar;

    // Raw register values, to facilitate reads.
    uint16 plsz; // Raw value of PLSZ.xxPLSZn
    uint16 bmsz; // Raw value of CHCTLA/CHCTLB.xxBMSZ
    uint16 caos; // Raw value of CRAOFA/CRAOFB.xxCAOSn

    void UpdatePLSZ() {
        pageShiftH = plsz & 1;
        pageShiftV = plsz >> 1;

        UpdatePageBaseAddresses();
    }

    void UpdateCHCTL() {
        static constexpr uint32 kBitmapSizesH[] = {512, 512, 1024, 1024};
        static constexpr uint32 kBitmapSizesV[] = {256, 512, 256, 512};
        bitmapSizeH = kBitmapSizesH[bmsz];
        bitmapSizeV = kBitmapSizesV[bmsz];

        UpdatePageBaseAddresses();
    }

    void UpdatePageBaseAddresses() {
        const uint32 chsz = cellSizeShift;
        const uint32 pnds = twoWordChar;
        const uint32 mapIndexMask = kMapIndexMasks[chsz][pnds][plsz];
        const uint32 pageSizeShift = kPageSizes[chsz][pnds];
        for (int i = 0; i < pageBaseAddresses.size(); i++) {
            pageBaseAddresses[i] = (mapIndices[i] & mapIndexMask) << pageSizeShift;
        }
    }
};

using NormBGParams = BGParams<false>;
using RotBGParams = BGParams<true>;

enum class SpriteColorCalculationCondition : uint8 {
    PriorityLessThanOrEqual,
    PriorityEqual,
    PriorityGreaterThanOrEqual,
    MsbEqualsOne,
};

struct SpriteParams {
    SpriteParams() {
        Reset();
    }

    void Reset() {
        type = 0;
        windowEnable = false;
        mixedFormat = false;
        colorCalcValue = 0;
        colorCalcCond = SpriteColorCalculationCondition::PriorityLessThanOrEqual;
        priorities.fill(0);
        colorDataOffset = 0;
    }

    // The sprite type (0..F).
    // Derived from SPCTL.SPTYPE3-0
    uint8 type;

    // Whether sprite window is in use.
    // Derived from SPCTL.SPWINEN
    bool windowEnable;

    // Whether sprite data uses palette only (false) or mixed palette/RGB (true) data.
    // Derived from SPCTL.SPCLMD
    bool mixedFormat;

    // The color calculation value to compare against the priority number of sprites.
    // Derived from SPCTL.SPCCN2-0
    uint8 colorCalcValue;

    // The color calculation condition.
    // Derived from SPCTL.SPCCCS1-0
    SpriteColorCalculationCondition colorCalcCond;

    // Sprite priority numbers for registers 0-7.
    // Derived from PRISA, PRISB, PRISC and PRISD.
    std::array<uint8, 8> priorities;

    // Sprite color calculation ratios for registers 0-7, ranging from 31:1 to 0:32.
    // Derived from CCRSA, CCRSB, CCRSC and CCRSD.
    std::array<uint8, 8> colorCalcRatios;

    // Sprite color data offset.
    // Derived from CRAOFB.SPCAOSn
    uint32 colorDataOffset;
};

struct SpriteData {
    uint16 colorData = 0;        // DC10-0
    uint8 colorCalcRatio = 0;    // CC2-0
    uint8 priority = 0;          // PR2-0
    bool shadowOrWindow = false; // SD
};

// Special Function Codes, derived from SFCODE.
struct SpecialFunctionCodes {
    SpecialFunctionCodes() {
        Reset();
    }

    void Reset() {
        colorMatches.fill(0);
    }

    // If the entry indexed by bits 3-1 of the color code is true, the special function is applied to the pixel
    std::array<bool, 8> colorMatches;
};

// TODO: consider splitting unions into individual fields for performance

// 180000   TVMD    TV Screen Mode
//
//   bits   r/w  code          description
//     15   R/W  DISP          TV Screen Display (0=no display, 1=display)
//   14-9        -             Reserved, must be zero
//      8   R/W  BDCLMD        Border Color Mode (0=black, 1=back screen)
//    7-6   R/W  LSMD1-0       Interlace Mode
//                               00 (0) = Non-Interlace
//                               01 (1) = (Forbidden)
//                               10 (2) = Single-density interlace
//                               11 (3) = Double-density interlace
//    5-4   R/W  VRESO1-0      Vertical Resolution
//                               00 (0) = 224 lines (NTSC or PAL)
//                               01 (1) = 240 lines (NTSC or PAL)
//                               10 (2) = 256 lines (PAL only)
//                               11 (3) = (Forbidden)
//      3        -             Reserved, must be zero
//    2-0   R/W  HRESO2-0      Horizontal Resolution
//                               000 (0) = 320 pixels - Normal Graphic A (NTSC or PAL)
//                               001 (1) = 352 pixels - Normal Graphic B (NTSC or PAL)
//                               010 (2) = 640 pixels - Hi-Res Graphic A (NTSC or PAL)
//                               011 (3) = 704 pixels - Hi-Res Graphic B (NTSC or PAL)
//                               100 (4) = 320 pixels - Exclusive Normal Graphic A (31 KHz monitor)
//                               101 (5) = 352 pixels - Exclusive Normal Graphic B (Hi-Vision monitor)
//                               110 (4) = 640 pixels - Exclusive Hi-Res Graphic A (31 KHz monitor)
//                               111 (5) = 704 pixels - Exclusive Hi-Res Graphic B (Hi-Vision monitor)
union TVMD_t {
    uint16 u16;
    struct {
        uint16 HRESOn : 3;
        uint16 _rsvd3 : 1;
        uint16 VRESOn : 2;
        uint16 LSMDn : 2;
        uint16 BDCLMD : 1;
        uint16 _rsvd9_14 : 6;
        uint16 DISP : 1;
    };
};

// 180002   EXTEN   External Signal Enable
//
//   bits   r/w  code          description
//  15-10        -             Reserved, must be zero
//      9   R/W  EXLTEN        External Latch Enable (0=on read, 1=on external signal)
//      8   R/W  EXSYEN        External Sync Enable (0=disable, 1=enable)
//    7-2        -             Reserved, must be zero
//      1   R/W  DASEL         Display Area Select (0=selected area, 1=full screen)
//      0   R/W  EXBGEN        External BG Enable (0=disable, 1=enable)
union EXTEN_t {
    uint16 u16;
    struct {
        uint16 EXBGEN : 1;
        uint16 DASEL : 1;
        uint16 _rsvd2_7 : 6;
        uint16 EXSYEN : 1;
        uint16 EXLTEN : 1;
        uint16 _rsvd10_15 : 6;
    };
};

// 180004   TVSTAT  Screen Status
//
//   bits   r/w  code          description
//  15-10        -             Reserved, must be zero
//      9   R    EXLTFG        External Latch Flag (0=not latched, 1=latched)
//      8   R    EXSYFG        External Sync Flag (0=not synced, 1=synced)
//    7-4        -             Reserved, must be zero
//      3   R    VBLANK        Vertical Blank Flag (0=vertical scan, 1=vertical retrace)
//      2   R    HBLANK        Horizontal Blank Flag (0=horizontal scan, 1=horizontal retrace)
//      1   R    ODD           Scan Field Flag (0=even, 1=odd)
//      0   R    PAL           TV Standard Flag (0=NTSC, 1=PAL)
union TVSTAT_t {
    uint16 u16;
    struct {
        uint16 PAL : 1;
        uint16 ODD : 1;
        uint16 HBLANK : 1;
        uint16 VBLANK : 1;
        uint16 _rsvd4_7 : 4;
        uint16 EXSYFG : 1;
        uint16 EXLTFG : 1;
        uint16 _rsvd10_15 : 6;
    };
};

// 180006   VRSIZE  VRAM Size
//
//   bits   r/w  code          description
//     15   R/W  VRAMSZ        VRAM Size (0=512 KiB, 1=1 MiB)
//   14-4        -             Reserved, must be zero
//    3-0   R    VER3-0        VDP2 Version Number
union VRSIZE_t {
    uint16 u16;
    struct {
        uint16 VERn : 4;
        uint16 _rsvd4_14 : 11;
        uint16 VRAMSZ : 1;
    };
};

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

// 18000E   RAMCTL  RAM Control
//
//   bits   r/w  code          description
//     15   R/W  CRKTE         Color RAM Coefficient Table Enable
//     14        -             Reserved, must be zero
//  13-12   R/W  CRMD1-0       Color RAM Mode
//                               00 (0) = RGB 5:5:5, 1024 words
//                               01 (1) = RGB 5:5:5, 2048 words
//                               10 (2) = RGB 8:8:8, 1024 words
//                               11 (3) = RGB 8:8:8, 1024 words  (same as mode 2, undocumented)
//  11-10        -             Reserved, must be zero
//      9   R/W  VRBMD         VRAM-B Mode (0=single partition, 1=two partitions)
//      8   R/W  VRAMD         VRAM-A Mode (0=single partition, 1=two partitions)
//    7-6   R/W  RDBSB1(1-0)   Rotation Data Bank Select for VRAM-B1
//    5-4   R/W  RDBSB0(1-0)   Rotation Data Bank Select for VRAM-B0 (or VRAM-B)
//    3-2   R/W  RDBSA1(1-0)   Rotation Data Bank Select for VRAM-A1
//    1-0   R/W  RDBSA0(1-0)   Rotation Data Bank Select for VRAM-A0 (or VRAM-B)
union RAMCTL_t {
    uint16 u16;
    struct {
        uint16 RDBSA0n : 2;
        uint16 RDBSA1n : 2;
        uint16 RDBSB0n : 2;
        uint16 RDBSB1n : 2;
        uint16 VRAMD : 1;
        uint16 VRBMD : 1;
        uint16 _rsvd10_11 : 2;
        uint16 CRMDn : 2;
        uint16 _rsvd14 : 1;
        uint16 CRKTE : 1;
    };
};

// 180010   CYCA0L  VRAM Cycle Pattern A0 Lower
//
//   bits   r/w  code          description
//  15-12     W  VCP0A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T0
//   11-8     W  VCP1A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T1
//    7-4     W  VCP2A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T2
//    3-0     W  VCP3A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T3
//
// 180012   CYCA0U  VRAM Cycle Pattern A0 Upper
//
//   bits   r/w  code          description
//  15-12     W  VCP4A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T4
//   11-8     W  VCP5A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T5
//    7-4     W  VCP6A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T6
//    3-0     W  VCP7A0(3-0)   VRAM-A0 (or VRAM-A) Timing for T7
//
// 180014   CYCA1L  VRAM Cycle Pattern A1 Lower
//
//   bits   r/w  code          description
//  15-12     W  VCP0A1(3-0)   VRAM-A1 Timing for T0
//   11-8     W  VCP1A1(3-0)   VRAM-A1 Timing for T1
//    7-4     W  VCP2A1(3-0)   VRAM-A1 Timing for T2
//    3-0     W  VCP3A1(3-0)   VRAM-A1 Timing for T3
//
// 180016   CYCA1U  VRAM Cycle Pattern A1 Upper
//
//   bits   r/w  code          description
//  15-12     W  VCP4A1(3-0)   VRAM-A1 Timing for T4
//   11-8     W  VCP5A1(3-0)   VRAM-A1 Timing for T5
//    7-4     W  VCP6A1(3-0)   VRAM-A1 Timing for T6
//    3-0     W  VCP7A1(3-0)   VRAM-A1 Timing for T7
//
// 180018   CYCB0L  VRAM Cycle Pattern B0 Lower
//
//   bits   r/w  code          description
//  15-12     W  VCP0B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T0
//   11-8     W  VCP1B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T1
//    7-4     W  VCP2B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T2
//    3-0     W  VCP3B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T3
//
// 18001A   CYCB0U  VRAM Cycle Pattern B0 Upper
//
//   bits   r/w  code          description
//  15-12     W  VCP4B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T4
//   11-8     W  VCP5B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T5
//    7-4     W  VCP6B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T6
//    3-0     W  VCP7B0(3-0)   VRAM-B0 (or VRAM-B) Timing for T7
//
// 18001C   CYCB1L  VRAM Cycle Pattern B1 Lower
//
//   bits   r/w  code          description
//  15-12     W  VCP0B1(3-0)   VRAM-B1 Timing for T0
//   11-8     W  VCP1B1(3-0)   VRAM-B1 Timing for T1
//    7-4     W  VCP2B1(3-0)   VRAM-B1 Timing for T2
//    3-0     W  VCP3B1(3-0)   VRAM-B1 Timing for T3
//
// 18001E   CYCB1U  VRAM Cycle Pattern B1 Upper
//
//   bits   r/w  code          description
//  15-12     W  VCP4B1(3-0)   VRAM-B1 Timing for T4
//   11-8     W  VCP5B1(3-0)   VRAM-B1 Timing for T5
//    7-4     W  VCP6B1(3-0)   VRAM-B1 Timing for T6
//    3-0     W  VCP7B1(3-0)   VRAM-B1 Timing for T7
union CYC_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 VCP3n : 4;
                uint16 VCP2n : 4;
                uint16 VCP1n : 4;
                uint16 VCP0n : 4;
            };
        } L;
        union {
            uint16 u16;
            struct {
                uint16 VCP7n : 4;
                uint16 VCP6n : 4;
                uint16 VCP5n : 4;
                uint16 VCP4n : 4;
            };
        } U;
    };
};

// 180022   MZCTL   Mosaic Control
//
//   bits   r/w  code          description
//  15-12     W  MZSZV3-0      Vertical Mosaic Size
//   11-8     W  MZSZH3-0      Horizontal Mosaic Size
//    7-5        -             Reserved, must be zero
//      4     W  R0MZE         RBG0 Mosaic Enable
//      3     W  N3MZE         NBG3 Mosaic Enable
//      2     W  N2MZE         NBG2 Mosaic Enable
//      1     W  N1MZE         NBG1 Mosaic Enable
//      0     W  N0MZE         NBG0/RBG1 Mosaic Enable
union MZCTL_t {
    uint16 u16;
    struct {
        uint16 N0MZE : 1;
        uint16 N1MZE : 1;
        uint16 N2MZE : 1;
        uint16 N3MZE : 1;
        uint16 R0MZE : 1;
        uint16 _rsvd5_7 : 3;
        uint16 MZSZHn : 4;
        uint16 MZSZVn : 4;
    };
};

// 180098   ZMCTL   Reduction Enable
//
//   bits   r/w  code          description
//  15-10        -             Reserved, must be zero
//      9     W  N1ZMQT        NBG1 Zoom Quarter
//      8     W  N1ZMHF        NBG1 Zoom Half
//    7-2        -             Reserved, must be zero
//      1     W  N0ZMQT        NBG0 Zoom Quarter
//      0     W  N0ZMHF        NBG0 Zoom Half
//
//  NxZMQT,NxZMHF:
//       0,0   no horizontal reduction, no restrictions
//       0,1   up to 1/2 horizontal reduction, max 256 character colors
//       1,0   up to 1/4 horizontal reduction, max 16 character colors
//       1,1   up to 1/4 horizontal reduction, max 16 character colors
union ZMCTL_t {
    uint16 u16;
    struct {
        uint16 N0ZMHF : 1;
        uint16 N0ZMQT : 1;
        uint16 _rsvd2_7 : 6;
        uint16 N1ZMHF : 1;
        uint16 N1ZMQT : 1;
        uint16 _rsvd10_15 : 6;
    };
};

// 18009A   SCRCTL  Line and Vertical Cell Scroll Control
//
//   bits   r/w  code          description
//  15-14        -             Reserved, must be zero
//  13-12     W  N1LSS1-0      NBG1 Line Scroll Interval
//                               00 (0) = Each line
//                               01 (1) = Every 2 lines
//                               10 (2) = Every 4 lines
//                               11 (3) = Every 8 lines
//                               NOTE: Values are doubled for single-density interlaced mode
//     11     W  N1LZMX        NBG1 Line Zoom X Enable (0=disable, 1=enable)
//     10     W  N1LSCY        NBG1 Line Scroll Y Enable (0=disable, 1=enable)
//      9     W  N1LSCX        NBG1 Line Scroll X Enable (0=disable, 1=enable)
//      8     W  N1VCSC        NBG1 Vertical Cell Scroll Enable (0=disable, 1=enable)
//    7-6        -             Reserved, must be zero
//    5-4     W  N0LSS1-0      NBG0 Line Scroll Interval
//                               00 (0) = Each line
//                               01 (1) = Every 2 lines
//                               10 (2) = Every 4 lines
//                               11 (3) = Every 8 lines
//                               NOTE: Values are doubled for single-density interlaced mode
//      3     W  N0LZMX        NBG0 Line Zoom X Enable (0=disable, 1=enable)
//      2     W  N0LSCY        NBG0 Line Scroll Y Enable (0=disable, 1=enable)
//      1     W  N0LSCX        NBG0 Line Scroll X Enable (0=disable, 1=enable)
//      0     W  N0VCSC        NBG0 Vertical Cell Scroll Enable (0=disable, 1=enable)
union SCRCTL_t {
    uint16 u16;
    struct {
        uint16 N0VCSC : 1;
        uint16 N0LSCX : 1;
        uint16 N0LSCY : 1;
        uint16 N0LZMX : 1;
        uint16 _rsvd6_7 : 2;
        uint16 N1VCSC : 1;
        uint16 N1LSCX : 1;
        uint16 N1LSCY : 1;
        uint16 N1LZMX : 1;
        uint16 _rsvd14_15 : 2;
    };
};

// 18009C   VCSTAU  Vertical Cell Scroll Table Address (upper)
//
//   bits   r/w  code          description
//   15-3        -             Reserved, must be zero
//    2-0     W  VCSTA18-16    Vertical Cell Scroll Table Base Address (bits 18-16)
//
// 18009E   VCSTAL  Vertical Cell Scroll Table Address (lower)
//
//   bits   r/w  code          description
//   15-1     W  VCSTA15-1     Vertical Cell Scroll Table Base Address (bits 15-1)
//      0        -             Reserved, must be zero
union VCSTA_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 _rsvd0 : 1;
                uint16 VCSTAn : 15;
            };
        } L;
        union {
            uint16 u16;
            struct {
                uint16 VCSTAn : 3;
                uint16 _rsvd3_15 : 13;
            };
        } U;
    };
    struct {
        uint32 : 1;
        uint32 VCSTAn : 18;
    };
};

// 1800A0   LSTA0U  NBG0 Line Cell Scroll Table Address (upper)
// 1800A4   LSTA1U  NBG1 Line Cell Scroll Table Address (upper)
//
//   bits   r/w  code          description
//   15-3        -             Reserved, must be zero
//    2-0     W  NxLSTA18-16   NBGx Line Cell Scroll Table Base Address (bits 18-16)
//
// 1800A2   LSTA0L  NBG0 Line Cell Scroll Table Address (lower)
// 1800A6   LSTA1L  NBG1 Line Cell Scroll Table Address (lower)
//
//   bits   r/w  code          description
//   15-1     W  NxLSTA15-1    NBGx Line Cell Scroll Table Base Address (bits 15-1)
//      0        -             Reserved, must be zero
union LSTA_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 _rsvd0 : 1;
                uint16 LSTAn : 15;
            };
        } L;
        union {
            uint16 u16;
            struct {
                uint16 LSTAn : 3;
                uint16 _rsvd3_15 : 13;
            };
        } U;
    };
    struct {
        uint32 : 1;
        uint32 LSTAn : 18;
    };
};

// 1800A8   LCTAU   Line Color Screen Table Address (upper)
//
//   bits   r/w  code          description
//     15     W  LCCLMD        Line Color Screen Mode (0=single color, 1=per line)
//   14-3        -             Reserved, must be zero
//    2-0     W  LCTA18-16     Line Color Screen Table Base Address (bits 18-16)
//
// 1800AA   LCTAL   Line Color Screen Table Address (lower)
//
//   bits   r/w  code          description
//   15-0     W  LCTA15-0      Line Color Screen Table Base Address (bits 15-0)
union LCTA_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            uint16 LCTAn;
        } L;
        union {
            uint16 u16;
            struct {
                uint16 LCTAn : 3;
                uint16 _rsvd3_15 : 13;
            };
        } U;
    };
    struct {
        uint32 LCTAn : 19;
    };
};

// 1800AC   BKTAU   Back Screen Table Address (upper)
//
//   bits   r/w  code          description
//     15     W  BKCLMD        Back Screen Color Mode (0=single color, 1=per line)
//   14-3        -             Reserved, must be zero
//    2-0     W  BKTA18-16     Back Screen Table Base Address (bits 18-16)
//
// 1800AE   BKTAL   Back Screen Table Address (lower)
//
//   bits   r/w  code          description
//   15-0     W  BKTA15-0      Back Screen Table Base Address (bits 15-0)
union BKTA_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            uint16 BKTAn;
        } L;
        union {
            uint16 u16;
            struct {
                uint16 BKTAn : 3;
                uint16 _rsvd3_15 : 13;
            };
        } U;
    };
    struct {
        uint32 BKTAn : 19;
    };
};

// 1800B0   RPMD    Rotation Parameter Mode
//
//   bits   r/w  code          description
//   15-2        -             Reserved, must be zero
//    1-0     W  RPMD1-0       Rotation Parameter Mode
//                               00 (0) = Rotation Parameter A
//                               01 (1) = Rotation Parameter B
//                               10 (2) = Screens switched via coeff. data from RPA table
//                               11 (3) = Screens switched via rotation parameter window
union RPMD_t {
    uint16 u16;
    struct {
        uint16 RPMDn : 2;
        uint16 _rsvd2_15 : 14;
    };
};

// 1800B2   RPRCTL  Rotation Parameter Read Control
//
//   bits   r/w  code          description
//  15-11        -             Reserved, must be zero
//     10     W  RBKASTRE      Enable for KAst of Rotation Parameter B
//      9     W  RBYSTRE       Enable for Yst of Rotation Parameter B
//      8     W  RBXSTRE       Enable for Xst of Rotation Parameter B
//    7-3        -             Reserved, must be zero
//      2     W  RAKASTRE      Enable for KAst of Rotation Parameter A
//      1     W  RAYSTRE       Enable for Yst of Rotation Parameter A
//      0     W  RAXSTRE       Enable for Xst of Rotation Parameter A
union RPRCTL_t {
    uint16 u16;
    struct {
        uint16 RAXSTRE : 1;
        uint16 RAYSTRE : 1;
        uint16 RABKSTRE : 1;
        uint16 _rsvd3_7 : 5;
        uint16 RBXSTRE : 1;
        uint16 RBYSTRE : 1;
        uint16 RBBKSTRE : 1;
        uint16 _rsvd11_15 : 5;
    };
};

// 1800B4   KTCTL   Coefficient Table Control
//
//   bits   r/w  code          description
//  15-13        -             Reserved, must be zero
//     12     W  RBKLCE        Use line color screen data from Rotation Parameter B coeff. data
//  11-10     W  RBKMD1-0      Coefficient Mode for Rotation Parameter B
//                               00 (0) = Use as scale coeff. kx, ky
//                               01 (1) = Use as scale coeff. kx
//                               10 (2) = Use as scale coeff. ky
//                               11 (3) = Use as viewpoint Xp after rotation conversion
//      9     W  RBKDBS        Coefficient Data Size for Rotation Parameter B
//                               0 = 2 words
//                               1 = 1 word
//      8     W  RBKTE         Coefficient Table Enable for Rotation Parameter B
//    7-5        -             Reserved, must be zero
//      4     W  RAKLCE        Use line color screen data from Rotation Parameter A coeff. data
//    3-2     W  RAKMD1-0      Coefficient Mode for Rotation Parameter A
//                               00 (0) = Use as scale coeff. kx, ky
//                               01 (1) = Use as scale coeff. kx
//                               10 (2) = Use as scale coeff. ky
//                               11 (3) = Use as viewpoint Xp after rotation conversion
//      1     W  RAKDBS        Coefficient Data Size for Rotation Parameter A
//                               0 = 2 words
//                               1 = 1 word
//      0     W  RAKTE         Coefficient Table Enable for Rotation Parameter A
union KTCTL_t {
    uint16 u16;
    struct {
        uint16 RAKTE : 1;
        uint16 RAKDBS : 1;
        uint16 RAKMDn : 2;
        uint16 RAKLCE : 1;
        uint16 _rsvd5_7 : 3;
        uint16 RBKTE : 1;
        uint16 RBKDBS : 1;
        uint16 RBKMDn : 2;
        uint16 RBKLCE : 1;
        uint16 _rsvd13_15 : 3;
    };
};

// 1800B6   KTAOF   Coefficient Table Address Offset
//
//   bits   r/w  code          description
//  15-11        -             Reserved, must be zero
//   10-8     W  RBKTAOS2-0    Coefficient Table Address Offset for Rotation Parameter B
//    7-3        -             Reserved, must be zero
//    2-0     W  RAKTAOS2-0    Coefficient Table Address Offset for Rotation Parameter A
union KTAOF_t {
    uint16 u16;
    struct {
        uint16 RAKTAOSn : 3;
        uint16 _rsvd3_7 : 5;
        uint16 RBKTAOSn : 3;
        uint16 _rsvd11_15 : 5;
    };
};

// 1800B8   OVPNRA  Rotation Parameter A Screen-Over Pattern Name
// 1800BA   OVPNRB  Rotation Parameter B Screen-Over Pattern Name
//
//   bits   r/w  code          description
//   15-0     W  RxOPN15-0     Over Pattern Name
//
// x:
//   A = Rotation Parameter A (OVPNRA)
//   B = Rotation Parameter B (OVPNRB)
using OVPNR_t = uint16;

// 1800BC   RPTAU   Rotation Parameters Table Address (upper)
//
//   bits   r/w  code          description
//   15-3        -             Reserved, must be zero
//    2-0     W  RPTA18-16     Rotation Parameters Table Base Address (bits 18-16)
//
// 1800BE   RPTAL   Rotation Parameters Table Address (lower)
//
//   bits   r/w  code          description
//   15-1     W  RPTA15-1      Rotation Parameters Table Base Address (bits 15-0)
//      0        -             Reserved, must be zero
union RPTA_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 _rsvd0 : 1;
                uint16 RPTAn : 15;
            };
        } L;
        union {
            uint16 u16;
            struct {
                uint16 RPTAn : 3;
                uint16 _rsvd3_15 : 13;
            };
        } U;
    };
    struct {
        uint32 : 1;
        uint32 RPTAn : 18;
    };
};

// 1800C0   WPSX0   Window 0 Horizontal Start Point
// 1800C4   WPEX0   Window 0 Horizontal End Point
// 1800C8   WPSX1   Window 1 Horizontal Start Point
// 1800CC   WPEX1   Window 1 Horizontal End Point
//
//   bits   r/w  code          description
//  15-10        -             Reserved, must be zero
//    9-0     W  WxSX9-0       Window x Start/End Horizontal Coordinate
//
// Valid coordinate bits vary depending on the screen mode:
//   Normal: bits 8-0 shifted left by 1; bit 0 is invalid
//   Hi-Res: bits 9-0
//   Excl. Normal: bits 8-0; bit 9 is invalid
//   Excl. Hi-Res: bits 9-1 shifted right by 1; bit 9 is invalid
//
// 1800C2   WPSY0   Window 0 Vertical Start Point
// 1800C6   WPEY0   Window 0 Vertical End Point
// 1800CA   WPSY1   Window 1 Vertical Start Point
// 1800CE   WPEY1   Window 1 Vertical End Point
//
//   bits   r/w  code          description
//   15-9        -             Reserved, must be zero
//    8-0     W  WxSY8-0       Window x Start/End Vertical Coordinate
//
// Double-density interlace mode uses bits 7-0 shifted left by 1; bit 0 is invalid.
// All other modes use bits 8-0 unmodified.
union WPXY_t {
    uint64 u64;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 WxSXn : 10;
                uint16 _rsvd10_15 : 6;
            };
        } S;
        union {
            uint16 u16;
            struct {
                uint16 WxEXn : 10;
                uint16 _rsvd10_15 : 6;
            };
        } E;
    } X;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 WxSYn : 10;
                uint16 _rsvd10_15 : 6;
            };
        } S;
        union {
            uint16 u16;
            struct {
                uint16 WxEYn : 10;
                uint16 _rsvd10_15 : 6;
            };
        } E;
    } Y;
};

// 1800D0   WCTLA   NBG0 and NBG1 Window Control
//
//   bits   r/w  code          description
//     15     W  N1LOG         NBG1 Window Logic (0=OR, 1=AND)
//     14        -             Reserved, must be zero
//     13     W  N1SWE         NBG1 Sprite Window Enable (0=disable, 1=enable)
//     12     W  N1SWA         NBG1 Sprite Window Area (0=inside, 1=outside)
//     11     W  N1W1E         NBG1 Window 1 Enable (0=disable, 1=enable)
//     10     W  N1W1A         NBG1 Window 1 Area (0=inside, 1=outside)
//      9     W  N1W0E         NBG1 Window 0 Enable (0=disable, 1=enable)
//      8     W  N1W0A         NBG1 Window 0 Area (0=inside, 1=outside)
//      7     W  N0LOG         NBG0 Window Logic (0=OR, 1=AND)
//      6        -             Reserved, must be zero
//      5     W  N0SWE         NBG0 Sprite Window Enable (0=disable, 1=enable)
//      4     W  N0SWA         NBG0 Sprite Window Area (0=inside, 1=outside)
//      3     W  N0W1E         NBG0 Window 1 Enable (0=disable, 1=enable)
//      2     W  N0W1A         NBG0 Window 1 Area (0=inside, 1=outside)
//      1     W  N0W0E         NBG0 Window 0 Enable (0=disable, 1=enable)
//      0     W  N0W0A         NBG0 Window 0 Area (0=inside, 1=outside)
//
// 1800D2   WCTLB   NBG2 and NBG3 Window Control
//
//   bits   r/w  code          description
//     15     W  N3LOG         NBG3 Window Logic (0=OR, 1=AND)
//     14        -             Reserved, must be zero
//     13     W  N3SWE         NBG3 Sprite Window Enable (0=disable, 1=enable)
//     12     W  N3SWA         NBG3 Sprite Window Area (0=inside, 1=outside)
//     11     W  N3W1E         NBG3 Window 1 Enable (0=disable, 1=enable)
//     10     W  N3W1A         NBG3 Window 1 Area (0=inside, 1=outside)
//      9     W  N3W0E         NBG3 Window 0 Enable (0=disable, 1=enable)
//      8     W  N3W0A         NBG3 Window 0 Area (0=inside, 1=outside)
//      7     W  N2LOG         NBG2 Window Logic (0=OR, 1=AND)
//      6        -             Reserved, must be zero
//      5     W  N2SWE         NBG2 Sprite Window Enable (0=disable, 1=enable)
//      4     W  N2SWA         NBG2 Sprite Window Area (0=inside, 1=outside)
//      3     W  N2W1E         NBG2 Window 1 Enable (0=disable, 1=enable)
//      2     W  N2W1A         NBG2 Window 1 Area (0=inside, 1=outside)
//      1     W  N2W0E         NBG2 Window 0 Enable (0=disable, 1=enable)
//      0     W  N2W0A         NBG2 Window 0 Area (0=inside, 1=outside)
//
// 1800D4   WCTLC   RBG0 and Sprite Window Control
//
//   bits   r/w  code          description
//     15     W  SPLOG         Sprite Window Logic (0=OR, 1=AND)
//     14        -             Reserved, must be zero
//     13     W  SPSWE         Sprite Sprite Window Enable (0=disable, 1=enable)
//     12     W  SPSWA         Sprite Sprite Window Area (0=inside, 1=outside)
//     11     W  SPW1E         Sprite Window 1 Enable (0=disable, 1=enable)
//     10     W  SPW1A         Sprite Window 1 Area (0=inside, 1=outside)
//      9     W  SPW0E         Sprite Window 0 Enable (0=disable, 1=enable)
//      8     W  SPW0A         Sprite Window 0 Area (0=inside, 1=outside)
//      7     W  R0LOG         RBG0 Window Logic (0=OR, 1=AND)
//      6        -             Reserved, must be zero
//      5     W  R0SWE         RBG0 Sprite Window Enable (0=disable, 1=enable)
//      4     W  R0SWA         RBG0 Sprite Window Area (0=inside, 1=outside)
//      3     W  R0W1E         RBG0 Window 1 Enable (0=disable, 1=enable)
//      2     W  R0W1A         RBG0 Window 1 Area (0=inside, 1=outside)
//      1     W  R0W0E         RBG0 Window 0 Enable (0=disable, 1=enable)
//      0     W  R0W0A         RBG0 Window 0 Area (0=inside, 1=outside)
//
// 1800D6   WCTLD   Rotation Window and Color Calculation Window Control
//
//   bits   r/w  code          description
//     15     W  CCLOG         Sprite Window Logic (0=OR, 1=AND)
//     14        -             Reserved, must be zero
//     13     W  CCSWE         Color Calculation Window Sprite Window Enable (0=disable, 1=enable)
//     12     W  CCSWA         Color Calculation Window Sprite Window Area (0=inside, 1=outside)
//     11     W  CCW1E         Color Calculation Window Window 1 Enable (0=disable, 1=enable)
//     10     W  CCW1A         Color Calculation Window Window 1 Area (0=inside, 1=outside)
//      9     W  CCW0E         Color Calculation Window Window 0 Enable (0=disable, 1=enable)
//      8     W  CCW0A         Color Calculation Window Window 0 Area (0=inside, 1=outside)
//      7     W  RPLOG         Rotation Window Logic (0=OR, 1=AND)
//    6-4        -             Reserved, must be zero
//      3     W  RPW1E         Rotation Window Window 1 Enable (0=disable, 1=enable)
//      2     W  RPW1A         Rotation Window Window 1 Area (0=inside, 1=outside)
//      1     W  RPW0E         Rotation Window Window 0 Enable (0=disable, 1=enable)
//      0     W  RPW0A         Rotation Window Window 0 Area (0=inside, 1=outside)
union WCTL_t {
    uint64 u64;
    union {
        uint16 u16;
        struct {
            uint16 N0W0A : 1;
            uint16 N0W0E : 1;
            uint16 N0W1A : 1;
            uint16 N0W1E : 1;
            uint16 N0SWA : 1;
            uint16 N0SWE : 1;
            uint16 _rsvd6 : 1;
            uint16 N0LOG : 1;
            uint16 N1W0A : 1;
            uint16 N1W0E : 1;
            uint16 N1W1A : 1;
            uint16 N1W1E : 1;
            uint16 N1SWA : 1;
            uint16 N1SWE : 1;
            uint16 _rsvd14 : 1;
            uint16 N1LOG : 1;
        };
    } A;
    union {
        uint16 u16;
        struct {
            uint16 N2W0A : 1;
            uint16 N2W0E : 1;
            uint16 N2W1A : 1;
            uint16 N2W1E : 1;
            uint16 N2SWA : 1;
            uint16 N2SWE : 1;
            uint16 _rsvd6 : 1;
            uint16 N2LOG : 1;
            uint16 N3W0A : 1;
            uint16 N3W0E : 1;
            uint16 N3W1A : 1;
            uint16 N3W1E : 1;
            uint16 N3SWA : 1;
            uint16 N3SWE : 1;
            uint16 _rsvd14 : 1;
            uint16 N3LOG : 1;
        };
    } B;
    union {
        uint16 u16;
        struct {
            uint16 RPW0A : 1;
            uint16 RPW0E : 1;
            uint16 RPW1A : 1;
            uint16 RPW1E : 1;
            uint16 _rsvd4_6 : 3;
            uint16 RPLOG : 1;
            uint16 CCW0A : 1;
            uint16 CCW0E : 1;
            uint16 CCW1A : 1;
            uint16 CCW1E : 1;
            uint16 CCSWA : 1;
            uint16 CCSWE : 1;
            uint16 _rsvd14 : 1;
            uint16 CCLOG : 1;
        };
    } C;
    union {
        uint16 u16;
        struct {
            uint16 R0W0A : 1;
            uint16 R0W0E : 1;
            uint16 R0W1A : 1;
            uint16 R0W1E : 1;
            uint16 R0SWA : 1;
            uint16 R0SWE : 1;
            uint16 _rsvd6 : 1;
            uint16 R0LOG : 1;
            uint16 SPW0A : 1;
            uint16 SPW0E : 1;
            uint16 SPW1A : 1;
            uint16 SPW1E : 1;
            uint16 SPSWA : 1;
            uint16 SPSWE : 1;
            uint16 _rsvd14 : 1;
            uint16 SPLOG : 1;
        };
    } D;
};

// 1800D8   LWTA0U  Window 0 Line Window Address Table (upper)
// 1800DC   LWTA1U  Window 1 Line Window Address Table (upper)
//
//   bits   r/w  code          description
//     15     W  WxLWE         Line Window Enable (0=disabled, 1=enabled)
//   14-3        -             Reserved, must be zero
//    2-0     W  WxLWTA18-16   Line Window Address Table (bits 18-16)
//
// 1800DA   LWTA0L  Window 0 Line Window Address Table (lower)
// 1800DE   LWTA1L  Window 1 Line Window Address Table (lower)
//
//   bits   r/w  code          description
//   15-1     W  WxLWTA15-1    Line Window Address Table (bits 15-1)
//      0        -             Reserved, must be zero
union LWTA_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 _rsvd0 : 1;
                uint16 LWTAn : 15;
            };
        } L;
        union {
            uint16 u16;
            struct {
                uint16 LWTAn : 3;
                uint16 _rsvd3_14 : 12;
                uint16 LWE : 1;
            };
        } U;
    };
    struct {
        uint32 : 1;
        uint32 LWTAn : 18;
    };
};

// 1800E2   SDCTL   Shadow Control
//
//   bits   r/w  code          description
//   15-9        -             Reserved, must be zero
//      8     W  TPSDSL        Transparent Shadow (0=disable, 1=enable)
//    7-6        -             Reserved, must be zero
//      5     W  BKSDEN        Back Screen Shadow Enable
//      4     W  R0SDEN        RBG0 Shadow Enable
//      3     W  N3SDEN        NBG3 Shadow Enable
//      2     W  N2SDEN        NBG2 Shadow Enable
//      1     W  N1SDEN        NBG1/EXBG Shadow Enable
//      0     W  N0SDEN        NBG0/RBG1 Shadow Enable
union SDCTL_t {
    uint16 u16;
    struct {
        uint16 N0SDEN : 1;
        uint16 N1SDEN : 1;
        uint16 N2SDEN : 1;
        uint16 N3SDEN : 1;
        uint16 R0SDEN : 1;
        uint16 BKSDEN : 1;
        uint16 _rsvd6_7 : 2;
        uint16 TPSDSL : 1;
        uint16 _rsvd9_15 : 7;
    };
};

// 1800EC   CCCTL   Color Calculation Control
//
//   bits   r/w  code          description
//     15     W  BOKEN         Gradation Enable (0=disable, 1=enable)
//  14-12     W  BOKN2-0       Gradation Screen Number
//                               000 (0) = Sprite
//                               001 (1) = RBG0
//                               010 (2) = NBG0/RBG1
//                               011 (3) = Invalid
//                               100 (4) = NBG1/EXBG
//                               101 (5) = NBG2
//                               110 (6) = NBG3
//                               111 (7) = Invalid
//     11        -             Reserved, must be zero
//     10     W  EXCCEN        Extended Color Calculation Enable (0=disable, 1=enable)
//      9     W  CCRTMD        Color Calculation Ratio Mode (0=top screen side, 1=second screen side)
//      8     W  CCMD          Color Calculation Mode (0=use color calculation register, 1=as is)
//      7        -             Reserved, must be zero
//      6     W  SPCCEN        Sprite Color Calculation Enable
//      5     W  LCCCEN        Line Color Color Calculation Enable
//      4     W  R0CCEN        RBG0 Color Calculation Enable
//      3     W  N3CCEN        NBG3 Color Calculation Enable
//      2     W  N2CCEN        NBG2 Color Calculation Enable
//      1     W  N1CCEN        NBG1 Color Calculation Enable
//      0     W  N0CCEN        NBG0 Color Calculation Enable
//
// xxCCEN: 0=disable, 1=enable
union CCCTL_t {
    uint16 u16;
    struct {
        uint16 N0CCEN : 1;
        uint16 N1CCEN : 1;
        uint16 N2CCEN : 1;
        uint16 N3CCEN : 1;
        uint16 R0CCEN : 1;
        uint16 LCCCEN : 1;
        uint16 SPCCEN : 1;
        uint16 _rsvd7 : 1;
        uint16 CCMD : 1;
        uint16 CCRTMD : 1;
        uint16 EXCCEN : 1;
        uint16 _rsvd11 : 1;
        uint16 BOKNn : 3;
        uint16 BOKEN : 1;
    };
};

// 1800EE   SFCCMD  Special Color Calculation Mode
//
//   bits   r/w  code          description
//  15-10        -             Reserved, must be zero
//    9-8     W  R0SCCM1-0     RBG0 Special Color Calculation Mode
//    7-6     W  N3SCCM1-0     NBG3 Special Color Calculation Mode
//    5-4     W  N2SCCM1-0     NBG2 Special Color Calculation Mode
//    3-2     W  N1SCCM1-0     NBG1 Special Color Calculation Mode
//    1-0     W  N0SCCM1-0     NBG0 Special Color Calculation Mode
union SFCCMD_t {
    uint16 u16;
    struct {
        uint16 N0SCCMn : 2;
        uint16 N1SCCMn : 2;
        uint16 N2SCCMn : 2;
        uint16 N3SCCMn : 2;
        uint16 R0SCCMn : 2;
        uint16 _rsvd10_15 : 6;
    };
};

// 180108   CCRNA   NBG0 and NBG1 Color Calculation Ratio
//
//   bits   r/w  code          description
//  15-13        -             Reserved, must be zero
//   12-8     W  N1CCRT4-0     NBG1 Color Calculation Ratio
//    7-5        -             Reserved, must be zero
//    4-0     W  N0CCRT4-0     NBG0 Color Calculation Ratio
//
// 18010A   CCRNB   NBG2 and NBG3 Color Calculation Ratio
//
//   bits   r/w  code          description
//  15-13        -             Reserved, must be zero
//   12-8     W  N3CCRT4-0     NBG3 Color Calculation Ratio
//    7-5        -             Reserved, must be zero
//    4-0     W  N2CCRT4-0     NBG2 Color Calculation Ratio
//
// 18010C   CCRR    RBG0 Color Calculation Ratio
//
//   bits   r/w  code          description
//   15-5        -             Reserved, must be zero
//    4-0     W  R0CCRT4-0     RBG0 Color Calculation Ratio
//
// 18010E   CCRLB   Line Color Screen and Back Screen Color Calculation Ratio
//
//   bits   r/w  code          description
//  15-13        -             Reserved, must be zero
//   12-8     W  BKCCRT4-0     Back Screen Color Calculation Ratio
//    7-5        -             Reserved, must be zero
//    4-0     W  LCCCRT4-0     Line Color Screen Color Calculation Ratio
union CCR_t {
    uint16 u16;
    struct {
        uint16 lCCRTn : 5; // (NA) NBG0, (NB) NBG2, (R) RBG0, (LB) Line Color Screen
        uint16 _rsvd5_7 : 3;
        uint16 uCCRTn : 5; // (NA) NBG1, (NB) NBG3, (R) reserved, (LB) Back Screen
        uint16 _rsvd13_15 : 3;
    };
};

// 180110   CLOFEN  Color Offset Enable
//
//   bits   r/w  code          description
//   15-7        -             Reserved, must be zero
//      6     W  SPCOEN        Sprite Color Offset Enable
//      5     W  BKCOEN        Back Screen Color Offset Enable
//      4     W  R0COEN        RBG0 Color Offset Enable
//      3     W  N3COEN        NBG3 Color Offset Enable
//      2     W  N2COEN        NBG2 Color Offset Enable
//      1     W  N1COEN        NBG1 Color Offset Enable
//      0     W  N0COEN        NBG0 Color Offset Enable
//
// For all bits:
//   0 = enable
//   1 = disable
union CLOFEN_t {
    uint16 u16;
    struct {
        uint16 N0COEN : 1;
        uint16 N1COEN : 1;
        uint16 N2COEN : 1;
        uint16 N3COEN : 1;
        uint16 R0COEN : 1;
        uint16 BKCOEN : 1;
        uint16 SPCOEN : 1;
        uint16 _rsvd7_15 : 9;
    };
};

// 180112   CLOFSL  Color Offset Select
//
//   bits   r/w  code          description
//   15-7        -             Reserved, must be zero
//      6     W  SPCOSL        Sprite Color Offset Select
//      5     W  BKCOSL        Backdrop Color Offset Select
//      4     W  R0COSL        RBG0 Color Offset Select
//      3     W  N3COSL        NBG3 Color Offset Select
//      2     W  N2COSL        NBG2 Color Offset Select
//      1     W  N1COSL        NBG1 Color Offset Select
//      0     W  N0COSL        NBG0 Color Offset Select
//
// For all bits:
//   0 = Color Offset A
//   1 = Color Offset B
union CLOFSL_t {
    uint16 u16;
    struct {
        uint16 N0COSL : 1;
        uint16 N1COSL : 1;
        uint16 N2COSL : 1;
        uint16 N3COSL : 1;
        uint16 R0COSL : 1;
        uint16 BKCOSL : 1;
        uint16 SPCOSL : 1;
        uint16 _rsvd7_15 : 9;
    };
};

// 180114   COAR    Color Offset A - Red
// 180116   COAG    Color Offset A - Green
// 180118   COAB    Color Offset A - Blue
// 18011A   COBR    Color Offset B - Red
// 18011C   COBG    Color Offset B - Green
// 18011E   COBB    Color Offset B - Blue
//
//   bits   r/w  code          description
//   15-9        -             Reserved, must be zero
//    8-0     W  COxc8-0       Color Offset Value
//
// x: A,B; c: R,G,B
union CO_t {
    uint16_t u16;
    struct {
        uint16 COxcn : 9;
        uint16 _rsvd9_15 : 7;
    };
};

struct VDP2Regs {
    void Reset() {
        TVMD.u16 = 0x0;
        TVSTAT.u16 &= 0xFFFE; // Preserve PAL flag
        HCNT = 0x0;
        VCNT = 0x0;
        RAMCTL.u16 = 0x0;
        VRSIZE.u16 = 0x0;
        CYCA0.u32 = 0x0;
        CYCA1.u32 = 0x0;
        CYCB0.u32 = 0x0;
        CYCB1.u32 = 0x0;
        MZCTL.u16 = 0x0;
        ZMCTL.u16 = 0x0;
        SCRCTL.u16 = 0x0;
        VCSTA.u32 = 0x0;
        LSTA0.u32 = 0x0;
        LSTA1.u32 = 0x0;
        LCTA.u32 = 0x0;
        RPMD.u16 = 0x0;
        RPRCTL.u16 = 0x0;
        KTCTL.u16 = 0x0;
        KTAOF.u16 = 0x0;
        OVPNRA = 0x0;
        OVPNRB = 0x0;
        RPTA.u32 = 0x0;
        WPXY0.u64 = 0x0;
        WPXY1.u64 = 0x0;
        WCTL.u64 = 0x0;
        LWTA0.u32 = 0x0;
        LWTA1.u32 = 0x0;
        SDCTL.u16 = 0x0;
        CCCTL.u16 = 0x0;
        SFCCMD.u16 = 0x0;
        CCRNA.u16 = 0x0;
        CCRNB.u16 = 0x0;
        CCRR.u16 = 0x0;
        CCRLB.u16 = 0x0;
        CLOFEN.u16 = 0x0;
        CLOFSL.u16 = 0x0;
        COAR.u16 = 0x0;
        COAG.u16 = 0x0;
        COAB.u16 = 0x0;
        COBR.u16 = 0x0;
        COBG.u16 = 0x0;
        COBB.u16 = 0x0;

        for (auto &bg : normBGParams) {
            bg.Reset();
        }
        for (auto &bg : rotBGParams) {
            bg.Reset();
        }
        for (auto &sp : specialFunctionCodes) {
            sp.Reset();
        }

        TVMDDirty = true;
    }

    TVMD_t TVMD;     // 180000   TVMD    TV Screen Mode
    EXTEN_t EXTEN;   // 180002   EXTEN   External Signal Enable
    TVSTAT_t TVSTAT; // 180004   TVSTAT  Screen Status (read-only)
    VRSIZE_t VRSIZE; // 180006   VRSIZE  VRAM Size
    uint16 HCNT;     // 180008   HCNT    H Counter (read-only)
    uint16 VCNT;     // 18000A   VCNT    V Counter (read-only)
                     // 18000C   -       Reserved (but not really)
    RAMCTL_t RAMCTL; // 18000E   RAMCTL  RAM Control
                     // 180010   CYCA0L  VRAM Cycle Pattern A0 Lower
    CYC_t CYCA0;     // 180012   CYCA0U  VRAM Cycle Pattern A0 Upper
                     // 180014   CYCA1L  VRAM Cycle Pattern A1 Lower
    CYC_t CYCA1;     // 180016   CYCA1U  VRAM Cycle Pattern A1 Upper
                     // 180018   CYCB0L  VRAM Cycle Pattern B0 Lower
    CYC_t CYCB0;     // 18001A   CYCB0U  VRAM Cycle Pattern B0 Upper
                     // 18001C   CYCB1L  VRAM Cycle Pattern B1 Lower
    CYC_t CYCB1;     // 18001E   CYCB1U  VRAM Cycle Pattern B1 Upper

    // 180020   BGON    Screen Display Enable
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //     12     W  R0TPON        RBG0 Transparent Display (0=enable, 1=disable)
    //     11     W  N3TPON        NBG3 Transparent Display (0=enable, 1=disable)
    //     10     W  N2TPON        NBG2 Transparent Display (0=enable, 1=disable)
    //      9     W  N1TPON        NBG1/EXBG Transparent Display (0=enable, 1=disable)
    //      8     W  N0TPON        NBG0/RBG1 Transparent Display (0=enable, 1=disable)
    //    7-6        -             Reserved, must be zero
    //      5     W  R1ON          RBG1 Display (0=disable, 1=enable)
    //      4     W  R0ON          RBG0 Display (0=disable, 1=enable)
    //      3     W  N3ON          NBG3 Display (0=disable, 1=enable)
    //      2     W  N2ON          NBG2 Display (0=disable, 1=enable)
    //      1     W  N1ON          NBG1 Display (0=disable, 1=enable)
    //      0     W  N0ON          NBG0 Display (0=disable, 1=enable)

    FORCE_INLINE uint16 ReadBGON() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, normBGParams[0].enabled);
        bit::deposit_into<1>(value, normBGParams[1].enabled);
        bit::deposit_into<2>(value, normBGParams[2].enabled);
        bit::deposit_into<3>(value, normBGParams[3].enabled);
        bit::deposit_into<4>(value, rotBGParams[0].enabled);
        bit::deposit_into<5>(value, rotBGParams[1].enabled);

        bit::deposit_into<8>(value, normBGParams[0].transparent);
        bit::deposit_into<9>(value, normBGParams[1].transparent);
        bit::deposit_into<10>(value, normBGParams[2].transparent);
        bit::deposit_into<11>(value, normBGParams[3].transparent);
        bit::deposit_into<12>(value, rotBGParams[0].transparent);
        return value;
    }

    FORCE_INLINE void WriteBGON(uint16 value) {
        normBGParams[0].enabled = bit::extract<0>(value);
        normBGParams[1].enabled = bit::extract<1>(value);
        normBGParams[2].enabled = bit::extract<2>(value);
        normBGParams[3].enabled = bit::extract<3>(value);
        rotBGParams[0].enabled = bit::extract<4>(value);
        rotBGParams[1].enabled = bit::extract<5>(value);

        normBGParams[0].transparent = bit::extract<8>(value);
        normBGParams[1].transparent = bit::extract<9>(value);
        normBGParams[2].transparent = bit::extract<10>(value);
        normBGParams[3].transparent = bit::extract<11>(value);
        rotBGParams[0].transparent = bit::extract<12>(value);
        rotBGParams[1].transparent = normBGParams[0].transparent;
    }

    MZCTL_t MZCTL; // 180022   MZCTL   Mosaic Control

    // 180024   SFSEL   Special Function Code Select
    //
    //   bits   r/w  code          description
    //   15-5        -             Reserved, must be zero
    //      4     W  R0SFCS        RBG0 Special Function Code Select
    //      3     W  N3SFCS        NBG3 Special Function Code Select
    //      2     W  N2SFCS        NBG2 Special Function Code Select
    //      1     W  N1SFCS        NBG1 Special Function Code Select
    //      0     W  N0SFCS        NBG0/RBG1 Special Function Code Select

    FORCE_INLINE uint16 ReadSFSEL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, normBGParams[0].specialFunctionSelect);
        bit::deposit_into<1>(value, normBGParams[1].specialFunctionSelect);
        bit::deposit_into<2>(value, normBGParams[2].specialFunctionSelect);
        bit::deposit_into<3>(value, normBGParams[3].specialFunctionSelect);
        bit::deposit_into<4>(value, rotBGParams[0].specialFunctionSelect);
        return value;
    }

    FORCE_INLINE void WriteSFSEL(uint16 value) {
        normBGParams[0].specialFunctionSelect = bit::extract<0>(value);
        normBGParams[1].specialFunctionSelect = bit::extract<1>(value);
        normBGParams[2].specialFunctionSelect = bit::extract<2>(value);
        normBGParams[3].specialFunctionSelect = bit::extract<3>(value);
        rotBGParams[0].specialFunctionSelect = bit::extract<4>(value);
        rotBGParams[1].specialFunctionSelect = normBGParams[0].specialFunctionSelect;
    }

    // 180026   SFCODE  Special Function Code
    //
    //   bits   r/w  code          description
    //   15-8        SFCDB7-0      Special Function Code B
    //    7-0        SFCDA7-0      Special Function Code A
    //
    // Each bit in SFCDxn matches the least significant 4 bits of the color code:
    //   n=0: 0x0 or 0x1
    //   n=1: 0x2 or 0x3
    //   n=2: 0x4 or 0x5
    //   n=3: 0x6 or 0x7
    //   n=4: 0x8 or 0x9
    //   n=5: 0xA or 0xB
    //   n=6: 0xC or 0xD
    //   n=7: 0xE or 0xF

    FORCE_INLINE uint16 ReadSFCODE() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, specialFunctionCodes[0].colorMatches[0]);
        bit::deposit_into<1>(value, specialFunctionCodes[0].colorMatches[1]);
        bit::deposit_into<2>(value, specialFunctionCodes[0].colorMatches[2]);
        bit::deposit_into<3>(value, specialFunctionCodes[0].colorMatches[3]);
        bit::deposit_into<4>(value, specialFunctionCodes[0].colorMatches[4]);
        bit::deposit_into<5>(value, specialFunctionCodes[0].colorMatches[5]);
        bit::deposit_into<6>(value, specialFunctionCodes[0].colorMatches[6]);
        bit::deposit_into<7>(value, specialFunctionCodes[0].colorMatches[7]);

        bit::deposit_into<8>(value, specialFunctionCodes[1].colorMatches[0]);
        bit::deposit_into<9>(value, specialFunctionCodes[1].colorMatches[1]);
        bit::deposit_into<10>(value, specialFunctionCodes[1].colorMatches[2]);
        bit::deposit_into<11>(value, specialFunctionCodes[1].colorMatches[3]);
        bit::deposit_into<12>(value, specialFunctionCodes[1].colorMatches[4]);
        bit::deposit_into<13>(value, specialFunctionCodes[1].colorMatches[5]);
        bit::deposit_into<14>(value, specialFunctionCodes[1].colorMatches[6]);
        bit::deposit_into<15>(value, specialFunctionCodes[1].colorMatches[7]);
        return value;
    }

    FORCE_INLINE void WriteSFCODE(uint16 value) {
        specialFunctionCodes[0].colorMatches[0] = bit::extract<0>(value);
        specialFunctionCodes[0].colorMatches[1] = bit::extract<1>(value);
        specialFunctionCodes[0].colorMatches[2] = bit::extract<2>(value);
        specialFunctionCodes[0].colorMatches[3] = bit::extract<3>(value);
        specialFunctionCodes[0].colorMatches[4] = bit::extract<4>(value);
        specialFunctionCodes[0].colorMatches[5] = bit::extract<5>(value);
        specialFunctionCodes[0].colorMatches[6] = bit::extract<6>(value);
        specialFunctionCodes[0].colorMatches[7] = bit::extract<7>(value);

        specialFunctionCodes[1].colorMatches[0] = bit::extract<8>(value);
        specialFunctionCodes[1].colorMatches[1] = bit::extract<9>(value);
        specialFunctionCodes[1].colorMatches[2] = bit::extract<10>(value);
        specialFunctionCodes[1].colorMatches[3] = bit::extract<11>(value);
        specialFunctionCodes[1].colorMatches[4] = bit::extract<12>(value);
        specialFunctionCodes[1].colorMatches[5] = bit::extract<13>(value);
        specialFunctionCodes[1].colorMatches[6] = bit::extract<14>(value);
        specialFunctionCodes[1].colorMatches[7] = bit::extract<15>(value);
    }

    // 180028   CHCTLA  Character Control Register A
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //  13-12     W  N1CHCN1-0     NBG1/EXBG Character Color Number
    //                               00 (0) =       16 colors - palette
    //                               01 (1) =      256 colors - palette
    //                               10 (2) =     2048 colors - palette
    //                               11 (3) =    32768 colors - RGB (NBG1)
    //                                        16777216 colors - RGB (EXBG)
    //  11-10     W  N1BMSZ1-0     NBG1 Bitmap Size
    //                               00 (0) = 512x256
    //                               01 (1) = 512x512
    //                               10 (2) = 1024x256
    //                               11 (3) = 1024x512
    //      9     W  N1BMEN        NBG1 Bitmap Enable (0=cells, 1=bitmap)
    //      8     W  N1CHSZ        NBG1 Character Size (0=1x1, 1=2x2)
    //      7        -             Reserved, must be zero
    //    6-4     W  N0CHCN2-0     NBG0/RBG1 Character Color Number
    //                               000 (0) =       16 colors - palette
    //                               001 (1) =      256 colors - palette
    //                               010 (2) =     2048 colors - palette
    //                               011 (3) =    32768 colors - RGB
    //                               100 (4) = 16777216 colors - RGB (Normal mode only)
    //                                           forbidden for Hi-Res or Exclusive Monitor
    //                               101 (5) = forbidden
    //                               110 (6) = forbidden
    //                               111 (7) = forbidden
    //    3-2     W  N0BMSZ1-0     NBG0 Bitmap Size
    //                               00 (0) = 512x256
    //                               01 (1) = 512x512
    //                               10 (2) = 1024x256
    //                               11 (3) = 1024x512
    //      1     W  N0BMEN        NBG0 Bitmap Enable (0=cells, 1=bitmap)
    //      0     W  N0CHSZ        NBG0 Character Size (0=1x1, 1=2x2)

    FORCE_INLINE uint16 ReadCHCTLA() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, normBGParams[0].cellSizeShift);
        bit::deposit_into<1>(value, normBGParams[0].bitmap);
        bit::deposit_into<2, 3>(value, normBGParams[0].bmsz);
        bit::deposit_into<4, 6>(value, static_cast<uint32>(normBGParams[0].colorFormat));

        bit::deposit_into<8>(value, normBGParams[1].cellSizeShift);
        bit::deposit_into<9>(value, normBGParams[1].bitmap);
        bit::deposit_into<10, 11>(value, normBGParams[1].bmsz);
        bit::deposit_into<12, 13>(value, static_cast<uint32>(normBGParams[1].colorFormat));
        return value;
    }

    FORCE_INLINE void WriteCHCTLA(uint16 value) {
        normBGParams[0].cellSizeShift = bit::extract<0>(value);
        normBGParams[0].bitmap = bit::extract<1>(value);
        normBGParams[0].bmsz = bit::extract<2, 3>(value);
        normBGParams[0].colorFormat = static_cast<ColorFormat>(bit::extract<4, 6>(value));
        normBGParams[0].UpdateCHCTL();

        rotBGParams[1].colorFormat = normBGParams[0].colorFormat;
        rotBGParams[1].UpdateCHCTL();

        normBGParams[1].cellSizeShift = bit::extract<8>(value);
        normBGParams[1].bitmap = bit::extract<9>(value);
        normBGParams[1].bmsz = bit::extract<10, 11>(value);
        normBGParams[1].colorFormat = static_cast<ColorFormat>(bit::extract<12, 13>(value));
        normBGParams[1].UpdateCHCTL();
    }

    // 18002A   CHCTLB  Character Control Register B
    //
    //   bits   r/w  code          description
    //     15        -             Reserved, must be zero
    //  14-12     W  R0CHCN2-0     RBG0 Character Color Number
    //                               NOTE: Exclusive Monitor cannot display this BG plane
    //                               000 (0) =       16 colors - palette
    //                               001 (1) =      256 colors - palette
    //                               010 (2) =     2048 colors - palette
    //                               011 (3) =    32768 colors - RGB
    //                               100 (4) = 16777216 colors - RGB (Normal mode only)
    //                                           forbidden for Hi-Res
    //                               101 (5) = forbidden
    //                               110 (6) = forbidden
    //                               111 (7) = forbidden
    //     11        -             Reserved, must be zero
    //     10     W  R0BMSZ        RBG0 Bitmap Size (0=512x256, 1=512x512)
    //      9     W  R0BMEN        RBG0 Bitmap Enable (0=cells, 1=bitmap)
    //      8     W  R0CHSZ        RBG0 Character Size (0=1x1, 1=2x2)
    //    7-6        -             Reserved, must be zero
    //      5     W  N3CHCN        NBG3 Character Color Number (0=16 colors, 1=256 colors; both palette)
    //      4     W  N3CHSZ        NBG3 Character Size (0=1x1, 1=2x2)
    //    3-2        -             Reserved, must be zero
    //      1     W  N2CHCN        NBG2 Character Color Number (0=16 colors, 1=256 colors; both palette)
    //      0     W  N2CHSZ        NBG2 Character Size (0=1x1, 1=2x2)

    FORCE_INLINE uint16 ReadCHCTLB() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, normBGParams[2].cellSizeShift);
        bit::deposit_into<1>(value, static_cast<uint32>(normBGParams[2].colorFormat));

        bit::deposit_into<4>(value, normBGParams[3].cellSizeShift);
        bit::deposit_into<5>(value, static_cast<uint32>(normBGParams[3].colorFormat));

        bit::deposit_into<8>(value, rotBGParams[0].cellSizeShift);
        bit::deposit_into<9>(value, rotBGParams[0].bitmap);
        bit::deposit_into<10>(value, rotBGParams[0].bmsz);
        bit::deposit_into<12, 14>(value, static_cast<uint32>(rotBGParams[0].colorFormat));
        return value;
    }

    FORCE_INLINE void WriteCHCTLB(uint16 value) {
        normBGParams[2].cellSizeShift = bit::extract<0>(value);
        normBGParams[2].colorFormat = static_cast<ColorFormat>(bit::extract<1>(value));
        normBGParams[2].UpdateCHCTL();

        normBGParams[3].cellSizeShift = bit::extract<4>(value);
        normBGParams[3].colorFormat = static_cast<ColorFormat>(bit::extract<5>(value));
        normBGParams[3].UpdateCHCTL();

        rotBGParams[0].cellSizeShift = bit::extract<8>(value);
        rotBGParams[0].bitmap = bit::extract<9>(value);
        rotBGParams[0].bmsz = bit::extract<10>(value);
        rotBGParams[0].colorFormat = static_cast<ColorFormat>(bit::extract<12, 14>(value));
        rotBGParams[0].UpdateCHCTL();
    }

    // 18002C   BMPNA   NBG0/NBG1 Bitmap Palette Number
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //     13     W  N1BMPR        NBG1 Special Priority
    //     12     W  N1BMCC        NBG1 Special Color Calculation
    //     11        -             Reserved, must be zero
    //   10-8     W  N1BMP6-4      NBG1 Palette Number
    //    7-6        -             Reserved, must be zero
    //      5     W  N0BMPR        NBG0 Special Priority
    //      4     W  N0BMCC        NBG0 Special Color Calculation
    //      3        -             Reserved, must be zero
    //    2-0     W  N0BMP6-4      NBG0 Palette Number

    FORCE_INLINE uint16 ReadBMPNA() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, normBGParams[0].supplBitmapPalNum >> 4u);
        bit::deposit_into<4>(value, normBGParams[0].supplBitmapSpecialColorCalc);
        bit::deposit_into<5>(value, normBGParams[0].supplBitmapSpecialPriority);

        bit::deposit_into<8, 10>(value, normBGParams[1].supplBitmapPalNum >> 4u);
        bit::deposit_into<12>(value, normBGParams[1].supplBitmapSpecialColorCalc);
        bit::deposit_into<13>(value, normBGParams[1].supplBitmapSpecialPriority);
        return value;
    }

    FORCE_INLINE void WriteBMPNA(uint16 value) {
        normBGParams[0].supplBitmapPalNum = bit::extract<0, 2>(value) << 4u;
        normBGParams[0].supplBitmapSpecialColorCalc = bit::extract<4>(value);
        normBGParams[0].supplBitmapSpecialPriority = bit::extract<5>(value);

        normBGParams[1].supplBitmapPalNum = bit::extract<8, 10>(value) << 4u;
        normBGParams[1].supplBitmapSpecialColorCalc = bit::extract<12>(value);
        normBGParams[1].supplBitmapSpecialPriority = bit::extract<13>(value);
    }

    // 18002E   BMPNB   RBG0 Bitmap Palette Number
    //
    //   bits   r/w  code          description
    //   15-6        -             Reserved, must be zero
    //      5     W  R0BMPR        RBG0 Special Priority
    //      4     W  R0BMCC        RBG0 Special Color Calculation
    //      3        -             Reserved, must be zero
    //    2-0     W  R0BMP6-4      RBG0 Palette Number

    FORCE_INLINE uint16 ReadBMPNB() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, rotBGParams[0].supplBitmapPalNum >> 4u);
        bit::deposit_into<4>(value, rotBGParams[0].supplBitmapSpecialColorCalc);
        bit::deposit_into<5>(value, rotBGParams[0].supplBitmapSpecialPriority);
        return value;
    }

    FORCE_INLINE void WriteBMPNB(uint16 value) {
        rotBGParams[0].supplBitmapPalNum = bit::extract<0, 2>(value) << 4u;
        rotBGParams[0].supplBitmapSpecialColorCalc = bit::extract<4>(value);
        rotBGParams[0].supplBitmapSpecialPriority = bit::extract<5>(value);
    }

    // 180030   PNCN0   NBG0/RBG1 Pattern Name Control
    // 180032   PNCN1   NBG1 Pattern Name Control
    // 180034   PNCN2   NBG2 Pattern Name Control
    // 180036   PNCN3   NBG3 Pattern Name Control
    // 180038   PNCR    RBG0 Pattern Name Control
    //
    //   bits   r/w  code          description
    //     15     W  xxPNB         Pattern Name Data Size (0=2 words, 1=1 word)
    //     14     W  xxCNSM        Character Number Supplement
    //                               0 = char number is 10 bits; H/V flip available
    //                               1 = char number is 12 bits; H/V flip unavailable
    //  13-10        -             Reserved, must be zero
    //      9     W  xxSPR         Special Priority bit
    //      8     W  xxSCC         Special Color Calculation bit
    //    7-5     W  xxSPLT6-4     Supplementary Palette bits 6-4
    //    4-0     W  xxSCN4-0      Supplementary Character Number bits 4-0

    FORCE_INLINE uint16 ReadPNCN(uint32 bgIndex) const {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, normBGParams[bgIndex].supplScrollCharNum);
        bit::deposit_into<5, 7>(value, normBGParams[bgIndex].supplScrollPalNum >> 4u);
        bit::deposit_into<8>(value, normBGParams[bgIndex].supplScrollSpecialColorCalc);
        bit::deposit_into<9>(value, normBGParams[bgIndex].supplScrollSpecialPriority);
        bit::deposit_into<14>(value, normBGParams[bgIndex].wideChar);
        bit::deposit_into<15>(value, !normBGParams[bgIndex].twoWordChar);
        return value;
    }

    FORCE_INLINE void WritePNCN(uint32 bgIndex, uint16 value) {
        normBGParams[bgIndex].supplScrollCharNum = bit::extract<0, 4>(value);
        normBGParams[bgIndex].supplScrollPalNum = bit::extract<5, 7>(value) << 4u;
        normBGParams[bgIndex].supplScrollSpecialColorCalc = bit::extract<8>(value);
        normBGParams[bgIndex].supplScrollSpecialPriority = bit::extract<9>(value);
        normBGParams[bgIndex].wideChar = bit::extract<14>(value);
        normBGParams[bgIndex].twoWordChar = !bit::extract<15>(value);
        normBGParams[bgIndex].UpdatePageBaseAddresses();

        if (bgIndex == 0) {
            rotBGParams[1].supplScrollCharNum = normBGParams[0].supplScrollCharNum;
            rotBGParams[1].supplScrollPalNum = normBGParams[0].supplScrollPalNum;
            rotBGParams[1].supplScrollSpecialColorCalc = normBGParams[0].supplScrollSpecialColorCalc;
            rotBGParams[1].supplScrollSpecialPriority = normBGParams[0].supplScrollSpecialPriority;
            rotBGParams[1].wideChar = normBGParams[0].wideChar;
            rotBGParams[1].twoWordChar = normBGParams[0].twoWordChar;
            rotBGParams[1].UpdatePageBaseAddresses();
        }
    }

    FORCE_INLINE uint16 ReadPNCR() const {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, rotBGParams[0].supplScrollCharNum);
        bit::deposit_into<5, 7>(value, rotBGParams[0].supplScrollPalNum >> 4u);
        bit::deposit_into<8>(value, rotBGParams[0].supplScrollSpecialColorCalc);
        bit::deposit_into<9>(value, rotBGParams[0].supplScrollSpecialPriority);
        bit::deposit_into<14>(value, rotBGParams[0].wideChar);
        bit::deposit_into<15>(value, !rotBGParams[0].twoWordChar);
        return value;
    }

    FORCE_INLINE void WritePNCR(uint16 value) {
        rotBGParams[0].supplScrollCharNum = bit::extract<0, 4>(value);
        rotBGParams[0].supplScrollPalNum = bit::extract<5, 7>(value) << 4u;
        rotBGParams[0].supplScrollSpecialColorCalc = bit::extract<8>(value);
        rotBGParams[0].supplScrollSpecialPriority = bit::extract<9>(value);
        rotBGParams[0].wideChar = bit::extract<14>(value);
        rotBGParams[0].twoWordChar = !bit::extract<15>(value);
        rotBGParams[0].UpdatePageBaseAddresses();
    }

    // 18003A   PLSZ    Plane Size
    //
    //   bits   r/w  code          description
    //  15-14     W  RBOVR1-0      Rotation Parameter B Screen-over Process
    //  13-12     W  RBPLSZ1-0     Rotation Parameter B Plane Size
    //  11-10     W  RAOVR1-0      Rotation Parameter A Screen-over Process
    //    9-8     W  RAPLSZ1-0     Rotation Parameter A Plane Size
    //    7-6     W  N3PLSZ1-0     NBG3 Plane Size
    //    5-4     W  N2PLSZ1-0     NBG2 Plane Size
    //    3-2     W  N1PLSZ1-0     NBG1 Plane Size
    //    1-0     W  N0PLSZ1-0     NBG0 Plane Size
    //
    //  xxOVR1-0:
    //    00 (0) = Repeat plane infinitely
    //    01 (1) = Use character pattern in screen-over pattern name register
    //    10 (2) = Transparent
    //    11 (3) = Force 512x512 with transparent outsides (256 line bitmaps draw twice)
    //
    //  xxPLSZ1-0:
    //    00 (0) = 1x1
    //    01 (1) = 2x1
    //    10 (2) = forbidden (but probably 1x2)
    //    11 (3) = 2x2

    FORCE_INLINE uint16 ReadPLSZ() const {
        uint16 value = 0;
        bit::deposit_into<0, 1>(value, normBGParams[0].plsz);
        bit::deposit_into<2, 3>(value, normBGParams[1].plsz);
        bit::deposit_into<4, 5>(value, normBGParams[2].plsz);
        bit::deposit_into<6, 7>(value, normBGParams[3].plsz);
        bit::deposit_into<8, 9>(value, rotBGParams[0].plsz);
        bit::deposit_into<10, 11>(value, static_cast<uint32>(rotBGParams[0].screenOverProcess));
        bit::deposit_into<12, 13>(value, rotBGParams[1].plsz);
        bit::deposit_into<14, 15>(value, static_cast<uint32>(rotBGParams[1].screenOverProcess));
        return value;
    }

    FORCE_INLINE void WritePLSZ(uint16 value) {
        normBGParams[0].plsz = bit::extract<0, 1>(value);
        normBGParams[1].plsz = bit::extract<2, 3>(value);
        normBGParams[2].plsz = bit::extract<4, 5>(value);
        normBGParams[3].plsz = bit::extract<6, 7>(value);
        rotBGParams[0].plsz = bit::extract<8, 9>(value);
        rotBGParams[0].screenOverProcess = static_cast<ScreenOverProcess>(bit::extract<10, 11>(value));
        rotBGParams[1].plsz = bit::extract<12, 13>(value);
        rotBGParams[1].screenOverProcess = static_cast<ScreenOverProcess>(bit::extract<14, 15>(value));
        for (auto &bg : normBGParams) {
            bg.UpdatePLSZ();
        }
        for (auto &bg : rotBGParams) {
            bg.UpdatePLSZ();
        }
    }

    // 18003C   MPOFN   NBG0-3 Map Offset
    //
    //   bits   r/w  code          description
    //     15        -             Reserved, must be zero
    //  14-12     W  M3MP8-6       NBG3 Map Offset
    //     11        -             Reserved, must be zero
    //   10-8     W  M2MP8-6       NBG2 Map Offset
    //      7        -             Reserved, must be zero
    //    6-4     W  M1MP8-6       NBG1 Map Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  M0MP8-6       NBG0 Map Offset

    FORCE_INLINE uint16 ReadMPOFN() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<6, 8>(normBGParams[0].mapIndices[0]));
        bit::deposit_into<4, 6>(value, bit::extract<6, 8>(normBGParams[1].mapIndices[0]));
        bit::deposit_into<8, 10>(value, bit::extract<6, 8>(normBGParams[2].mapIndices[0]));
        bit::deposit_into<12, 14>(value, bit::extract<6, 8>(normBGParams[3].mapIndices[0]));
        return value;
    }

    FORCE_INLINE void WriteMPOFN(uint16 value) {
        for (int i = 0; i < 4; i++) {
            bit::deposit_into<6, 8>(normBGParams[0].mapIndices[i], bit::extract<0, 2>(value));
            bit::deposit_into<6, 8>(normBGParams[1].mapIndices[i], bit::extract<4, 6>(value));
            bit::deposit_into<6, 8>(normBGParams[2].mapIndices[i], bit::extract<8, 10>(value));
            bit::deposit_into<6, 8>(normBGParams[3].mapIndices[i], bit::extract<12, 14>(value));
        }
        // shift by 17 is the same as multiply by 0x20000, which is the boundary for bitmap data
        normBGParams[0].bitmapBaseAddress = bit::extract<0, 2>(value) << 17u;
        normBGParams[1].bitmapBaseAddress = bit::extract<4, 6>(value) << 17u;
        normBGParams[2].bitmapBaseAddress = bit::extract<8, 10>(value) << 17u;
        normBGParams[3].bitmapBaseAddress = bit::extract<12, 14>(value) << 17u;

        for (auto &bg : normBGParams) {
            bg.UpdatePageBaseAddresses();
        }
    }

    // 18003E   MPOFR   Rotation Parameter A/B Map Offset
    //
    //   bits   r/w  code          description
    //   15-7        -             Reserved, must be zero
    //    6-4     W  RBMP8-6       Rotation Parameter B Map Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  RAMP8-6       Rotation Parameter A Map Offset

    FORCE_INLINE uint16 ReadMPOFR() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<6, 8>(rotBGParams[0].mapIndices[0]));
        bit::deposit_into<4, 6>(value, bit::extract<6, 8>(rotBGParams[1].mapIndices[0]));
        return value;
    }

    FORCE_INLINE void WriteMPOFR(uint16 value) {
        for (int i = 0; i < 4; i++) {
            bit::deposit_into<6, 8>(rotBGParams[0].mapIndices[i], bit::extract<0, 2>(value));
            bit::deposit_into<6, 8>(rotBGParams[1].mapIndices[i], bit::extract<4, 6>(value));
        }
        // shift by 17 is the same as multiply by 0x20000, which is the boundary for bitmap data
        rotBGParams[0].bitmapBaseAddress = bit::extract<0, 2>(value) << 17u;
        rotBGParams[1].bitmapBaseAddress = bit::extract<4, 6>(value) << 17u;

        for (auto &bg : rotBGParams) {
            bg.UpdatePageBaseAddresses();
        }
    }

    // 180040   MPABN0  NBG0 Normal Scroll Screen Map for Planes A,B
    // 180042   MPCDN0  NBG0 Normal Scroll Screen Map for Planes C,D
    // 180044   MPABN1  NBG1 Normal Scroll Screen Map for Planes A,B
    // 180046   MPCDN1  NBG1 Normal Scroll Screen Map for Planes C,D
    // 180048   MPABN2  NBG2 Normal Scroll Screen Map for Planes A,B
    // 18004A   MPCDN2  NBG2 Normal Scroll Screen Map for Planes C,D
    // 18004C   MPABN3  NBG3 Normal Scroll Screen Map for Planes A,B
    // 18004E   MPCDN3  NBG3 Normal Scroll Screen Map for Planes C,D
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //   13-8     W  xxMPy5-0      BG xx Plane y Map
    //    7-6        -             Reserved, must be zero
    //    5-0     W  xxMPy5-0      BG xx Plane y Map
    //
    // xx:
    //   N0 = NBG0 (MPyyN0)
    //   N1 = NBG1 (MPyyN1)
    //   N2 = NBG2 (MPyyN2)
    //   N3 = NBG3 (MPyyN3)
    // y:
    //   A = Plane A (bits  5-0 of MPABxx)
    //   B = Plane B (bits 13-8 of MPABxx)
    //   C = Plane C (bits  5-0 of MPCDxx)
    //   D = Plane D (bits 13-8 of MPCDxx)

    FORCE_INLINE uint16 ReadMPN(uint32 bgIndex, uint32 planeIndex) const {
        uint16 value = 0;
        auto &bg = normBGParams[bgIndex];
        bit::deposit_into<0, 5>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 0]));
        bit::deposit_into<8, 13>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 1]));
        return value;
    }

    FORCE_INLINE void WriteMPN(uint32 bgIndex, uint32 planeIndex, uint16 value) {
        auto &bg = normBGParams[bgIndex];
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 0], bit::extract<0, 5>(value));
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 1], bit::extract<8, 13>(value));
        bg.UpdatePageBaseAddresses();
    }

    // 180050   MPABRA  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
    // 180052   MPCDRA  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
    // 180054   MPEFRA  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
    // 180056   MPGHRA  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
    // 180058   MPIJRA  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
    // 18005A   MPKLRA  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
    // 18005C   MPMNRA  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    // 18005E   MPOPRA  Rotation Parameter A Scroll Surface Map for Screen Planes O,P
    // 180060   MPABRB  Rotation Parameter A Scroll Surface Map for Screen Planes A,B
    // 180062   MPCDRB  Rotation Parameter A Scroll Surface Map for Screen Planes C,D
    // 180064   MPEFRB  Rotation Parameter A Scroll Surface Map for Screen Planes E,F
    // 180066   MPGHRB  Rotation Parameter A Scroll Surface Map for Screen Planes G,H
    // 180068   MPIJRB  Rotation Parameter A Scroll Surface Map for Screen Planes I,J
    // 18006A   MPKLRB  Rotation Parameter A Scroll Surface Map for Screen Planes K,L
    // 18006C   MPMNRB  Rotation Parameter A Scroll Surface Map for Screen Planes M,N
    // 18006E   MPOPRB  Rotation Parameter A Scroll Surface Map for Screen Planes O,P
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //   13-8     W  RxMPy5-0      Rotation Parameter x Screen Plane y Map
    //    7-6        -             Reserved, must be zero
    //    5-0     W  RxMPy5-0      Rotation Parameter x Screen Plane y Map
    //
    // x:
    //   A = Rotation Parameter A (MPyyRA)
    //   B = Rotation Parameter A (MPyyRB)
    // y:
    //   A = Screen Plane A (bits  5-0 of MPABxx)
    //   B = Screen Plane B (bits 13-8 of MPABxx)
    //   C = Screen Plane C (bits  5-0 of MPCDxx)
    //   D = Screen Plane D (bits 13-8 of MPCDxx)
    //   ...
    //   M = Screen Plane M (bits  5-0 of MPMNxx)
    //   N = Screen Plane N (bits 13-8 of MPMNxx)
    //   O = Screen Plane O (bits  5-0 of MPOPxx)
    //   P = Screen Plane P (bits 13-8 of MPOPxx)

    FORCE_INLINE uint16 ReadMPR(uint32 bgIndex, uint32 planeIndex) const {
        uint16 value = 0;
        auto &bg = rotBGParams[bgIndex];
        bit::deposit_into<0, 5>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 0]));
        bit::deposit_into<8, 13>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 1]));
        return value;
    }

    FORCE_INLINE void WriteMPR(uint32 bgIndex, uint32 planeIndex, uint16 value) {
        auto &bg = rotBGParams[bgIndex];
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 0], bit::extract<0, 5>(value));
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 1], bit::extract<8, 13>(value));
        bg.UpdatePageBaseAddresses();
    }

    // 180070   SCXIN0  NBG0 Horizontal Screen Scroll Value (integer part)
    // 180072   SCXDN0  NBG0 Horizontal Screen Scroll Value (fractional part)
    // 180074   SCYIN0  NBG0 Vertical Screen Scroll Value (integer part)
    // 180076   SCYDN0  NBG0 Vertical Screen Scroll Value (fractional part)
    // 180080   SCXIN1  NBG1 Horizontal Screen Scroll Value (integer part)
    // 180082   SCXDN1  NBG1 Horizontal Screen Scroll Value (fractional part)
    // 180084   SCYIN1  NBG1 Vertical Screen Scroll Value (integer part)
    // 180086   SCYDN1  NBG1 Vertical Screen Scroll Value (fractional part)
    //
    // SCdINx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-0     W  NxSCdI10-0    Horizontal/Vertical Screen Scroll Value (integer part)
    //
    // SCdDNx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //   15-8     W  NxSCdD1-8     Horizontal/Vertical Screen Scroll Value (fractional part)
    //    7-0        -             Reserved, must be zero
    //
    // 180090   SCXN2   NBG2 Horizontal Screen Scroll Value
    // 180092   SCYN2   NBG2 Vertical Screen Scroll Value
    // 180094   SCXN3   NBG3 Horizontal Screen Scroll Value
    // 180096   SCYN3   NBG3 Vertical Screen Scroll Value
    //
    // SCdNx:  (d=X,Y; x=2,3)
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-0     W  NxSCd10-0     Horizontal/Vertical Screen Scroll Value (integer)

    FORCE_INLINE uint16 ReadSCXIN(uint32 bgIndex) const {
        return bit::extract<8, 18>(normBGParams[bgIndex].scrollAmountH);
    }

    FORCE_INLINE void WriteSCXIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 18>(normBGParams[bgIndex].scrollAmountH, bit::extract<0, 10>(value));
    }

    FORCE_INLINE uint16 ReadSCXDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(normBGParams[bgIndex].scrollAmountH);
    }

    FORCE_INLINE void WriteSCXDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(normBGParams[bgIndex].scrollAmountH, bit::extract<8, 15>(value));
    }

    FORCE_INLINE uint16 ReadSCYIN(uint32 bgIndex) const {
        return bit::extract<8, 18>(normBGParams[bgIndex].scrollAmountV);
    }

    FORCE_INLINE void WriteSCYIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 18>(normBGParams[bgIndex].scrollAmountV, bit::extract<0, 10>(value));
    }

    FORCE_INLINE uint16 ReadSCYDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(normBGParams[bgIndex].scrollAmountV);
    }

    FORCE_INLINE void WriteSCYDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(normBGParams[bgIndex].scrollAmountV, bit::extract<8, 15>(value));
    }

    // 180078   ZMXIN0  NBG0 Horizontal Coordinate Increment (integer part)
    // 18007A   ZMXDN0  NBG0 Horizontal Coordinate Increment (fractional part)
    // 18007C   ZMYIN0  NBG0 Vertical Coordinate Increment (integer part)
    // 18007E   ZMYDN0  NBG0 Vertical Coordinate Increment (fractional part)
    // 180088   ZMXIN1  NBG1 Horizontal Coordinate Increment (integer part)
    // 18008A   ZMXDN1  NBG1 Horizontal Coordinate Increment (fractional part)
    // 18008C   ZMYIN1  NBG1 Vertical Coordinate Increment (integer part)
    // 18008E   ZMYDN1  NBG1 Vertical Coordinate Increment (fractional part)
    //
    // ZMdINx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //   15-3        -             Reserved, must be zero
    //    2-0     W  NxZMdI2-0     Horizontal/Vertical Coordinate Increment (integer part)
    //
    // ZMdDNx:  (d=X,Y; x=0,1)
    //   bits   r/w  code          description
    //   15-8     W  NxZMdD1-8     Horizontal/Vertical Coordinate Increment (fractional part)
    //    7-0        -             Reserved, must be zero

    FORCE_INLINE uint16 ReadZMXIN(uint32 bgIndex) const {
        return bit::extract<8, 10>(normBGParams[bgIndex].scrollIncH);
    }

    FORCE_INLINE void WriteZMXIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 10>(normBGParams[bgIndex].scrollIncH, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadZMXDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(normBGParams[bgIndex].scrollIncH);
    }

    FORCE_INLINE void WriteZMXDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(normBGParams[bgIndex].scrollIncH, bit::extract<8, 15>(value));
    }

    FORCE_INLINE uint16 ReadZMYIN(uint32 bgIndex) const {
        return bit::extract<8, 10>(normBGParams[bgIndex].scrollIncV);
    }

    FORCE_INLINE void WriteZMYIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 10>(normBGParams[bgIndex].scrollIncV, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadZMYDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(normBGParams[bgIndex].scrollIncV);
    }

    FORCE_INLINE void WriteZMYDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(normBGParams[bgIndex].scrollIncV, bit::extract<8, 15>(value));
    }

    ZMCTL_t ZMCTL;   // 180098   ZMCTL   Reduction Enable
    SCRCTL_t SCRCTL; // 18009A   SCRCTL  Line and Vertical Cell Scroll Control
                     // 18009C   VCSTAU  Vertical Cell Scroll Table Address (upper)
    VCSTA_t VCSTA;   // 18009E   VCSTAL  Vertical Cell Scroll Table Address (lower)
                     // 1800A0   LSTA0U  NBG0 Line Cell Scroll Table Address (upper)
    LSTA_t LSTA0;    // 1800A2   LSTA0L  NBG0 Line Cell Scroll Table Address (lower)
                     // 1800A4   LSTA1U  NBG1 Line Cell Scroll Table Address (upper)
    LSTA_t LSTA1;    // 1800A6   LSTA1L  NBG1 Line Cell Scroll Table Address (lower)
                     // 1800A8   LCTAU   Line Color Screen Table Address (upper)
    LCTA_t LCTA;     // 1800AA   LCTAL   Line Color Screen Table Address (lower)
                     // 1800AC   BKTAU   Back Screen Table Address (upper)
    BKTA_t BKTA;     // 1800AE   BKTAL   Back Screen Table Address (lower)
    RPMD_t RPMD;     // 1800B0   RPMD    Rotation Parameter Mode
    RPRCTL_t RPRCTL; // 1800B2   RPRCTL  Rotation Parameter Read Control
    KTCTL_t KTCTL;   // 1800B4   KTCTL   Coefficient Table Control
    KTAOF_t KTAOF;   // 1800B6   KTAOF   Coefficient Table Address Offset
    OVPNR_t OVPNRA;  // 1800B8   OVPNRA  Rotation Parameter A Screen-Over Pattern Name
    OVPNR_t OVPNRB;  // 1800BA   OVPNRB  Rotation Parameter B Screen-Over Pattern Name
                     // 1800BC   RPTAU   Rotation Parameters Table Address (upper)
    RPTA_t RPTA;     // 1800BE   RPTAL   Rotation Parameters Table Address (lower)
                     // 1800C0   WPSX0   Window 0 Horizontal Start Point
                     // 1800C2   WPSY0   Window 0 Vertical Start Point
                     // 1800C4   WPEX0   Window 0 Horizontal End Point
    WPXY_t WPXY0;    // 1800C6   WPEY0   Window 0 Vertical End Point
                     // 1800C8   WPSX1   Window 1 Horizontal Start Point
                     // 1800CA   WPSY1   Window 1 Vertical Start Point
                     // 1800CC   WPEX1   Window 1 Horizontal End Point
    WPXY_t WPXY1;    // 1800CE   WPEY1   Window 1 Vertical End Point
                     // 1800D0   WCTLA   NBG0 and NBG1 Window Control
                     // 1800D2   WCTLB   NBG2 and NBG3 Window Control
                     // 1800D4   WCTLC   RBG0 and Sprite Window Control
    WCTL_t WCTL;     // 1800D6   WCTLD   Rotation Window and Color Calculation Window Control
                     // 1800D8   LWTA0U  Window 0 Line Window Address Table (upper)
    LWTA_t LWTA0;    // 1800DA   LWTA0L  Window 0 Line Window Address Table (lower)
                     // 1800DC   LWTA1U  Window 1 Line Window Address Table (upper)
    LWTA_t LWTA1;    // 1800DE   LWTA1L  Window 1 Line Window Address Table (lower)

    // 1800E0   SPCTL   Sprite Control
    //
    //   bits   r/w  code          description
    //  15-14        -             Reserved, must be zero
    //  13-12     W  SPCCCS1-0     Sprite Color Calculation Condition
    //                               00 (0) = Priority Number <= Color Calculation Number
    //                               01 (1) = Priority Number == Color Calculation Number
    //                               10 (2) = Priority Number >= Color Calculation Number
    //                               11 (3) = Color Data MSB == 1
    //     11        -             Reserved, must be zero
    //   10-8     W  SPCCN2-0      Color Calculation Number
    //    7-6        -             Reserved, must be zero
    //      5     W  SPCLMD        Sprite Color Format Data (0=palette only, 1=palette and RGB)
    //      4     W  SPWINEN       Sprite Window Enable (0=disable, 1=enable)
    //    3-0     W  SPTYPE3-0     Sprite Type (0,1,2,...,D,E,F)

    FORCE_INLINE uint16 ReadSPCTL() const {
        uint16 value = 0;
        bit::deposit_into<0, 3>(value, spriteParams.type);
        bit::deposit_into<4>(value, spriteParams.windowEnable);
        bit::deposit_into<5>(value, spriteParams.mixedFormat);
        bit::deposit_into<8, 10>(value, spriteParams.colorCalcValue);
        bit::deposit_into<12, 13>(value, static_cast<uint16>(spriteParams.colorCalcCond));
        return value;
    }

    FORCE_INLINE void WriteSPCTL(uint16 value) {
        spriteParams.type = bit::extract<0, 3>(value);
        spriteParams.windowEnable = bit::extract<4>(value);
        spriteParams.mixedFormat = bit::extract<5>(value);
        spriteParams.colorCalcValue = bit::extract<8, 10>(value);
        spriteParams.colorCalcCond = static_cast<SpriteColorCalculationCondition>(bit::extract<12, 13>(value));
    }

    SDCTL_t SDCTL; // 1800E2   SDCTL   Shadow Control

    // 1800E4   CRAOFA  NBG0-NBG3 Color RAM Address Offset
    //
    //   bits   r/w  code          description
    //     15        -             Reserved, must be zero
    //  14-12     W  N3CAOS2-0     NBG3 Color RAM Adress Offset
    //     11        -             Reserved, must be zero
    //   10-8     W  N2CAOS2-0     NBG2 Color RAM Adress Offset
    //      7        -             Reserved, must be zero
    //    6-4     W  N1CAOS2-0     NBG1/EXBG Color RAM Adress Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  N0CAOS2-0     NBG0/RBG1 Color RAM Adress Offset

    FORCE_INLINE uint16 ReadCRAOFA() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, normBGParams[0].caos);
        bit::deposit_into<4, 6>(value, normBGParams[1].caos);
        bit::deposit_into<8, 10>(value, normBGParams[2].caos);
        bit::deposit_into<12, 14>(value, normBGParams[3].caos);
        return value;
    }

    FORCE_INLINE void WriteCRAOFA(uint16 value) {
        normBGParams[0].caos = bit::extract<0, 2>(value);
        normBGParams[1].caos = bit::extract<4, 6>(value);
        normBGParams[2].caos = bit::extract<8, 10>(value);
        normBGParams[3].caos = bit::extract<12, 14>(value);
        rotBGParams[0].caos = normBGParams[0].caos;
    }

    // 1800E6   CRAOFB  RBG0 and Sprite Color RAM Address Offset
    //
    //   bits   r/w  code          description
    //   15-7        -             Reserved, must be zero
    //    6-4     W  SPCAOS2-0     Sprite Color RAM Adress Offset
    //      3        -             Reserved, must be zero
    //    2-0     W  R0CAOS2-0     RBG0 Color RAM Adress Offset

    FORCE_INLINE uint16 ReadCRAOFB() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, rotBGParams[0].caos);
        bit::deposit_into<4, 6>(value, spriteParams.colorDataOffset >> 8u);
        return value;
    }

    FORCE_INLINE void WriteCRAOFB(uint16 value) {
        rotBGParams[0].caos = bit::extract<0, 2>(value);
        spriteParams.colorDataOffset = bit::extract<4, 6>(value) << 8u;
    }

    // 1800E8   LNCLEN  Line Color Screen Enable
    //
    //   bits   r/w  code          description
    //   15-6        -             Reserved, must be zero
    //      5     W  SPLCEN        Sprite Line Color Screen Enable
    //      4     W  R0LCEN        RBG0 Line Color Screen Enable
    //      3     W  N3LCEN        NBG3 Line Color Screen Enable
    //      2     W  N2LCEN        NBG2 Line Color Screen Enable
    //      1     W  N1LCEN        NBG1 Line Color Screen Enable
    //      0     W  N0LCEN        NBG0 Line Color Screen Enable

    FORCE_INLINE uint16 ReadLNCLEN() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, normBGParams[0].lineColorScreenEnable);
        bit::deposit_into<1>(value, normBGParams[1].lineColorScreenEnable);
        bit::deposit_into<2>(value, normBGParams[2].lineColorScreenEnable);
        bit::deposit_into<3>(value, normBGParams[3].lineColorScreenEnable);
        bit::deposit_into<4>(value, rotBGParams[0].lineColorScreenEnable);
        // TODO: bit::deposit_into<5>(value, ?m_SpriteParams?.lineColorScreenEnable);
        return value;
    }

    FORCE_INLINE void WriteLNCLEN(uint16 value) {
        normBGParams[0].lineColorScreenEnable = bit::extract<0>(value);
        normBGParams[1].lineColorScreenEnable = bit::extract<1>(value);
        normBGParams[2].lineColorScreenEnable = bit::extract<2>(value);
        normBGParams[3].lineColorScreenEnable = bit::extract<3>(value);
        rotBGParams[0].lineColorScreenEnable = bit::extract<4>(value);
        rotBGParams[1].lineColorScreenEnable = normBGParams[0].lineColorScreenEnable;
        // TODO: ?m_SpriteParams?.lineColorScreenEnable = bit::extract<5>(value);
    }

    // 1800EA   SFPRMD  Special Priority Mode
    //
    //   bits   r/w  code          description
    //  15-10        -             Reserved, must be zero
    //    9-8     W  R0SPRM1-0     RBG0 Special Priority Mode
    //    7-6     W  N3SPRM1-0     NBG3 Special Priority Mode
    //    5-4     W  N2SPRM1-0     NBG2 Special Priority Mode
    //    3-2     W  N1SPRM1-0     NBG1/EXBG Special Priority Mode
    //    1-0     W  N0SPRM1-0     NBG0/RBG1 Special Priority Mode
    //
    // For all parameters, use LSB of priority number:
    //   00 (0) = per screen
    //   01 (1) = per character
    //   10 (2) = per pixel
    //   11 (3) = (forbidden)

    FORCE_INLINE uint16 ReadSFPRMD() const {
        uint16 value = 0;
        bit::deposit_into<0, 1>(value, static_cast<uint32>(normBGParams[0].priorityMode));
        bit::deposit_into<2, 3>(value, static_cast<uint32>(normBGParams[1].priorityMode));
        bit::deposit_into<4, 5>(value, static_cast<uint32>(normBGParams[2].priorityMode));
        bit::deposit_into<6, 7>(value, static_cast<uint32>(normBGParams[3].priorityMode));
        bit::deposit_into<8, 9>(value, static_cast<uint32>(rotBGParams[0].priorityMode));
        return value;
    }

    FORCE_INLINE void WriteSFPRMD(uint16 value) {
        normBGParams[0].priorityMode = static_cast<PriorityMode>(bit::extract<0, 1>(value));
        normBGParams[1].priorityMode = static_cast<PriorityMode>(bit::extract<2, 3>(value));
        normBGParams[2].priorityMode = static_cast<PriorityMode>(bit::extract<4, 5>(value));
        normBGParams[3].priorityMode = static_cast<PriorityMode>(bit::extract<6, 7>(value));
        rotBGParams[0].priorityMode = static_cast<PriorityMode>(bit::extract<8, 9>(value));
        rotBGParams[1].priorityMode = normBGParams[0].priorityMode;
    }

    CCCTL_t CCCTL;   // 1800EC   CCCTL   Color Calculation Control
    SFCCMD_t SFCCMD; // 1800EE   SFCCMD  Special Color Calculation Mode

    // 1800F0   PRISA   Sprite 0 and 1 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  S1PRIN2-0     Sprite 1 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  S0PRIN2-0     Sprite 0 Priority Number
    //
    // 1800F2   PRISB   Sprite 2 and 3 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  S3PRIN2-0     Sprite 3 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  S3PRIN2-0     Sprite 2 Priority Number
    //
    // 1800F4   PRISC   Sprite 4 and 5 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  S5PRIN2-0     Sprite 5 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  S4PRIN2-0     Sprite 4 Priority Number
    //
    // 1800F6   PRISD   Sprite 6 and 7 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  S7PRIN2-0     Sprite 7 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  S6PRIN2-0     Sprite 6 Priority Number

    FORCE_INLINE uint16 ReadPRISn(uint32 offset) const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, spriteParams.priorities[offset * 2 + 0]);
        bit::deposit_into<8, 10>(value, spriteParams.priorities[offset * 2 + 1]);
        return value;
    }

    FORCE_INLINE void WritePRISn(uint32 offset, uint16 value) {
        spriteParams.priorities[offset * 2 + 0] = bit::extract<0, 2>(value);
        spriteParams.priorities[offset * 2 + 1] = bit::extract<8, 10>(value);
    }

    // 1800F8   PRINA   NBG0 and NBG1 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  N1PRIN2-0     NBG1 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  N0PRIN2-0     NBG0/RBG1 Priority Number

    FORCE_INLINE uint16 ReadPRINA() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, normBGParams[0].priorityNumber);
        bit::deposit_into<8, 10>(value, normBGParams[1].priorityNumber);
        return value;
    }

    FORCE_INLINE void WritePRINA(uint16 value) {
        normBGParams[0].priorityNumber = bit::extract<0, 2>(value);
        normBGParams[1].priorityNumber = bit::extract<8, 10>(value);
        rotBGParams[1].priorityNumber = normBGParams[0].priorityNumber;
    }

    // 1800FA   PRINB   NBG2 and NBG3 Priority Number
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  N3PRIN2-0     NBG3 Priority Number
    //    7-3        -             Reserved, must be zero
    //    2-0     W  N2PRIN2-0     NBG2 Priority Number

    FORCE_INLINE uint16 ReadPRINB() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, normBGParams[2].priorityNumber);
        bit::deposit_into<8, 10>(value, normBGParams[3].priorityNumber);
        return value;
    }

    FORCE_INLINE void WritePRINB(uint16 value) {
        normBGParams[2].priorityNumber = bit::extract<0, 2>(value);
        normBGParams[3].priorityNumber = bit::extract<8, 10>(value);
    }

    // 1800FC   PRIR    RBG0 Priority Number
    //
    //   bits   r/w  code          description
    //   15-3        -             Reserved, must be zero
    //    2-0     W  R0PRIN2-0     RBG0 Priority Number

    FORCE_INLINE uint16 ReadPRIR() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, rotBGParams[0].priorityNumber);
        return value;
    }

    FORCE_INLINE void WritePRIR(uint16 value) {
        rotBGParams[0].priorityNumber = bit::extract<0, 2>(value);
    }

    // 1800FE   -       Reserved

    // 180100   CCRSA   Sprite 0 and 1 Color Calculation Ratio
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //   12-8     W  S1CCRT4-0     Sprite Register 1 Color Calculation Ratio
    //    7-5        -             Reserved, must be zero
    //    4-0     W  S0CCRT4-0     Sprite Register 0 Color Calculation Ratio
    //
    // 180102   CCRSB   Sprite 2 and 3 Color Calculation Ratio
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //   12-8     W  S3CCRT4-0     Sprite Register 3 Color Calculation Ratio
    //    7-5        -             Reserved, must be zero
    //    4-0     W  S2CCRT4-0     Sprite Register 2 Color Calculation Ratio
    //
    // 180104   CCRSC   Sprite 4 and 5 Color Calculation Ratio
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //   12-8     W  S5CCRT4-0     Sprite Register 5 Color Calculation Ratio
    //    7-5        -             Reserved, must be zero
    //    4-0     W  S4CCRT4-0     Sprite Register 4 Color Calculation Ratio
    //
    // 180106   CCRSD   Sprite 6 and 7 Color Calculation Ratio
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //   12-8     W  S7CCRT4-0     Sprite Register 7 Color Calculation Ratio
    //    7-5        -             Reserved, must be zero
    //    4-0     W  S6CCRT4-0     Sprite Register 6 Color Calculation Ratio

    FORCE_INLINE uint16 ReadCCRSn(uint32 offset) const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, spriteParams.colorCalcRatios[offset * 2 + 0]);
        bit::deposit_into<8, 10>(value, spriteParams.colorCalcRatios[offset * 2 + 1]);
        return value;
    }

    FORCE_INLINE void WriteCCRSn(uint32 offset, uint16 value) {
        spriteParams.colorCalcRatios[offset * 2 + 0] = bit::extract<0, 2>(value);
        spriteParams.colorCalcRatios[offset * 2 + 1] = bit::extract<8, 10>(value);
    }

    CCR_t CCRNA;     // 180108   CCRNA   NBG0 and NBG1 Color Calculation Ratio
    CCR_t CCRNB;     // 18010A   CCRNB   NBG2 and NBG3 Color Calculation Ratio
    CCR_t CCRR;      // 18010C   CCRR    RBG0 Color Calculation Ratio
    CCR_t CCRLB;     // 18010E   CCRLB   Line Color Screen and Back Screen Color Calculation Ratio
    CLOFEN_t CLOFEN; // 180110   CLOFEN  Color Offset Enable
    CLOFSL_t CLOFSL; // 180112   CLOFSL  Color Offset Select
    CO_t COAR;       // 180114   COAR    Color Offset A - Red
    CO_t COAG;       // 180116   COAG    Color Offset A - Green
    CO_t COAB;       // 180118   COAB    Color Offset A - Blue
    CO_t COBR;       // 18011A   COBR    Color Offset B - Red
    CO_t COBG;       // 18011C   COBG    Color Offset B - Green
    CO_t COBB;       // 18011E   COBB    Color Offset B - Blue

    // -------------------------------------------------------------------------

    // Indicates if TVMD has changed.
    // The screen resolution is updated on VBlank.
    bool TVMDDirty;

    std::array<NormBGParams, 4> normBGParams;
    std::array<RotBGParams, 2> rotBGParams;
    SpriteParams spriteParams;

    std::array<SpecialFunctionCodes, 2> specialFunctionCodes;
};

} // namespace satemu::vdp

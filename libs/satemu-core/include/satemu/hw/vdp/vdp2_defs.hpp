#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/data_ops.hpp>

#include <array>
#include <span>

namespace satemu::vdp {

// Character color formats
enum class ColorFormat : uint8 {
    Palette16,
    Palette256,
    Palette2048,
    RGB555,
    RGB888,
};

inline constexpr bool IsPaletteColorFormat(ColorFormat format) {
    using enum ColorFormat;
    return format == Palette16 || format == Palette256 || format == Palette2048;
}

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

enum class SpecialColorCalcMode : uint8 {
    PerScreen,
    PerCharacter,
    PerDot,
    ColorDataMSB,
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

// NBG and RBG parameters.
struct BGParams {
    BGParams() {
        Reset();
    }

    void Reset() {
        enabled = false;
        enableTransparency = false;
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

        verticalCellScrollEnable = false;
        lineScrollXEnable = false;
        lineScrollYEnable = false;
        lineZoomEnable = false;
        lineScrollInterval = 0;
        lineScrollTableAddress = 0;

        mosaicEnable = false;

        colorOffsetEnable = false;
        colorOffsetSelect = false;

        colorCalcEnable = false;
        colorCalcRatio = 0;
        specialColorCalcMode = SpecialColorCalcMode::PerScreen;

        plsz = 0;
        bmsz = 0;
        caos = 0;
    }

    // Whether to display this background.
    // Derived from BGON.xxON
    bool enabled;

    // If true, honor transparency bit in color data.
    // Derived from BGON.xxTPON
    bool enableTransparency;

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
    std::array<uint16, 16> mapIndices;

    // Page base addresses for NBG planes A-D or RBG planes A-P.
    // Derived from mapIndices, CHCTLA/CHCTLB.xxCHSZ, PNCNn/PNCR.xxPNB and PLSZ.xxPLSZn
    std::array<uint32, 16> pageBaseAddresses;

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

    // Whether to use the vertical cell scroll table in VRAM.
    // Only valid for NBG0 and NBG1.
    // Derived from SCRCTL.NnVCSC
    bool verticalCellScrollEnable;

    // Whether to use the horizontal line scroll table in VRAM.
    // Only valid for NBG0 and NBG1.
    // Derived from SCRCTL.NnLSCX
    bool lineScrollXEnable;

    // Whether to use the vertical line scroll table in VRAM.
    // Only valid for NBG0 and NBG1.
    // Derived from SCRCTL.NnLSCY
    bool lineScrollYEnable;

    // Whether to use horizontal line zoom/scaling.
    // Only valid for NBG0 and NBG1.
    // Derived from SCRCTL.NnLZMX
    bool lineZoomEnable;

    // Line scroll table interval shift. The interval is calculated as (1 << lineScrollInterval).
    // Only valid for NBG0 and NBG1.
    // Derived from SCRCTL.NnLSS1-0
    uint8 lineScrollInterval;

    // Line scroll table base address.
    // Only valid for NBG0 and NBG1.
    // Derived from LSTAnU/L
    uint32 lineScrollTableAddress;

    // Enables the mosaic effect.
    // If vertical cell scroll is also enabled, the mosaic effect is bypassed.
    // Derived from MZCTL.xxMZE
    bool mosaicEnable;

    // Enables the color offset effect.
    // Derived from CLOFEN.xxCOEN
    bool colorOffsetEnable;

    // Selects the color offset parameters to use: A (false) or B (true).
    // Derived from CLOFEN.xxCOSL
    bool colorOffsetSelect;

    // Enables color calculation.
    // Derived from CCCTL.xxCCEN
    bool colorCalcEnable;

    // Color calculation ratio, ranging from 31:1 to 0:32.
    // The ratio is calculated as (32-colorCalcRatio) : (colorCalcRatio).
    // Derived from CCRNA/B.NxCCRTn
    uint8 colorCalcRatio;

    // Special color calculation mode.
    // Derived from SFCCMD.xxSCCMn
    SpecialColorCalcMode specialColorCalcMode;

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

enum class RotationParamMode : uint8 { RotationParamA, RotationParamB, Coefficient, Window };

struct CommonRotationParams {
    CommonRotationParams() {
        Reset();
    }

    void Reset() {
        baseAddress = 0;
        rotParamMode = RotationParamMode::RotationParamA;
    }

    uint32 baseAddress;
    RotationParamMode rotParamMode;
};

enum class CoefficientDataMode : uint8 { ScaleCoeffXY, ScaleCoeffX, ScaleCoeffY, ViewpointX };

// Rotation Parameter A/B
struct RotationParams {
    RotationParams() {
        Reset();
    }

    void Reset() {
        readXst = false;
        readYst = false;
        readKAst = false;
        coeffTableEnable = false;
        coeffDataSize = 0;
        coeffUseLineColorData = false;
        coeffTableAddressOffset = 0;
        screenOverPatternName = 0;
    }

    // Read Xst on the next scanline. Automatically cleared when read.
    // Derived from RPRCTL.RxXSTRE
    bool readXst;

    // Read Yst on the next scanline. Automatically cleared when read.
    // Derived from RPRCTL.RxYSTRE
    bool readYst;

    // Read KAst on the next scanline. Automatically cleared when read.
    // Derived from RPRCTL.RxKASTRE
    bool readKAst;

    // Enable use of the coefficient table.
    // Derived from KTCTL.RxKTE
    bool coeffTableEnable;

    // Size of coefficient data: 1 word (0) or 2 words (1).
    // Derived from KTCTL.RxKDBS
    uint8 coeffDataSize;

    // Coefficient data mode.
    // Derived from KTCTL.RxKMD1-0
    CoefficientDataMode coeffDataMode = CoefficientDataMode::ScaleCoeffXY;

    // Enables use of line color data within coefficient data.
    // Derived from KTCTL.RxKLCE
    bool coeffUseLineColorData;

    // Coefficient table address offset.
    // Derived from KTAOF.RxKTAOS2-0
    uint32 coeffTableAddressOffset;

    // Screen-over pattern name value.
    // Derived from OVPNRA/B
    uint16 screenOverPatternName;
};

struct RotationParamTable {
    void ReadFrom(std::span<uint8> input) {
        Xst = bit::extract_signed<6, 28>(util::ReadBE<uint32>(&input[0x00]));
        Yst = bit::extract_signed<6, 28>(util::ReadBE<uint32>(&input[0x04]));
        Zst = bit::extract_signed<6, 28>(util::ReadBE<uint32>(&input[0x08]));

        dXst = bit::extract_signed<6, 18>(util::ReadBE<uint32>(&input[0x0C]));
        dYst = bit::extract_signed<6, 18>(util::ReadBE<uint32>(&input[0x10]));

        dX = bit::extract_signed<6, 18>(util::ReadBE<uint32>(&input[0x14]));
        dY = bit::extract_signed<6, 18>(util::ReadBE<uint32>(&input[0x18]));

        A = bit::extract_signed<6, 19>(util::ReadBE<uint32>(&input[0x1C]));
        B = bit::extract_signed<6, 19>(util::ReadBE<uint32>(&input[0x20]));
        C = bit::extract_signed<6, 19>(util::ReadBE<uint32>(&input[0x24]));
        D = bit::extract_signed<6, 19>(util::ReadBE<uint32>(&input[0x28]));
        E = bit::extract_signed<6, 19>(util::ReadBE<uint32>(&input[0x2C]));
        F = bit::extract_signed<6, 19>(util::ReadBE<uint32>(&input[0x30]));

        Px = bit::extract_signed<0, 13>(util::ReadBE<uint16>(&input[0x34]));
        Py = bit::extract_signed<0, 13>(util::ReadBE<uint16>(&input[0x36]));
        Pz = bit::extract_signed<0, 13>(util::ReadBE<uint16>(&input[0x38]));

        Cx = bit::extract_signed<0, 13>(util::ReadBE<uint16>(&input[0x3C]));
        Cy = bit::extract_signed<0, 13>(util::ReadBE<uint16>(&input[0x3E]));
        Cz = bit::extract_signed<0, 13>(util::ReadBE<uint16>(&input[0x40]));

        Mx = bit::extract_signed<6, 29>(util::ReadBE<uint32>(&input[0x44]));
        My = bit::extract_signed<6, 29>(util::ReadBE<uint32>(&input[0x48]));

        kx = bit::extract_signed<0, 24>(util::ReadBE<uint32>(&input[0x4C]));
        ky = bit::extract_signed<0, 24>(util::ReadBE<uint32>(&input[0x50]));

        KAst = bit::extract<6, 31>(util::ReadBE<uint32>(&input[0x54]));
        dKAst = bit::extract_signed<6, 25>(util::ReadBE<uint32>(&input[0x58]));
        dKAx = bit::extract_signed<6, 25>(util::ReadBE<uint32>(&input[0x5C]));
    }

    // Screen start coordinates (signed 13.10 fixed point)
    sint32 Xst;
    sint32 Yst;
    sint32 Zst;

    // Screen vertical coordinate increments (signed 3.10 fixed point)
    sint32 dXst;
    sint32 dYst;

    // Screen horizontal coordinate increments (signed 3.10 fixed point)
    sint32 dX;
    sint32 dY;

    // Rotation matrix parameters (signed 4.10 fixed point)
    sint32 A;
    sint32 B;
    sint32 C;
    sint32 D;
    sint32 E;
    sint32 F;

    // Viewpoint coordinates (signed 14-bit integer)
    sint16 Px;
    sint16 Py;
    sint16 Pz;

    // Center point coordinates (signed 14-bit integer)
    sint16 Cx;
    sint16 Cy;
    sint16 Cz;

    // Horizontal shift (signed 14.10 fixed point)
    sint32 Mx;
    sint32 My;

    // Scaling coefficients (signed 8.16 fixed point)
    sint32 kx;
    sint32 ky;

    // Coefficient table parameters
    uint32 KAst;  // Coefficient table start address (unsigned 16.10 fixed point)
    sint32 dKAst; // Coefficient table vertical increment (signed 10.10 fixed point)
    sint32 dKAx;  // Coefficient table horizontal increment (signed 10.10 fixed point)
};

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
        colorCalcEnable = false;
        colorCalcValue = 0;
        colorCalcCond = SpriteColorCalculationCondition::PriorityLessThanOrEqual;
        priorities.fill(0);
        colorCalcRatios.fill(0);
        colorDataOffset = 0;
        colorOffsetEnable = false;
        colorOffsetSelect = false;
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

    // Enables color calculation.
    // Derived from CCCTL.SPCCEN
    bool colorCalcEnable;

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
    // The ratio is calculated as (32-colorCalcRatio) : (colorCalcRatio).
    // Derived from CCRSA, CCRSB, CCRSC and CCRSD.
    std::array<uint8, 8> colorCalcRatios;

    // Sprite color data offset.
    // Derived from CRAOFB.SPCAOSn
    uint32 colorDataOffset;

    // Enables the color offset effect.
    // Derived from CLOFEN.SPCOEN
    bool colorOffsetEnable;

    // Selects the color offset parameters to use: A (false) or B (true).
    // Derived from CLOFEN.SPCOSL
    bool colorOffsetSelect;
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

struct LineBackScreenParams {
    LineBackScreenParams() {
        Reset();
    }

    void Reset() {
        perLine = false;
        baseAddress = 0;
        colorOffsetEnable = false;
        colorOffsetSelect = false;
        colorCalcEnable = false;
        colorCalcRatio = 0;
    }

    // Whether the line/back screen specifies a color for the whole screen (false) or per line (true).
    // Derived from LCTAU.LCCLMD or BKTAU.BKCLMD
    bool perLine;

    // Base address of line/back screen data.
    // Derived from LCTAU/L.LCTA18-0 or BKTAU/L.BKTA18-0
    uint32 baseAddress;

    // Enables the color offset effect.
    // Only valid for the back screen.
    // Derived from CLOFEN.BKCOEN
    bool colorOffsetEnable;

    // Selects the color offset parameters to use: A (false) or B (true).
    // Only valid for the back screen.
    // Derived from CLOFEN.BKCOSL
    bool colorOffsetSelect;

    // Enables color calculation.
    // Derived from CCCTL.LCCCEN
    bool colorCalcEnable;

    // Color calculation ratio, ranging from 31:1 to 0:32.
    // The ratio is calculated as (32-colorCalcRatio) : (colorCalcRatio).
    // Derived from CCRNA/B.NxCCRTn
    uint8 colorCalcRatio;
};

struct ColorOffsetParams {
    ColorOffsetParams() {
        Reset();
    }

    void Reset() {
        r = 0;
        g = 0;
        b = 0;
    }

    // Color offset values as signed 9-bit integers.
    // Derived from COAR/G/B and COBR/G/B
    sint16 r;
    sint16 g;
    sint16 b;
};

enum class ColorGradScreen : uint8 { Sprite, RBG0, NBG0_RBG1, Invalid3, NBG1_EXBG, NBG2, NBG3, Invalid7 };

struct ColorCalcParams {
    ColorCalcParams() {
        Reset();
    }

    void Reset() {
        colorGradEnable = false;
        colorGradScreen = ColorGradScreen::Sprite;
        extendedColorCalcEnable = false;
        useSecondScreenRatio = false;
        useAdditiveBlend = false;
    }

    // Enables color gradation.
    // Derived from CCCTL.BOKEN
    bool colorGradEnable;

    // Which screen to apply the color gradation function.
    // Derived from CCCTL.BOKN2-0
    ColorGradScreen colorGradScreen;

    // Enables extended color calculation.
    // Derived from CCCTL.EXCCEN
    bool extendedColorCalcEnable;

    // Use the ratio from the first (false) or second (true) topmost screen.
    // Derived from CCCTL.CCRTMD
    bool useSecondScreenRatio;

    // Whether to use alpha (false) or additive (true) blending.
    // Derived from CCCTL.CCMD
    bool useAdditiveBlend;
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
//                               110 (6) = 640 pixels - Exclusive Hi-Res Graphic A (31 KHz monitor)
//                               111 (7) = 704 pixels - Exclusive Hi-Res Graphic B (Hi-Vision monitor)
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

// 18000E   RAMCTL  RAM Control
//
//   bits   r/w  code          description
//     15   R/W  CRKTE         Color RAM Coefficient Table Enable
//                               If enabled, Color RAM Mode should be set to 01
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
//    1-0   R/W  RDBSA0(1-0)   Rotation Data Bank Select for VRAM-A0 (or VRAM-A)
//
// RDBSxn(1-0):
//   00 (0) = bank not used by rotation backgrounds
//   01 (1) = bank used for coefficient table
//   10 (2) = bank used for pattern name table
//   11 (3) = bank used for character/bitmap pattern table
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

} // namespace satemu::vdp

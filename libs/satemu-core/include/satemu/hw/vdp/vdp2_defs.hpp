#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/size_ops.hpp>

#include <array>

namespace satemu::vdp2 {

constexpr size_t kVDP2VRAMSize = 512_KiB;
constexpr size_t kCRAMSize = 4_KiB;

// Character color formats
enum class ColorFormat {
    Palette16,
    Palette256,
    Palette2048,
    RGB555,
    RGB888,
};

// Rotation BG screen-over process
enum class ScreenOverProcess {
    Repeat,
    RepeatChar,
    Transparent,
    Fixed512,
};

// Special priority modes
enum class PriorityMode {
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

        priorityNumber = 0;
        priorityMode = PriorityMode::PerScreen;

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

    // Priority number from 0 (transparent) to 7 (highest).
    // Derived from PRINA/PRINB/PRIR.xxPRINn
    uint8 priorityNumber;

    // Special priority mode for scroll screens.
    // Derived from SFPRMD.xxSPRMn
    PriorityMode priorityMode;

    // Cell size shift corresponding to the dimensions of a character pattern (0=1x1, 1=2x2).
    // Derived from CHCTLA/CHCTLB.xxCHSZ
    uint32 cellSizeShift;

    // Page shifts are either 0 or 1, used when determining which plane a particular (x,y) coordinate belongs to.
    // A shift of 0 corresponds to 1 page per plane dimension.
    // A shift of 1 corresponds to 2 pages per plane dimension.
    uint32 pageShiftH; // Horizontal page shift, derived from PLSZ.xxPLSZn
    uint32 pageShiftV; // Vertical page shift, derived from PLSZ.xxPLSZn

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

// 180024   SFSEL   Special Function Code Select
//
//   bits   r/w  code          description
//   15-5        -             Reserved, must be zero
//      4     W  R0SFCS        RBG0 Special Function Code Select
//      3     W  N3SFCS        NBG3 Special Function Code Select
//      2     W  N2SFCS        NBG2 Special Function Code Select
//      1     W  N1SFCS        NBG1 Special Function Code Select
//      0     W  N0SFCS        NBG0/RBG1 Special Function Code Select
union SFSEL_t {
    uint16 u16;
    struct {
        uint16 N0SFCS : 1;
        uint16 N1SFCS : 1;
        uint16 N2SFCS : 1;
        uint16 N3SFCS : 1;
        uint16 R0SFCS : 1;
        uint16 _rsvd5_15 : 11;
    };
};

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
union SFCODE_t {
    uint16 u16;
    struct {
        uint8 SFCDBn;
        uint8 SFCDAn;
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
union SPCTL_t {
    uint16 u16;
    struct {
        uint16 SPTYPEn : 4;
        uint16 SPWINEN : 1;
        uint16 SPCLMD : 1;
        uint16 _rsvd6_7 : 2;
        uint16 SPCCNn : 3;
        uint16 SPCCCSn : 2;
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
union LNCLEN_t {
    uint16 u16;
    struct {
        uint16 N0LCEN : 1;
        uint16 N1LCEN : 1;
        uint16 N2LCEN : 1;
        uint16 N3LCEN : 1;
        uint16 R0LCEN : 1;
        uint16 SPLCEN : 1;
        uint16 _rsvd6_15 : 10;
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
union PRI_t {
    uint16 u16;
    struct {
        uint16 lPRINn : 3; // (NA) NBG0, (NB) NBG2, (R) RBG0
        uint16 _rsvd3_7 : 5;
        uint16 uPRINn : 3; // (NA) NBG1, (NB) NBG3, (R) reserved
        uint16 _rsvd11_15 : 5;
    };
};

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
union CCRS_t {
    uint16 u16;
    struct {
        uint16 lCCRTn : 5; // (A) Sprite 0, (B) Sprite 2, (C) Sprite 4, (D) Sprite 6
        uint16 _rsvd5_7 : 3;
        uint16 uCCRTn : 5; // (A) Sprite 1, (B) Sprite 3, (C) Sprite 5, (D) Sprite 7
        uint16 _rsvd13_15 : 3;
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

} // namespace satemu::vdp2

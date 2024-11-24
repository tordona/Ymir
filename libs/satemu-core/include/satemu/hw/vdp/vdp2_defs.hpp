#pragma once

#include <satemu/core_types.hpp>

#include <satemu/util/size_ops.hpp>

namespace satemu::vdp2 {

constexpr size_t kVDP2VRAMSize = 512_KiB;
constexpr size_t kCRAMSize = 4_KiB;

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
//  15-12     W  VCP0B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T0
//   11-8     W  VCP1B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T1
//    7-4     W  VCP2B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T2
//    3-0     W  VCP3B0(3-0)   VRAM-b0 (or VRAM-B) Timing for T3
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
union BGON_t {
    uint16 u16;
    struct {
        uint16 N0ON : 1;
        uint16 N1ON : 1;
        uint16 N2ON : 1;
        uint16 N3ON : 1;
        uint16 R0ON : 1;
        uint16 R1ON : 1;
        uint16 _rsvd6_7 : 2;
        uint16 N0TPON : 1;
        uint16 N1TPON : 1;
        uint16 N2TPON : 1;
        uint16 N3TPON : 1;
        uint16 R0TPON : 1;
        uint16 _rsvd15 : 1;
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
union CHCTLA_t {
    uint16 u16;
    struct {
        uint16 N0CHSZ : 1;
        uint16 N0BMEN : 1;
        uint16 N0BMSZn : 2;
        uint16 N0CHCNn : 3;
        uint16 _rsvd7 : 1;
        uint16 N1CHSZ : 1;
        uint16 N1BMEN : 1;
        uint16 N1BMSZn : 2;
        uint16 N1CHCNn : 2;
        uint16 _rsvd14_15 : 2;
    };
};

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
union CHCTLB_t {
    uint16 u16;
    struct {
        uint16 N2CHSZ : 1;
        uint16 N2CHCN : 1;
        uint16 _rsvd2_3 : 2;
        uint16 N3CHSZ : 1;
        uint16 N3CHCN : 1;
        uint16 _rsvd6_7 : 2;
        uint16 R0CHSZ : 1;
        uint16 R0BMEN : 1;
        uint16 R0BMSZ : 1;
        uint16 _rsvd11 : 1;
        uint16 R0CHCNn : 3;
        uint16 _rsvd15 : 1;
    };
};

// 18002C   BMPNA   NBG0/NBG1 Bitmap Palette Number
//
//   bits   r/w  code          description
//  15-14        -             Reserved, must be zero
//     13     W  N1BMPR        NBG1 Special Priority
//     12     W  N1BMCC        NBG1 Special Color Calculation
//     11        -             Reserved, must be zero
//   10-8     W  N1BMP6-4      -
//    7-6        -             Reserved, must be zero
//      5     W  N0BMPR        NBG0 Special Priority
//      4     W  N0BMCC        NBG0 Special Color Calculation
//      3        -             Reserved, must be zero
//    2-0     W  N0BMP6-4      -
union BMPNA_t {
    uint16 u16;
    struct {
        uint16 N0BMPn : 3;
        uint16 _rsvd3 : 1;
        uint16 N0BMCC : 1;
        uint16 N0BMPR : 1;
        uint16 _rsvd6_7 : 2;
        uint16 N1BMPn : 3;
        uint16 _rsvd11 : 1;
        uint16 N1BMCC : 1;
        uint16 N1BMPR : 1;
        uint16 _rsvd14_15 : 2;
    };
};

// 18002E   BMPNB   RBG0 Bitmap Palette Number
//
//   bits   r/w  code          description
//   15-6        -             Reserved, must be zero
//      5     W  R0BMPR        RBG0 Special Priority
//      4     W  R0BMCC        RBG0 Special Color Calculation
//      3        -             Reserved, must be zero
//    2-0     W  R0BMP6-4      -
union BMPNB_t {
    uint16 u16;
    struct {
        uint16 N0BMPn : 3;
        uint16 _rsvd3 : 1;
        uint16 N0BMCC : 1;
        uint16 N0BMPR : 1;
        uint16 _rsvd6_15 : 10;
    };
};

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
//    7-5     W  xxSPLT6-4     Supplementary Palette bits
//    4-0     W  xxSCN4-0      Supplementary Character Number bits
union PNC_t {
    uint16 u16;
    struct {
        uint16 SCNn : 5;
        uint16 SPLTn : 3;
        uint16 SCC : 1;
        uint16 SPR : 1;
        uint16 _rsvd : 4;
        uint16 CNSM : 1;
        uint16 PNB : 1;
    };
};

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
union PLSZ_t {
    uint16 u16;
    struct {
        uint16 N0PLSZn : 2;
        uint16 N1PLSZn : 2;
        uint16 N2PLSZn : 2;
        uint16 N3PLSZn : 2;
        uint16 RAPLSZn : 2;
        uint16 RAOVRn : 2;
        uint16 RBPLSZn : 2;
        uint16 RBOVRn : 2;
    };
};

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
union MPOFN_t {
    uint16 u16;
    struct {
        uint16 M0MPn : 3;
        uint16 _rsvd3 : 1;
        uint16 M1MPn : 3;
        uint16 _rsvd7 : 1;
        uint16 M2MPn : 3;
        uint16 _rsvd11 : 1;
        uint16 M3MPn : 3;
        uint16 _rsvd15 : 1;
    };
};

// 18003E   MPOFR   Rotation Parameter A/B Map Offset
//
//   bits   r/w  code          description
//   15-7        -             Reserved, must be zero
//    6-4     W  RBMP8-6       Rotation Parameter B Map Offset
//      3        -             Reserved, must be zero
//    2-0     W  RAMP8-6       Rotation Parameter A Map Offset
union MPOFR_t {
    uint16 u16;
    struct {
        uint16 RAMPn : 3;
        uint16 _rsvd3 : 1;
        uint16 RBMPn : 3;
        uint16 _rsvd7_15 : 9;
    };
};

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
union MPBG_t {
    uint32 u32;

    struct {
        uint8 param : 6;
        uint8 _rsvd : 2;
    } arr[4];

    struct {
        union {
            uint16 u16;
            struct {
                uint16 MPAn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPBn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } AB;
        union {
            uint16 u16;
            struct {
                uint16 MPCn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPDn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } CD;
    };
};

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
union MPRP_t {
    struct {
        struct {
            uint64 u64;
        } ABCDEFGH;
        struct {
            uint64 u64;
        } IJKLMNOP;
    };

    struct {
        struct {
            uint32 u32;
        } ABCD;
        struct {
            uint32 u32;
        } EFGH;
        struct {
            uint32 u32;
        } IJKL;
        struct {
            uint32 u32;
        } MNOP;
    };

    struct {
        uint8 param : 6;
        uint8 _rsvd : 2;
    } arr[16];

    struct {
        union {
            uint16 u16;
            struct {
                uint16 MPAn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPBn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } AB;
        union {
            uint16 u16;
            struct {
                uint16 MPCn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPDn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } CD;
        union {
            uint16 u16;
            struct {
                uint16 MPEn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPFn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } EF;
        union {
            uint16 u16;
            struct {
                uint16 MPGn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPHn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } GH;
        union {
            uint16 u16;
            struct {
                uint16 MPIn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPJn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } IJ;
        union {
            uint16 u16;
            struct {
                uint16 MPKn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPLn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } KL;
        union {
            uint16 u16;
            struct {
                uint16 MPMn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPNn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } MN;
        union {
            uint16 u16;
            struct {
                uint16 MPOn : 6;
                uint16 _rsvd6_7 : 2;
                uint16 MPPn : 6;
                uint16 _rsvd14_15 : 2;
            };
        } OP;
    };
};

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
union SCXYID_t {
    uint64 u64;
    struct {
        struct {
            union {
                uint16 u16;
                struct {
                    uint16 NSCXI : 11;
                    uint16 _rsvd11_15 : 5;
                };
            } I;
            union {
                uint16 u16;
                struct {
                    uint16 _rsvd0_7 : 8;
                    uint16 NSCXD : 8;
                };
            } D;
        } X;
        struct {
            union {
                uint16 u16;
                struct {
                    uint16 NSCYI : 11;
                    uint16 _rsvd11_15 : 5;
                };
            } I;
            union {
                uint16 u16;
                struct {
                    uint16 _rsvd0_7 : 8;
                    uint16 NSCYD : 8;
                };
            } D;
        } Y;
    };
};

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
union ZMXYID_t {
    uint64 u64;
    struct {
        struct {
            union {
                uint16 u16;
                struct {
                    uint16 NZMXI : 3;
                    uint16 _rsvd11_15 : 13;
                };
            } I;
            union {
                uint16 u16;
                struct {
                    uint16 _rsvd0_7 : 8;
                    uint16 NZMXD : 8;
                };
            } D;
        } X;
        struct {
            union {
                uint16 u16;
                struct {
                    uint16 NZMYI : 3;
                    uint16 _rsvd11_15 : 13;
                };
            } I;
            union {
                uint16 u16;
                struct {
                    uint16 _rsvd0_7 : 8;
                    uint16 NZMYD : 8;
                };
            } D;
        } Y;
    };
};

// 180090   SCXN2   NBG2 Horizontal Screen Scroll Value
// 180092   SCYN2   NBG2 Vertical Screen Scroll Value
// 180094   SCXN3   NBG3 Horizontal Screen Scroll Value
// 180096   SCYN3   NBG3 Vertical Screen Scroll Value
//
// SCdNx:  (d=X,Y; x=2,3)
//   bits   r/w  code          description
//  15-11        -             Reserved, must be zero
//   10-0     W  NxSCd10-0     Horizontal/Vertical Screen Scroll Value (integer)
union SCXY_t {
    uint32 u32;
    struct {
        union {
            uint16 u16;
            struct {
                uint16 NSCX : 11;
                uint16 _rsvd11_15 : 5;
            };
        } X;
        union {
            uint16 u16;
            struct {
                uint16 NSCY : 11;
                uint16 _rsvd11_15 : 5;
            };
        } Y;
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
//     10     W  RBKASTRE      Enable for KA-st of Rotation Parameter B
//      9     W  RBYSTRE       Enable for Y-st of Rotation Parameter B
//      8     W  RBXSTRE       Enable for X-st of Rotation Parameter B
//    7-3        -             Reserved, must be zero
//      2     W  RAKASTRE      Enable for KA-st of Rotation Parameter A
//      1     W  RAYSTRE       Enable for Y-st of Rotation Parameter A
//      0     W  RAXSTRE       Enable for X-st of Rotation Parameter A
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

} // namespace satemu::vdp2

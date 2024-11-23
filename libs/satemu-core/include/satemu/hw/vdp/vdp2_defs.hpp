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
//  14-12   R/W  R0CHCN2-0     RBG0 Character Color Number
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
//     10   R/W  R0BMSZ        RBG0 Bitmap Size (0=512x256, 1=512x512)
//      9   R/W  R0BMEN        RBG0 Bitmap Enable (0=cells, 1=bitmap)
//      8   R/W  R0CHSZ        RBG0 Character Size (0=1x1, 1=2x2)
//    7-6        -             Reserved, must be zero
//      5   R/W  N3CHCN        NBG3 Character Color Number (0=16 colors, 1=256 colors; both palette)
//      4   R/W  N3CHSZ        NBG3 Character Size (0=1x1, 1=2x2)
//    3-2        -             Reserved, must be zero
//      1   R/W  N2CHCN        NBG2 Character Color Number (0=16 colors, 1=256 colors; both palette)
//      0   R/W  N2CHSZ        NBG2 Character Size (0=1x1, 1=2x2)
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

} // namespace satemu::vdp2

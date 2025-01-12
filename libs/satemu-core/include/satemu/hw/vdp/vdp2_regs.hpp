#pragma once

#include "vdp2_defs.hpp"

#include <satemu/util/bit_ops.hpp>
#include <satemu/util/inline.hpp>

namespace satemu::vdp {

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
        ZMCTL.u16 = 0x0;

        bgEnabled.fill(false);
        for (auto &bg : bgParams) {
            bg.Reset();
        }
        spriteParams.Reset();
        lineScreenParams.Reset();
        backScreenParams.Reset();

        for (auto &param : rotParams) {
            param.Reset();
        }
        commonRotParams.Reset();

        for (auto &param : windowParams) {
            param.Reset();
        }

        verticalCellScrollTableAddress = 0;
        cellScrollTableAddress = 0;

        mosaicH = 1;
        mosaicV = 1;

        for (auto &colorOffset : colorOffsetParams) {
            colorOffset.Reset();
        }

        colorCalcParams.Reset();

        for (auto &sp : specialFunctionCodes) {
            sp.Reset();
        }

        transparentShadowEnable = false;

        TVMDDirty = true;
    }

    RegTVMD TVMD;     // 180000   TVMD    TV Screen Mode
    RegEXTEN EXTEN;   // 180002   EXTEN   External Signal Enable
    RegTVSTAT TVSTAT; // 180004   TVSTAT  Screen Status (read-only)
    RegVRSIZE VRSIZE; // 180006   VRSIZE  VRAM Size
    uint16 HCNT;      // 180008   HCNT    H Counter (read-only)
    uint16 VCNT;      // 18000A   VCNT    V Counter (read-only)
                      // 18000C   -       Reserved (but not really)
    RegRAMCTL RAMCTL; // 18000E   RAMCTL  RAM Control
                      // 180010   CYCA0L  VRAM Cycle Pattern A0 Lower
    RegCYC CYCA0;     // 180012   CYCA0U  VRAM Cycle Pattern A0 Upper
                      // 180014   CYCA1L  VRAM Cycle Pattern A1 Lower
    RegCYC CYCA1;     // 180016   CYCA1U  VRAM Cycle Pattern A1 Upper
                      // 180018   CYCB0L  VRAM Cycle Pattern B0 Lower
    RegCYC CYCB0;     // 18001A   CYCB0U  VRAM Cycle Pattern B0 Upper
                      // 18001C   CYCB1L  VRAM Cycle Pattern B1 Lower
    RegCYC CYCB1;     // 18001E   CYCB1U  VRAM Cycle Pattern B1 Upper

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
        bit::deposit_into<0>(value, bgEnabled[0]);
        bit::deposit_into<1>(value, bgEnabled[1]);
        bit::deposit_into<2>(value, bgEnabled[2]);
        bit::deposit_into<3>(value, bgEnabled[3]);
        bit::deposit_into<4>(value, bgEnabled[4]);
        bit::deposit_into<5>(value, bgEnabled[5]);

        bit::deposit_into<8>(value, !bgParams[1].enableTransparency);
        bit::deposit_into<9>(value, !bgParams[2].enableTransparency);
        bit::deposit_into<10>(value, !bgParams[3].enableTransparency);
        bit::deposit_into<11>(value, !bgParams[4].enableTransparency);
        bit::deposit_into<12>(value, !bgParams[0].enableTransparency);
        return value;
    }

    FORCE_INLINE void WriteBGON(uint16 value) {
        bgEnabled[0] = bit::extract<0>(value);
        bgEnabled[1] = bit::extract<1>(value);
        bgEnabled[2] = bit::extract<2>(value);
        bgEnabled[3] = bit::extract<3>(value);
        bgEnabled[4] = bit::extract<4>(value);
        bgEnabled[5] = bit::extract<5>(value);

        bgParams[1].enableTransparency = !bit::extract<8>(value);
        bgParams[2].enableTransparency = !bit::extract<9>(value);
        bgParams[3].enableTransparency = !bit::extract<10>(value);
        bgParams[4].enableTransparency = !bit::extract<11>(value);
        bgParams[0].enableTransparency = !bit::extract<12>(value);
    }

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

    FORCE_INLINE uint16 ReadMZCTL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].mosaicEnable);
        bit::deposit_into<1>(value, bgParams[2].mosaicEnable);
        bit::deposit_into<2>(value, bgParams[3].mosaicEnable);
        bit::deposit_into<3>(value, bgParams[4].mosaicEnable);
        bit::deposit_into<4>(value, bgParams[0].mosaicEnable);
        bit::deposit_into<8, 11>(value, mosaicH - 1);
        bit::deposit_into<12, 15>(value, mosaicV - 1);
        return value;
    }

    FORCE_INLINE void WriteMZCTL(uint16 value) {
        bgParams[1].mosaicEnable = bit::extract<0>(value);
        bgParams[2].mosaicEnable = bit::extract<1>(value);
        bgParams[3].mosaicEnable = bit::extract<2>(value);
        bgParams[4].mosaicEnable = bit::extract<3>(value);
        bgParams[0].mosaicEnable = bit::extract<4>(value);
        mosaicH = bit::extract<8, 11>(value) + 1;
        mosaicV = bit::extract<12, 15>(value) + 1;
    }

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
        bit::deposit_into<0>(value, bgParams[1].specialFunctionSelect);
        bit::deposit_into<1>(value, bgParams[2].specialFunctionSelect);
        bit::deposit_into<2>(value, bgParams[3].specialFunctionSelect);
        bit::deposit_into<3>(value, bgParams[4].specialFunctionSelect);
        bit::deposit_into<4>(value, bgParams[0].specialFunctionSelect);
        return value;
    }

    FORCE_INLINE void WriteSFSEL(uint16 value) {
        bgParams[1].specialFunctionSelect = bit::extract<0>(value);
        bgParams[2].specialFunctionSelect = bit::extract<1>(value);
        bgParams[3].specialFunctionSelect = bit::extract<2>(value);
        bgParams[4].specialFunctionSelect = bit::extract<3>(value);
        bgParams[0].specialFunctionSelect = bit::extract<4>(value);
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
    //  11-10     W  N1BMSZ1-0     NBG1/EXBG Bitmap Size
    //                               00 (0) = 512x256
    //                               01 (1) = 512x512
    //                               10 (2) = 1024x256
    //                               11 (3) = 1024x512
    //      9     W  N1BMEN        NBG1/EXBG Bitmap Enable (0=cells, 1=bitmap)
    //      8     W  N1CHSZ        NBG1/EXBG Character Size (0=1x1, 1=2x2)
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
    //    3-2     W  N0BMSZ1-0     NBG0/RBG1 Bitmap Size
    //                               00 (0) = 512x256
    //                               01 (1) = 512x512
    //                               10 (2) = 1024x256
    //                               11 (3) = 1024x512
    //      1     W  N0BMEN        NBG0/RBG1 Bitmap Enable (0=cells, 1=bitmap)
    //      0     W  N0CHSZ        NBG0/RBG1 Character Size (0=1x1, 1=2x2)

    FORCE_INLINE uint16 ReadCHCTLA() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].cellSizeShift);
        bit::deposit_into<1>(value, bgParams[1].bitmap);
        bit::deposit_into<2, 3>(value, bgParams[1].bmsz);
        bit::deposit_into<4, 6>(value, static_cast<uint32>(bgParams[1].colorFormat));

        bit::deposit_into<8>(value, bgParams[2].cellSizeShift);
        bit::deposit_into<9>(value, bgParams[2].bitmap);
        bit::deposit_into<10, 11>(value, bgParams[2].bmsz);
        bit::deposit_into<12, 13>(value, static_cast<uint32>(bgParams[2].colorFormat));
        return value;
    }

    FORCE_INLINE void WriteCHCTLA(uint16 value) {
        bgParams[1].cellSizeShift = bit::extract<0>(value);
        bgParams[1].bitmap = bit::extract<1>(value);
        bgParams[1].bmsz = bit::extract<2, 3>(value);
        bgParams[1].colorFormat = static_cast<ColorFormat>(bit::extract<4, 6>(value));
        bgParams[1].UpdateCHCTL();

        bgParams[2].cellSizeShift = bit::extract<8>(value);
        bgParams[2].bitmap = bit::extract<9>(value);
        bgParams[2].bmsz = bit::extract<10, 11>(value);
        bgParams[2].colorFormat = static_cast<ColorFormat>(bit::extract<12, 13>(value));
        bgParams[2].UpdateCHCTL();
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
        bit::deposit_into<0>(value, bgParams[3].cellSizeShift);
        bit::deposit_into<1>(value, static_cast<uint32>(bgParams[3].colorFormat));

        bit::deposit_into<4>(value, bgParams[4].cellSizeShift);
        bit::deposit_into<5>(value, static_cast<uint32>(bgParams[4].colorFormat));

        bit::deposit_into<8>(value, bgParams[0].cellSizeShift);
        bit::deposit_into<9>(value, bgParams[0].bitmap);
        bit::deposit_into<10>(value, bgParams[0].bmsz);
        bit::deposit_into<12, 14>(value, static_cast<uint32>(bgParams[0].colorFormat));
        return value;
    }

    FORCE_INLINE void WriteCHCTLB(uint16 value) {
        bgParams[3].cellSizeShift = bit::extract<0>(value);
        bgParams[3].colorFormat = static_cast<ColorFormat>(bit::extract<1>(value));
        bgParams[3].UpdateCHCTL();

        bgParams[4].cellSizeShift = bit::extract<4>(value);
        bgParams[4].colorFormat = static_cast<ColorFormat>(bit::extract<5>(value));
        bgParams[4].UpdateCHCTL();

        bgParams[0].cellSizeShift = bit::extract<8>(value);
        bgParams[0].bitmap = bit::extract<9>(value);
        bgParams[0].bmsz = bit::extract<10>(value);
        bgParams[0].colorFormat = static_cast<ColorFormat>(bit::extract<12, 14>(value));
        bgParams[0].UpdateCHCTL();
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
        bit::deposit_into<0, 2>(value, bgParams[1].supplBitmapPalNum >> 8u);
        bit::deposit_into<4>(value, bgParams[1].supplBitmapSpecialColorCalc);
        bit::deposit_into<5>(value, bgParams[1].supplBitmapSpecialPriority);

        bit::deposit_into<8, 10>(value, bgParams[2].supplBitmapPalNum >> 8u);
        bit::deposit_into<12>(value, bgParams[2].supplBitmapSpecialColorCalc);
        bit::deposit_into<13>(value, bgParams[2].supplBitmapSpecialPriority);
        return value;
    }

    FORCE_INLINE void WriteBMPNA(uint16 value) {
        bgParams[1].supplBitmapPalNum = bit::extract<0, 2>(value) << 8u;
        bgParams[1].supplBitmapSpecialColorCalc = bit::extract<4>(value);
        bgParams[1].supplBitmapSpecialPriority = bit::extract<5>(value);

        bgParams[2].supplBitmapPalNum = bit::extract<8, 10>(value) << 8u;
        bgParams[2].supplBitmapSpecialColorCalc = bit::extract<12>(value);
        bgParams[2].supplBitmapSpecialPriority = bit::extract<13>(value);
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
        bit::deposit_into<0, 2>(value, bgParams[0].supplBitmapPalNum >> 8u);
        bit::deposit_into<4>(value, bgParams[0].supplBitmapSpecialColorCalc);
        bit::deposit_into<5>(value, bgParams[0].supplBitmapSpecialPriority);
        return value;
    }

    FORCE_INLINE void WriteBMPNB(uint16 value) {
        bgParams[0].supplBitmapPalNum = bit::extract<0, 2>(value) << 8u;
        bgParams[0].supplBitmapSpecialColorCalc = bit::extract<4>(value);
        bgParams[0].supplBitmapSpecialPriority = bit::extract<5>(value);
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
        bit::deposit_into<0, 4>(value, bgParams[bgIndex].supplScrollCharNum);
        bit::deposit_into<5, 7>(value, bgParams[bgIndex].supplScrollPalNum >> 4u);
        bit::deposit_into<8>(value, bgParams[bgIndex].supplScrollSpecialColorCalc);
        bit::deposit_into<9>(value, bgParams[bgIndex].supplScrollSpecialPriority);
        bit::deposit_into<14>(value, bgParams[bgIndex].extChar);
        bit::deposit_into<15>(value, !bgParams[bgIndex].twoWordChar);
        return value;
    }

    FORCE_INLINE void WritePNCN(uint32 bgIndex, uint16 value) {
        bgParams[bgIndex].supplScrollCharNum = bit::extract<0, 4>(value);
        bgParams[bgIndex].supplScrollPalNum = bit::extract<5, 7>(value) << 4u;
        bgParams[bgIndex].supplScrollSpecialColorCalc = bit::extract<8>(value);
        bgParams[bgIndex].supplScrollSpecialPriority = bit::extract<9>(value);
        bgParams[bgIndex].extChar = bit::extract<14>(value);
        bgParams[bgIndex].twoWordChar = !bit::extract<15>(value);
        bgParams[bgIndex].UpdatePageBaseAddresses();
    }

    FORCE_INLINE uint16 ReadPNCR() const {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, bgParams[0].supplScrollCharNum);
        bit::deposit_into<5, 7>(value, bgParams[0].supplScrollPalNum >> 4u);
        bit::deposit_into<8>(value, bgParams[0].supplScrollSpecialColorCalc);
        bit::deposit_into<9>(value, bgParams[0].supplScrollSpecialPriority);
        bit::deposit_into<14>(value, bgParams[0].extChar);
        bit::deposit_into<15>(value, !bgParams[0].twoWordChar);
        return value;
    }

    FORCE_INLINE void WritePNCR(uint16 value) {
        bgParams[0].supplScrollCharNum = bit::extract<0, 4>(value);
        bgParams[0].supplScrollPalNum = bit::extract<5, 7>(value) << 4u;
        bgParams[0].supplScrollSpecialColorCalc = bit::extract<8>(value);
        bgParams[0].supplScrollSpecialPriority = bit::extract<9>(value);
        bgParams[0].extChar = bit::extract<14>(value);
        bgParams[0].twoWordChar = !bit::extract<15>(value);
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
        bit::deposit_into<0, 1>(value, bgParams[1].plsz);
        bit::deposit_into<2, 3>(value, bgParams[2].plsz);
        bit::deposit_into<4, 5>(value, bgParams[3].plsz);
        bit::deposit_into<6, 7>(value, bgParams[4].plsz);

        bit::deposit_into<8, 9>(value, rotParams[0].plsz);
        bit::deposit_into<10, 11>(value, static_cast<uint32>(rotParams[0].screenOverProcess));
        bit::deposit_into<12, 13>(value, rotParams[1].plsz);
        bit::deposit_into<14, 15>(value, static_cast<uint32>(rotParams[1].screenOverProcess));
        return value;
    }

    FORCE_INLINE void WritePLSZ(uint16 value) {
        bgParams[1].plsz = bit::extract<0, 1>(value);
        bgParams[2].plsz = bit::extract<2, 3>(value);
        bgParams[3].plsz = bit::extract<4, 5>(value);
        bgParams[4].plsz = bit::extract<6, 7>(value);
        bgParams[1].UpdatePLSZ();
        bgParams[2].UpdatePLSZ();
        bgParams[3].UpdatePLSZ();
        bgParams[4].UpdatePLSZ();

        rotParams[0].plsz = bit::extract<8, 9>(value);
        rotParams[0].screenOverProcess = static_cast<ScreenOverProcess>(bit::extract<10, 11>(value));
        rotParams[1].plsz = bit::extract<12, 13>(value);
        rotParams[1].screenOverProcess = static_cast<ScreenOverProcess>(bit::extract<14, 15>(value));
        rotParams[0].UpdatePLSZ();
        rotParams[1].UpdatePLSZ();
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
        bit::deposit_into<0, 2>(value, bit::extract<6, 8>(bgParams[1].mapIndices[0]));
        bit::deposit_into<4, 6>(value, bit::extract<6, 8>(bgParams[2].mapIndices[0]));
        bit::deposit_into<8, 10>(value, bit::extract<6, 8>(bgParams[3].mapIndices[0]));
        bit::deposit_into<12, 14>(value, bit::extract<6, 8>(bgParams[4].mapIndices[0]));
        return value;
    }

    FORCE_INLINE void WriteMPOFN(uint16 value) {
        for (int i = 0; i < 4; i++) {
            bit::deposit_into<6, 8>(bgParams[1].mapIndices[i], bit::extract<0, 2>(value));
            bit::deposit_into<6, 8>(bgParams[2].mapIndices[i], bit::extract<4, 6>(value));
            bit::deposit_into<6, 8>(bgParams[3].mapIndices[i], bit::extract<8, 10>(value));
            bit::deposit_into<6, 8>(bgParams[4].mapIndices[i], bit::extract<12, 14>(value));
        }
        // shift by 17 is the same as multiply by 0x20000, which is the boundary for bitmap data
        bgParams[1].bitmapBaseAddress = bit::extract<0, 2>(value) << 17u;
        bgParams[2].bitmapBaseAddress = bit::extract<4, 6>(value) << 17u;
        bgParams[3].bitmapBaseAddress = bit::extract<8, 10>(value) << 17u;
        bgParams[4].bitmapBaseAddress = bit::extract<12, 14>(value) << 17u;

        bgParams[1].UpdatePageBaseAddresses();
        bgParams[2].UpdatePageBaseAddresses();
        bgParams[3].UpdatePageBaseAddresses();
        bgParams[4].UpdatePageBaseAddresses();
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
        bit::deposit_into<0, 2>(value, bit::extract<6, 8>(rotParams[0].mapIndices[0]));
        bit::deposit_into<4, 6>(value, bit::extract<6, 8>(rotParams[1].mapIndices[0]));
        return value;
    }

    FORCE_INLINE void WriteMPOFR(uint16 value) {
        for (int i = 0; i < 16; i++) {
            bit::deposit_into<6, 8>(rotParams[0].mapIndices[i], bit::extract<0, 2>(value));
            bit::deposit_into<6, 8>(rotParams[1].mapIndices[i], bit::extract<4, 6>(value));
        }
        // shift by 17 is the same as multiply by 0x20000, which is the boundary for bitmap data
        rotParams[0].bitmapBaseAddress = bit::extract<0, 2>(value) << 17u;
        rotParams[1].bitmapBaseAddress = bit::extract<4, 6>(value) << 17u;
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
        auto &bg = bgParams[bgIndex];
        bit::deposit_into<0, 5>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 0]));
        bit::deposit_into<8, 13>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 1]));
        return value;
    }

    FORCE_INLINE void WriteMPN(uint32 bgIndex, uint32 planeIndex, uint16 value) {
        auto &bg = bgParams[bgIndex];
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
    // 180060   MPABRB  Rotation Parameter B Scroll Surface Map for Screen Planes A,B
    // 180062   MPCDRB  Rotation Parameter B Scroll Surface Map for Screen Planes C,D
    // 180064   MPEFRB  Rotation Parameter B Scroll Surface Map for Screen Planes E,F
    // 180066   MPGHRB  Rotation Parameter B Scroll Surface Map for Screen Planes G,H
    // 180068   MPIJRB  Rotation Parameter B Scroll Surface Map for Screen Planes I,J
    // 18006A   MPKLRB  Rotation Parameter B Scroll Surface Map for Screen Planes K,L
    // 18006C   MPMNRB  Rotation Parameter B Scroll Surface Map for Screen Planes M,N
    // 18006E   MPOPRB  Rotation Parameter B Scroll Surface Map for Screen Planes O,P
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

    FORCE_INLINE uint16 ReadMPR(uint32 paramIndex, uint32 planeIndex) const {
        uint16 value = 0;
        auto &bg = rotParams[paramIndex];
        bit::deposit_into<0, 5>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 0]));
        bit::deposit_into<8, 13>(value, bit::extract<0, 5>(bg.mapIndices[planeIndex * 2 + 1]));
        return value;
    }

    FORCE_INLINE void WriteMPR(uint32 paramIndex, uint32 planeIndex, uint16 value) {
        auto &bg = rotParams[paramIndex];
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 0], bit::extract<0, 5>(value));
        bit::deposit_into<0, 5>(bg.mapIndices[planeIndex * 2 + 1], bit::extract<8, 13>(value));
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
        return bit::extract<8, 18>(bgParams[bgIndex].scrollAmountH);
    }

    FORCE_INLINE void WriteSCXIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 18>(bgParams[bgIndex].scrollAmountH, bit::extract<0, 10>(value));
    }

    FORCE_INLINE uint16 ReadSCXDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(bgParams[bgIndex].scrollAmountH);
    }

    FORCE_INLINE void WriteSCXDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(bgParams[bgIndex].scrollAmountH, bit::extract<8, 15>(value));
    }

    FORCE_INLINE uint16 ReadSCYIN(uint32 bgIndex) const {
        return bit::extract<8, 18>(bgParams[bgIndex].scrollAmountV);
    }

    FORCE_INLINE void WriteSCYIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 18>(bgParams[bgIndex].scrollAmountV, bit::extract<0, 10>(value));
    }

    FORCE_INLINE uint16 ReadSCYDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(bgParams[bgIndex].scrollAmountV);
    }

    FORCE_INLINE void WriteSCYDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(bgParams[bgIndex].scrollAmountV, bit::extract<8, 15>(value));
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
        return bit::extract<8, 10>(bgParams[bgIndex].scrollIncH);
    }

    FORCE_INLINE void WriteZMXIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 10>(bgParams[bgIndex].scrollIncH, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadZMXDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(bgParams[bgIndex].scrollIncH);
    }

    FORCE_INLINE void WriteZMXDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(bgParams[bgIndex].scrollIncH, bit::extract<8, 15>(value));
    }

    FORCE_INLINE uint16 ReadZMYIN(uint32 bgIndex) const {
        return bit::extract<8, 10>(bgParams[bgIndex].scrollIncV);
    }

    FORCE_INLINE void WriteZMYIN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<8, 10>(bgParams[bgIndex].scrollIncV, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadZMYDN(uint32 bgIndex) const {
        return bit::extract<0, 7>(bgParams[bgIndex].scrollIncV);
    }

    FORCE_INLINE void WriteZMYDN(uint32 bgIndex, uint16 value) {
        bit::deposit_into<0, 7>(bgParams[bgIndex].scrollIncV, bit::extract<8, 15>(value));
    }

    RegZMCTL ZMCTL; // 180098   ZMCTL   Reduction Enable

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

    FORCE_INLINE uint16 ReadSCRCTL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].verticalCellScrollEnable);
        bit::deposit_into<1>(value, bgParams[1].lineScrollXEnable);
        bit::deposit_into<2>(value, bgParams[1].lineScrollYEnable);
        bit::deposit_into<3>(value, bgParams[1].lineZoomEnable);
        bit::deposit_into<4, 5>(value, bgParams[1].lineScrollInterval);

        bit::deposit_into<8>(value, bgParams[2].verticalCellScrollEnable);
        bit::deposit_into<9>(value, bgParams[2].lineScrollXEnable);
        bit::deposit_into<10>(value, bgParams[2].lineScrollYEnable);
        bit::deposit_into<11>(value, bgParams[2].lineZoomEnable);
        bit::deposit_into<12, 13>(value, bgParams[2].lineScrollInterval);
        return value;
    }

    FORCE_INLINE void WriteSCRCTL(uint16 value) {
        bgParams[1].verticalCellScrollEnable = bit::extract<0>(value);
        bgParams[1].lineScrollXEnable = bit::extract<1>(value);
        bgParams[1].lineScrollYEnable = bit::extract<2>(value);
        bgParams[1].lineZoomEnable = bit::extract<3>(value);
        bgParams[1].lineScrollInterval = bit::extract<4, 5>(value);

        bgParams[2].verticalCellScrollEnable = bit::extract<8>(value);
        bgParams[2].lineScrollXEnable = bit::extract<9>(value);
        bgParams[2].lineScrollYEnable = bit::extract<10>(value);
        bgParams[2].lineZoomEnable = bit::extract<11>(value);
        bgParams[2].lineScrollInterval = bit::extract<12, 13>(value);
    }

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

    FORCE_INLINE uint16 ReadVCSTAU() const {
        return bit::extract<17, 19>(verticalCellScrollTableAddress);
    }

    FORCE_INLINE void WriteVCSTAU(uint16 value) {
        bit::deposit_into<17, 19>(verticalCellScrollTableAddress, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadVCSTAL() const {
        return bit::extract<2, 16>(verticalCellScrollTableAddress) << 1u;
    }

    FORCE_INLINE void WriteVCSTAL(uint16 value) {
        bit::deposit_into<2, 16>(verticalCellScrollTableAddress, bit::extract<1, 15>(value));
    }

    // 1800A0   LSTA0U  NBG0 Line Scroll Table Address (upper)
    // 1800A4   LSTA1U  NBG1 Line Scroll Table Address (upper)
    //
    //   bits   r/w  code          description
    //   15-3        -             Reserved, must be zero
    //    2-0     W  NxLSTA18-16   NBGx Line Scroll Table Base Address (bits 18-16)
    //
    // 1800A2   LSTA0L  NBG0 Line Scroll Table Address (lower)
    // 1800A6   LSTA1L  NBG1 Line Scroll Table Address (lower)
    //
    //   bits   r/w  code          description
    //   15-1     W  NxLSTA15-1    NBGx Line Scroll Table Base Address (bits 15-1)
    //      0        -             Reserved, must be zero

    FORCE_INLINE uint16 ReadLSTAnU(uint8 bgIndex) const {
        return bit::extract<17, 19>(bgParams[bgIndex].lineScrollTableAddress);
    }

    FORCE_INLINE void WriteLSTAnU(uint8 bgIndex, uint16 value) {
        bit::deposit_into<17, 19>(bgParams[bgIndex].lineScrollTableAddress, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadLSTAnL(uint8 bgIndex) const {
        return bit::extract<2, 16>(bgParams[bgIndex].lineScrollTableAddress) << 1u;
    }

    FORCE_INLINE void WriteLSTAnL(uint8 bgIndex, uint16 value) {
        bit::deposit_into<2, 16>(bgParams[bgIndex].lineScrollTableAddress, bit::extract<1, 15>(value));
    }

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

    FORCE_INLINE uint16 ReadLCTAU() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<17, 19>(lineScreenParams.baseAddress));
        bit::deposit_into<15>(value, lineScreenParams.perLine);
        return value;
    }

    FORCE_INLINE void WriteLCTAU(uint16 value) {
        bit::deposit_into<17, 19>(lineScreenParams.baseAddress, bit::extract<0, 2>(value));
        lineScreenParams.perLine = bit::extract<15>(value);
    }

    FORCE_INLINE uint16 ReadLCTAL() const {
        return bit::extract<1, 16>(lineScreenParams.baseAddress);
    }

    FORCE_INLINE void WriteLCTAL(uint16 value) {
        bit::deposit_into<1, 16>(lineScreenParams.baseAddress, value);
    }

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

    FORCE_INLINE uint16 ReadBKTAU() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<17, 19>(backScreenParams.baseAddress));
        bit::deposit_into<15>(value, backScreenParams.perLine);
        return value;
    }

    FORCE_INLINE void WriteBKTAU(uint16 value) {
        bit::deposit_into<17, 19>(backScreenParams.baseAddress, bit::extract<0, 2>(value));
        backScreenParams.perLine = bit::extract<15>(value);
    }

    FORCE_INLINE uint16 ReadBKTAL() const {
        return bit::extract<1, 16>(backScreenParams.baseAddress);
    }

    FORCE_INLINE void WriteBKTAL(uint16 value) {
        bit::deposit_into<1, 16>(backScreenParams.baseAddress, value);
    }

    // 1800B0   RPMD    Rotation Parameter Mode
    //
    //   bits   r/w  code          description
    //   15-2        -             Reserved, must be zero
    //    1-0     W  RPMD1-0       Rotation Parameter Mode
    //                               00 (0) = Rotation Parameter A
    //                               01 (1) = Rotation Parameter B
    //                               10 (2) = Screens switched via coeff. data from RPA table
    //                               11 (3) = Screens switched via rotation parameter window

    FORCE_INLINE uint16 ReadRPMD() const {
        return static_cast<uint16>(commonRotParams.rotParamMode);
    }

    FORCE_INLINE void WriteRPMD(uint16 value) {
        commonRotParams.rotParamMode = static_cast<RotationParamMode>(bit::extract<0, 1>(value));
    }

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

    FORCE_INLINE uint16 ReadRPRCTL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, rotParams[0].readXst);
        bit::deposit_into<1>(value, rotParams[0].readYst);
        bit::deposit_into<2>(value, rotParams[0].readKAst);

        bit::deposit_into<8>(value, rotParams[1].readXst);
        bit::deposit_into<9>(value, rotParams[1].readYst);
        bit::deposit_into<10>(value, rotParams[1].readKAst);
        return value;
    }

    FORCE_INLINE void WriteRPRCTL(uint16 value) {
        rotParams[0].readXst = bit::extract<0>(value);
        rotParams[0].readYst = bit::extract<1>(value);
        rotParams[0].readKAst = bit::extract<2>(value);

        rotParams[1].readXst = bit::extract<8>(value);
        rotParams[1].readYst = bit::extract<9>(value);
        rotParams[1].readKAst = bit::extract<10>(value);
    }

    // 1800B4   KTCTL   Coefficient Table Control
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //     12     W  RBKLCE        Use line color screen data from Rotation Parameter B coeff. data
    //  11-10     W  RBKMD1-0      Coefficient Mode for Rotation Parameter B
    //                               00 (0) = Use as scale coefficient kx, ky
    //                               01 (1) = Use as scale coefficient kx
    //                               10 (2) = Use as scale coefficient ky
    //                               11 (3) = Use as viewpoint Xp after rotation conversion
    //      9     W  RBKDBS        Coefficient Data Size for Rotation Parameter B
    //                               0 = 2 words
    //                               1 = 1 word
    //      8     W  RBKTE         Coefficient Table Enable for Rotation Parameter B
    //    7-5        -             Reserved, must be zero
    //      4     W  RAKLCE        Use line color screen data from Rotation Parameter A coeff. data
    //    3-2     W  RAKMD1-0      Coefficient Mode for Rotation Parameter A
    //                               00 (0) = Use as scale coefficient kx, ky
    //                               01 (1) = Use as scale coefficient kx
    //                               10 (2) = Use as scale coefficient ky
    //                               11 (3) = Use as viewpoint Xp after rotation conversion
    //      1     W  RAKDBS        Coefficient Data Size for Rotation Parameter A
    //                               0 = 2 words
    //                               1 = 1 word
    //      0     W  RAKTE         Coefficient Table Enable for Rotation Parameter A

    FORCE_INLINE uint16 ReadKTCTL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, rotParams[0].coeffTableEnable);
        bit::deposit_into<1>(value, rotParams[0].coeffDataSize);
        bit::deposit_into<2, 3>(value, static_cast<uint8>(rotParams[0].coeffDataMode));
        bit::deposit_into<4>(value, rotParams[0].coeffUseLineColorData);

        bit::deposit_into<8>(value, rotParams[1].coeffTableEnable);
        bit::deposit_into<9>(value, rotParams[1].coeffDataSize);
        bit::deposit_into<10, 11>(value, static_cast<uint8>(rotParams[1].coeffDataMode));
        bit::deposit_into<12>(value, rotParams[1].coeffUseLineColorData);
        return value;
    }

    FORCE_INLINE void WriteKTCTL(uint16 value) {
        rotParams[0].coeffTableEnable = bit::extract<0>(value);
        rotParams[0].coeffDataSize = bit::extract<1>(value);
        rotParams[0].coeffDataMode = static_cast<CoefficientDataMode>(bit::extract<2, 3>(value));
        rotParams[0].coeffUseLineColorData = bit::extract<4>(value);

        rotParams[1].coeffTableEnable = bit::extract<8>(value);
        rotParams[1].coeffDataSize = bit::extract<9>(value);
        rotParams[1].coeffDataMode = static_cast<CoefficientDataMode>(bit::extract<10, 11>(value));
        rotParams[1].coeffUseLineColorData = bit::extract<12>(value);
    }

    // 1800B6   KTAOF   Coefficient Table Address Offset
    //
    //   bits   r/w  code          description
    //  15-11        -             Reserved, must be zero
    //   10-8     W  RBKTAOS2-0    Coefficient Table Address Offset for Rotation Parameter B
    //    7-3        -             Reserved, must be zero
    //    2-0     W  RAKTAOS2-0    Coefficient Table Address Offset for Rotation Parameter A

    FORCE_INLINE uint16 ReadKTAOF() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<16, 18>(rotParams[0].coeffTableAddressOffset));

        bit::deposit_into<8, 10>(value, bit::extract<16, 18>(rotParams[1].coeffTableAddressOffset));
        return value;
    }

    FORCE_INLINE void WriteKTAOF(uint16 value) {
        bit::deposit_into<16, 18>(rotParams[0].coeffTableAddressOffset, bit::extract<0, 2>(value));

        bit::deposit_into<16, 18>(rotParams[1].coeffTableAddressOffset, bit::extract<8, 10>(value));
    }

    // 1800B8   OVPNRA  Rotation Parameter A Screen-Over Pattern Name
    // 1800BA   OVPNRB  Rotation Parameter B Screen-Over Pattern Name
    //
    //   bits   r/w  code          description
    //   15-0     W  RxOPN15-0     Over Pattern Name
    //
    // x:
    //   A = Rotation Parameter A (OVPNRA)
    //   B = Rotation Parameter B (OVPNRB)

    FORCE_INLINE uint16 ReadOVPNRn(uint8 index) const {
        return rotParams[index].screenOverPatternName;
    }

    FORCE_INLINE void WriteOVPNRn(uint8 index, uint16 value) {
        rotParams[index].screenOverPatternName = value;
    }

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

    FORCE_INLINE uint16 ReadRPTAU() const {
        return bit::extract<17, 19>(commonRotParams.baseAddress);
    }

    FORCE_INLINE void WriteRPTAU(uint16 value) {
        bit::deposit_into<17, 19>(commonRotParams.baseAddress, bit::extract<0, 2>(value));
    }

    FORCE_INLINE uint16 ReadRPTAL() const {
        return bit::extract<2, 16>(commonRotParams.baseAddress) << 1u;
    }

    FORCE_INLINE void WriteRPTAL(uint16 value) {
        bit::deposit_into<2, 16>(commonRotParams.baseAddress, bit::extract<1, 15>(value));
    }

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

    FORCE_INLINE uint16 ReadWPSXn(uint8 index) const {
        return windowParams[index].startX;
    }

    FORCE_INLINE void WriteWPSXn(uint8 index, uint16 value) {
        windowParams[index].startX = bit::extract<0, 9>(value);
    }

    FORCE_INLINE uint16 ReadWPEXn(uint8 index) const {
        return windowParams[index].endX;
    }

    FORCE_INLINE void WriteWPEXn(uint8 index, uint16 value) {
        windowParams[index].endX = bit::extract<0, 9>(value);
    }

    FORCE_INLINE uint16 ReadWPSYn(uint8 index) const {
        return windowParams[index].startY;
    }

    FORCE_INLINE void WriteWPSYn(uint8 index, uint16 value) {
        windowParams[index].startY = bit::extract<0, 8>(value);
    }

    FORCE_INLINE uint16 ReadWPEYn(uint8 index) const {
        return windowParams[index].endY;
    }

    FORCE_INLINE void WriteWPEYn(uint8 index, uint16 value) {
        windowParams[index].endY = bit::extract<0, 8>(value);
    }

    // 1800D0   WCTLA   NBG0 and NBG1 Window Control
    //
    //   bits   r/w  code          description
    //     15     W  N1LOG         NBG1/EXBG Window Logic (0=OR, 1=AND)
    //     14        -             Reserved, must be zero
    //     13     W  N1SWE         NBG1/EXBG Sprite Window Enable (0=disable, 1=enable)
    //     12     W  N1SWA         NBG1/EXBG Sprite Window Area (0=inside, 1=outside)
    //     11     W  N1W1E         NBG1/EXBG Window 1 Enable (0=disable, 1=enable)
    //     10     W  N1W1A         NBG1/EXBG Window 1 Area (0=inside, 1=outside)
    //      9     W  N1W0E         NBG1/EXBG Window 0 Enable (0=disable, 1=enable)
    //      8     W  N1W0A         NBG1/EXBG Window 0 Area (0=inside, 1=outside)
    //      7     W  N0LOG         NBG0/RBG1 Window Logic (0=OR, 1=AND)
    //      6        -             Reserved, must be zero
    //      5     W  N0SWE         NBG0/RBG1 Sprite Window Enable (0=disable, 1=enable)
    //      4     W  N0SWA         NBG0/RBG1 Sprite Window Area (0=inside, 1=outside)
    //      3     W  N0W1E         NBG0/RBG1 Window 1 Enable (0=disable, 1=enable)
    //      2     W  N0W1A         NBG0/RBG1 Window 1 Area (0=inside, 1=outside)
    //      1     W  N0W0E         NBG0/RBG1 Window 0 Enable (0=disable, 1=enable)
    //      0     W  N0W0A         NBG0/RBG1 Window 0 Area (0=inside, 1=outside)

    FORCE_INLINE uint16 ReadWCTLA() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].windowSet.inverted[0]);
        bit::deposit_into<1>(value, bgParams[1].windowSet.enabled[0]);
        bit::deposit_into<2>(value, bgParams[1].windowSet.inverted[1]);
        bit::deposit_into<3>(value, bgParams[1].windowSet.enabled[1]);
        bit::deposit_into<4>(value, bgParams[1].windowSet.inverted[2]);
        bit::deposit_into<5>(value, bgParams[1].windowSet.enabled[2]);
        bit::deposit_into<7>(value, static_cast<uint16>(bgParams[1].windowSet.logic));

        bit::deposit_into<8>(value, bgParams[2].windowSet.inverted[0]);
        bit::deposit_into<9>(value, bgParams[2].windowSet.enabled[0]);
        bit::deposit_into<10>(value, bgParams[2].windowSet.inverted[1]);
        bit::deposit_into<11>(value, bgParams[2].windowSet.enabled[1]);
        bit::deposit_into<12>(value, bgParams[2].windowSet.inverted[2]);
        bit::deposit_into<13>(value, bgParams[2].windowSet.enabled[2]);
        bit::deposit_into<15>(value, static_cast<uint16>(bgParams[2].windowSet.logic));
        return value;
    }

    FORCE_INLINE void WriteWCTLA(uint16 value) {
        bgParams[1].windowSet.inverted[0] = bit::extract<0>(value);
        bgParams[1].windowSet.enabled[0] = bit::extract<1>(value);
        bgParams[1].windowSet.inverted[1] = bit::extract<2>(value);
        bgParams[1].windowSet.enabled[1] = bit::extract<3>(value);
        bgParams[1].windowSet.inverted[2] = bit::extract<4>(value);
        bgParams[1].windowSet.enabled[2] = bit::extract<5>(value);
        bgParams[1].windowSet.logic = static_cast<WindowLogic>(bit::extract<7>(value));

        bgParams[2].windowSet.inverted[0] = bit::extract<8>(value);
        bgParams[2].windowSet.enabled[0] = bit::extract<9>(value);
        bgParams[2].windowSet.inverted[1] = bit::extract<10>(value);
        bgParams[2].windowSet.enabled[1] = bit::extract<11>(value);
        bgParams[2].windowSet.inverted[2] = bit::extract<12>(value);
        bgParams[2].windowSet.enabled[2] = bit::extract<13>(value);
        bgParams[2].windowSet.logic = static_cast<WindowLogic>(bit::extract<15>(value));
    }

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

    FORCE_INLINE uint16 ReadWCTLB() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[3].windowSet.inverted[0]);
        bit::deposit_into<1>(value, bgParams[3].windowSet.enabled[0]);
        bit::deposit_into<2>(value, bgParams[3].windowSet.inverted[1]);
        bit::deposit_into<3>(value, bgParams[3].windowSet.enabled[1]);
        bit::deposit_into<4>(value, bgParams[3].windowSet.inverted[2]);
        bit::deposit_into<5>(value, bgParams[3].windowSet.enabled[2]);
        bit::deposit_into<7>(value, static_cast<uint16>(bgParams[3].windowSet.logic));

        bit::deposit_into<8>(value, bgParams[4].windowSet.inverted[0]);
        bit::deposit_into<9>(value, bgParams[4].windowSet.enabled[0]);
        bit::deposit_into<10>(value, bgParams[4].windowSet.inverted[1]);
        bit::deposit_into<11>(value, bgParams[4].windowSet.enabled[1]);
        bit::deposit_into<12>(value, bgParams[4].windowSet.inverted[2]);
        bit::deposit_into<13>(value, bgParams[4].windowSet.enabled[2]);
        bit::deposit_into<15>(value, static_cast<uint16>(bgParams[4].windowSet.logic));
        return value;
    }

    FORCE_INLINE void WriteWCTLB(uint16 value) {
        bgParams[3].windowSet.inverted[0] = bit::extract<0>(value);
        bgParams[3].windowSet.enabled[0] = bit::extract<1>(value);
        bgParams[3].windowSet.inverted[1] = bit::extract<2>(value);
        bgParams[3].windowSet.enabled[1] = bit::extract<3>(value);
        bgParams[3].windowSet.inverted[2] = bit::extract<4>(value);
        bgParams[3].windowSet.enabled[2] = bit::extract<5>(value);
        bgParams[3].windowSet.logic = static_cast<WindowLogic>(bit::extract<7>(value));

        bgParams[4].windowSet.inverted[0] = bit::extract<8>(value);
        bgParams[4].windowSet.enabled[0] = bit::extract<9>(value);
        bgParams[4].windowSet.inverted[1] = bit::extract<10>(value);
        bgParams[4].windowSet.enabled[1] = bit::extract<11>(value);
        bgParams[4].windowSet.inverted[2] = bit::extract<12>(value);
        bgParams[4].windowSet.enabled[2] = bit::extract<13>(value);
        bgParams[4].windowSet.logic = static_cast<WindowLogic>(bit::extract<15>(value));
    }

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

    FORCE_INLINE uint16 ReadWCTLC() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[0].windowSet.inverted[0]);
        bit::deposit_into<1>(value, bgParams[0].windowSet.enabled[0]);
        bit::deposit_into<2>(value, bgParams[0].windowSet.inverted[1]);
        bit::deposit_into<3>(value, bgParams[0].windowSet.enabled[1]);
        bit::deposit_into<4>(value, bgParams[0].windowSet.inverted[2]);
        bit::deposit_into<5>(value, bgParams[0].windowSet.enabled[2]);
        bit::deposit_into<7>(value, static_cast<uint16>(bgParams[0].windowSet.logic));

        bit::deposit_into<8>(value, spriteParams.windowSet.inverted[0]);
        bit::deposit_into<9>(value, spriteParams.windowSet.enabled[0]);
        bit::deposit_into<10>(value, spriteParams.windowSet.inverted[1]);
        bit::deposit_into<11>(value, spriteParams.windowSet.enabled[1]);
        bit::deposit_into<12>(value, spriteParams.windowSet.inverted[2]);
        bit::deposit_into<13>(value, spriteParams.windowSet.enabled[2]);
        bit::deposit_into<15>(value, static_cast<uint16>(spriteParams.windowSet.logic));
        return value;
    }

    FORCE_INLINE void WriteWCTLC(uint16 value) {
        bgParams[0].windowSet.inverted[0] = bit::extract<0>(value);
        bgParams[0].windowSet.enabled[0] = bit::extract<1>(value);
        bgParams[0].windowSet.inverted[1] = bit::extract<2>(value);
        bgParams[0].windowSet.enabled[1] = bit::extract<3>(value);
        bgParams[0].windowSet.inverted[2] = bit::extract<4>(value);
        bgParams[0].windowSet.enabled[2] = bit::extract<5>(value);
        bgParams[0].windowSet.logic = static_cast<WindowLogic>(bit::extract<7>(value));

        spriteParams.windowSet.inverted[0] = bit::extract<8>(value);
        spriteParams.windowSet.enabled[0] = bit::extract<9>(value);
        spriteParams.windowSet.inverted[1] = bit::extract<10>(value);
        spriteParams.windowSet.enabled[1] = bit::extract<11>(value);
        spriteParams.windowSet.inverted[2] = bit::extract<12>(value);
        spriteParams.windowSet.enabled[2] = bit::extract<13>(value);
        spriteParams.windowSet.logic = static_cast<WindowLogic>(bit::extract<15>(value));
    }

    // 1800D6   WCTLD   Rotation Window and Color Calculation Window Control
    //
    //   bits   r/w  code          description
    //     15     W  CCLOG         Color Calculation Window Logic (0=OR, 1=AND)
    //     14        -             Reserved, must be zero
    //     13     W  CCSWE         Color Calculation Window Sprite Window Enable (0=disable, 1=enable)
    //     12     W  CCSWA         Color Calculation Window Sprite Window Area (0=inside, 1=outside)
    //     11     W  CCW1E         Color Calculation Window Window 1 Enable (0=disable, 1=enable)
    //     10     W  CCW1A         Color Calculation Window Window 1 Area (0=inside, 1=outside)
    //      9     W  CCW0E         Color Calculation Window Window 0 Enable (0=disable, 1=enable)
    //      8     W  CCW0A         Color Calculation Window Window 0 Area (0=inside, 1=outside)
    //      7     W  RPLOG         Rotation Parameter Window Logic (0=OR, 1=AND)
    //    6-4        -             Reserved, must be zero
    //      3     W  RPW1E         Rotation Parameter Window 1 Enable (0=disable, 1=enable)
    //      2     W  RPW1A         Rotation Parameter Window 1 Area (0=inside, 1=outside)
    //      1     W  RPW0E         Rotation Parameter Window 0 Enable (0=disable, 1=enable)
    //      0     W  RPW0A         Rotation Parameter Window 0 Area (0=inside, 1=outside)

    FORCE_INLINE uint16 ReadWCTLD() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, commonRotParams.windowSet.inverted[0]);
        bit::deposit_into<1>(value, commonRotParams.windowSet.enabled[0]);
        bit::deposit_into<2>(value, commonRotParams.windowSet.inverted[1]);
        bit::deposit_into<3>(value, commonRotParams.windowSet.enabled[1]);
        bit::deposit_into<7>(value, static_cast<uint16>(commonRotParams.windowSet.logic));

        bit::deposit_into<8>(value, colorCalcParams.windowSet.inverted[0]);
        bit::deposit_into<9>(value, colorCalcParams.windowSet.enabled[0]);
        bit::deposit_into<10>(value, colorCalcParams.windowSet.inverted[1]);
        bit::deposit_into<11>(value, colorCalcParams.windowSet.enabled[1]);
        bit::deposit_into<12>(value, colorCalcParams.windowSet.inverted[2]);
        bit::deposit_into<13>(value, colorCalcParams.windowSet.enabled[2]);
        bit::deposit_into<15>(value, static_cast<uint16>(colorCalcParams.windowSet.logic));
        return value;
    }

    FORCE_INLINE void WriteWCTLD(uint16 value) {
        commonRotParams.windowSet.inverted[0] = bit::extract<0>(value);
        commonRotParams.windowSet.enabled[0] = bit::extract<1>(value);
        commonRotParams.windowSet.inverted[1] = bit::extract<2>(value);
        commonRotParams.windowSet.enabled[1] = bit::extract<3>(value);
        commonRotParams.windowSet.logic = static_cast<WindowLogic>(bit::extract<7>(value));

        colorCalcParams.windowSet.inverted[0] = bit::extract<8>(value);
        colorCalcParams.windowSet.enabled[0] = bit::extract<9>(value);
        colorCalcParams.windowSet.inverted[1] = bit::extract<10>(value);
        colorCalcParams.windowSet.enabled[1] = bit::extract<11>(value);
        colorCalcParams.windowSet.inverted[2] = bit::extract<12>(value);
        colorCalcParams.windowSet.enabled[2] = bit::extract<13>(value);
        colorCalcParams.windowSet.logic = static_cast<WindowLogic>(bit::extract<15>(value));
    }

    // 1800D8   LWTA0U  Window 0 Line Window Table Address (upper)
    // 1800DC   LWTA1U  Window 1 Line Window Table Address (upper)
    //
    //   bits   r/w  code          description
    //     15     W  WxLWE         Line Window Enable (0=disabled, 1=enabled)
    //   14-3        -             Reserved, must be zero
    //    2-0     W  WxLWTA18-16   Line Window Table Address (bits 18-16)
    //
    // 1800DA   LWTA0L  Window 0 Line Window Table Address (lower)
    // 1800DE   LWTA1L  Window 1 Line Window Table Address (lower)
    //
    //   bits   r/w  code          description
    //   15-1     W  WxLWTA15-1    Line Window Table Address (bits 15-1)
    //      0        -             Reserved, must be zero

    FORCE_INLINE uint16 ReadLWTAnU(uint8 index) const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bit::extract<17, 19>(windowParams[index].lineWindowTableAddress));
        bit::deposit_into<15>(value, windowParams[index].lineWindowTableEnable);
        return value;
    }

    FORCE_INLINE void WriteLWTAnU(uint8 index, uint16 value) {
        bit::deposit_into<17, 19>(windowParams[index].lineWindowTableAddress, bit::extract<0, 2>(value));
        windowParams[index].lineWindowTableEnable = bit::extract<15>(value);
    }

    FORCE_INLINE uint16 ReadLWTAnL(uint8 index) const {
        uint16 value = 0;
        bit::deposit_into<1, 15>(value, bit::extract<2, 16>(windowParams[index].lineWindowTableAddress));
        return value;
    }

    FORCE_INLINE void WriteLWTAnL(uint8 index, uint16 value) {
        bit::deposit_into<2, 16>(windowParams[index].lineWindowTableAddress, bit::extract<1, 15>(value));
    }

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
        bit::deposit_into<4>(value, spriteParams.spriteWindowEnable);
        bit::deposit_into<5>(value, spriteParams.mixedFormat);
        bit::deposit_into<8, 10>(value, spriteParams.colorCalcValue);
        bit::deposit_into<12, 13>(value, static_cast<uint16>(spriteParams.colorCalcCond));
        return value;
    }

    FORCE_INLINE void WriteSPCTL(uint16 value) {
        spriteParams.type = bit::extract<0, 3>(value);
        spriteParams.spriteWindowEnable = bit::extract<4>(value);
        spriteParams.mixedFormat = bit::extract<5>(value);
        spriteParams.colorCalcValue = bit::extract<8, 10>(value);
        spriteParams.colorCalcCond = static_cast<SpriteColorCalculationCondition>(bit::extract<12, 13>(value));
    }

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

    FORCE_INLINE uint16 ReadSDCTL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].shadowEnable);
        bit::deposit_into<1>(value, bgParams[2].shadowEnable);
        bit::deposit_into<2>(value, bgParams[3].shadowEnable);
        bit::deposit_into<3>(value, bgParams[4].shadowEnable);
        bit::deposit_into<4>(value, bgParams[0].shadowEnable);
        bit::deposit_into<5>(value, backScreenParams.shadowEnable);
        bit::deposit_into<8>(value, transparentShadowEnable);
        return value;
    }

    FORCE_INLINE void WriteSDCTL(uint16 value) {
        bgParams[1].shadowEnable = bit::extract<0>(value);
        bgParams[2].shadowEnable = bit::extract<1>(value);
        bgParams[3].shadowEnable = bit::extract<2>(value);
        bgParams[4].shadowEnable = bit::extract<3>(value);
        bgParams[0].shadowEnable = bit::extract<4>(value);
        backScreenParams.shadowEnable = bit::extract<5>(value);
        transparentShadowEnable = bit::extract<8>(value);
    }

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
        bit::deposit_into<0, 2>(value, bgParams[1].cramOffset >> 8u);
        bit::deposit_into<4, 6>(value, bgParams[2].cramOffset >> 8u);
        bit::deposit_into<8, 10>(value, bgParams[3].cramOffset >> 8u);
        bit::deposit_into<12, 14>(value, bgParams[4].cramOffset >> 8u);
        return value;
    }

    FORCE_INLINE void WriteCRAOFA(uint16 value) {
        bgParams[1].cramOffset = bit::extract<0, 2>(value) << 8u;
        bgParams[2].cramOffset = bit::extract<4, 6>(value) << 8u;
        bgParams[3].cramOffset = bit::extract<8, 10>(value) << 8u;
        bgParams[4].cramOffset = bit::extract<12, 14>(value) << 8u;
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
        bit::deposit_into<0, 2>(value, bgParams[0].cramOffset >> 8u);
        bit::deposit_into<4, 6>(value, spriteParams.colorDataOffset >> 8u);
        return value;
    }

    FORCE_INLINE void WriteCRAOFB(uint16 value) {
        bgParams[0].cramOffset = bit::extract<0, 2>(value) << 8u;
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
        bit::deposit_into<0>(value, bgParams[1].lineColorScreenEnable);
        bit::deposit_into<1>(value, bgParams[2].lineColorScreenEnable);
        bit::deposit_into<2>(value, bgParams[3].lineColorScreenEnable);
        bit::deposit_into<3>(value, bgParams[4].lineColorScreenEnable);
        bit::deposit_into<4>(value, bgParams[0].lineColorScreenEnable);
        bit::deposit_into<5>(value, spriteParams.lineColorScreenEnable);
        return value;
    }

    FORCE_INLINE void WriteLNCLEN(uint16 value) {
        bgParams[1].lineColorScreenEnable = bit::extract<0>(value);
        bgParams[2].lineColorScreenEnable = bit::extract<1>(value);
        bgParams[3].lineColorScreenEnable = bit::extract<2>(value);
        bgParams[4].lineColorScreenEnable = bit::extract<3>(value);
        bgParams[0].lineColorScreenEnable = bit::extract<4>(value);
        spriteParams.lineColorScreenEnable = bit::extract<5>(value);
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
        bit::deposit_into<0, 1>(value, static_cast<uint32>(bgParams[1].priorityMode));
        bit::deposit_into<2, 3>(value, static_cast<uint32>(bgParams[2].priorityMode));
        bit::deposit_into<4, 5>(value, static_cast<uint32>(bgParams[3].priorityMode));
        bit::deposit_into<6, 7>(value, static_cast<uint32>(bgParams[4].priorityMode));
        bit::deposit_into<8, 9>(value, static_cast<uint32>(bgParams[0].priorityMode));
        return value;
    }

    FORCE_INLINE void WriteSFPRMD(uint16 value) {
        bgParams[1].priorityMode = static_cast<PriorityMode>(bit::extract<0, 1>(value));
        bgParams[2].priorityMode = static_cast<PriorityMode>(bit::extract<2, 3>(value));
        bgParams[3].priorityMode = static_cast<PriorityMode>(bit::extract<4, 5>(value));
        bgParams[4].priorityMode = static_cast<PriorityMode>(bit::extract<6, 7>(value));
        bgParams[0].priorityMode = static_cast<PriorityMode>(bit::extract<8, 9>(value));
    }

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
    //      9     W  CCRTMD        Color Calculation Ratio Mode (0=top screen, 1=second screen)
    //      8     W  CCMD          Color Calculation Mode (0=use color calculation register, 1=as is)
    //      7        -             Reserved, must be zero
    //      6     W  SPCCEN        Sprite Color Calculation Enable
    //      5     W  LCCCEN        Line Color Color Calculation Enable
    //      4     W  R0CCEN        RBG0 Color Calculation Enable
    //      3     W  N3CCEN        NBG3 Color Calculation Enable
    //      2     W  N2CCEN        NBG2 Color Calculation Enable
    //      1     W  N1CCEN        NBG1/EXBG Color Calculation Enable
    //      0     W  N0CCEN        NBG0/RBG1 Color Calculation Enable
    //
    // xxCCEN: 0=disable, 1=enable

    FORCE_INLINE uint16 ReadCCCTL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].colorCalcEnable);
        bit::deposit_into<1>(value, bgParams[2].colorCalcEnable);
        bit::deposit_into<2>(value, bgParams[3].colorCalcEnable);
        bit::deposit_into<3>(value, bgParams[4].colorCalcEnable);
        bit::deposit_into<4>(value, bgParams[0].colorCalcEnable);
        bit::deposit_into<5>(value, lineScreenParams.colorCalcEnable);
        bit::deposit_into<6>(value, spriteParams.colorCalcEnable);

        bit::deposit_into<8>(value, colorCalcParams.useAdditiveBlend);
        bit::deposit_into<9>(value, colorCalcParams.useSecondScreenRatio);
        bit::deposit_into<10>(value, colorCalcParams.extendedColorCalcEnable);
        bit::deposit_into<12, 14>(value, static_cast<uint8>(colorCalcParams.colorGradScreen));
        bit::deposit_into<15>(value, colorCalcParams.colorGradEnable);
        return value;
    }

    FORCE_INLINE void WriteCCCTL(uint16 value) {
        bgParams[1].colorCalcEnable = bit::extract<0>(value);
        bgParams[2].colorCalcEnable = bit::extract<1>(value);
        bgParams[3].colorCalcEnable = bit::extract<2>(value);
        bgParams[4].colorCalcEnable = bit::extract<3>(value);
        bgParams[0].colorCalcEnable = bit::extract<4>(value);
        lineScreenParams.colorCalcEnable = bit::extract<5>(value);
        spriteParams.colorCalcEnable = bit::extract<6>(value);

        colorCalcParams.useAdditiveBlend = bit::extract<8>(value);
        colorCalcParams.useSecondScreenRatio = bit::extract<9>(value);
        colorCalcParams.extendedColorCalcEnable = bit::extract<10>(value);
        colorCalcParams.colorGradScreen = static_cast<ColorGradScreen>(bit::extract<12, 14>(value));
        colorCalcParams.colorGradEnable = bit::extract<15>(value);
    }

    // 1800EE   SFCCMD  Special Color Calculation Mode
    //
    //   bits   r/w  code          description
    //  15-10        -             Reserved, must be zero
    //    9-8     W  R0SCCM1-0     RBG0 Special Color Calculation Mode
    //    7-6     W  N3SCCM1-0     NBG3 Special Color Calculation Mode
    //    5-4     W  N2SCCM1-0     NBG2 Special Color Calculation Mode
    //    3-2     W  N1SCCM1-0     NBG1 Special Color Calculation Mode
    //    1-0     W  N0SCCM1-0     NBG0 Special Color Calculation Mode

    FORCE_INLINE uint16 ReadSFCCMD() const {
        uint16 value = 0;
        bit::deposit_into<0, 1>(value, static_cast<uint8>(bgParams[1].specialColorCalcMode));
        bit::deposit_into<2, 3>(value, static_cast<uint8>(bgParams[2].specialColorCalcMode));
        bit::deposit_into<4, 5>(value, static_cast<uint8>(bgParams[3].specialColorCalcMode));
        bit::deposit_into<6, 7>(value, static_cast<uint8>(bgParams[4].specialColorCalcMode));
        bit::deposit_into<8, 9>(value, static_cast<uint8>(bgParams[0].specialColorCalcMode));
        return value;
    }

    FORCE_INLINE void WriteSFCCMD(uint16 value) {
        bgParams[1].specialColorCalcMode = static_cast<SpecialColorCalcMode>(bit::extract<0, 1>(value));
        bgParams[2].specialColorCalcMode = static_cast<SpecialColorCalcMode>(bit::extract<2, 3>(value));
        bgParams[3].specialColorCalcMode = static_cast<SpecialColorCalcMode>(bit::extract<4, 5>(value));
        bgParams[4].specialColorCalcMode = static_cast<SpecialColorCalcMode>(bit::extract<6, 7>(value));
        bgParams[0].specialColorCalcMode = static_cast<SpecialColorCalcMode>(bit::extract<8, 9>(value));
    }

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
        bit::deposit_into<0, 2>(value, bgParams[1].priorityNumber);
        bit::deposit_into<8, 10>(value, bgParams[2].priorityNumber);
        return value;
    }

    FORCE_INLINE void WritePRINA(uint16 value) {
        bgParams[1].priorityNumber = bit::extract<0, 2>(value);
        bgParams[2].priorityNumber = bit::extract<8, 10>(value);
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
        bit::deposit_into<0, 2>(value, bgParams[3].priorityNumber);
        bit::deposit_into<8, 10>(value, bgParams[4].priorityNumber);
        return value;
    }

    FORCE_INLINE void WritePRINB(uint16 value) {
        bgParams[3].priorityNumber = bit::extract<0, 2>(value);
        bgParams[4].priorityNumber = bit::extract<8, 10>(value);
    }

    // 1800FC   PRIR    RBG0 Priority Number
    //
    //   bits   r/w  code          description
    //   15-3        -             Reserved, must be zero
    //    2-0     W  R0PRIN2-0     RBG0 Priority Number

    FORCE_INLINE uint16 ReadPRIR() const {
        uint16 value = 0;
        bit::deposit_into<0, 2>(value, bgParams[0].priorityNumber);
        return value;
    }

    FORCE_INLINE void WritePRIR(uint16 value) {
        bgParams[0].priorityNumber = bit::extract<0, 2>(value);
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
        bit::deposit_into<0, 4>(value, spriteParams.colorCalcRatios[offset * 2 + 0] - 1);
        bit::deposit_into<8, 12>(value, spriteParams.colorCalcRatios[offset * 2 + 1] - 1);
        return value;
    }

    FORCE_INLINE void WriteCCRSn(uint32 offset, uint16 value) {
        spriteParams.colorCalcRatios[offset * 2 + 0] = bit::extract<0, 4>(value) + 1;
        spriteParams.colorCalcRatios[offset * 2 + 1] = bit::extract<8, 12>(value) + 1;
    }

    // 180108   CCRNA   NBG0 and NBG1 Color Calculation Ratio
    //
    //   bits   r/w  code          description
    //  15-13        -             Reserved, must be zero
    //   12-8     W  N1CCRT4-0     NBG1/EXBG Color Calculation Ratio
    //    7-5        -             Reserved, must be zero
    //    4-0     W  N0CCRT4-0     NBG0/RBG1 Color Calculation Ratio
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

    FORCE_INLINE uint16 ReadCCRNA() const {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, bgParams[1].colorCalcRatio - 1);
        bit::deposit_into<8, 12>(value, bgParams[2].colorCalcRatio - 1);
        return value;
    }

    FORCE_INLINE void WriteCCRNA(uint16 value) {
        bgParams[1].colorCalcRatio = bit::extract<0, 4>(value) + 1;
        bgParams[2].colorCalcRatio = bit::extract<8, 12>(value) + 1;
    }

    FORCE_INLINE uint16 ReadCCRNB() const {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, bgParams[3].colorCalcRatio - 1);
        bit::deposit_into<8, 12>(value, bgParams[4].colorCalcRatio - 1);
        return value;
    }

    FORCE_INLINE void WriteCCRNB(uint16 value) {
        bgParams[3].colorCalcRatio = bit::extract<0, 4>(value) + 1;
        bgParams[4].colorCalcRatio = bit::extract<8, 12>(value) + 1;
    }

    FORCE_INLINE uint16 ReadCCRR() const {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, bgParams[0].colorCalcRatio - 1);
        return value;
    }

    FORCE_INLINE void WriteCCRR(uint16 value) {
        bgParams[0].colorCalcRatio = bit::extract<0, 4>(value) + 1;
    }

    FORCE_INLINE uint16 ReadCCRLB() const {
        uint16 value = 0;
        bit::deposit_into<0, 4>(value, lineScreenParams.colorCalcRatio - 1);
        bit::deposit_into<8, 12>(value, backScreenParams.colorCalcRatio - 1);
        return value;
    }

    FORCE_INLINE void WriteCCRLB(uint16 value) {
        lineScreenParams.colorCalcRatio = bit::extract<0, 4>(value) + 1;
        backScreenParams.colorCalcRatio = bit::extract<8, 12>(value) + 1;
    }

    // 180110   CLOFEN  Color Offset Enable
    //
    //   bits   r/w  code          description
    //   15-7        -             Reserved, must be zero
    //      6     W  SPCOEN        Sprite Color Offset Enable
    //      5     W  BKCOEN        Back Screen Color Offset Enable
    //      4     W  R0COEN        RBG0 Color Offset Enable
    //      3     W  N3COEN        NBG3 Color Offset Enable
    //      2     W  N2COEN        NBG2 Color Offset Enable
    //      1     W  N1COEN        NBG1/EXBG Color Offset Enable
    //      0     W  N0COEN        NBG0/RBG1 Color Offset Enable

    FORCE_INLINE uint16 ReadCLOFEN() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].colorOffsetEnable);
        bit::deposit_into<1>(value, bgParams[2].colorOffsetEnable);
        bit::deposit_into<2>(value, bgParams[3].colorOffsetEnable);
        bit::deposit_into<3>(value, bgParams[4].colorOffsetEnable);
        bit::deposit_into<4>(value, bgParams[0].colorOffsetEnable);
        bit::deposit_into<5>(value, backScreenParams.colorOffsetEnable);
        bit::deposit_into<6>(value, spriteParams.colorOffsetEnable);
        return value;
    }

    FORCE_INLINE void WriteCLOFEN(uint16 value) {
        bgParams[1].colorOffsetEnable = bit::extract<0>(value);
        bgParams[2].colorOffsetEnable = bit::extract<1>(value);
        bgParams[3].colorOffsetEnable = bit::extract<2>(value);
        bgParams[4].colorOffsetEnable = bit::extract<3>(value);
        bgParams[0].colorOffsetEnable = bit::extract<4>(value);
        backScreenParams.colorOffsetEnable = bit::extract<5>(value);
        spriteParams.colorOffsetEnable = bit::extract<6>(value);
    }

    // 180112   CLOFSL  Color Offset Select
    //
    //   bits   r/w  code          description
    //   15-7        -             Reserved, must be zero
    //      6     W  SPCOSL        Sprite Color Offset Select
    //      5     W  BKCOSL        Back Screen Color Offset Select
    //      4     W  R0COSL        RBG0 Color Offset Select
    //      3     W  N3COSL        NBG3 Color Offset Select
    //      2     W  N2COSL        NBG2 Color Offset Select
    //      1     W  N1COSL        NBG1 Color Offset Select
    //      0     W  N0COSL        NBG0 Color Offset Select
    //
    // For all bits:
    //   0 = Color Offset A
    //   1 = Color Offset B

    FORCE_INLINE uint16 ReadCLOFSL() const {
        uint16 value = 0;
        bit::deposit_into<0>(value, bgParams[1].colorOffsetSelect);
        bit::deposit_into<1>(value, bgParams[2].colorOffsetSelect);
        bit::deposit_into<2>(value, bgParams[3].colorOffsetSelect);
        bit::deposit_into<3>(value, bgParams[4].colorOffsetSelect);
        bit::deposit_into<4>(value, bgParams[0].colorOffsetSelect);
        bit::deposit_into<5>(value, backScreenParams.colorOffsetSelect);
        bit::deposit_into<6>(value, spriteParams.colorOffsetSelect);
        return value;
    }

    FORCE_INLINE void WriteCLOFSL(uint16 value) {
        bgParams[1].colorOffsetSelect = bit::extract<0>(value);
        bgParams[2].colorOffsetSelect = bit::extract<1>(value);
        bgParams[3].colorOffsetSelect = bit::extract<2>(value);
        bgParams[4].colorOffsetSelect = bit::extract<3>(value);
        bgParams[0].colorOffsetSelect = bit::extract<4>(value);
        backScreenParams.colorOffsetSelect = bit::extract<5>(value);
        spriteParams.colorOffsetSelect = bit::extract<6>(value);
    }

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

    FORCE_INLINE uint16 ReadCOxR(uint8 select) const {
        return colorOffsetParams[select].r;
    }
    FORCE_INLINE uint16 ReadCOxG(uint8 select) const {
        return colorOffsetParams[select].g;
    }
    FORCE_INLINE uint16 ReadCOxB(uint8 select) const {
        return colorOffsetParams[select].b;
    }

    FORCE_INLINE void WriteCOxR(uint8 select, uint16 value) {
        colorOffsetParams[select].r = bit::extract_signed<0, 8>(value);
    }
    FORCE_INLINE void WriteCOxG(uint8 select, uint16 value) {
        colorOffsetParams[select].g = bit::extract_signed<0, 8>(value);
    }
    FORCE_INLINE void WriteCOxB(uint8 select, uint16 value) {
        colorOffsetParams[select].b = bit::extract_signed<0, 8>(value);
    }

    // -------------------------------------------------------------------------

    // Indicates if TVMD has changed.
    // The screen resolution is updated on VBlank.
    bool TVMDDirty;

    // Whether to display each background:
    // [0] NBG0
    // [1] NBG1
    // [2] NBG2
    // [3] NBG3
    // [4] RBG0
    // [5] RBG1
    // Note that when RBG1 is enabled, RBG0 is required to be enabled as well and all NBGs are disabled.
    // We'll assume that RBG0 is always enabled when RBG1 is also enabled.
    // Derived from BGON.xxON
    std::array<bool, 6> bgEnabled;

    // Background parameters:
    // [0] RBG0
    // [1] NBG0/RBG1
    // [2] NBG1/EXBG
    // [3] NBG2
    // [4] NBG3
    std::array<BGParams, 5> bgParams;
    SpriteParams spriteParams;
    LineBackScreenParams lineScreenParams;
    LineBackScreenParams backScreenParams;

    // Rotation Parameters A and B
    std::array<RotationParams, 2> rotParams;
    CommonRotationParams commonRotParams;

    // Window 0 and 1 parameters
    std::array<WindowParams, 2> windowParams;

    // Vertical cell scroll table base address.
    // Only valid for NBG0 and NBG1.
    // Derived from VCSTAU/L
    uint32 verticalCellScrollTableAddress;

    // Current vertical cell scroll table address.
    // Reset at the start of every frame and incremented every cell or 8 bitmap pixels.
    uint32 cellScrollTableAddress;

    // Mosaic dimensions.
    // Applies to all backgrounds with the mosaic effect enabled.
    // Rotation backgrounds only use the horizontal dimension.
    // Derived from MZCTL.MZSZH/V
    uint8 mosaicH; // Horizontal mosaic size
    uint8 mosaicV; // Vertical mosaic size

    // Color offset parameters.
    // Derived from COAR/G/B and COBR/G/B
    std::array<ColorOffsetParams, 2> colorOffsetParams;

    // Color calculation parameters.
    // Derived from CCTL, CCRNA/B, CCRR and CCRLB
    ColorCalcParams colorCalcParams;

    std::array<SpecialFunctionCodes, 2> specialFunctionCodes;

    // Enables transparent shadow sprites.
    // Derived from SDCTL.TPSDSL
    bool transparentShadowEnable;
};

} // namespace satemu::vdp

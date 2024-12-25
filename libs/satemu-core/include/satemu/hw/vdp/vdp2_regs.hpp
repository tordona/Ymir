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
        MZCTL.u16 = 0x0;
        ZMCTL.u16 = 0x0;
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
        spriteParams.Reset();

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

        bit::deposit_into<8>(value, !normBGParams[0].enableTransparency);
        bit::deposit_into<9>(value, !normBGParams[1].enableTransparency);
        bit::deposit_into<10>(value, !normBGParams[2].enableTransparency);
        bit::deposit_into<11>(value, !normBGParams[3].enableTransparency);
        bit::deposit_into<12>(value, !rotBGParams[0].enableTransparency);
        return value;
    }

    FORCE_INLINE void WriteBGON(uint16 value) {
        normBGParams[0].enabled = bit::extract<0>(value);
        normBGParams[1].enabled = bit::extract<1>(value);
        normBGParams[2].enabled = bit::extract<2>(value);
        normBGParams[3].enabled = bit::extract<3>(value);
        rotBGParams[0].enabled = bit::extract<4>(value);
        rotBGParams[1].enabled = bit::extract<5>(value);

        normBGParams[0].enableTransparency = !bit::extract<8>(value);
        normBGParams[1].enableTransparency = !bit::extract<9>(value);
        normBGParams[2].enableTransparency = !bit::extract<10>(value);
        normBGParams[3].enableTransparency = !bit::extract<11>(value);
        rotBGParams[0].enableTransparency = !bit::extract<12>(value);
        rotBGParams[1].enableTransparency = !normBGParams[0].enableTransparency;
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

    ZMCTL_t ZMCTL; // 180098   ZMCTL   Reduction Enable

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
        bit::deposit_into<0>(value, normBGParams[0].verticalCellScrollEnable);
        bit::deposit_into<1>(value, normBGParams[0].lineScrollXEnable);
        bit::deposit_into<2>(value, normBGParams[0].lineScrollYEnable);
        bit::deposit_into<3>(value, normBGParams[0].lineZoomEnable);
        bit::deposit_into<4, 5>(value, normBGParams[0].lineScrollInterval);

        bit::deposit_into<8>(value, normBGParams[1].verticalCellScrollEnable);
        bit::deposit_into<9>(value, normBGParams[1].lineScrollXEnable);
        bit::deposit_into<10>(value, normBGParams[1].lineScrollYEnable);
        bit::deposit_into<11>(value, normBGParams[1].lineZoomEnable);
        bit::deposit_into<12, 13>(value, normBGParams[1].lineScrollInterval);
        return value;
    }

    FORCE_INLINE void WriteSCRCTL(uint16 value) {
        normBGParams[0].verticalCellScrollEnable = bit::extract<0>(value);
        normBGParams[0].lineScrollXEnable = bit::extract<1>(value);
        normBGParams[0].lineScrollYEnable = bit::extract<2>(value);
        normBGParams[0].lineZoomEnable = bit::extract<3>(value);
        normBGParams[0].lineScrollInterval = bit::extract<4, 5>(value);

        normBGParams[1].verticalCellScrollEnable = bit::extract<8>(value);
        normBGParams[1].lineScrollXEnable = bit::extract<9>(value);
        normBGParams[1].lineScrollYEnable = bit::extract<10>(value);
        normBGParams[1].lineZoomEnable = bit::extract<11>(value);
        normBGParams[1].lineScrollInterval = bit::extract<12, 13>(value);
    }

    /**/             // 18009C   VCSTAU  Vertical Cell Scroll Table Address (upper)
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

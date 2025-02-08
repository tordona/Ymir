#pragma once

#include <satemu/core/types.hpp>

namespace satemu::scu {

// SCU interrupts
//  bit  vec   lvl  source  reason
//    0   40     F  VDP2    VBlank IN
//    1   41     E  VDP2    VBlank OUT
//    2   42     D  VDP2    HBlank IN
//    3   43     C  SCU     Timer 0
//    4   44     B  SCU     Timer 1
//    5   45     A  SCU     DSP End
//    6   46     9  SCSP    Sound Request
//    7   47     8  SM      System Manager
//    8   48     8  PAD     PAD Interrupt
//    9   49     6  A-Bus   Level 2 DMA End
//   10   4A     6  A-Bus   Level 1 DMA End
//   11   4B     5  A-Bus   Level 0 DMA End
//   12   4C     3  SCU     DMA-illegal
//   13   4D     2  VDP1    Sprite Draw End
//   14   -
//   15   -
//   16   50     7  A-Bus   External Interrupt 00
//   17   51     7  A-Bus   External Interrupt 01
//   18   52     7  A-Bus   External Interrupt 02
//   19   53     7  A-Bus   External Interrupt 03
//   20   54     4  A-Bus   External Interrupt 04
//   21   55     4  A-Bus   External Interrupt 05
//   22   56     4  A-Bus   External Interrupt 06
//   23   57     4  A-Bus   External Interrupt 07
//   24   58     1  A-Bus   External Interrupt 08
//   25   59     1  A-Bus   External Interrupt 09
//   26   5A     1  A-Bus   External Interrupt 0A
//   27   5B     1  A-Bus   External Interrupt 0B
//   28   5C     1  A-Bus   External Interrupt 0C
//   29   5D     1  A-Bus   External Interrupt 0D
//   30   5E     1  A-Bus   External Interrupt 0E
//   31   5F     1  A-Bus   External Interrupt 0F
union InterruptStatus {
    uint32 u32;
    struct {
        uint16 internal;
        uint16 external;
    };
    struct {
        uint32 VDP2_VBlankIN : 1;
        uint32 VDP2_VBlankOUT : 1;
        uint32 VDP2_HBlankIN : 1;
        uint32 SCU_Timer0 : 1;
        uint32 SCU_Timer1 : 1;
        uint32 SCU_DSPEnd : 1;
        uint32 SCSP_SoundRequest : 1;
        uint32 SM_SystemManager : 1;
        uint32 PAD_PADInterrupt : 1;
        uint32 ABus_Level2DMAEnd : 1;
        uint32 ABus_Level1DMAEnd : 1;
        uint32 ABus_Level0DMAEnd : 1;
        uint32 ABus_DMAIllegal : 1;
        uint32 VDP1_SpriteDrawEnd : 1;
        uint32 _rsvd14 : 1;
        uint32 _rsvd15 : 1;
        uint32 ABus_ExtIntr0 : 1;
        uint32 ABus_ExtIntr1 : 1;
        uint32 ABus_ExtIntr2 : 1;
        uint32 ABus_ExtIntr3 : 1;
        uint32 ABus_ExtIntr4 : 1;
        uint32 ABus_ExtIntr5 : 1;
        uint32 ABus_ExtIntr6 : 1;
        uint32 ABus_ExtIntr7 : 1;
        uint32 ABus_ExtIntr8 : 1;
        uint32 ABus_ExtIntr9 : 1;
        uint32 ABus_ExtIntrA : 1;
        uint32 ABus_ExtIntrB : 1;
        uint32 ABus_ExtIntrC : 1;
        uint32 ABus_ExtIntrD : 1;
        uint32 ABus_ExtIntrE : 1;
        uint32 ABus_ExtIntrF : 1;
    };
    struct {
        uint32 : 16;
        uint32 ABus_ExtIntrs : 16;
    };
};
static_assert(sizeof(InterruptStatus) == sizeof(uint32));

union InterruptMask {
    uint32 u32;
    uint16 internal : 15; // excludes A-Bus external interrupts bit
    struct {
        uint32 VDP2_VBlankIN : 1;
        uint32 VDP2_VBlankOUT : 1;
        uint32 VDP2_HBlankIN : 1;
        uint32 SCU_Timer0 : 1;
        uint32 SCU_Timer1 : 1;
        uint32 SCU_DSPEnd : 1;
        uint32 SCSP_SoundRequest : 1;
        uint32 SM_SystemManager : 1;
        uint32 PAD_PADInterrupt : 1;
        uint32 ABus_Level2DMAEnd : 1;
        uint32 ABus_Level1DMAEnd : 1;
        uint32 ABus_Level0DMAEnd : 1;
        uint32 ABus_DMAIllegal : 1;
        uint32 VDP1_SpriteDrawEnd : 1;
        uint32 _rsvd14 : 1;
        uint32 ABus_ExtIntrs : 1;
    };
};
static_assert(sizeof(InterruptMask) == sizeof(uint32));

} // namespace satemu::scu

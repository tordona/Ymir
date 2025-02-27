#pragma once

#include <satemu/core/types.hpp>

namespace satemu::sh2 {

// addr r/w  access   init      code    name
// 060  R/W  8,16     0000      IPRB    Interrupt priority setting register B
//
//   bits   r/w  code       description
//   15-12  R/W  SCIIP3-0   Serial Communication Interface (SCI) Interrupt Priority Level
//   11-8   R/W  FRTIP3-0   Free-Running Timer (FRT) Interrupt Priority Level
//    7-0   R/W  Reserved   Must be zero
//
//   Interrupt priority levels range from 0 to 15.

// 062  R/W  8,16     0000      VCRA    Vector number setting register A
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  SERV6-0  Serial Communication Interface (SCI) Receive-Error Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  SRXV6-0  Serial Communication Interface (SCI) Receive-Data-Full Interrupt Vector Number

// 064  R/W  8,16     0000      VCRB    Vector number setting register B
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  STXV6-0  Serial Communication Interface (SCI) Transmit-Data-Empty Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  STEV6-0  Serial Communication Interface (SCI) Transmit-End Interrupt Vector Number

// 066  R/W  8,16     0000      VCRC    Vector number setting register C
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  FICV6-0  Free-Running Timer (FRT) Input-Capture Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  FOCV6-0  Free-Running Timer (FRT) Output-Compare Interrupt Vector Number

// 068  R/W  8,16     0000      VCRD    Vector number setting register D
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  FOVV6-0  Free-Running Timer (FRT) Overflow Interrupt Vector Number
//    7-0   R    -        Reserved - must be zero

// 0E0  R/W  8,16     0000      ICR     Interrupt control register
//
//   bits   r/w  code   description
//     15   R    NMIL   NMI Input Level
//   14-9   R    -      Reserved - must be zero
//      8   R/W  NMIE   NMI Edge Select (0=falling, 1=rising)
//    7-1   R    -      Reserved - must be zero
//      0   R/W  VECMD  IRL Interrupt Vector Mode Select (0=auto, 1=external)
//                      Auto-vector mode assigns 71 to IRL15 and IRL14, and 64 to IRL1.
//                      External vector mode reads from external vector number input pins D7-D0.
//
//    The default value may be either 8000 or 0000 because NMIL is an external signal.
union RegICR {
    uint16 u16;
    struct {
        uint16 VECMD : 1;
        uint16 _rsvd1_7 : 7;
        uint16 NMIE : 1;
        uint16 _rsvd9_14 : 6;
        uint16 NMIL : 1;
    };
};

// 0E2  R/W  8,16     0000      IPRA    Interrupt priority setting register A
//
//   bits   r/w  code       description
//   15-12  R/W  DIVUIP3-0  Division Unit (DIVU) Interrupt Priority Level
//   11-8   R/W  DMACIP3-0  DMA Controller (DMAC) Interrupt Priority Level
//    7-4   R/W  WDTIP3-0   Watchdog Timer (WDT) Interrupt Priority Level
//    3-0   R    -          Reserved - must be zero
//
//   Interrupt priority levels range from 0 to 15.
//
//   The DMAC priority level is assigned to both channels.
//   If both channels raise an interrupt, channel 0 is prioritized.
//
//   WDTIP3-0 includes both the watchdog timer and bus state controller (BSC).
//   WDT interrupt has priority over BSC.

// 0E4  R/W  8,16     0000      VCRWDT  Vector number setting register WDT
//
//   bits   r/w  code     description
//     15   R    -        Reserved - must be zero
//   14-8   R/W  WITV6-0  Watchdog Timer (WDT) Interval Interrupt Vector Number
//      7   R    -        Reserved - must be zero
//    6-0   R/W  BCMV6-0  Bus State Controller (BSC) Compare Match Interrupt Vector Number

} // namespace satemu::sh2

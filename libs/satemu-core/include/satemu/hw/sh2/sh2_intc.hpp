#pragma once

#include <satemu/core/types.hpp>

#include <satemu/util/inline.hpp>

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

// -----------------------------------------------------------------------------

// Interrupt sources, sorted by default priority from lowest to highest
enum class InterruptSource : uint8 {
    None,          // #   Source        Priority       Vector
    FRT_OVI,       // 1   FRT OVI       IPRB.FRTIPn    VCRD.FOVVn
    FRT_OCI,       // 2   FRT OCI       IPRB.FRTIPn    VCRC.FOCVn
    FRT_ICI,       // 3   FRT ICI       IPRB.FRTIPn    VCRC.FICVn
    SCI_TEI,       // 4   SCI TEI       IPRB.SCIIPn    VCRB.STEVn
    SCI_TXI,       // 5   SCI TXI       IPRB.SCIIPn    VCRB.STXVn
    SCI_RXI,       // 6   SCI RXI       IPRB.SCIIPn    VCRA.SRXVn
    SCI_ERI,       // 7   SCI ERI       IPRB.SCIIPn    VCRA.SERVn
    BSC_REF_CMI,   // 8   BSC REF CMI   IPRA.WDTIPn    VCRWDT
    WDT_ITI,       // 9   WDT ITI       IPRA.WDTIPn    VCRWDT
    DMAC1_XferEnd, // 10  DMAC1 end     IPRA.DMACIPn   VCRDMA1
    DMAC0_XferEnd, // 11  DMAC2 end     IPRA.DMACIPn   VCRDMA0
    DIVU_OVFI,     // 12  DIVU OVFI     IPRA.DIVUIPn   VCRDIV
    IRL,           // 13  IRL#          15-1           0x40 + (level >> 1)
    UserBreak,     // 14  UBC break     15             0x0C
    NMI            // 15  NMI           16             0x0B
};

struct InterruptController {
    InterruptController() {
        Reset();
    }

    void Reset() {
        ICR.u16 = 0x0000;

        levels.fill(0);
        vectors.fill(0);

        SetLevel(InterruptSource::IRL, 1);
        SetVector(InterruptSource::IRL, 0x40);

        SetLevel(InterruptSource::UserBreak, 15);
        SetVector(InterruptSource::UserBreak, 0x0C);

        SetLevel(InterruptSource::NMI, 16);
        SetVector(InterruptSource::NMI, 0x0B);

        NMI = false;

        pending.source = InterruptSource::None;
        pending.level = 0;

        externalVector = 0;
    }

    RegICR ICR; // 0E0  R/W  8,16     0000      ICR     Interrupt control register

    std::array<uint8, 16> levels;
    std::array<uint8, 16> vectors;

    struct PendingInterruptInfo {
        InterruptSource source;
        uint8 level;
    } pending;

    bool NMI;
    uint8 externalVector;

    // Gets the interrupt vector number for the specified interrupt source.
    FORCE_INLINE uint8 GetVector(InterruptSource source) const {
        return vectors[static_cast<size_t>(source)];
    }

    // Sets the interrupt vector number for the specified interrupt source.
    FORCE_INLINE void SetVector(InterruptSource source, uint8 vector) {
        vectors[static_cast<size_t>(source)] = vector;
    }

    // Gets the interrupt level for the specified interrupt source.
    FORCE_INLINE uint8 GetLevel(InterruptSource source) const {
        return levels[static_cast<size_t>(source)];
    }

    // Sets the interrupt level for the specified interrupt source.
    FORCE_INLINE void SetLevel(InterruptSource source, uint8 priority) {
        levels[static_cast<size_t>(source)] = priority;
    }
};

} // namespace satemu::sh2

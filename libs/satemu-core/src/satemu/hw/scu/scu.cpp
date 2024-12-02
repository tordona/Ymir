#include <satemu/hw/scu/scu.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::scu {

SCU::SCU(vdp1::VDP1 &vdp1, vdp2::VDP2 &vdp2, scsp::SCSP &scsp, cdblock::CDBlock &cdblock, sh2::SH2 &sh2)
    : m_VDP1(vdp1)
    , m_VDP2(vdp2)
    , m_SCSP(scsp)
    , m_CDBlock(cdblock)
    , m_SH2(sh2) {
    Reset(true);
}

void SCU::PostConstructInit() {
    m_SH2.SetExternalInterruptCallback({this, [](void *ptr) {
                                            auto &scu = *static_cast<scu::SCU *>(ptr);
                                            scu.UpdateInterruptLevel(true);
                                        }});
}

void SCU::Reset(bool hard) {
    m_intrMask.u32 = 0;
    m_intrStatus.u32 = 0;
}

void SCU::TriggerHBlankIN() {
    m_intrStatus.VDP2_HBlankIN = 1;
    // TODO: also increment Timer 0 and trigger Timer 0 interrupt if the counter matches the compare register
    UpdateInterruptLevel(false);
}

void SCU::TriggerVBlankIN() {
    m_intrStatus.VDP2_VBlankIN = 1;
    UpdateInterruptLevel(false);
}

void SCU::TriggerVBlankOUT() {
    m_intrStatus.VDP2_VBlankOUT = 1;
    // TODO: also reset Timer 0
    UpdateInterruptLevel(false);
}

void SCU::TriggerSystemManager() {
    m_intrStatus.SM_SystemManager = 1;
    UpdateInterruptLevel(false);
}

void SCU::UpdateInterruptLevel(bool acknowledge) {
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

    static constexpr uint32 kBaseIntrLevels[] = {0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, //
                                                 0x8, 0x6, 0x6, 0x5, 0x3, 0x2, 0x0, 0x0, //
                                                 0x0};
    static constexpr uint32 kABusIntrLevels[] = {0x7, 0x7, 0x7, 0x7, 0x4, 0x4, 0x4, 0x4, //
                                                 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, //
                                                 0x0};

    const uint32 intrBits = m_intrStatus.u32 & ~m_intrMask.u16;
    if (intrBits != 0) {
        const uint16 intrIndexBase = std::countr_zero<uint16>(intrBits >> 0u);
        const uint16 intrIndexABus = std::countr_zero<uint16>(intrBits >> 16u);

        const uint8 intrLevelBase = kBaseIntrLevels[intrIndexBase];
        const uint8 intrLevelABus = kABusIntrLevels[intrIndexABus];

        if (acknowledge) {
            if (intrLevelBase >= intrLevelABus) {
                m_intrStatus.u32 &= ~(1u << intrIndexBase);
            } else {
                m_intrStatus.u32 &= ~(1u << (intrIndexABus + 16u));
            }
        }

        if (intrLevelBase >= intrLevelABus) {
            m_SH2.SetExternalInterrupt(intrLevelBase, intrIndexBase + 0x40);
        } else {
            m_SH2.SetExternalInterrupt(intrLevelABus, intrIndexABus + 0x50);
        }
    } else {
        m_SH2.SetExternalInterrupt(0, 0);
    }
}

} // namespace satemu::scu

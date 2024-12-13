#include <satemu/hw/scu/scu.hpp>

#include <satemu/hw/sh2/sh2.hpp>

namespace satemu::scu {

SCU::SCU(vdp::VDP &vdp, scsp::SCSP &scsp, cdblock::CDBlock &cdblock, sh2::SH2 &sh2)
    : m_VDP(vdp)
    , m_SCSP(scsp)
    , m_CDBlock(cdblock)
    , m_SH2(sh2) {
    Reset(true);
}

void SCU::Reset(bool hard) {
    m_intrMask.u32 = 0;
    m_intrStatus.u32 = 0;

    m_dspState.Reset();
}

void SCU::Advance(uint64 cycles) {
    RunDSP(cycles);
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

void SCU::TriggerHBlankIN() {
    m_intrStatus.VDP2_HBlankIN = 1;
    // TODO: also increment Timer 0 and trigger Timer 0 interrupt if the counter matches the compare register
    UpdateInterruptLevel(false);
}

void SCU::TriggerDSPEnd() {
    m_intrStatus.SCU_DSPEnd = 1;
    UpdateInterruptLevel(false);
}

void SCU::TriggerSoundRequest(bool level) {
    m_intrStatus.SCSP_SoundRequest = level;
    UpdateInterruptLevel(false);
}

void SCU::TriggerSystemManager() {
    m_intrStatus.SM_SystemManager = 1;
    UpdateInterruptLevel(false);
}

void SCU::TriggerExternalInterrupt0() {
    m_intrStatus.ABus_ExtIntr0 = 1;
    UpdateInterruptLevel(false);
}

void SCU::AcknowledgeExternalInterrupt() {
    UpdateInterruptLevel(true);
}

void SCU::RunDSP(uint64 cycles) {
    // TODO: proper cycle counting

    // Bail out if not executing
    if (!m_dspState.programExecuting && !m_dspState.programStep) {
        return;
    }

    // Bail out if paused
    if (m_dspState.programPaused) {
        return;
    }

    // Execute next command
    const uint32 command = m_dspState.programRAM[m_dspState.programAddress++];
    const uint32 cmdCategory = bit::extract<30, 31>(command);

    switch (cmdCategory) {
    case 0b00: DSPCmd_Operation(command); break;
    case 0b10: DSPCmd_LoadImm(command); break;
    case 0b11: DSPCmd_Special(command); break;
    }

    // Clear stepping flag to ensure the DSP only runs one command when stepping
    m_dspState.programStep = false;
}

FORCE_INLINE void SCU::DSPCmd_Operation(uint32 command) {
    // ALU
    switch (bit::extract<26, 29>(command)) {
    case 0b0000: // NOP
        // TODO: implement
        break;
    case 0b0001: // AND
        // TODO: implement
        break;
    case 0b0010: // OR
        // TODO: implement
        break;
    case 0b0011: // XOR
        // TODO: implement
        break;
    case 0b0100: // ADD
        // TODO: implement
        break;
    case 0b0101: // SUB
        // TODO: implement
        break;
    case 0b0110: // AD2
        // TODO: implement
        break;
    case 0b1000: // SR
        // TODO: implement
        break;
    case 0b1001: // RR
        // TODO: implement
        break;
    case 0b1010: // SL
        // TODO: implement
        break;
    case 0b1011: // RL
        // TODO: implement
        break;
    case 0b1111: // RL8
        // TODO: implement
        break;
    }

    // X-Bus
    switch (bit::extract<23, 25>(command)) {
    case 0b000: // NOP
        // TODO: implement
        break;
    case 0b010: // MOV MUL,P
        // TODO: implement
        break;
    case 0b011: // MOV [s],P
        // TODO: implement
        break;
    case 0b100: // MOV [s],X
        // TODO: implement
        break;
    }

    // Y-Bus
    switch (bit::extract<17, 19>(command)) {
    case 0b000: // NOP
        // TODO: implement
        break;
    case 0b001: // CLR A
        // TODO: implement
        break;
    case 0b010: // MOV ALU,A
        // TODO: implement
        break;
    case 0b011: // MOV [s],A
        // TODO: implement
        break;
    case 0b100: // MOV [s],Y
        // TODO: implement
        break;
    }

    // D1-Bus
    switch (bit::extract<12, 13>(command)) {
    case 0b00: // NOP
        break;
    case 0b01: { // MOV SImm, [d]
        const sint32 imm = bit::sign_extend<8>(bit::extract<0, 7>(command));
        const uint32 dst = bit::extract<8, 11>(command);
        // TODO: implement
        break;
    }
    case 0b11: { // MOV [s], [d]
        const sint32 src = bit::extract<0, 3>(command);
        const uint32 dst = bit::extract<8, 11>(command);
        // TODO: implement
        break;
    }
    }
}

FORCE_INLINE void SCU::DSPCmd_LoadImm(uint32 command) {
    const uint32 dst = bit::extract<26, 29>(command);
    if (bit::extract<25>(command)) {
        // Conditional transfer
        const sint32 imm = bit::sign_extend<19>(bit::extract<0, 18>(command));
        const sint32 cond = bit::extract<19, 24>(command);
        // 000001: NZ  (Z=0)
        // 000010: NS  (S=0)
        // 000011: NZS (Z=0 && S=0)
        // 000100: NC  (C=0)
        // 001000: NT0 (T0=0)
        // 100001: Z   (Z=1)
        // 100010: S   (S=1)
        // 100011: ZS  (Z=1 || S=1)
        // 100100: C   (C=1)
        // 101000: T0  (T0=1)
    } else {
        // Unconditional transfer
        const sint32 imm = bit::sign_extend<25>(bit::extract<0, 24>(command));
    }
}

FORCE_INLINE void SCU::DSPCmd_Special(uint32 command) {
    const uint32 cmdSubcategory = bit::extract<28, 29>(command);
    switch (cmdSubcategory) {
    case 0b00: DSPCmd_Special_DMA(command); break;
    case 0b01: DSPCmd_Special_Jump(command); break;
    case 0b10: DSPCmd_Special_LoopBottom(command); break;
    case 0b11: DSPCmd_Special_End(command); break;
    }
}

FORCE_INLINE void SCU::DSPCmd_Special_DMA(uint32 command) {}

FORCE_INLINE void SCU::DSPCmd_Special_Jump(uint32 command) {}

FORCE_INLINE void SCU::DSPCmd_Special_LoopBottom(uint32 command) {}

FORCE_INLINE void SCU::DSPCmd_Special_End(uint32 command) {
    const bool setEndIntr = bit::extract<27>(command);
    m_dspState.programEnded = true;
    if (setEndIntr) {
        TriggerDSPEnd();
    }
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
            m_SH2.SetExternalInterrupt(0, 0);
        } else {
            if (intrLevelBase >= intrLevelABus) {
                m_SH2.SetExternalInterrupt(intrLevelBase, intrIndexBase + 0x40);
            } else {
                m_SH2.SetExternalInterrupt(intrLevelABus, intrIndexABus + 0x50);
            }
        }
    } else {
        m_SH2.SetExternalInterrupt(0, 0);
    }
}

} // namespace satemu::scu

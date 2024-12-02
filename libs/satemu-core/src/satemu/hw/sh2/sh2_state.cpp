#include <satemu/hw/sh2/sh2_state.hpp>

#include <cassert>

namespace satemu::sh2 {

SH2State::SH2State(bool master) {
    BCR1.MASTER = !master;
    Reset(true);
}

void SH2State::Reset(bool hard) {
    // Initial values:
    // - R0-R14 = undefined
    // - R15 = ReadLong(VBR + 4)

    // - SR = bits I3-I0 set, reserved bits clear, the rest is undefined
    // - GBR = undefined
    // - VBR = 0x00000000

    // - MACH, MACL = undefined
    // - PR = undefined
    // - PC = ReadLong(VBR)

    R.fill(0);
    PR = 0;

    SR.u32 = 0;
    SR.I0 = SR.I1 = SR.I2 = SR.I3 = 1;
    GBR = 0;
    VBR = 0x00000000;

    MAC.u64 = 0;

    // NOTE: this needs to be done externally since this class doesn't have access to SH2Bus
    // PC = MemReadLong(VBR);
    // R[15] = MemReadLong(VBR + 4);

    // On-chip registers
    IPRB.val.u16 = 0x0000;
    VCRA.val.u16 = 0x0000;
    VCRB.val.u16 = 0x0000;
    VCRC.val.u16 = 0x0000;
    VCRD.val.u16 = 0x0000;
    ICR.val.u16 = 0x0000;
    IPRA.val.u16 = 0x0000;
    VCRWDT.val.u16 = 0x0000;
    VCRDIV = 0x0000; // undefined initial value
    VCRDMA0 = 0x00;  // undefined initial value
    VCRDMA1 = 0x00;  // undefined initial value
    BCR1.u15 = 0x03F0;
    BCR2.u16 = 0x00FC;
    WCR.u16 = 0xAAFF;
    MCR.u16 = 0x0000;

    cacheEntries.fill({});
    WriteCCR(0x00);

    DVSR = 0x0;  // undefined initial value
    DVDNT = 0x0; // undefined initial value
    DVCR.u32 = 0x00000000;
    DVDNTH = 0x0; // undefined initial value
    DVDNTL = 0x0; // undefined initial value

    pendingExternalIntrLevel = 0;
    pendingExternalIntrVecNum = 0;
    pendingInterrupt.priority = 0;
    pendingInterrupt.vecNum = 0;
}

void SH2State::SetExternalInterrupt(uint8 level, uint8 vecNum) {
    assert(level < 16);
    pendingExternalIntrLevel = level;
    pendingExternalIntrVecNum = vecNum;
}

// -----------------------------------------------------------------------------
// On-chip peripherals

void SH2State::WriteCCR(uint8 value) {
    if (CCR.u8 == value) {
        return;
    }

    CCR.u8 = value;
    if (CCR.CP) {
        // TODO: purge cache
        CCR.CP = 0;
    }
}

void SH2State::DIVUBegin32() {
    static constexpr sint32 kMinValue = std::numeric_limits<sint32>::min();
    static constexpr sint32 kMaxValue = std::numeric_limits<sint32>::max();

    const sint32 dividend = static_cast<sint32>(DVDNT);
    const sint32 divisor = static_cast<sint32>(DVSR);

    if (divisor != 0) {
        // TODO: schedule event to run this after 39 cycles

        if (dividend == kMinValue && divisor == -1) [[unlikely]] {
            // Handle extreme case
            DVDNTL = DVDNT = kMinValue;
            DVDNTH = 0;
        } else {
            DVDNTL = DVDNT = dividend / divisor;
            DVDNTH = dividend % divisor;
        }
    } else {
        // Overflow
        // TODO: schedule event to run this after 6 cycles

        // Store results after 6 cycles - 3 for setting flags and 3 for calculations
        DVDNTH = dividend >> 29;
        if (DVCR.OVFIE) {
            DVDNTL = DVDNT = (dividend << 3) | ((dividend >> 31) & 7);
        } else {
            // DVDNT/DVDNTL is saturated if the interrupt signal is disabled
            DVDNTL = DVDNT = dividend < 0 ? kMinValue : kMaxValue;
        }

        // Signal overflow
        DVCR.OVF = 1;
    }
}

void SH2State::DIVUBegin64() {
    // TODO: implement
}

// -----------------------------------------------------------------------------
// Interrupts

bool SH2State::CheckInterrupts() {
    // TODO: check interrupts from these sources (in order of priority, when priority numbers are the same):
    //   name             priority       vecnum
    //   NMI              16             0x0B
    //   User break       15             0x0C
    //   IRLs 15-1        15-1           0x40 + (level >> 1)
    //   DIVU OVFI        IPRA.DIVUIPn   VCRDIV
    //   DMAC0 xfer end   IPRA.DMACIPn   VCRDMA0
    //   DMAC1 xfer end   IPRA.DMACIPn   VCRDMA1
    //   WDT ITI          IPRA.WDTIPn    VCRWDT
    //   BSC REF CMI      IPRA.WDTIPn    VCRWDT
    //   SCI ERI          IPRB.SCIIPn    VCRA.SERVn
    //   SCI RXI          IPRB.SCIIPn    VCRA.SRXVn
    //   SCI TXI          IPRB.SCIIPn    VCRB.STXVn
    //   SCI TEI          IPRB.SCIIPn    VCRB.STEVn
    //   FRT ICI          IPRB.FRTIPn    VCRC.FICVn
    //   FRT OCI          IPRB.FRTIPn    VCRC.FOCVn
    //   FRT OVI          IPRB.FRTIPn    VCRD.FOVVn
    // use the vector number of the exception with highest priority
    // TODO: external vector fetch

    // TODO: NMI, user break (before IRLs)

    bool usingExternalIntr = true;
    pendingInterrupt.priority = pendingExternalIntrLevel;
    pendingInterrupt.vecNum = ICR.VECMD ? pendingExternalIntrVecNum : 0x40 + (pendingExternalIntrLevel >> 1u);

    auto update = [&](uint8 intrPriority, uint8 vecNum) {
        if (intrPriority > pendingInterrupt.priority) {
            pendingInterrupt.priority = intrPriority;
            pendingInterrupt.vecNum = vecNum;
            usingExternalIntr = false;
        }
    };

    if (DVCR.OVF && DVCR.OVFIE) {
        update(IPRA.DIVUIPn, VCRDIV);
    }

    // TODO: DMAC0, DMAC1 transfer end
    // TODO: WDT ITI, BSC REF CMI
    // TODO: SCI ERI, RXI, TXI, TEI
    // TODO: FRT ICI, OCI, OVI

    const bool result = pendingInterrupt.priority > SR.ILevel;
    if (result && usingExternalIntr) {
        cbAcknowledgeExternalInterrupt();
    }
    return result;
}

} // namespace satemu::sh2

#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <vector>

namespace satemu::sh2 {

// -----------------------------------------------------------------------------
// Private access to SH2 members

struct PrivateAccess {
    static std::array<uint32, 16> &R(SH2 &sh2) {
        return sh2.R;
    }

    static uint32 &PC(SH2 &sh2) {
        return sh2.PC;
    }

    static RegSR &SR(SH2 &sh2) {
        return sh2.SR;
    }

    static uint32 &VBR(SH2 &sh2) {
        return sh2.VBR;
    }

    static InterruptController &INTC(SH2 &sh2) {
        return sh2.INTC;
    }

    static void RaiseInterrupt(SH2 &sh2, InterruptSource source) {
        sh2.RaiseInterrupt(source);
    }

    static bool CheckInterrupts(SH2 &sh2) {
        return sh2.CheckInterrupts();
    }
};

} // namespace satemu::sh2

// -----------------------------------------------------------------------------
// Test subject class

using namespace satemu;

struct TestSubject : debug::ISH2Tracer {
    mutable sys::Bus bus{};
    mutable sh2::SH2 sh2{bus, true};

    TestSubject() {
        // Setup tracer to collect interrupts into a vector
        sh2.UseTracer(this);

        sh2.SetExternalInterruptAcknowledgeCallback(util::MakeClassMemberRequiredCallback<&TestSubject::IntrAck>(this));

        // TODO: set up predictable vector table
        bus.MapMemory(0x000'0000, 0x7FF'FFFF,
                      {
                          .ctx = this,
                          .read8 = [](uint32 address, void *ctx) -> uint8 {
                              return static_cast<TestSubject *>(ctx)->Read8(address);
                          },
                          .read16 = [](uint32 address, void *ctx) -> uint16 {
                              return static_cast<TestSubject *>(ctx)->Read16(address);
                          },
                          .read32 = [](uint32 address, void *ctx) -> uint32 {
                              return static_cast<TestSubject *>(ctx)->Read32(address);
                          },
                          .write8 = [](uint32 address, uint8 value,
                                       void *ctx) { static_cast<TestSubject *>(ctx)->Write8(address, value); },
                          .write16 = [](uint32 address, uint16 value,
                                        void *ctx) { static_cast<TestSubject *>(ctx)->Write16(address, value); },
                          .write32 = [](uint32 address, uint32 value,
                                        void *ctx) { static_cast<TestSubject *>(ctx)->Write32(address, value); },
                      });
    }

    void ClearCaptures() const {
        interrupts.clear();
        memoryAccesses.clear();
        intrAcked = false;
    }

    void ClearMemoryMocks() const {
        // TODO
    }

    template <mem_primitive T>
    void MockMemoryRead(uint32 address, T value) const {
        // TODO
    }

    // -------------------------------------------------------------------------
    // Callbacks

    void IntrAck() {
        intrAcked = true;
    }

    // -------------------------------------------------------------------------
    // Memory accessors

    uint8 Read8(uint32 address) {
        memoryAccesses.push_back({address, 0, false, sizeof(uint8)});
        // TODO: return mocked data if present
        return 0;
    }

    uint16 Read16(uint32 address) {
        memoryAccesses.push_back({address, 0, false, sizeof(uint16)});
        // TODO: return mocked data if present
        return 0;
    }

    uint32 Read32(uint32 address) {
        memoryAccesses.push_back({address, 0, false, sizeof(uint32)});
        // TODO: return mocked data if present
        return 0;
    }

    void Write8(uint32 address, uint8 value) {
        memoryAccesses.push_back({address, value, true, sizeof(uint8)});
    }

    void Write16(uint32 address, uint16 value) {
        memoryAccesses.push_back({address, value, true, sizeof(uint16)});
    }

    void Write32(uint32 address, uint32 value) {
        memoryAccesses.push_back({address, value, true, sizeof(uint32)});
    }

    // -------------------------------------------------------------------------
    // ISH2Tracer implementation

    void Interrupt(uint8 vecNum, uint8 level) override {
        interrupts.push_back({vecNum, level});
    }

    // -------------------------------------------------------------------------
    // Traces and mocked data

    struct InterruptInfo {
        uint8 vecNum;
        uint8 level;
    };

    struct MemoryAccessInfo {
        uint32 address;
        uint32 data;
        bool write;
        uint32 size;
    };

    mutable std::vector<InterruptInfo> interrupts;
    mutable std::vector<MemoryAccessInfo> memoryAccesses;
    mutable bool intrAcked;
};

// -----------------------------------------------------------------------------
// Tests

// Test full interrupt flow:
// - Entry and exit (with RTE instruction)
// - VBR handling
// - External interrupt vector fetch and acknowledgement
TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupt flow works correctly", "[sh2][intc][flow]") {
    sh2.Reset(true);
    ClearCaptures();
    ClearMemoryMocks();

    constexpr uint32 startPC = 0x1000;
    constexpr uint32 startSP = 0x2000;
    constexpr uint8 startILevel = 0;
    constexpr uint32 intrPC1 = 0x10000;
    constexpr uint32 intrPC2 = 0x20000;
    constexpr uint32 startVBR1 = 0;
    constexpr uint32 startVBR2 = 0x100000;
    constexpr uint8 intrVec = 0x70;
    constexpr uint8 intrLevel = 5;

    constexpr uint16 instrRTE = 0x002B;

    // Setup interrupt handlers with just the RTE instruction
    MockMemoryRead<uint16>(intrPC1, instrRTE);
    MockMemoryRead<uint16>(intrPC2, instrRTE);

    // Setup interrupt vectors at two different locations
    MockMemoryRead<uint32>(startVBR1 + intrVec * sizeof(uint32), intrPC1);
    MockMemoryRead<uint32>(startVBR2 + intrVec * sizeof(uint32), intrPC2);

    // TODO: need to somehow preserve memory writes to the stack

    sh2::PrivateAccess::PC(sh2) = startPC;    // point PC somewhere
    sh2::PrivateAccess::R(sh2)[15] = startSP; // point stack pointer elsewhere
    sh2::PrivateAccess::VBR(sh2) = startVBR1; // point VBR to the first table
    sh2::PrivateAccess::SR(sh2).ILevel = startILevel;
    sh2::PrivateAccess::INTC(sh2).ICR.VECMD = 1; // use external interrupt vector
    sh2::PrivateAccess::INTC(sh2).SetVector(sh2::InterruptSource::IRL, intrVec);
    sh2::PrivateAccess::INTC(sh2).SetLevel(sh2::InterruptSource::IRL, intrLevel);
    sh2::PrivateAccess::RaiseInterrupt(sh2, sh2::InterruptSource::IRL);
    REQUIRE(sh2::PrivateAccess::CheckInterrupts(sh2));

    // Jump to interrupt handler
    sh2.Advance<true>(1);

    REQUIRE(interrupts.size() == 1);
    CHECK(interrupts[0].vecNum == intrVec);
    CHECK(interrupts[0].level == intrLevel);
    CHECK(intrAcked);
    CHECK(sh2.GetPC() == intrPC1);
    CHECK(sh2.GetGPRs()[15] == startSP - 8); // should write PC and SR
    CHECK(sh2.GetSR().ILevel == intrLevel);
    // TODO: check memory accesses

    ClearCaptures();

    // This should be the RTE instruction
    sh2.Advance<true>(1);

    REQUIRE(interrupts.empty());
    CHECK(sh2.GetPC() == startPC);       // TODO: should be back to the starting PC
    CHECK(sh2.GetGPRs()[15] == startSP); // should read back PC and SR
    CHECK(sh2.GetSR().ILevel == startILevel);
    // TODO: check memory accesses

    // -----

    sh2::PrivateAccess::VBR(sh2) = startVBR2; // point VBR to the second table
    sh2::PrivateAccess::RaiseInterrupt(sh2, sh2::InterruptSource::IRL);
    REQUIRE(sh2::PrivateAccess::CheckInterrupts(sh2));

    // Jump to interrupt handler
    sh2.Advance<true>(1);

    REQUIRE(interrupts.size() == 1);
    CHECK(interrupts[0].vecNum == intrVec);
    CHECK(interrupts[0].level == intrLevel);
    CHECK(intrAcked);
    CHECK(sh2.GetPC() == intrPC2);
    CHECK(sh2.GetGPRs()[15] == startSP - 8); // should write PC and SR
    CHECK(sh2.GetSR().ILevel == intrLevel);
    // TODO: check memory accesses

    ClearCaptures();

    // This should be the RTE instruction
    sh2.Advance<true>(1);

    REQUIRE(interrupts.empty());
    CHECK(sh2.GetPC() == startPC);       // TODO: should be back to the starting PC
    CHECK(sh2.GetGPRs()[15] == startSP); // should read back PC and SR
    CHECK(sh2.GetSR().ILevel == startILevel);
    // TODO: check memory accesses
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are handled correctly", "[sh2][intc][single]") {
    sh2.Reset(true);
    ClearCaptures();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test every interrupt individually, including NMI

    /*
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
    */

    SECTION("WDT ITI interrupt") {
        sh2::PrivateAccess::INTC(sh2).SetVector(sh2::InterruptSource::WDT_ITI, 0x70);
        sh2::PrivateAccess::INTC(sh2).SetLevel(sh2::InterruptSource::WDT_ITI, 5);
        sh2::PrivateAccess::RaiseInterrupt(sh2, sh2::InterruptSource::WDT_ITI);
        REQUIRE(sh2::PrivateAccess::CheckInterrupts(sh2));

        sh2.Advance<true>(1);

        REQUIRE(interrupts.size() == 1);
        CHECK(interrupts[0].vecNum == 0x70);
        CHECK(interrupts[0].level == 5);
        CHECK(sh2.GetPC() == 0); // TODO: proper PC
        CHECK(sh2.GetSR().ILevel == 5);
        // TODO: check memory accesses
    }
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are raised from sources", "[sh2][intc][sources]") {
    sh2.Reset(true);
    ClearCaptures();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test interrupts being raised by the actual sources
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are prioritized correctly", "[sh2][intc][priorities]") {
    sh2.Reset(true);
    ClearCaptures();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test interrupt priorities
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are masked correctly", "[sh2][intc][level-mask]") {
    sh2.Reset(true);
    ClearCaptures();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test interrupts being masked by SR.ILevel
}

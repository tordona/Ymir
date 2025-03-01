#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <map>
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

namespace sh2_intr {

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

    void ClearAll() const {
        sh2.Reset(true);
        ClearCaptures();
        ClearMemoryMocks();
    }

    void ClearCaptures() const {
        interrupts.clear();
        memoryAccesses.clear();
        intrAcked = false;
    }

    void ClearMemoryMocks() const {
        mockedReads8.clear();
        mockedReads16.clear();
        mockedReads32.clear();
    }

    void MockMemoryRead8(uint32 address, uint8 value) const {
        mockedReads8[address] = value;
    }

    void MockMemoryRead16(uint32 address, uint16 value) const {
        mockedReads16[address] = value;
    }

    void MockMemoryRead32(uint32 address, uint32 value) const {
        mockedReads32[address] = value;
    }

    // -------------------------------------------------------------------------
    // Callbacks

    void IntrAck() {
        intrAcked = true;
    }

    // -------------------------------------------------------------------------
    // Memory accessors

    uint8 Read8(uint32 address) {
        auto it = mockedReads8.find(address);
        const uint8 value = it != mockedReads8.end() ? it->second : 0;
        memoryAccesses.push_back({address, value, false, sizeof(uint8)});
        return value;
    }

    uint16 Read16(uint32 address) {
        auto it = mockedReads16.find(address);
        const uint16 value = it != mockedReads16.end() ? it->second : 0;
        memoryAccesses.push_back({address, value, false, sizeof(uint16)});
        return value;
    }

    uint32 Read32(uint32 address) {
        auto it = mockedReads32.find(address);
        const uint32 value = it != mockedReads32.end() ? it->second : 0;
        memoryAccesses.push_back({address, value, false, sizeof(uint32)});
        return value;
    }

    void Write8(uint32 address, uint8 value) {
        MockMemoryRead8(address, value);
        memoryAccesses.push_back({address, value, true, sizeof(uint8)});
    }

    void Write16(uint32 address, uint16 value) {
        MockMemoryRead16(address, value);
        memoryAccesses.push_back({address, value, true, sizeof(uint16)});
    }

    void Write32(uint32 address, uint32 value) {
        MockMemoryRead32(address, value);
        memoryAccesses.push_back({address, value, true, sizeof(uint32)});
    }

    // -------------------------------------------------------------------------
    // ISH2Tracer implementation

    void Interrupt(uint8 vecNum, uint8 level) override {
        interrupts.push_back({vecNum, level});
    }

    // TODO: trace exceptions

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

        constexpr auto operator<=>(const MemoryAccessInfo &) const = default;
    };

    mutable std::vector<InterruptInfo> interrupts;
    mutable std::vector<MemoryAccessInfo> memoryAccesses;
    mutable bool intrAcked;

    mutable std::map<uint32, uint8> mockedReads8;
    mutable std::map<uint32, uint16> mockedReads16;
    mutable std::map<uint32, uint32> mockedReads32;
};

// -----------------------------------------------------------------------------
// Tests

// Test full interrupt flow:
// - Entry and exit (with RTE instruction)
// - VBR handling
// - External interrupt vector fetch and acknowledgement
TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupt flow works correctly", "[sh2][intc][flow]") {
    ClearAll();

    constexpr uint32 startPC = 0x1000;
    constexpr uint32 startSP = 0x2000;
    constexpr uint32 startSR = 0x00000000 | (0x0 << 4);
    constexpr uint32 intrPC1 = 0x10000;
    constexpr uint32 intrPC2 = 0x20000;
    constexpr uint32 startVBR1 = 0;
    constexpr uint32 startVBR2 = 0x100000;
    constexpr uint8 intrVec = 0x70;
    constexpr uint8 intrLevel = 5;

    constexpr uint16 instrNOP = 0x0009;
    constexpr uint16 instrRTE = 0x002B;

    // Setup interrupt handlers with NOP, RTE, NOP (delay slot)
    MockMemoryRead16(intrPC1 + 0, instrNOP);
    MockMemoryRead16(intrPC1 + 2, instrRTE);
    MockMemoryRead16(intrPC1 + 4, instrNOP);

    MockMemoryRead16(intrPC2 + 0, instrNOP);
    MockMemoryRead16(intrPC2 + 2, instrRTE);
    MockMemoryRead16(intrPC2 + 4, instrNOP);

    // Setup interrupt vectors at two different locations
    MockMemoryRead32(startVBR1 + intrVec * sizeof(uint32), intrPC1);
    MockMemoryRead32(startVBR2 + intrVec * sizeof(uint32), intrPC2);

    sh2::PrivateAccess::PC(sh2) = startPC;    // point PC somewhere
    sh2::PrivateAccess::R(sh2)[15] = startSP; // point stack pointer elsewhere
    sh2::PrivateAccess::VBR(sh2) = startVBR1; // point VBR to the first table
    sh2::PrivateAccess::SR(sh2).u32 = startSR;
    sh2::PrivateAccess::INTC(sh2).ICR.VECMD = 1; // use external interrupt vector
    sh2::PrivateAccess::INTC(sh2).SetVector(sh2::InterruptSource::IRL, intrVec);
    sh2::PrivateAccess::INTC(sh2).SetLevel(sh2::InterruptSource::IRL, intrLevel);
    sh2::PrivateAccess::RaiseInterrupt(sh2, sh2::InterruptSource::IRL);
    REQUIRE(sh2::PrivateAccess::CheckInterrupts(sh2));

    // Jump to interrupt handler and execute first instruction (should be a NOP)
    sh2.Advance<true>(1);

    // Check results:
    // - one interrupt of the specified vector+level
    REQUIRE(interrupts.size() == 1);
    CHECK(interrupts[0].vecNum == intrVec);
    CHECK(interrupts[0].level == intrLevel);
    // - external interrupt acknowledged
    CHECK(intrAcked);
    // - PC at the interrupt vector + 2 (since it executed one instruction of the interrupt handler)
    CHECK(sh2.GetPC() == intrPC1 + 2);
    // - PC and SR pushed to the stack
    CHECK(sh2.GetGPRs()[15] == startSP - 8); // should write PC and SR
    // - SR.I3-0 set to the interrupt level
    CHECK(sh2.GetSR().ILevel == intrLevel);
    // - memory accesses:
    //   [0] push SR to stack
    //   [1] push PC-4 to stack
    //   [2] read PC from VBR + vector*4
    //   [3] read NOP instruction from PC
    REQUIRE(memoryAccesses.size() == 4);
    CHECK(memoryAccesses[0] == MemoryAccessInfo{startSP - 4, startSR, true, sizeof(uint32)});
    CHECK(memoryAccesses[1] == MemoryAccessInfo{startSP - 8, startPC - 4, true, sizeof(uint32)});
    CHECK(memoryAccesses[2] == MemoryAccessInfo{startVBR1 + intrVec * sizeof(uint32), intrPC1, false, sizeof(uint32)});
    CHECK(memoryAccesses[3] == MemoryAccessInfo{intrPC1, instrNOP, false, sizeof(uint16)});

    ClearCaptures();

    // This should be the RTE instruction
    sh2.Advance<true>(1);

    // Check results:
    // - no interrupts
    REQUIRE(interrupts.empty());
    // - PC at the NOP instruction in the delay slot of the RTE
    CHECK(sh2.GetPC() == intrPC1 + 4);
    // - PC and SR popped from the stack
    CHECK(sh2.GetGPRs()[15] == startSP);
    // - SR.I3-0 set to the previous value
    CHECK(sh2.GetSR().u32 == startSR);
    // - memory accesses:
    //   [0] read RTE instruction from PC
    //   [1] pop PC from stack
    //   [2] pop SR from stack
    REQUIRE(memoryAccesses.size() == 3);
    CHECK(memoryAccesses[0] == MemoryAccessInfo{intrPC1 + 2, instrRTE, false, sizeof(uint16)});
    CHECK(memoryAccesses[1] == MemoryAccessInfo{startSP - 8, startPC - 4, false, sizeof(uint32)});
    CHECK(memoryAccesses[2] == MemoryAccessInfo{startSP - 4, startSR, false, sizeof(uint32)});

    ClearCaptures();

    // This should be the NOP instruction in the delay slot
    sh2.Advance<true>(1);

    // Check results:
    // - no interrupts
    REQUIRE(interrupts.empty());
    // - PC back to the starting point
    CHECK(sh2.GetPC() == startPC);
    // - no stack operations
    CHECK(sh2.GetGPRs()[15] == startSP);
    // - no changes to SR
    CHECK(sh2.GetSR().u32 == startSR);
    // - memory accesses:
    //   [0] read NOP instruction from PC
    REQUIRE(memoryAccesses.size() == 1);
    CHECK(memoryAccesses[0] == MemoryAccessInfo{intrPC1 + 4, instrNOP, false, sizeof(uint16)});

    ClearCaptures();

    // -----

    sh2::PrivateAccess::VBR(sh2) = startVBR2; // point VBR to the second table
    sh2::PrivateAccess::RaiseInterrupt(sh2, sh2::InterruptSource::IRL);
    REQUIRE(sh2::PrivateAccess::CheckInterrupts(sh2));

    // Jump to interrupt handler and execute first instruction (should be a NOP)
    sh2.Advance<true>(1);

    // Check results:
    // - one interrupt of the specified vector+level
    REQUIRE(interrupts.size() == 1);
    CHECK(interrupts[0].vecNum == intrVec);
    CHECK(interrupts[0].level == intrLevel);
    // - external interrupt acknowledged
    CHECK(intrAcked);
    // - PC at the interrupt vector + 2 (since it executed one instruction of the interrupt handler)
    CHECK(sh2.GetPC() == intrPC2 + 2);
    // - PC and SR pushed to the stack
    CHECK(sh2.GetGPRs()[15] == startSP - 8); // should write PC and SR
    // - SR.I3-0 set to the interrupt level
    CHECK(sh2.GetSR().ILevel == intrLevel);
    // - memory accesses:
    //   [0] push SR to stack
    //   [1] push PC-4 to stack
    //   [2] read PC from VBR + vector*4
    //   [3] read NOP instruction from PC
    REQUIRE(memoryAccesses.size() == 4);
    CHECK(memoryAccesses[0] == MemoryAccessInfo{startSP - 4, startSR, true, sizeof(uint32)});
    CHECK(memoryAccesses[1] == MemoryAccessInfo{startSP - 8, startPC - 4, true, sizeof(uint32)});
    CHECK(memoryAccesses[2] == MemoryAccessInfo{startVBR2 + intrVec * sizeof(uint32), intrPC2, false, sizeof(uint32)});
    CHECK(memoryAccesses[3] == MemoryAccessInfo{intrPC2, instrNOP, false, sizeof(uint16)});

    ClearCaptures();

    // This should be the RTE instruction
    sh2.Advance<true>(1);

    // Check results:
    // - no interrupts
    REQUIRE(interrupts.empty());
    // - PC at the NOP instruction in the delay slot of the RTE
    CHECK(sh2.GetPC() == intrPC2 + 4);
    // - PC and SR popped from the stack
    CHECK(sh2.GetGPRs()[15] == startSP);
    // - SR.I3-0 set to the previous value
    CHECK(sh2.GetSR().u32 == startSR);
    // - memory accesses:
    //   [0] read RTE instruction from PC
    //   [1] pop PC from stack
    //   [2] pop SR from stack
    REQUIRE(memoryAccesses.size() == 3);
    CHECK(memoryAccesses[0] == MemoryAccessInfo{intrPC2 + 2, instrRTE, false, sizeof(uint16)});
    CHECK(memoryAccesses[1] == MemoryAccessInfo{startSP - 8, startPC - 4, false, sizeof(uint32)});
    CHECK(memoryAccesses[2] == MemoryAccessInfo{startSP - 4, startSR, false, sizeof(uint32)});

    ClearCaptures();

    // This should be the NOP instruction in the delay slot
    sh2.Advance<true>(1);

    // Check results:
    // - no interrupts
    REQUIRE(interrupts.empty());
    // - PC back to the starting point
    CHECK(sh2.GetPC() == startPC);
    // - no stack operations
    CHECK(sh2.GetGPRs()[15] == startSP);
    // - no changes to SR
    CHECK(sh2.GetSR().u32 == startSR);
    // - memory accesses:
    //   [0] read NOP instruction from PC
    REQUIRE(memoryAccesses.size() == 1);
    CHECK(memoryAccesses[0] == MemoryAccessInfo{intrPC2 + 4, instrNOP, false, sizeof(uint16)});
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are handled correctly", "[sh2][intc][single]") {
    ClearAll();

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
    ClearAll();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test interrupts being raised by the actual sources
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are prioritized correctly", "[sh2][intc][priorities]") {
    ClearAll();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test interrupt priorities
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are masked correctly", "[sh2][intc][level-mask]") {
    ClearAll();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test interrupts being masked by SR.ILevel
}

} // namespace sh2_intr

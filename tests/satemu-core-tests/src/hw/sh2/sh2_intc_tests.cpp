#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <fmt/format.h>

#include <vector>

namespace satemu::sh2 {

struct PrivateAccess {
    static RegSR &SR(SH2 &sh2) {
        return sh2.SR;
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

using namespace satemu;

struct TestSubject : debug::ISH2Tracer {
    mutable sys::Bus bus{};
    mutable sh2::SH2 sh2{bus, true};

    // TODO: cleanup Bus + SH2 initialization to not output those "unhandled 32-bit read" log messages

    TestSubject() {
        // Setup tracer to collect interrupts into a vector
        sh2.UseTracer(this);

        // TODO: set up predictable vector table
        // TODO: record all accesses
        bus.MapMemory(0x00000, 0x10000,
                      {
                          .read8 = [](uint32 address, void *) -> uint8 { return 0; },
                          .read16 = [](uint32 address, void *) -> uint16 { return 0; },
                          .read32 = [](uint32 address, void *) -> uint32 { return 0; },
                      });
    }

    struct InterruptInfo {
        uint8 vecNum;
        uint8 level;
    };

    mutable std::vector<InterruptInfo> interrupts;

    // -------------------------------------------------------------------------
    // ISH2Tracer implementation

    void Interrupt(uint8 vecNum, uint8 level) override {
        interrupts.push_back({vecNum, level});
    }
};

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are handled correctly", "[sh2][intc][single]") {
    sh2.Reset(true);
    interrupts.clear();

    sh2::PrivateAccess::SR(sh2).ILevel = 0;

    // TODO: test every interrupt individually, including NMI

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
        // TODO: check memory reads and writes
    }
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are raised from sources", "[sh2][intc][sources]") {
    sh2.Reset(true);

    // TODO: test interrupts being raised by the actual sources
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts priorities are handled correctly",
                             "[sh2][intc][priorities]") {
    sh2.Reset(true);

    // TODO: test interrupt priorities
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupt masking is handled correctly", "[sh2][intc][level-mask]") {
    sh2.Reset(true);

    // TODO: test interrupts being masked by SR.ILevel
}

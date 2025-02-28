#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/sh2/sh2.hpp>

#include <fmt/format.h>

struct TestSubject {
    mutable satemu::sys::Bus bus{};
    mutable satemu::sh2::SH2 sh2{bus, true};

    // TODO: cleanup Bus + SH2 initialization to not output those "unhandled 32-bit read" log messages

    TestSubject() {
        // TODO: setup tracer to collect interrupts into a vector
        // sh2.UseTracer();

        // TODO: set up predictable vector table
        // TODO: record all accesses
        bus.MapMemory(0x00000, 0x10000,
                      {
                          .read8 = [](uint32 address, void *) -> uint8 { return 0; },
                          .read16 = [](uint32 address, void *) -> uint16 { return 0; },
                          .read32 = [](uint32 address, void *) -> uint32 { return 0; },
                      });
    }
};

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts are handled correctly", "[sh2][intc][single]") {
    sh2.Reset(true);

    // TODO: test every interrupt individually, including NMI

    SECTION("WDT ITI interrupt") {
        // TODO: raise interrupt
        // TODO: check that the CPU needs to handle interrupt on next cycle

        sh2.Advance<true>(1);

        // TODO: check interrupt triggered on tracer
        CHECK(sh2.GetPC() == 0);
        // TODO: check memory reads and writes
    }
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupts priorities are handled correctly", "[sh2][intc][priority]") {
    sh2.Reset(true);

    // TODO: test interrupt priorities
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 interrupt masking is handled correctly", "[sh2][intc][mask]") {
    sh2.Reset(true);

    // TODO: test interrupts being masked by SR.ILevel
}

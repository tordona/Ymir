#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <satemu/hw/sh2/sh2_divu.hpp>

#include <satemu/core/types.hpp>

struct TestData {
    struct Regs {
        uint32 DVSR;
        uint32 DVDNT;
        uint32 DVDNTL;
        uint32 DVDNTH;
        uint32 DVDNTUL;
        uint32 DVDNTUH;
        uint32 DVCR;

        constexpr auto operator<=>(const Regs &) const = default;
    };

    Regs input;
    Regs output32;
    Regs output64;
};

struct TestSubject {
    mutable satemu::sh2::DivisionUnit divu{};

    TestData::Regs Compute32(const TestData::Regs &input) const {
        divu.DVSR = input.DVSR;
        divu.DVDNT = input.DVDNT;
        divu.DVDNTL = input.DVDNTL;
        divu.DVDNTH = input.DVDNTH;
        divu.DVDNTUL = input.DVDNTUL;
        divu.DVDNTUH = input.DVDNTUH;
        divu.DVCR.u32 = input.DVCR;

        divu.Calc32();
        return {divu.DVSR, divu.DVDNT, divu.DVDNTL, divu.DVDNTH, divu.DVDNTUL, divu.DVDNTUH, divu.DVCR.u32};
    }

    TestData::Regs Compute64(const TestData::Regs &input) const {
        divu.DVSR = input.DVSR;
        divu.DVDNT = input.DVDNT;
        divu.DVDNTL = input.DVDNTL;
        divu.DVDNTH = input.DVDNTH;
        divu.DVDNTUL = input.DVDNTUL;
        divu.DVDNTUH = input.DVDNTUH;
        divu.DVCR.u32 = input.DVCR;

        divu.Calc64();
        return {divu.DVSR, divu.DVDNT, divu.DVDNTL, divu.DVDNTH, divu.DVDNTUL, divu.DVDNTUH, divu.DVCR.u32};
    }
};

constexpr auto testdata = {
    TestData{.input = {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00},
             .output32 = {0x00000000, 0x7FFFFFFF, 0x7FFFFFFF, 0x00000000, 0x7FFFFFFF, 0x00000000, 0x01},
             .output64 = {0x00000000, 0x7FFFFFFF, 0x7FFFFFFF, 0x00000000, 0x7FFFFFFF, 0x00000000, 0x01}},
    TestData{.input = {0xFFFFFFFF, 0x80000000, 0x80000000, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFF, 0x00},
             .output32 = {0xFFFFFFFF, 0x80000000, 0x80000000, 0x00000000, 0x80000000, 0x00000000, 0x00},
             .output64 = {0xFFFFFFFF, 0x80000000, 0x80000000, 0x00000000, 0x80000000, 0x00000000, 0x00}},
#include "sh2_divu_testdata.inc"
};

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SH2 DIVU divisions are computed correctly", "[sh2][divu]") {
    const auto &testData = GENERATE(values<TestData>(testdata));

    CHECK(Compute32(testData.input) == testData.output32);
    CHECK(Compute64(testData.input) == testData.output64);
}

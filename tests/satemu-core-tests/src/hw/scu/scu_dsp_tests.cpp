#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/scu/scu_dsp.hpp>

using namespace satemu;

namespace scu_dsp {

struct TestSubject {
    mutable sys::Bus bus;
    mutable scu::SCUDSP dsp{bus};

    bool dspEndTriggered = false;

    TestSubject() {
        dsp.SetTriggerDSPEndCallback(util::MakeClassMemberRequiredCallback<&TestSubject::TriggerDSPEnd>(this));
    }

    void ClearAll() {
        dspEndTriggered = false;
    }

    void TriggerDSPEnd() {
        dspEndTriggered = true;
    }
};

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SCU DSP instructions execute correctly", "[scu][scudsp][instructions]") {
    dsp.Reset(true);

    SECTION("ALU - AND") {}

    SECTION("ALU - OR") {}

    SECTION("ALU - XOR") {}

    SECTION("ALU - ADD") {}

    SECTION("ALU - SUB") {}

    SECTION("ALU - AD2") {}

    SECTION("ALU - SR") {}

    SECTION("ALU - RR") {}

    SECTION("ALU - SL") {}

    SECTION("ALU - RL") {}

    SECTION("ALU - RL8") {}

    SECTION("X-Bus - MOV MUL,P") {}

    // [s] = CT0..CT3, MC0..MC3
    SECTION("X-Bus - MOV [s],P") {}

    // [s] = CT0..CT3, MC0..MC3
    SECTION("X-Bus - MOV [s],X") {}

    SECTION("Y-Bus - CLR A") {}

    SECTION("Y-Bus - MOV ALU,A") {}

    // [s] = CT0..CT3, MC0..MC3
    SECTION("Y-Bus - MOV [s],A") {}

    // [s] = CT0..CT3, MC0..MC3
    SECTION("Y-Bus - MOV [s],Y") {}

    // [d] = CT0..CT3, MC0..MC3, RX, P, RA0, WA0, LOP, TOP
    SECTION("D1-Bus - MOV SImm,[d]") {}

    // [s] = CT0..CT3, MC0..MC3, ALUL, ALUH
    // [d] = CT0..CT3, MC0..MC3, RX, P, RA0, WA0, LOP, TOP
    SECTION("D1-Bus - MOV [s],[d]") {}

    SECTION("Combined - AND  MOV MUL,P  MOV[s],X  MOV ALU,A  MOV [s],Y  MOV [s],[d]") {}

    // [d] =
    SECTION("MVI SImm,[d]") {}
}

// TODO: test complete programs

// TODO: test DMA

} // namespace scu_dsp
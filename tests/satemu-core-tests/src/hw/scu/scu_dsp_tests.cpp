#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/scu/scu_dsp.hpp>

using namespace satemu;

namespace scu_dsp {

struct TestSubject {
    mutable sys::Bus bus;
    mutable scu::SCUDSP dsp{bus};

    mutable bool dspEndTriggered = false;

    TestSubject() {
        dsp.SetTriggerDSPEndCallback(util::MakeClassMemberRequiredCallback<&TestSubject::TriggerDSPEnd>(this));
    }

    void ClearAll() const {
        dsp.Reset(true);
        dspEndTriggered = false;
    }

    void TriggerDSPEnd() {
        dspEndTriggered = true;
    }
};

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SCU DSP ALU operations compute correctly", "[scu][scudsp][alu]") {
    ClearAll();

    SECTION("AND") {
        dsp.zero = true;
        dsp.sign = true;
        dsp.carry = true;
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0xCFF00;
            dsp.ALU_AND();

            CHECK(dsp.ALU.L == 0x8F000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
        }

        SECTION("zero") {
            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0x20FF0;
            dsp.ALU_AND();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
        }

        SECTION("sign") {
            dsp.AC.L = 0x8001234F;
            dsp.P.L = 0x8005678F;
            dsp.ALU_AND();

            CHECK(dsp.ALU.L == 0x8001230F);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
        }

        // Carry should always be false
        CHECK(dsp.carry == false);

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("OR") {
        dsp.zero = true;
        dsp.sign = true;
        dsp.carry = true;
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0xCFF00;
            dsp.ALU_OR();

            CHECK(dsp.ALU.L == 0xDFF0F);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
        }

        SECTION("zero") {
            dsp.AC.L = 0;
            dsp.P.L = 0;
            dsp.ALU_OR();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
        }

        SECTION("sign") {
            dsp.AC.L = 0x8001234F;
            dsp.P.L = 0x8005678F;
            dsp.ALU_OR();

            CHECK(dsp.ALU.L == 0x800567CF);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
        }

        // Carry should always be false
        CHECK(dsp.carry == false);

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("XOR") {
        dsp.zero = true;
        dsp.sign = true;
        dsp.carry = true;
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0xCFF00;
            dsp.ALU_XOR();

            CHECK(dsp.ALU.L == 0x50F0F);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
        }

        SECTION("zero") {
            dsp.AC.L = 0x1234;
            dsp.P.L = 0x1234;
            dsp.ALU_XOR();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
        }

        SECTION("sign") {
            dsp.AC.L = 0x8001234F;
            dsp.P.L = 0x0005678F;
            dsp.ALU_XOR();

            CHECK(dsp.ALU.L == 0x800444C0);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
        }

        // Carry should always be false
        CHECK(dsp.carry == false);

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("ADD") {
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 123;
            dsp.P.L = 321;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == 444);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0;
            dsp.P.L = 0;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero, carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0xFFFFFFFF;
            dsp.P.L = 1;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero, carry, overflow") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0x80000000;
            dsp.P.L = 0x80000000;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == true);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = -123;
            dsp.P.L = 1;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == -122);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("sign, carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = -123;
            dsp.P.L = -1;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == -124);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == false);
        }

        SECTION("sign, overflow") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0x7FFFFFFF;
            dsp.P.L = 1;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == 0x80000000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == true);
        }

        SECTION("carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 100;
            dsp.P.L = -1;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == 99);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == false);
        }

        SECTION("carry, overflow") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0x80000000;
            dsp.P.L = -1;
            dsp.ALU_ADD();

            CHECK(dsp.ALU.L == 0x7FFFFFFF);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == true);
        }

        // ALU.H should not be modified
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("SUB") {
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 321;
            dsp.P.L = 123;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == 198);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero (with zeros)") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0;
            dsp.P.L = 0;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero (with positives)") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0x7FFFFFFF;
            dsp.P.L = 0x7FFFFFFF;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero (with negatives)") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0x80000000;
            dsp.P.L = 0x80000000;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = -123;
            dsp.P.L = 1;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == -124);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("sign, carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 1;
            dsp.P.L = 123;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == -122);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == false);
        }

        SECTION("sign, carry, overflow") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 1;
            dsp.P.L = 0x80000001;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == 0x80000000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == true);
        }

        SECTION("overflow") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.L = 0x80000000;
            dsp.P.L = 0x7FFFFFFF;
            dsp.ALU_SUB();

            CHECK(dsp.ALU.L == 1);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == true);
        }

        // ALU.H should not be modified
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("AD2") {}

    SECTION("SR") {}

    SECTION("RR") {}

    SECTION("SL") {}

    SECTION("RL") {}

    SECTION("RL8") {}
}

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
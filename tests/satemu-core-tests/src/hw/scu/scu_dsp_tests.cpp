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
        dsp.carry = true;
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;

            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0xCFF00;
            dsp.ALU_AND();

            CHECK(dsp.ALU.L == 0x8F000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;

            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0x20FF0;
            dsp.ALU_AND();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = false;

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
        dsp.carry = true;
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;

            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0xCFF00;
            dsp.ALU_OR();

            CHECK(dsp.ALU.L == 0xDFF0F);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;

            dsp.AC.L = 0;
            dsp.P.L = 0;
            dsp.ALU_OR();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = false;

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
        dsp.carry = true;
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;

            dsp.AC.L = 0x9F00F;
            dsp.P.L = 0xCFF00;
            dsp.ALU_XOR();

            CHECK(dsp.ALU.L == 0x50F0F);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;

            dsp.AC.L = 0x1234;
            dsp.P.L = 0x1234;
            dsp.ALU_XOR();

            CHECK(dsp.ALU.L == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = false;

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

        SECTION("zero (with zeros)") {
            dsp.zero = false;
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
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = false;
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
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = false;
            dsp.overflow = false;

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
            dsp.sign = false;
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
            dsp.sign = false;
            dsp.carry = false;
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
            dsp.sign = false;
            dsp.carry = true;
            dsp.overflow = false;

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
            dsp.carry = false;
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
            dsp.carry = false;
            dsp.overflow = false;

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
            dsp.zero = false;
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
            dsp.zero = false;
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
            dsp.zero = false;
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
            dsp.sign = false;
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
            dsp.sign = false;
            dsp.carry = false;
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
            dsp.sign = false;
            dsp.carry = false;
            dsp.overflow = false;

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
            dsp.overflow = false;

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

    SECTION("AD2") {
        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.u64 = 123;
            dsp.P.u64 = 321;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.u64 == 444);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.u64 = 0;
            dsp.P.u64 = 0;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.u64 == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero, carry") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = false;
            dsp.overflow = true;

            dsp.AC.u64 = -1;
            dsp.P.u64 = 1;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.u64 == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == false);
        }

        SECTION("zero, carry, overflow") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = false;
            dsp.overflow = false;

            dsp.AC.u64 = 0x800000000000;
            dsp.P.u64 = 0x800000000000;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.u64 == 0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == true);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = true;
            dsp.overflow = true;

            dsp.AC.u64 = -123;
            dsp.P.u64 = 1;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.s64 == -122);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == false);
        }

        SECTION("sign, carry") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = false;
            dsp.overflow = true;

            dsp.AC.u64 = -123;
            dsp.P.u64 = -1;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.s64 == -124);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == false);
        }

        SECTION("sign, overflow") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = true;
            dsp.overflow = false;

            dsp.AC.u64 = 0x7FFFFFFFFFFF;
            dsp.P.u64 = 1;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.u64 == 0x800000000000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == false);
            CHECK(dsp.overflow == true);
        }

        SECTION("carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = false;
            dsp.overflow = true;

            dsp.AC.u64 = 100;
            dsp.P.u64 = -1;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.u64 == 99);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == false);
        }

        SECTION("carry, overflow") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = false;
            dsp.overflow = false;

            dsp.AC.u64 = 0x800000000000;
            dsp.P.u64 = -1;
            dsp.ALU_AD2();

            CHECK(dsp.ALU.u64 == 0x7FFFFFFFFFFF);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
            CHECK(dsp.overflow == true);
        }
    }

    SECTION("SR") {
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x10;
            dsp.ALU_SR();

            CHECK(dsp.ALU.L == 0x8);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x0;
            dsp.ALU_SR();

            CHECK(dsp.ALU.L == 0x0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("zero, carry") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = false;

            dsp.AC.L = 0x1;
            dsp.ALU_SR();

            CHECK(dsp.ALU.L == 0x0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
        }

        SECTION("carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = false;

            dsp.AC.L = 0x11;
            dsp.ALU_SR();

            CHECK(dsp.ALU.L == 0x8);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
        }

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("RR") {
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x10;
            dsp.ALU_RR();

            CHECK(dsp.ALU.L == 0x8);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x0;
            dsp.ALU_RR();

            CHECK(dsp.ALU.L == 0x0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("sign, carry") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = false;

            dsp.AC.L = 0x1;
            dsp.ALU_RR();

            CHECK(dsp.ALU.L == 0x80000000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == true);
        }

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("SL") {
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x10;
            dsp.ALU_SL();

            CHECK(dsp.ALU.L == 0x20);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x0;
            dsp.ALU_SL();

            CHECK(dsp.ALU.L == 0x0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("zero, carry") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = false;

            dsp.AC.L = 0x80000000;
            dsp.ALU_SL();

            CHECK(dsp.ALU.L == 0x0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
        }

        SECTION("carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = false;

            dsp.AC.L = 0x80000001;
            dsp.ALU_SL();

            CHECK(dsp.ALU.L == 0x2);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
        }

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("RL") {
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x10;
            dsp.ALU_RL();

            CHECK(dsp.ALU.L == 0x20);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x0;
            dsp.ALU_RL();

            CHECK(dsp.ALU.L == 0x0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = true;

            dsp.AC.L = 0x40000000;
            dsp.ALU_RL();

            CHECK(dsp.ALU.L == 0x80000000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == false);
        }

        SECTION("sign, carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = false;

            dsp.AC.L = 0x80000000;
            dsp.ALU_RL();

            CHECK(dsp.ALU.L == 0x1);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
        }

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

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

    // [d] = CT0..CT3, RX, P, RA0, WA0, LOP, TOP
    SECTION("MVI SImm,[d]") {}

    // [d] = CT0..CT3, RX, P, RA0, WA0, LOP, TOP
    SECTION("MVI <cond>,SImm,[d]") {}
}

// TODO: test complete programs

// TODO: test DMA

} // namespace scu_dsp
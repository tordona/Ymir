#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/scu/scu_dsp.hpp>

#include <string_view>

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

// -----------------------------------------------------------------------------

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

        SECTION("carry") {
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

        SECTION("sign, carry") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = false;

            dsp.AC.L = 0xC0000000;
            dsp.ALU_RL();

            CHECK(dsp.ALU.L == 0x80000001);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == true);
        }

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }

    SECTION("RL8") {
        dsp.overflow = true;
        dsp.ALU.H = 0xDEAD;

        SECTION("no flags") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x10;
            dsp.ALU_RL8();

            CHECK(dsp.ALU.L == 0x1000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("zero") {
            dsp.zero = false;
            dsp.sign = true;
            dsp.carry = true;

            dsp.AC.L = 0x0;
            dsp.ALU_RL8();

            CHECK(dsp.ALU.L == 0x0);
            CHECK(dsp.zero == true);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == false);
        }

        SECTION("sign") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = true;

            dsp.AC.L = 0x800000;
            dsp.ALU_RL8();

            CHECK(dsp.ALU.L == 0x80000000);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == false);
        }

        SECTION("sign, carry") {
            dsp.zero = true;
            dsp.sign = false;
            dsp.carry = false;

            dsp.AC.L = 0x01800000;
            dsp.ALU_RL8();

            CHECK(dsp.ALU.L == 0x80000001);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == true);
            CHECK(dsp.carry == true);
        }

        SECTION("carry") {
            dsp.zero = true;
            dsp.sign = true;
            dsp.carry = false;

            dsp.AC.L = 0x01000000;
            dsp.ALU_RL8();

            CHECK(dsp.ALU.L == 0x00000001);
            CHECK(dsp.zero == false);
            CHECK(dsp.sign == false);
            CHECK(dsp.carry == true);
        }

        // These should not be modified
        CHECK(dsp.overflow == true);
        CHECK(dsp.ALU.H == 0xDEAD);
    }
}

// -----------------------------------------------------------------------------

struct DSPState {
    std::array<uint32, 256> programRAM;
    std::array<std::array<uint32, 64>, 4> dataRAM;

    uint8 PC; // program address

    bool sign;
    bool zero;
    bool carry;
    bool overflow;

    // DSP data address
    std::array<uint8, 4> CT;

    uint64 ALU; // ALU operation output
    uint64 AC;  // ALU operation input 1
    uint64 P;   // ALU operation input 2 / Multiplication output
    sint32 RX;  // Multiplication input 1
    sint32 RY;  // Multiplication input 2

    uint16 LOP; // Loop count
    uint8 TOP;  // loop top

    uint32 RA0; // DMA read address
    uint32 WA0; // DMA write address
};

struct TestData {
    std::string_view name;
    DSPState initialState;
    DSPState finalState;
    uint32 numSteps;
    // TODO: bus accesses
    // TODO: mocked memory
};

constexpr auto testdata = {
    TestData{
        .name = "NOP",
        .initialState =
            {
                .programRAM = {0x00000000},
                .PC = 0,
            },
        .finalState =
            {
                .programRAM = {0x00000000},
                .PC = 1,
            },
        .numSteps = 1,
    },
#include "scu_dsp_testdata.inc"
};

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SCU DSP instructions execute correctly", "[scu][scudsp][instructions]") {
    dsp.Reset(true);

    for (const auto &test : testdata) {
        DYNAMIC_SECTION(test.name) {
            // Initialize DSP state
            dsp.programRAM = test.initialState.programRAM;
            dsp.dataRAM = test.initialState.dataRAM;
            dsp.PC = test.initialState.PC;
            dsp.sign = test.initialState.sign;
            dsp.zero = test.initialState.zero;
            dsp.carry = test.initialState.carry;
            dsp.overflow = test.initialState.overflow;
            dsp.CT = test.initialState.CT;
            dsp.ALU.u64 = test.initialState.ALU;
            dsp.AC.u64 = test.initialState.AC;
            dsp.P.u64 = test.initialState.P;
            dsp.RX = test.initialState.RX;
            dsp.RY = test.initialState.RY;
            dsp.loopCount = test.initialState.LOP;
            dsp.loopTop = test.initialState.TOP;
            dsp.dmaReadAddr = test.initialState.RA0;
            dsp.dmaWriteAddr = test.initialState.WA0;

            // Setup execution
            dsp.PC = 0;
            dsp.programExecuting = true;
            dsp.programEnded = false;
            dsp.programPaused = false;
            dsp.programStep = false;

            // Run for the specified number of cycles
            dsp.Run(test.numSteps);

            // Compare DSP state against expected state
            CHECK(dsp.programRAM == test.finalState.programRAM);
            CHECK(dsp.dataRAM == test.finalState.dataRAM);
            CHECK(dsp.PC == test.finalState.PC);
            CHECK(dsp.sign == test.finalState.sign);
            CHECK(dsp.zero == test.finalState.zero);
            CHECK(dsp.carry == test.finalState.carry);
            CHECK(dsp.overflow == test.finalState.overflow);
            CHECK(dsp.CT == test.finalState.CT);
            CHECK(dsp.ALU.u64 == test.finalState.ALU);
            CHECK(dsp.AC.u64 == test.finalState.AC);
            CHECK(dsp.P.u64 == test.finalState.P);
            CHECK(dsp.RX == test.finalState.RX);
            CHECK(dsp.RY == test.finalState.RY);
            CHECK(dsp.loopCount == test.finalState.LOP);
            CHECK(dsp.loopTop == test.finalState.TOP);
            CHECK(dsp.dmaReadAddr == test.finalState.RA0);
            CHECK(dsp.dmaWriteAddr == test.finalState.WA0);
        }
    }
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SCU DSP loop instructions execute correctly",
                             "[scu][scudsp][instructions]") {
    dsp.Reset(true);

    SECTION("LPS") {
        dsp.programRAM[0] = 0xE8000000; // LPS
        dsp.programRAM[1] = 0x10040000; // ADD  MOV ALU,A
        dsp.AC.u64 = 1;
        dsp.P.u64 = 1;
        dsp.loopCount = 2;

        // Setup execution
        dsp.PC = 0;
        dsp.programExecuting = true;
        dsp.programEnded = false;
        dsp.programPaused = false;
        dsp.programStep = false;

        // Run step 1 - LPS
        // LOP > 0 => set TOP = PC and LOP = LOP-1
        dsp.Run(1);

        CHECK(dsp.PC == 1);
        CHECK(dsp.loopTop == 0);
        CHECK(dsp.loopCount == 1);
        CHECK(dsp.ALU.u64 == 0);
        CHECK(dsp.AC.u64 == 1);
        CHECK(dsp.P.u64 == 1);

        // Run step 2 - ADD  MOV ALU,A
        dsp.Run(1);

        CHECK(dsp.PC == 0);
        CHECK(dsp.loopTop == 0);
        CHECK(dsp.loopCount == 1);
        CHECK(dsp.ALU.u64 == 2);
        CHECK(dsp.AC.u64 == 2);
        CHECK(dsp.P.u64 == 1);

        // Run step 3 - LPS
        // LOP > 0 => set TOP = PC and LOP = LOP-1
        dsp.Run(1);

        CHECK(dsp.PC == 1);
        CHECK(dsp.loopTop == 0);
        CHECK(dsp.loopCount == 0);
        CHECK(dsp.ALU.u64 == 2);
        CHECK(dsp.AC.u64 == 2);
        CHECK(dsp.P.u64 == 1);

        // Run step 4 - ADD  MOV ALU,A
        dsp.Run(1);

        CHECK(dsp.PC == 0);
        CHECK(dsp.loopTop == 0);
        CHECK(dsp.loopCount == 0);
        CHECK(dsp.ALU.u64 == 3);
        CHECK(dsp.AC.u64 == 3);
        CHECK(dsp.P.u64 == 1);

        // Run step 5 - LPS
        // LOP = 0 => skip instruction and set LOP = LOP-1
        dsp.Run(1);

        CHECK(dsp.PC == 1);
        CHECK(dsp.loopTop == 0);
        CHECK(dsp.loopCount == 0xFFF);
        CHECK(dsp.ALU.u64 == 3);
        CHECK(dsp.AC.u64 == 3);
        CHECK(dsp.P.u64 == 1);

        // Run step 6 - ADD  MOV ALU,A
        dsp.Run(1);

        CHECK(dsp.PC == 2);
        CHECK(dsp.loopTop == 0);
        CHECK(dsp.loopCount == 0xFFF);
        CHECK(dsp.ALU.u64 == 4);
        CHECK(dsp.AC.u64 == 4);
        CHECK(dsp.P.u64 == 1);
    }

    SECTION("BTM") {
        dsp.programRAM[0] = 0x00000000; // NOP
        dsp.programRAM[1] = 0x10040000; // ADD  MOV ALU,A
        dsp.programRAM[2] = 0xE0000000; // BTM
        dsp.programRAM[3] = 0x28040000; // SL   MOV ALU,A
        dsp.AC.u64 = 1;
        dsp.P.u64 = 1;
        dsp.loopTop = 1;
        dsp.loopCount = 2;

        // Setup execution
        dsp.PC = 1;
        dsp.programExecuting = true;
        dsp.programEnded = false;
        dsp.programPaused = false;
        dsp.programStep = false;

        // Run step 1 - ADD  MOV ALU,A
        dsp.Run(1);

        CHECK(dsp.PC == 2);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 2);
        CHECK(dsp.ALU.u64 == 2);
        CHECK(dsp.AC.u64 == 2);
        CHECK(dsp.P.u64 == 1);

        // Run step 2 - BTM
        // LOP > 0 => setup delayed jump and set LOP = LOP-1
        dsp.Run(1);

        CHECK(dsp.PC == 3);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 1);
        CHECK(dsp.ALU.u64 == 2);
        CHECK(dsp.AC.u64 == 2);
        CHECK(dsp.P.u64 == 1);

        // Run step 3 - SL   MOV ALU,A
        // Jump back to TOP
        dsp.Run(1);

        CHECK(dsp.PC == 1);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 1);
        CHECK(dsp.ALU.u64 == 4);
        CHECK(dsp.AC.u64 == 4);
        CHECK(dsp.P.u64 == 1);

        // Run step 4 - ADD  MOV ALU,A
        dsp.Run(1);

        CHECK(dsp.PC == 2);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 1);
        CHECK(dsp.ALU.u64 == 5);
        CHECK(dsp.AC.u64 == 5);
        CHECK(dsp.P.u64 == 1);

        // Run step 5 - BTM
        // LOP > 0 => setup delayed jump and set LOP = LOP-1
        dsp.Run(1);

        CHECK(dsp.PC == 3);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 0);
        CHECK(dsp.ALU.u64 == 5);
        CHECK(dsp.AC.u64 == 5);
        CHECK(dsp.P.u64 == 1);

        // Run step 6 - SL   MOV ALU,A
        // Jump back to TOP
        dsp.Run(1);

        CHECK(dsp.PC == 1);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 0);
        CHECK(dsp.ALU.u64 == 10);
        CHECK(dsp.AC.u64 == 10);
        CHECK(dsp.P.u64 == 1);

        // Run step 7 - ADD  MOV ALU,A
        dsp.Run(1);

        CHECK(dsp.PC == 2);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 0);
        CHECK(dsp.ALU.u64 == 11);
        CHECK(dsp.AC.u64 == 11);
        CHECK(dsp.P.u64 == 1);

        // Run step 8 - BTM
        // LOP = 0 => skip instruction and set LOP = LOP-1
        dsp.Run(1);

        CHECK(dsp.PC == 3);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 0xFFF);
        CHECK(dsp.ALU.u64 == 11);
        CHECK(dsp.AC.u64 == 11);
        CHECK(dsp.P.u64 == 1);

        // Run step 9 - SL   MOV ALU,A
        dsp.Run(1);

        CHECK(dsp.PC == 4);
        CHECK(dsp.loopTop == 1);
        CHECK(dsp.loopCount == 0xFFF);
        CHECK(dsp.ALU.u64 == 22);
        CHECK(dsp.AC.u64 == 22);
        CHECK(dsp.P.u64 == 1);
    }
}

TEST_CASE_PERSISTENT_FIXTURE(TestSubject, "SCU DSP end instructions execute correctly", "[scu][scudsp][instructions]") {
    dsp.Reset(true);

    SECTION("END") {
        dsp.programRAM[0] = 0xF0000000; // END

        // Setup execution
        dsp.PC = 0;
        dsp.programExecuting = true;
        dsp.programEnded = false;
        dsp.programPaused = false;
        dsp.programStep = false;

        dsp.Run(1);

        CHECK(dsp.PC == 1);
        CHECK(dsp.programExecuting == false);
        CHECK(dsp.programEnded == true);
        CHECK(dspEndTriggered == false);
    }

    SECTION("ENDI") {
        dsp.programRAM[0] = 0xF8000000; // ENDI

        // Setup execution
        dsp.PC = 0;
        dsp.programExecuting = true;
        dsp.programEnded = false;
        dsp.programPaused = false;
        dsp.programStep = false;

        dsp.Run(1);

        CHECK(dsp.PC == 1);
        CHECK(dsp.programExecuting == false);
        CHECK(dsp.programEnded == true);
        CHECK(dspEndTriggered == true);
    }
}

// TODO: test DMA transfers
// [RAM]=MC0..3 or PRG
// D0=SCU A-Bus, SCU B-Bus or WRAM High
// [s]=M0..M3, MC0..MC3
// "DMA [RAM],D0,SImm"
// "DMA [RAM],D0,[s]"
// "DMAH [RAM],D0,SImm"
// "DMAH [RAM],D0,[s]"
// "DMA D0,[RAM],SImm"
// "DMA D0,[RAM],[s]"
// "DMAH D0,[RAM],SImm"
// "DMAH D0,[RAM],[s]"
//
// Also test NT0 and T0 conditions with:
// "MVI <cond>,SImm,[d]"
// "JMP <cond>,Imm"

// TODO: test complete programs

// TODO: test DSP control (start, stop, pause, step, etc.)

// TODO: test additional behavior
// - program and data RAM cannot be accessed externally while DSP is running

} // namespace scu_dsp

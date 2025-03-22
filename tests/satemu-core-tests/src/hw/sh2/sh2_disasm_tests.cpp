#include <catch2/catch_test_macros.hpp>

#include <satemu/hw/sh2/sh2_disasm.hpp>

using namespace satemu;

TEST_CASE("SH2 disassembler smoke tests", "[sh2][disasm][smoke]") {
    SECTION("mov.l @(0x0600035C), r6") {
        const auto &disasm = sh2::g_disasmTable.disasm[0xD600];

        REQUIRE(disasm.mnemonic == sh2::Mnemonic::MOV);
        CHECK(disasm.opSize == sh2::OperandSize::Long);
        CHECK(disasm.op1.type == sh2::Operand::Type::AtDispPC);
        CHECK(disasm.op1.immDisp == 0);
        CHECK(disasm.op2.type == sh2::Operand::Type::Rn);
        CHECK(disasm.op2.reg == 6);
    }
}
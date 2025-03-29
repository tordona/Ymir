#include "sh2_disassembly_view.hpp"

#include <satemu/hw/sh2/sh2_disasm.hpp>

#include <imgui.h>

namespace app::ui {

SH2DisassemblyView::SH2DisassemblyView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2DisassemblyView::Display() {
    using namespace satemu;

    ImGui::BeginGroup();

    // TODO: compute height
    // TODO: improve design
    // - alternate color lines
    // - breakpoint, watchpoint, PC symbols
    // - branch arrows
    // - cursor

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    auto &probe = m_sh2.GetProbe();
    const uint32 baseAddress = probe.PC() & ~1;
    for (uint32 i = 0; i < 32; i++) {
        const uint32 address = baseAddress + i * sizeof(uint16);
        const uint16 opcode = m_context.saturn.mainBus.Peek<uint16>(address);
        const sh2::OpcodeDisasm &disasm = sh2::Disassemble(opcode);

        auto memRead = [&](uint32 address) -> uint32 {
            switch (disasm.opSize) {
            case sh2::OperandSize::Byte: return probe.MemPeekByte(address, false);
            case sh2::OperandSize::Word: return probe.MemPeekWord(address, false);
            case sh2::OperandSize::Long: return probe.MemPeekLong(address, false);
            default: assert(false); return 0;
            }
        };

        auto getOp = [&](const sh2::Operand &op) -> uint32 {
            switch (op.type) {
            case sh2::Operand::Type::Imm: return op.immDisp;
            case sh2::Operand::Type::Rn: return probe.R(op.reg);
            case sh2::Operand::Type::AtRn: return memRead(probe.R(op.reg));
            case sh2::Operand::Type::AtRnPlus: return memRead(probe.R(op.reg));
            case sh2::Operand::Type::AtMinusRn: return memRead(probe.R(op.reg));
            case sh2::Operand::Type::AtDispRn: return memRead(probe.R(op.reg) + op.immDisp);
            case sh2::Operand::Type::AtR0Rn: return memRead(probe.R(op.reg) + probe.R(0));
            case sh2::Operand::Type::AtDispGBR: return memRead(probe.GBR() + op.immDisp);
            case sh2::Operand::Type::AtR0GBR: return memRead(probe.GBR() + probe.R(0));
            case sh2::Operand::Type::AtDispPC: return memRead(address + op.immDisp);
            case sh2::Operand::Type::AtDispPCWordAlign: return memRead((address & ~3) + op.immDisp);
            case sh2::Operand::Type::DispPC: return memRead(address + op.immDisp);
            case sh2::Operand::Type::RnPC: return op.reg + address;
            case sh2::Operand::Type::SR: return probe.SR().u32;
            case sh2::Operand::Type::GBR: return probe.GBR();
            case sh2::Operand::Type::VBR: return probe.VBR();
            case sh2::Operand::Type::MACH: return probe.MAC().H;
            case sh2::Operand::Type::MACL: return probe.MAC().L;
            case sh2::Operand::Type::PR: return probe.PR();
            default: assert(false); return 0;
            }
        };

        auto getOp1 = [&] { return getOp(disasm.op1); };
        auto getOp2 = [&] { return getOp(disasm.op2); };

        auto filterAscii = [](char c) { return c < 0x20 ? '.' : c; };

        auto drawAddress = [&] {
            ImGui::TextColored(m_colors.disasm.address, "%08X", address);
            ImGui::SameLine(0.0f, m_style.disasmSpacing);
        };

        auto drawOpcode = [&] {
            if (m_settings.displayOpcodeBytes) {
                ImGui::TextColored(m_colors.disasm.bytes, "%04X", opcode);
                ImGui::SameLine(0.0f, m_style.disasmSpacing);
            }
            if (m_settings.displayOpcodeAscii) {
                char ascii[2] = {0};
                ascii[0] = filterAscii((opcode >> 8) & 0xFF);
                ascii[1] = filterAscii((opcode >> 0) & 0xFF);
                ImGui::TextColored(m_colors.disasm.ascii, "%c%c", ascii[0], ascii[1]);
                ImGui::SameLine(0.0f, m_style.disasmSpacing);
            }
        };

        auto drawMnemonic = [&]() {
            std::string_view mnemonic = "(?)";
            std::string_view mnemonicCond = "";
            bool condPass = false;
            std::string_view mnemonicSuffix = "";
            std::string_view sizeSuffix = "";
            bool legal = true;

            switch (disasm.mnemonic) {
            case sh2::Mnemonic::NOP: mnemonic = "nop"; break;
            case sh2::Mnemonic::SLEEP: mnemonic = "sleep"; break;
            case sh2::Mnemonic::MOV: mnemonic = "mov"; break;
            case sh2::Mnemonic::MOVA: mnemonic = "mova"; break;
            case sh2::Mnemonic::MOVT: mnemonic = "movt"; break;
            case sh2::Mnemonic::CLRT: mnemonic = "clrt"; break;
            case sh2::Mnemonic::SETT: mnemonic = "sett"; break;
            case sh2::Mnemonic::EXTU: mnemonic = "extu"; break;
            case sh2::Mnemonic::EXTS: mnemonic = "exts"; break;
            case sh2::Mnemonic::SWAP: mnemonic = "swap"; break;
            case sh2::Mnemonic::XTRCT: mnemonic = "xtrct"; break;
            case sh2::Mnemonic::LDC: mnemonic = "ldc"; break;
            case sh2::Mnemonic::LDS: mnemonic = "lds"; break;
            case sh2::Mnemonic::STC: mnemonic = "stc"; break;
            case sh2::Mnemonic::STS: mnemonic = "sts"; break;
            case sh2::Mnemonic::ADD: mnemonic = "add"; break;
            case sh2::Mnemonic::ADDC: mnemonic = "addc"; break;
            case sh2::Mnemonic::ADDV: mnemonic = "addv"; break;
            case sh2::Mnemonic::AND: mnemonic = "and"; break;
            case sh2::Mnemonic::NEG: mnemonic = "neg"; break;
            case sh2::Mnemonic::NEGC: mnemonic = "negc"; break;
            case sh2::Mnemonic::NOT: mnemonic = "not"; break;
            case sh2::Mnemonic::OR: mnemonic = "or"; break;
            case sh2::Mnemonic::ROTCL: mnemonic = "rotcl"; break;
            case sh2::Mnemonic::ROTCR: mnemonic = "rotcr"; break;
            case sh2::Mnemonic::ROTL: mnemonic = "rotl"; break;
            case sh2::Mnemonic::ROTR: mnemonic = "rotr"; break;
            case sh2::Mnemonic::SHAL: mnemonic = "shal"; break;
            case sh2::Mnemonic::SHAR: mnemonic = "shar"; break;
            case sh2::Mnemonic::SHLL: mnemonic = "shll"; break;
            case sh2::Mnemonic::SHLL2: mnemonic = "shll2"; break;
            case sh2::Mnemonic::SHLL8: mnemonic = "shll8"; break;
            case sh2::Mnemonic::SHLL16: mnemonic = "shll16"; break;
            case sh2::Mnemonic::SHLR: mnemonic = "shlr"; break;
            case sh2::Mnemonic::SHLR2: mnemonic = "shlr2"; break;
            case sh2::Mnemonic::SHLR8: mnemonic = "shlr8"; break;
            case sh2::Mnemonic::SHLR16: mnemonic = "shlr16"; break;
            case sh2::Mnemonic::SUB: mnemonic = "sub"; break;
            case sh2::Mnemonic::SUBC: mnemonic = "subc"; break;
            case sh2::Mnemonic::SUBV: mnemonic = "subv"; break;
            case sh2::Mnemonic::XOR: mnemonic = "xor"; break;
            case sh2::Mnemonic::DT: mnemonic = "dt"; break;
            case sh2::Mnemonic::CLRMAC: mnemonic = "clrmac"; break;
            case sh2::Mnemonic::MAC: mnemonic = "mac"; break;
            case sh2::Mnemonic::MUL: mnemonic = "mul"; break;
            case sh2::Mnemonic::MULS: mnemonic = "muls"; break;
            case sh2::Mnemonic::MULU: mnemonic = "mulu"; break;
            case sh2::Mnemonic::DMULS: mnemonic = "dmuls"; break;
            case sh2::Mnemonic::DMULU: mnemonic = "dmulu"; break;
            case sh2::Mnemonic::DIV0S: mnemonic = "div0s"; break;
            case sh2::Mnemonic::DIV0U: mnemonic = "div0u"; break;
            case sh2::Mnemonic::DIV1: mnemonic = "div1"; break;
            case sh2::Mnemonic::CMP_EQ:
                mnemonic = "cmp/";
                mnemonicCond = "eq";
                condPass = getOp1() == getOp2();
                break;
            case sh2::Mnemonic::CMP_GE:
                mnemonic = "cmp/";
                mnemonicCond = "ge";
                condPass = static_cast<sint32>(getOp1()) >= static_cast<sint32>(getOp2());
                break;
            case sh2::Mnemonic::CMP_GT:
                mnemonic = "cmp/";
                mnemonicCond = "gt";
                condPass = static_cast<sint32>(getOp1()) > static_cast<sint32>(getOp2());
                break;
            case sh2::Mnemonic::CMP_HI:
                mnemonic = "cmp/";
                mnemonicCond = "hi";
                condPass = getOp1() > getOp2();
                break;
            case sh2::Mnemonic::CMP_HS:
                mnemonic = "cmp/";
                mnemonicCond = "hs";
                condPass = getOp1() >= getOp2();
                break;
            case sh2::Mnemonic::CMP_PL:
                mnemonic = "cmp/";
                mnemonicCond = "pl";
                condPass = static_cast<sint32>(getOp1()) > 0;
                break;
            case sh2::Mnemonic::CMP_PZ:
                mnemonic = "cmp/";
                mnemonicCond = "pz";
                condPass = static_cast<sint32>(getOp1()) >= 0;
                break;
            case sh2::Mnemonic::CMP_STR: //
            {
                mnemonic = "cmp/";
                mnemonicCond = "str";
                const uint32 tmp = getOp1() ^ getOp2();
                const uint8 hh = tmp >> 24u;
                const uint8 hl = tmp >> 16u;
                const uint8 lh = tmp >> 8u;
                const uint8 ll = tmp >> 0u;
                condPass = !(hh && hl && lh && ll);
                break;
            }
            case sh2::Mnemonic::TAS: mnemonic = "tas"; break;
            case sh2::Mnemonic::TST: mnemonic = "tst"; break;
            case sh2::Mnemonic::BF:
                mnemonic = "b";
                mnemonicCond = "f";
                condPass = !probe.SR().T;
                break;
            case sh2::Mnemonic::BFS:
                mnemonic = "b";
                mnemonicCond = "f";
                mnemonicSuffix = "/s";
                condPass = !probe.SR().T;
                break;
            case sh2::Mnemonic::BT:
                mnemonic = "b";
                mnemonicCond = "t";
                condPass = probe.SR().T;
                break;
            case sh2::Mnemonic::BTS:
                mnemonic = mnemonic = "b";
                mnemonicCond = "t";
                mnemonicSuffix = "/s";
                condPass = probe.SR().T;
                break;
            case sh2::Mnemonic::BRA: mnemonic = "bra"; break;
            case sh2::Mnemonic::BRAF: mnemonic = "braf"; break;
            case sh2::Mnemonic::BSR: mnemonic = "bsr"; break;
            case sh2::Mnemonic::BSRF: mnemonic = "bsrf"; break;
            case sh2::Mnemonic::JMP: mnemonic = "jmp"; break;
            case sh2::Mnemonic::JSR: mnemonic = "jsr"; break;
            case sh2::Mnemonic::TRAPA: mnemonic = "trapa"; break;
            case sh2::Mnemonic::RTE: mnemonic = "rte"; break;
            case sh2::Mnemonic::RTS: mnemonic = "rts"; break;
            case sh2::Mnemonic::Illegal:
                mnemonic = "(illegal)";
                legal = false;
                break;
            default: break;
            }

            switch (disasm.opSize) {
            case sh2::OperandSize::Byte: sizeSuffix = "b"; break;
            case sh2::OperandSize::Word: sizeSuffix = "w"; break;
            case sh2::OperandSize::Long: sizeSuffix = "l"; break;
            default: break;
            }

            const auto color = legal ? m_colors.disasm.mnemonic : m_colors.disasm.illegalMnemonic;
            ImGui::TextColored(color, "%s", mnemonic.data());
            if (!mnemonicCond.empty()) {
                ImGui::SameLine(0, 0);
                const auto condColor = condPass ? m_colors.disasm.condPass : m_colors.disasm.condFail;
                ImGui::TextColored(condColor, "%s", mnemonicCond.data());
            }
            if (!mnemonicSuffix.empty()) {
                ImGui::SameLine(0, 0);
                ImGui::TextColored(color, "%s", mnemonicSuffix.data());
            }
            if (!sizeSuffix.empty()) {
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.separator, ".");
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.sizeSuffix, "%s", sizeSuffix.data());
            }

            if (disasm.op1.type != sh2::Operand::Type::None) {
                int padding = 9 - mnemonic.length() - mnemonicCond.length() - mnemonicSuffix.length();
                if (!sizeSuffix.empty()) {
                    padding -= 1 + sizeSuffix.length();
                }
                ImGui::SameLine(0, ImGui::CalcTextSize("x").x * padding);
                ImGui::Dummy(ImVec2(0, 0));
            }
        };

        auto drawImm = [&](sint32 imm) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.immediate, "#0x%X", static_cast<uint32>(imm));
        };

        auto drawRegRead = [&](std::string_view regName) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.regRead, "%s", regName.data());
        };

        auto drawRegWrite = [&](std::string_view regName) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.regWrite, "%s", regName.data());
        };

        auto drawRegReadWrite = [&](std::string_view regName) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.regReadWrite, "%s", regName.data());
        };

        auto drawReg = [&](std::string_view regName, bool write) {
            if (write) {
                drawRegWrite(regName);
            } else {
                drawRegRead(regName);
            }
        };

        auto drawRnRead = [&](uint8 rn) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.regRead, "r%u", rn);
        };

        auto drawRnWrite = [&](uint8 rn) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.regWrite, "r%u", rn);
        };

        auto drawRnReadWrite = [&](uint8 rn) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.regReadWrite, "r%u", rn);
        };

        auto drawRn = [&](uint8 rn, bool write) {
            if (write) {
                drawRnWrite(rn);
            } else {
                drawRnRead(rn);
            }
        };

        auto drawRWSymbol = [&](std::string_view symbol, bool write) {
            const auto color = write ? m_colors.disasm.regWrite : m_colors.disasm.regRead;
            ImGui::SameLine(0, 0);
            ImGui::TextColored(color, "%s", symbol.data());
        };

        auto drawPlus = [&] {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.addrInc, "+");
        };

        auto drawMinus = [&] {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.addrDec, "-");
        };

        auto drawComma = [&] {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.separator, ", ");
        };

        auto drawOp = [&](const sh2::Operand &op, bool write) {
            if (op.type == sh2::Operand::Type::None) {
                return;
            }

            switch (op.type) {
            case sh2::Operand::Type::Imm: drawImm(op.immDisp); break;
            case sh2::Operand::Type::Rn: drawRn(op.reg, write); break;
            case sh2::Operand::Type::AtRn:
                drawRWSymbol("@", write);
                drawRnRead(op.reg);
                break;
            case sh2::Operand::Type::AtRnPlus:
                drawRWSymbol("@", write);
                drawRnReadWrite(op.reg);
                drawPlus();
                break;
            case sh2::Operand::Type::AtMinusRn:
                drawRWSymbol("@", write);
                drawMinus();
                drawRnReadWrite(op.reg);
                break;
            case sh2::Operand::Type::AtDispRn:
                drawRWSymbol("@(", write);
                drawImm(op.immDisp);
                drawComma();
                drawRnRead(op.reg);
                drawRWSymbol(")", write);
                break;
            case sh2::Operand::Type::AtR0Rn:
                drawRWSymbol("@(", write);
                drawRegRead(0);
                drawComma();
                drawRnRead(op.reg);
                drawRWSymbol(")", write);
                break;
            case sh2::Operand::Type::AtDispGBR:
                drawRWSymbol("@(", write);
                drawImm(op.immDisp);
                drawComma();
                drawRegRead("gbr");
                drawRWSymbol(")", write);
                break;
            case sh2::Operand::Type::AtR0GBR:
                drawRWSymbol("@(", write);
                drawRnRead(0);
                drawComma();
                drawRegRead("gbr");
                drawRWSymbol(")", write);
                break;
            case sh2::Operand::Type::AtDispPC:
                drawRWSymbol("@(", write);
                drawImm(address + op.immDisp);
                drawRWSymbol(")", write);
                break;
            case sh2::Operand::Type::AtDispPCWordAlign:
                drawRWSymbol("@(", write);
                drawImm((address & ~3) + op.immDisp);
                drawRWSymbol(")", write);
                break;
            case sh2::Operand::Type::DispPC: drawImm(address + op.immDisp); break;
            case sh2::Operand::Type::RnPC: drawRnRead(op.reg); break;
            case sh2::Operand::Type::SR: drawReg("sr", write); break;
            case sh2::Operand::Type::GBR: drawReg("gbr", write); break;
            case sh2::Operand::Type::VBR: drawReg("vbr", write); break;
            case sh2::Operand::Type::MACH: drawReg("mach", write); break;
            case sh2::Operand::Type::MACL: drawReg("macl", write); break;
            case sh2::Operand::Type::PR: drawReg("pr", write); break;
            default: break;
            }
        };

        // -------------------------------------------------------------------------------------------------------------

        drawAddress();
        drawOpcode();
        drawMnemonic();

        drawOp(disasm.op1, false);
        if (disasm.op1.type != sh2::Operand::Type::None && disasm.op2.type != sh2::Operand::Type::None) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(m_colors.disasm.separator, ", ");
        }
        drawOp(disasm.op2, true);
    }
    ImGui::PopFont();

    ImGui::EndGroup();
}

} // namespace app::ui

#include "sh2_debugger.hpp"

#include <satemu/core/types.hpp>

#include <satemu/hw/sh2/sh2_disasm.hpp>

#include <imgui.h>

using namespace satemu;

namespace app {

SH2Debugger::SH2Debugger(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_sh2(master ? context.saturn.masterSH2 : context.saturn.slaveSH2) {}

void SH2Debugger::Display() {
    // TODO: improve layout
    // TODO: move to a separate file (as standalone function?)

    auto &regs = m_sh2.GetGPRs();
    std::string name = fmt::format("{}SH2", m_master ? "M" : "S");
    if (ImGui::Begin(name.c_str(), &Open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::BeginGroup();
        if (m_master || m_context.saturn.slaveSH2Enabled) {
            auto drawReg32 = [&](std::string name, uint32 value) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name.c_str());

                ImGui::SameLine(50.0f);

                ImGui::PushFont(m_context.fonts.monospaceMedium);
                float charWidth = ImGui::CalcTextSize("F").x;
                ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + charWidth * 8);
                const std::string lblField = fmt::format("##input_{}", name);
                ImGui::InputScalar(lblField.c_str(), ImGuiDataType_U32, &value, nullptr, nullptr, "%08X",
                                   ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::PopFont();
            };

            for (uint32 i = 0; i < 16; i++) {
                drawReg32(fmt::format("R{}", i), regs[i]);
            }

            drawReg32("PC", m_sh2.GetPC());
            drawReg32("PR", m_sh2.GetPR());

            auto mac = m_sh2.GetMAC();
            drawReg32("MACH", mac.H);
            drawReg32("MACL", mac.L);

            auto sr = m_sh2.GetSR();
            drawReg32("SR", sr.u32);
            bool M = sr.M;
            bool Q = sr.Q;
            bool S = sr.S;
            bool T = sr.T;

            ImGui::BeginGroup();
            ImGui::Checkbox("##M", &M);
            ImGui::Text("M");
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::Checkbox("##Q", &Q);
            ImGui::Text("Q");
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::Checkbox("##S", &S);
            ImGui::Text("S");
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::Checkbox("##T", &T);
            ImGui::Text("T");
            ImGui::EndGroup();

            // TODO: make this editable
            ImGui::Text("I=%X", (uint8)sr.ILevel);

            drawReg32("GBR", m_sh2.GetGBR());
            drawReg32("VBR", m_sh2.GetVBR());

            /*if (debugTrace) {
                drawText(x, y + 280, "vec lv");
                for (size_t i = 0; i < tracer.interruptsCount; i++) {
                    const size_t pos =
                        (tracer.interruptsPos - tracer.interruptsCount + i) % tracer.interrupts.size();
                    const auto &intr = tracer.interrupts[pos];
                    drawText(x, y + 290 + i * 10, fmt::format("{:02X}  {:02X}", intr.vecNum, intr.level));
                }
            } else {
                ImGui::TextUnformatted("(trace disabled)");
            }*/
        } else {
            ImGui::TextUnformatted("(disabled)");
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::PushFont(m_context.fonts.monospaceMedium);
        const uint32 baseAddr = m_sh2.GetPC() & ~1;
        for (uint32 i = 0; i < 32; i++) {
            const uint32 addr = baseAddr + i * sizeof(uint16);

            // TODO: should peek instead
            const uint16 instr = m_context.saturn.mainBus.Read<uint16>(addr);

            ImGui::Text("%08X %04X ", addr, instr);
            ImGui::SameLine(0, 0);

            const sh2::OpcodeDisasm &disasm = sh2::g_disasmTable.disasm[instr];
            switch (disasm.mnemonic) {
            case sh2::Mnemonic::NOP: ImGui::TextUnformatted("nop"); break;
            case sh2::Mnemonic::SLEEP: ImGui::TextUnformatted("sleep"); break;
            case sh2::Mnemonic::MOV: ImGui::TextUnformatted("mov"); break;
            case sh2::Mnemonic::MOVA: ImGui::TextUnformatted("mova"); break;
            case sh2::Mnemonic::MOVT: ImGui::TextUnformatted("movt"); break;
            case sh2::Mnemonic::CLRT: ImGui::TextUnformatted("clrt"); break;
            case sh2::Mnemonic::SETT: ImGui::TextUnformatted("sett"); break;
            case sh2::Mnemonic::EXTU: ImGui::TextUnformatted("extu"); break;
            case sh2::Mnemonic::EXTS: ImGui::TextUnformatted("exts"); break;
            case sh2::Mnemonic::SWAP: ImGui::TextUnformatted("swap"); break;
            case sh2::Mnemonic::XTRCT: ImGui::TextUnformatted("xtrct"); break;
            case sh2::Mnemonic::LDC: ImGui::TextUnformatted("ldc"); break;
            case sh2::Mnemonic::LDS: ImGui::TextUnformatted("lds"); break;
            case sh2::Mnemonic::STC: ImGui::TextUnformatted("stc"); break;
            case sh2::Mnemonic::STS: ImGui::TextUnformatted("sts"); break;
            case sh2::Mnemonic::ADD: ImGui::TextUnformatted("add"); break;
            case sh2::Mnemonic::ADDC: ImGui::TextUnformatted("addc"); break;
            case sh2::Mnemonic::ADDV: ImGui::TextUnformatted("addv"); break;
            case sh2::Mnemonic::AND: ImGui::TextUnformatted("and"); break;
            case sh2::Mnemonic::NEG: ImGui::TextUnformatted("neg"); break;
            case sh2::Mnemonic::NEGC: ImGui::TextUnformatted("negc"); break;
            case sh2::Mnemonic::NOT: ImGui::TextUnformatted("not"); break;
            case sh2::Mnemonic::OR: ImGui::TextUnformatted("or"); break;
            case sh2::Mnemonic::ROTCL: ImGui::TextUnformatted("rotcl"); break;
            case sh2::Mnemonic::ROTCR: ImGui::TextUnformatted("rotcr"); break;
            case sh2::Mnemonic::ROTL: ImGui::TextUnformatted("rotl"); break;
            case sh2::Mnemonic::ROTR: ImGui::TextUnformatted("rotr"); break;
            case sh2::Mnemonic::SHAL: ImGui::TextUnformatted("shal"); break;
            case sh2::Mnemonic::SHAR: ImGui::TextUnformatted("shar"); break;
            case sh2::Mnemonic::SHLL: ImGui::TextUnformatted("shll"); break;
            case sh2::Mnemonic::SHLL2: ImGui::TextUnformatted("shll2"); break;
            case sh2::Mnemonic::SHLL8: ImGui::TextUnformatted("shll8"); break;
            case sh2::Mnemonic::SHLL16: ImGui::TextUnformatted("shll16"); break;
            case sh2::Mnemonic::SHLR: ImGui::TextUnformatted("shlr"); break;
            case sh2::Mnemonic::SHLR2: ImGui::TextUnformatted("shlr2"); break;
            case sh2::Mnemonic::SHLR8: ImGui::TextUnformatted("shlr8"); break;
            case sh2::Mnemonic::SHLR16: ImGui::TextUnformatted("shlr16"); break;
            case sh2::Mnemonic::SUB: ImGui::TextUnformatted("sub"); break;
            case sh2::Mnemonic::SUBC: ImGui::TextUnformatted("subc"); break;
            case sh2::Mnemonic::SUBV: ImGui::TextUnformatted("subv"); break;
            case sh2::Mnemonic::XOR: ImGui::TextUnformatted("xor"); break;
            case sh2::Mnemonic::DT: ImGui::TextUnformatted("dt"); break;
            case sh2::Mnemonic::CLRMAC: ImGui::TextUnformatted("clrmac"); break;
            case sh2::Mnemonic::MAC: ImGui::TextUnformatted("mac"); break;
            case sh2::Mnemonic::MUL: ImGui::TextUnformatted("mul"); break;
            case sh2::Mnemonic::MULS: ImGui::TextUnformatted("muls"); break;
            case sh2::Mnemonic::MULU: ImGui::TextUnformatted("mulu"); break;
            case sh2::Mnemonic::DMULS: ImGui::TextUnformatted("dmuls"); break;
            case sh2::Mnemonic::DMULU: ImGui::TextUnformatted("dmulu"); break;
            case sh2::Mnemonic::DIV0S: ImGui::TextUnformatted("div0s"); break;
            case sh2::Mnemonic::DIV0U: ImGui::TextUnformatted("div0u"); break;
            case sh2::Mnemonic::DIV1: ImGui::TextUnformatted("div1"); break;
            case sh2::Mnemonic::CMP_EQ: ImGui::TextUnformatted("cmp/eq"); break;
            case sh2::Mnemonic::CMP_GE: ImGui::TextUnformatted("cmp/ge"); break;
            case sh2::Mnemonic::CMP_GT: ImGui::TextUnformatted("cmp/gt"); break;
            case sh2::Mnemonic::CMP_HI: ImGui::TextUnformatted("cmp/hi"); break;
            case sh2::Mnemonic::CMP_HS: ImGui::TextUnformatted("cmp/hs"); break;
            case sh2::Mnemonic::CMP_PL: ImGui::TextUnformatted("cmp/pl"); break;
            case sh2::Mnemonic::CMP_PZ: ImGui::TextUnformatted("cmp/pz"); break;
            case sh2::Mnemonic::CMP_STR: ImGui::TextUnformatted("cmp/str"); break;
            case sh2::Mnemonic::TAS: ImGui::TextUnformatted("tas"); break;
            case sh2::Mnemonic::TST: ImGui::TextUnformatted("tst"); break;
            case sh2::Mnemonic::BF: ImGui::TextUnformatted("bf"); break;
            case sh2::Mnemonic::BFS: ImGui::TextUnformatted("bf/s"); break;
            case sh2::Mnemonic::BT: ImGui::TextUnformatted("bt"); break;
            case sh2::Mnemonic::BTS: ImGui::TextUnformatted("bt/s"); break;
            case sh2::Mnemonic::BRA: ImGui::TextUnformatted("bra"); break;
            case sh2::Mnemonic::BRAF: ImGui::TextUnformatted("braf"); break;
            case sh2::Mnemonic::BSR: ImGui::TextUnformatted("bsr"); break;
            case sh2::Mnemonic::BSRF: ImGui::TextUnformatted("bsrf"); break;
            case sh2::Mnemonic::JMP: ImGui::TextUnformatted("jmp"); break;
            case sh2::Mnemonic::JSR: ImGui::TextUnformatted("jsr"); break;
            case sh2::Mnemonic::TRAPA: ImGui::TextUnformatted("trapa"); break;
            case sh2::Mnemonic::RTE: ImGui::TextUnformatted("rte"); break;
            case sh2::Mnemonic::RTS: ImGui::TextUnformatted("rts"); break;
            case sh2::Mnemonic::Illegal: ImGui::TextUnformatted("(illegal)"); break;
            default: break;
            }

            switch (disasm.opSize) {
            case sh2::OperandSize::Byte:
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(".b");
                break;
            case sh2::OperandSize::Word:
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(".w");
                break;
            case sh2::OperandSize::Long:
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(".l");
                break;
            default: break;
            }

            auto printOp = [&](const sh2::Operand &op) {
                if (op.type != sh2::Operand::Type::None) {
                    ImGui::SameLine(0, 0);
                }
                switch (op.type) {
                case sh2::Operand::Type::Imm: ImGui::Text(" #0x%X", op.immDisp); break;
                case sh2::Operand::Type::Rn: ImGui::Text(" r%u", op.reg); break;
                case sh2::Operand::Type::AtRn: ImGui::Text(" @r%u", op.reg); break;
                case sh2::Operand::Type::AtRnPlus: ImGui::Text(" @r%u+", op.reg); break;
                case sh2::Operand::Type::AtMinusRn: ImGui::Text(" @-r%u", op.reg); break;
                case sh2::Operand::Type::AtDispRn: ImGui::Text(" @(0x%X,r%u)", op.immDisp, op.reg); break;
                case sh2::Operand::Type::AtR0Rn: ImGui::Text(" @(r0,r%u)", op.reg); break;
                case sh2::Operand::Type::AtDispGBR: ImGui::Text(" @(0x%X,gbr)", op.immDisp); break;
                case sh2::Operand::Type::AtR0GBR: ImGui::TextUnformatted(" @(r0,gbr)"); break;
                case sh2::Operand::Type::AtDispPC: ImGui::Text(" @(0x%X,pc)", op.immDisp + addr); break;
                case sh2::Operand::Type::DispPC: ImGui::Text(" 0x%X", op.immDisp + addr); break;
                case sh2::Operand::Type::RnPC: ImGui::Text(" r%u[+pc]", op.reg); break;
                case sh2::Operand::Type::SR: ImGui::TextUnformatted(" sr"); break;
                case sh2::Operand::Type::GBR: ImGui::TextUnformatted(" gbr"); break;
                case sh2::Operand::Type::VBR: ImGui::TextUnformatted(" vbr"); break;
                case sh2::Operand::Type::MACH: ImGui::TextUnformatted(" mach"); break;
                case sh2::Operand::Type::MACL: ImGui::TextUnformatted(" macl"); break;
                case sh2::Operand::Type::PR: ImGui::TextUnformatted(" pr"); break;
                default: break;
                }
            };

            printOp(disasm.op1);
            if (disasm.op1.type != sh2::Operand::Type::None && disasm.op2.type != sh2::Operand::Type::None) {
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(",");
            }
            printOp(disasm.op2);
        }
        ImGui::PopFont();
        ImGui::EndGroup();
    }
    ImGui::End();
}

} // namespace app

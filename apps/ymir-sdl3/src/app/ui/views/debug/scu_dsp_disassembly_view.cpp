#include "scu_dsp_disassembly_view.hpp"

#include <ymir/hw/scu/scu_dsp_disasm.hpp>

#include <string_view>

using namespace ymir;

namespace app::ui {

SCUDSPDisassemblyView::SCUDSPDisassemblyView(SharedContext &context)
    : m_context(context)
    , m_dsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPDisassemblyView::Display() {
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::BeginGroup();

    auto tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
    if (m_style.disasm.altLineColors) {
        tableFlags |= ImGuiTableFlags_RowBg;
    }
    if (ImGui::BeginTable("dsp_disasm", 3, tableFlags)) {
        ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed, paddingWidth * 2 + hexCharWidth * 2);
        ImGui::TableSetupColumn("Opcode", ImGuiTableColumnFlags_WidthFixed, paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Instructions", ImGuiTableColumnFlags_WidthFixed,
                                paddingWidth * 2 + hexCharWidth * (3 + 10 + 10 + 10 + 10 + 14) +
                                    m_style.disasm.instrSeparation * 5);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        for (uint32 pc = 0; pc <= 0xFF; pc++) {
            const uint32 opcode = m_dsp.programRAM[pc].u32;
            const auto disasm = scu::Disassemble(opcode);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
                ImGui::TextColored(m_colors.disasm.address, "%02X", pc);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
                ImGui::TextColored(m_colors.disasm.bytes, "%08X", opcode);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);

                auto drawMnemonic = [&](std::string_view mnemonic) {
                    ImGui::TextColored(m_colors.disasm.mnemonic, "%s", mnemonic.data());
                };

                auto drawNOP = [&] {
                    if (m_style.disasm.drawNOPs) {
                        ImGui::TextColored(m_colors.disasm.nopMnemonic, "NOP");
                    }
                };

                auto drawIllegalMnemonic = [&] { ImGui::TextColored(m_colors.disasm.illegalMnemonic, "(illegal)"); };

                auto drawComma = [&] {
                    ImGui::SameLine(0, 0);
                    ImGui::TextColored(m_colors.disasm.separator, ", ");
                    ImGui::SameLine(0, 0);
                };

                auto drawU8 = [&](uint8 imm) { ImGui::TextColored(m_colors.disasm.immediate, "#0x%X", imm); };
                auto drawS8 = [&](sint8 imm) {
                    ImGui::TextColored(m_colors.disasm.immediate, "#%s0x%X", (imm < 0 ? "-" : ""), abs(imm));
                };
                auto drawS32 = [&](sint32 imm) {
                    ImGui::TextColored(m_colors.disasm.immediate, "#%s0x%X", (imm < 0 ? "-" : ""), abs(imm));
                };

                auto drawRegRead = [&](std::string_view reg) {
                    ImGui::TextColored(m_colors.disasm.regRead, "%s", reg.data());
                };
                auto drawRegWrite = [&](std::string_view reg) {
                    ImGui::TextColored(m_colors.disasm.regWrite, "%s", reg.data());
                };

                auto checkCond = [&](scu::SCUDSPInstruction::Cond cond) {
                    using enum scu::SCUDSPInstruction::Cond;
                    switch (cond) {
                    case None: return true;
                    case NZ: return m_dsp.CondCheck(0b000001);
                    case NS: return m_dsp.CondCheck(0b000010);
                    case NZS: return m_dsp.CondCheck(0b000011);
                    case NC: return m_dsp.CondCheck(0b000100);
                    case NT0: return m_dsp.CondCheck(0b001000);
                    case Z: return m_dsp.CondCheck(0b100001);
                    case S: return m_dsp.CondCheck(0b100010);
                    case ZS: return m_dsp.CondCheck(0b100011);
                    case C: return m_dsp.CondCheck(0b100100);
                    case T0: return m_dsp.CondCheck(0b101000);
                    default: return false;
                    }
                };

                auto drawCond = [&](scu::SCUDSPInstruction::Cond cond) {
                    const bool pass = checkCond(cond);
                    const auto color = pass ? m_colors.disasm.condPass : m_colors.disasm.condFail;
                    ImGui::TextColored(color, "%s", scu::ToString(cond).data());
                };

                switch (disasm.type) {
                case scu::SCUDSPInstruction::Type::Operation: //
                {
                    float offset = 0.0f;
                    // Advance cursor by numChars plus one for separation
                    auto align = [&](uint32 numChars) {
                        offset += hexCharWidth * numChars + m_style.disasm.instrSeparation;
                        ImGui::SameLine(offset);
                    };
                    if (disasm.operation.aluOp == scu::SCUDSPInstruction::ALUOp::NOP) {
                        drawNOP();
                    } else {
                        drawMnemonic(scu::ToString(disasm.operation.aluOp));
                    }
                    align(3);

                    switch (disasm.operation.xbusPOp) {
                    case scu::SCUDSPInstruction::XBusPOp::NOP: drawNOP(); break;
                    case scu::SCUDSPInstruction::XBusPOp::MOV_MUL_P:
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawRegRead("MUL");
                        drawComma();
                        drawRegWrite("P");
                        break;
                    case scu::SCUDSPInstruction::XBusPOp::MOV_S_P:
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawRegRead(scu::ToString(disasm.operation.xbusSrc));
                        drawComma();
                        drawRegWrite("P");
                        break;
                    }
                    align(10);

                    if (disasm.operation.xbusXOp) {
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawRegRead(scu::ToString(disasm.operation.xbusSrc));
                        drawComma();
                        drawRegWrite("X");
                    } else {
                        drawNOP();
                    }
                    align(10);

                    switch (disasm.operation.ybusAOp) {
                    case scu::SCUDSPInstruction::YBusAOp::NOP: drawNOP(); break;
                    case scu::SCUDSPInstruction::YBusAOp::CLR_A:
                        drawMnemonic("CLR ");
                        ImGui::SameLine(0, 0);
                        drawRegWrite("A");
                        break;
                    case scu::SCUDSPInstruction::YBusAOp::MOV_ALU_A:
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawRegRead("ALU");
                        drawComma();
                        drawRegWrite("A");
                        break;
                    case scu::SCUDSPInstruction::YBusAOp::MOV_S_A:
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawRegRead(scu::ToString(disasm.operation.xbusSrc));
                        drawComma();
                        drawRegWrite("A");
                        break;
                    }
                    align(10);

                    if (disasm.operation.ybusYOp) {
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawRegRead(scu::ToString(disasm.operation.ybusSrc));
                        drawComma();
                        drawRegWrite("Y");
                    } else {
                        drawNOP();
                    }
                    align(10);

                    switch (disasm.operation.d1BusOp) {
                    case scu::SCUDSPInstruction::D1BusOp::NOP: drawNOP(); break;
                    case scu::SCUDSPInstruction::D1BusOp::MOV_SIMM_D:
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawS8(disasm.operation.d1BusSrc.imm);
                        drawComma();
                        drawRegWrite(scu::ToString(disasm.operation.d1BusDst));
                        break;
                    case scu::SCUDSPInstruction::D1BusOp::MOV_S_D:
                        drawMnemonic("MOV ");
                        ImGui::SameLine(0, 0);
                        drawRegRead(scu::ToString(disasm.operation.d1BusSrc.reg));
                        drawComma();
                        drawRegWrite(scu::ToString(disasm.operation.d1BusDst));
                        break;
                    }

                    break;
                }
                case scu::SCUDSPInstruction::Type::MVI:
                    drawMnemonic("MVI");
                    ImGui::SameLine();
                    drawS32(disasm.mvi.imm);
                    drawComma();
                    drawRegWrite(scu::ToString(disasm.mvi.dst));
                    if (disasm.mvi.cond != scu::SCUDSPInstruction::Cond::None) {
                        drawComma();
                        drawCond(disasm.mvi.cond);
                    }
                    break;
                case scu::SCUDSPInstruction::Type::DMA:
                    drawMnemonic(disasm.dma.hold ? "DMAH" : "DMA");
                    ImGui::SameLine();
                    if (disasm.dma.toD0) {
                        drawRegRead(scu::ToString(disasm.dma.ramOp));
                    } else {
                        drawRegRead("D0");
                    }
                    drawComma();
                    if (disasm.dma.toD0) {
                        drawRegWrite("D0");
                    } else {
                        drawRegWrite(scu::ToString(disasm.dma.ramOp));
                    }
                    drawComma();
                    if (disasm.dma.countType) {
                        auto reg = fmt::format("M{}{}", (disasm.dma.count.ct < 4 ? "" : "C"), disasm.dma.count.ct & 3);
                        drawRegRead(reg);
                    } else {
                        drawU8(disasm.dma.count.imm);
                    }
                    break;
                case scu::SCUDSPInstruction::Type::JMP:
                    drawMnemonic("JMP");
                    if (disasm.jmp.cond != scu::SCUDSPInstruction::Cond::None) {
                        ImGui::SameLine();
                        drawCond(disasm.jmp.cond);
                        drawComma();
                    }
                    ImGui::SameLine();
                    drawU8(disasm.jmp.target);
                    break;
                case scu::SCUDSPInstruction::Type::LPS: drawMnemonic("LPS"); break;
                case scu::SCUDSPInstruction::Type::BTM: drawMnemonic("BTM"); break;
                case scu::SCUDSPInstruction::Type::END: drawMnemonic("END"); break;
                case scu::SCUDSPInstruction::Type::ENDI: drawMnemonic("ENDI"); break;
                default: drawIllegalMnemonic(); break;
                }

                ImGui::PopFont();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
}

} // namespace app::ui

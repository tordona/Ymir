#include "sh2_disassembly_view.hpp"

#include <ymir/hw/sh2/sh2_disasm.hpp>

#include <imgui.h>

namespace app::ui {

SH2DisassemblyView::SH2DisassemblyView(SharedContext &context, ymir::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2DisassemblyView::Display() {
    using namespace ymir;

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const ImVec2 disasmCharSize = ImGui::CalcTextSize("x");
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    ImGui::PopFont();

    ImDrawList *drawList = ImGui::GetWindowDrawList();

    auto availArea = ImGui::GetContentRegionAvail();

    if (ImGui::BeginChild("##disasm", availArea, ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        const uint32 lines = availArea.y / lineHeight + 1;
        // TODO: branch arrows
        // TODO: cursor

        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        auto &probe = m_sh2.GetProbe();
        const uint32 pc = probe.PC() & ~1;
        const uint32 pr = probe.PR() & ~1;
        const uint32 baseAddress = (pc - lines - 1) & ~1;
        for (uint32 i = 0; i < lines; i++) {
            const uint32 address = baseAddress + i * sizeof(uint16);
            const uint16 prevOpcode = m_context.saturn.mainBus.Peek<uint16>(address - 2);
            const uint16 opcode = m_context.saturn.mainBus.Peek<uint16>(address);
            const sh2::DisassembledInstruction &prevDisasm = sh2::Disassemble(prevOpcode);
            const sh2::DisassembledInstruction &disasm = sh2::Disassemble(opcode);

            auto memRead = [&](uint32 address) -> uint32 {
                switch (disasm.opSize) {
                case sh2::OperandSize::Byte: return probe.MemPeekByte(address, false);
                case sh2::OperandSize::Word: return probe.MemPeekWord(address, false);
                case sh2::OperandSize::Long: return probe.MemPeekLong(address, false);
                default: return 0;
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
                default: return 0;
                }
            };

            auto getOp1 = [&] { return getOp(disasm.op1); };
            auto getOp2 = [&] { return getOp(disasm.op2); };

            auto filterAscii = [](char c) { return c < 0x20 ? '.' : c; };

            auto drawHighlight = [&] {
                ImVec4 color{};
                bool filled = true;
                /*if (address == m_cursor.address) {
                    color = m_colors.disasm.cursorBgColor;
                    if (!childWindowFocused) {
                        filled = false;
                    }
                } else*/
                if (address == pc) {
                    color = m_colors.disasm.pcBgColor;
                } else if (address == pr) {
                    color = m_colors.disasm.prBgColor;
                } /*else if (probe.IsBreakpointSet(address)) {
                    color = m_colors.disasm.bkptBgColor;
                } else if (probe.IsWatchpointSet(address)) {
                    color = m_colors.disasm.wtptBgColor;
                }*/
                else if (m_settings.altLineColors && m_settings.altLineAddresses ? (address & 2) : (i & 1)) {
                    color = m_colors.disasm.altLineBgColor;
                }
                if (color.w == 0.0f) {
                    return;
                }
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 size{ImGui::GetContentRegionAvail().x,
                                  ImGui::CalcTextSize("X").y + ImGui::GetStyle().FramePadding.y};
                const ImVec2 end{pos.x + size.x, pos.y + size.y};
                if (filled) {
                    drawList->AddRectFilled(pos, end, ImGui::ColorConvertFloat4ToU32(color));
                } else {
                    ImVec4 fillColor = color;
                    fillColor.w *= 0.4f;
                    auto borderPos = ImVec2(pos.x + 0.5f, pos.y + 0.5f);
                    auto borderEnd = ImVec2(end.x - 0.5f, end.y - 0.5f);
                    drawList->AddRectFilled(pos, end, ImGui::ColorConvertFloat4ToU32(fillColor));
                    drawList->AddRect(borderPos, borderEnd, ImGui::ColorConvertFloat4ToU32(color), 0.0f,
                                      ImDrawFlags_None, 2.0f);
                }
            };

            auto drawIcons = [&] {
                auto pos = ImGui::GetCursorScreenPos();
                pos.x -= 1.5f;
                pos.y -= 1.5f;
                const ImVec2 baseCenter{pos.x + lineHeight * 0.5f, pos.y + lineHeight * 0.5f};
                if (address == pc) {
                    const ImVec2 center{baseCenter.x + lineHeight * 3.0f, baseCenter.y};
                    const ImVec2 points[] = {
                        {center.x - lineHeight * 0.25f, center.y - lineHeight * 0.25f},
                        {center.x + lineHeight * 0.25f, center.y},
                        {center.x - lineHeight * 0.25f, center.y + lineHeight * 0.25f},
                        {center.x - lineHeight * 0.15f, center.y},
                    };
                    drawList->AddConcavePolyFilled(points, std::size(points),
                                                   ImGui::ColorConvertFloat4ToU32(m_colors.disasm.pcIconColor));
                }
                if (address == pr) {
                    const ImVec2 center{baseCenter.x + lineHeight * 2.0f, baseCenter.y};
                    const ImVec2 points[] = {
                        {center.x - lineHeight * 0.25f, center.y - lineHeight * 0.25f},
                        {center.x + lineHeight * 0.25f, center.y},
                        {center.x - lineHeight * 0.25f, center.y + lineHeight * 0.25f},
                        {center.x - lineHeight * 0.15f, center.y},
                    };
                    drawList->AddConcavePolyFilled(points, std::size(points),
                                                   ImGui::ColorConvertFloat4ToU32(m_colors.disasm.prIconColor));
                }
                /*if (probe.IsBreakpointSet(address)) {
                    const ImVec2 center{pos.x + lineHeight * 1.5f, pos.y + lineHeight * 0.5f};
                    const ImU32 color = ImGui::ColorConvertFloat4ToU32(m_colors.disasm.bkptIconColor);
                    if (m_context.saturn.IsDebugTracingEnabled()) {
                        drawList->AddCircleFilled(center, lineHeight * 0.25f, color);
                    } else {
                        drawList->AddCircle(center, lineHeight * 0.25f, color, 0, 2.0f);
                    }
                }*/
                /*if (probe.IsWatchpointSet(address)) {
                    const ImVec2 center{pos.x + lineHeight * 0.5f, pos.y + lineHeight * 0.5f};
                    const ImVec2 p1{center.x, center.y - lineHeight * 0.25f};
                    const ImVec2 p2{center.x - lineHeight * 0.25f, center.y};
                    const ImVec2 p3{center.x, center.y + lineHeight * 0.25f};
                    const ImVec2 p4{center.x + lineHeight * 0.25f, center.y};
                    const ImU32 color = ImGui::ColorConvertFloat4ToU32(m_colors.disasm.wtptIconColor);
                    if (m_context.saturn.IsDebugTracingEnabled()) {
                        drawList->AddQuadFilled(p1, p2, p3, p4, color);
                    } else {
                        drawList->AddQuad(p1, p2, p3, p4, color, 2.0f);
                    }
                }*/
                ImGui::Dummy(ImVec2(lineHeight * 4, 0.0f));
                ImGui::SameLine(0.0f, 0.0f);
            };

            auto drawAddress = [&] { ImGui::TextColored(m_colors.disasm.address, "%08X", address); };

            auto drawOpcodeBytes = [&] {
                if (m_settings.displayOpcodeBytes) {
                    ImGui::TextColored(m_colors.disasm.bytes, "%04X", opcode);
                }
            };
            auto drawOpcodeAscii = [&] {
                if (m_settings.displayOpcodeAscii) {
                    char ascii[2] = {0};
                    ascii[0] = filterAscii((opcode >> 8) & 0xFF);
                    ascii[1] = filterAscii((opcode >> 0) & 0xFF);
                    ImGui::TextColored(m_colors.disasm.ascii, "%c%c", ascii[0], ascii[1]);
                }
            };

            auto drawDelaySlotPrefix = [&] {
                const float xofs = disasmCharSize.x * 2;
                ImGui::SameLine(0, xofs);
                ImVec2 startPos = ImGui::GetCursorScreenPos();
                startPos.x -= xofs;

                const ImVec2 points[] = {
                    ImVec2(startPos.x + disasmCharSize.x * 0.4f, startPos.y),
                    ImVec2(startPos.x + disasmCharSize.x * 0.4f, startPos.y + disasmCharSize.y * 0.6f),
                    ImVec2(startPos.x + disasmCharSize.x * 1.4f, startPos.y + disasmCharSize.y * 0.6f),
                };
                drawList->AddPolyline(points, std::size(points),
                                      ImGui::ColorConvertFloat4ToU32(m_colors.disasm.delaySlot), ImDrawFlags_None,
                                      2.0f);
                ImGui::Dummy(ImVec2(0, 0));
            };

            auto drawMnemonic = [&](std::string_view mnemonic) {
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.mnemonic, "%s", mnemonic.data());
            };

            auto drawIllegalMnemonic = [&] {
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.illegalMnemonic, "(illegal)");
            };

            auto drawUnknownMnemonic = [&] {
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.illegalMnemonic, "(?)");
            };

            auto drawCond = [&](std::string_view cond, bool pass) {
                ImGui::SameLine(0, 0);
                const auto condColor = pass ? m_colors.disasm.condPass : m_colors.disasm.condFail;
                ImGui::TextColored(condColor, "%s", cond.data());
            };

            auto drawSeparator = [&](std::string_view sep) {
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.separator, "%s", sep.data());
            };

            auto drawSize = [&](std::string_view size) {
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.separator, ".");
                ImGui::SameLine(0, 0);
                ImGui::TextColored(m_colors.disasm.sizeSuffix, "%s", size.data());
            };

            auto drawInstruction = [&] {
                const float startX = ImGui::GetCursorPosX();
                ImGui::Dummy(ImVec2(0, 0));

                if (prevDisasm.hasDelaySlot) {
                    if (!disasm.validInDelaySlot) {
                        drawIllegalMnemonic();
                        return;
                    }
                    drawDelaySlotPrefix();
                }

                switch (disasm.mnemonic) {
                case sh2::Mnemonic::NOP: drawMnemonic("nop"); break;
                case sh2::Mnemonic::SLEEP: drawMnemonic("sleep"); break;
                case sh2::Mnemonic::MOV: drawMnemonic("mov"); break;
                case sh2::Mnemonic::MOVA: drawMnemonic("mova"); break;
                case sh2::Mnemonic::MOVT: drawMnemonic("movt"); break;
                case sh2::Mnemonic::CLRT: drawMnemonic("clrt"); break;
                case sh2::Mnemonic::SETT: drawMnemonic("sett"); break;
                case sh2::Mnemonic::EXTU: drawMnemonic("extu"); break;
                case sh2::Mnemonic::EXTS: drawMnemonic("exts"); break;
                case sh2::Mnemonic::SWAP: drawMnemonic("swap"); break;
                case sh2::Mnemonic::XTRCT: drawMnemonic("xtrct"); break;
                case sh2::Mnemonic::LDC: drawMnemonic("ldc"); break;
                case sh2::Mnemonic::LDS: drawMnemonic("lds"); break;
                case sh2::Mnemonic::STC: drawMnemonic("stc"); break;
                case sh2::Mnemonic::STS: drawMnemonic("sts"); break;
                case sh2::Mnemonic::ADD: drawMnemonic("add"); break;
                case sh2::Mnemonic::ADDC: drawMnemonic("addc"); break;
                case sh2::Mnemonic::ADDV: drawMnemonic("addv"); break;
                case sh2::Mnemonic::AND: drawMnemonic("and"); break;
                case sh2::Mnemonic::NEG: drawMnemonic("neg"); break;
                case sh2::Mnemonic::NEGC: drawMnemonic("negc"); break;
                case sh2::Mnemonic::NOT: drawMnemonic("not"); break;
                case sh2::Mnemonic::OR: drawMnemonic("or"); break;
                case sh2::Mnemonic::ROTCL: drawMnemonic("rotcl"); break;
                case sh2::Mnemonic::ROTCR: drawMnemonic("rotcr"); break;
                case sh2::Mnemonic::ROTL: drawMnemonic("rotl"); break;
                case sh2::Mnemonic::ROTR: drawMnemonic("rotr"); break;
                case sh2::Mnemonic::SHAL: drawMnemonic("shal"); break;
                case sh2::Mnemonic::SHAR: drawMnemonic("shar"); break;
                case sh2::Mnemonic::SHLL: drawMnemonic("shll"); break;
                case sh2::Mnemonic::SHLL2: drawMnemonic("shll2"); break;
                case sh2::Mnemonic::SHLL8: drawMnemonic("shll8"); break;
                case sh2::Mnemonic::SHLL16: drawMnemonic("shll16"); break;
                case sh2::Mnemonic::SHLR: drawMnemonic("shlr"); break;
                case sh2::Mnemonic::SHLR2: drawMnemonic("shlr2"); break;
                case sh2::Mnemonic::SHLR8: drawMnemonic("shlr8"); break;
                case sh2::Mnemonic::SHLR16: drawMnemonic("shlr16"); break;
                case sh2::Mnemonic::SUB: drawMnemonic("sub"); break;
                case sh2::Mnemonic::SUBC: drawMnemonic("subc"); break;
                case sh2::Mnemonic::SUBV: drawMnemonic("subv"); break;
                case sh2::Mnemonic::XOR: drawMnemonic("xor"); break;
                case sh2::Mnemonic::DT: drawMnemonic("dt"); break;
                case sh2::Mnemonic::CLRMAC: drawMnemonic("clrmac"); break;
                case sh2::Mnemonic::MAC: drawMnemonic("mac"); break;
                case sh2::Mnemonic::MUL: drawMnemonic("mul"); break;
                case sh2::Mnemonic::MULS: drawMnemonic("muls"); break;
                case sh2::Mnemonic::MULU: drawMnemonic("mulu"); break;
                case sh2::Mnemonic::DMULS: drawMnemonic("dmuls"); break;
                case sh2::Mnemonic::DMULU: drawMnemonic("dmulu"); break;
                case sh2::Mnemonic::DIV0S: drawMnemonic("div0s"); break;
                case sh2::Mnemonic::DIV0U: drawMnemonic("div0u"); break;
                case sh2::Mnemonic::DIV1: drawMnemonic("div1"); break;
                case sh2::Mnemonic::CMP_EQ:
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("eq", getOp1() == getOp2());
                    break;
                case sh2::Mnemonic::CMP_GE:
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("ge", static_cast<sint32>(getOp1()) >= static_cast<sint32>(getOp2()));
                    break;
                case sh2::Mnemonic::CMP_GT:
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("gt", static_cast<sint32>(getOp1()) > static_cast<sint32>(getOp2()));
                    break;
                case sh2::Mnemonic::CMP_HI:
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("hi", getOp1() > getOp2());
                    break;
                case sh2::Mnemonic::CMP_HS:
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("hs", getOp1() >= getOp2());
                    break;
                case sh2::Mnemonic::CMP_PL:
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("pl", static_cast<sint32>(getOp1()) > 0);
                    break;
                case sh2::Mnemonic::CMP_PZ:
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("pz", static_cast<sint32>(getOp1()) >= 0);
                    break;
                case sh2::Mnemonic::CMP_STR: //
                {
                    const uint32 tmp = getOp1() ^ getOp2();
                    const uint8 hh = tmp >> 24u;
                    const uint8 hl = tmp >> 16u;
                    const uint8 lh = tmp >> 8u;
                    const uint8 ll = tmp >> 0u;
                    drawMnemonic("cmp");
                    drawSeparator("/");
                    drawCond("str", !(hh && hl && lh && ll));
                    break;
                }
                case sh2::Mnemonic::TAS: drawMnemonic("tas"); break;
                case sh2::Mnemonic::TST: drawMnemonic("tst"); break;
                case sh2::Mnemonic::BF:
                    drawMnemonic("b");
                    drawCond("f", !probe.SR().T);
                    break;
                case sh2::Mnemonic::BFS:
                    drawMnemonic("b");
                    drawCond("f", !probe.SR().T);
                    drawSeparator("/");
                    drawMnemonic("s");
                    break;
                case sh2::Mnemonic::BT:
                    drawMnemonic("b");
                    drawCond("t", probe.SR().T);
                    break;
                case sh2::Mnemonic::BTS:
                    drawMnemonic("b");
                    drawCond("t", probe.SR().T);
                    drawSeparator("/");
                    drawMnemonic("s");
                    break;
                case sh2::Mnemonic::BRA: drawMnemonic("bra"); break;
                case sh2::Mnemonic::BRAF: drawMnemonic("braf"); break;
                case sh2::Mnemonic::BSR: drawMnemonic("bsr"); break;
                case sh2::Mnemonic::BSRF: drawMnemonic("bsrf"); break;
                case sh2::Mnemonic::JMP: drawMnemonic("jmp"); break;
                case sh2::Mnemonic::JSR: drawMnemonic("jsr"); break;
                case sh2::Mnemonic::TRAPA: drawMnemonic("trapa"); break;
                case sh2::Mnemonic::RTE: drawMnemonic("rte"); break;
                case sh2::Mnemonic::RTS: drawMnemonic("rts"); break;
                case sh2::Mnemonic::Illegal: drawIllegalMnemonic(); break;
                default: drawUnknownMnemonic(); break;
                }

                switch (disasm.opSize) {
                case sh2::OperandSize::Byte: drawSize("b"); break;
                case sh2::OperandSize::Word: drawSize("w"); break;
                case sh2::OperandSize::Long: drawSize("l"); break;
                default: break;
                }

                ImGui::SameLine(0, 0);
                const float endX = ImGui::GetCursorPosX();
                ImGui::SameLine(0, disasmCharSize.x * 10 - endX + startX);
                ImGui::Dummy(ImVec2(0, 0));
            };

            auto drawImm = [&](sint32 imm) {
                ImGui::TextColored(m_colors.disasm.immediate, "#0x%X", static_cast<uint32>(imm));
            };

            auto drawRegRead = [&](std::string_view regName) {
                ImGui::TextColored(m_colors.disasm.regRead, "%s", regName.data());
            };

            auto drawRegWrite = [&](std::string_view regName) {
                ImGui::TextColored(m_colors.disasm.regWrite, "%s", regName.data());
            };

            auto drawRegReadWrite = [&](std::string_view regName) {
                ImGui::TextColored(m_colors.disasm.regReadWrite, "%s", regName.data());
            };

            auto drawReg = [&](std::string_view regName, bool read, bool write) {
                if (read && write) {
                    drawRegReadWrite(regName);
                } else if (write) {
                    drawRegWrite(regName);
                } else {
                    drawRegRead(regName);
                }
            };

            auto drawRnRead = [&](uint8 rn) { ImGui::TextColored(m_colors.disasm.regRead, "r%u", rn); };
            auto drawRnWrite = [&](uint8 rn) { ImGui::TextColored(m_colors.disasm.regWrite, "r%u", rn); };
            auto drawRnReadWrite = [&](uint8 rn) { ImGui::TextColored(m_colors.disasm.regReadWrite, "r%u", rn); };

            auto drawRn = [&](uint8 rn, bool read, bool write) {
                if (read && write) {
                    drawRnReadWrite(rn);
                } else if (write) {
                    drawRnWrite(rn);
                } else {
                    drawRnRead(rn);
                }
            };

            auto drawRWSymbol = [&](std::string_view symbol, bool write) {
                const auto color = write ? m_colors.disasm.regWrite : m_colors.disasm.regRead;
                ImGui::TextColored(color, "%s", symbol.data());
            };

            auto drawPlus = [&] { ImGui::TextColored(m_colors.disasm.addrInc, "+"); };
            auto drawMinus = [&] { ImGui::TextColored(m_colors.disasm.addrDec, "-"); };
            auto drawComma = [&] { ImGui::TextColored(m_colors.disasm.separator, ", "); };

            auto drawOp = [&](const sh2::Operand &op) {
                if (op.type == sh2::Operand::Type::None) {
                    return false;
                }

                switch (op.type) {
                case sh2::Operand::Type::Imm: drawImm(op.immDisp); break;
                case sh2::Operand::Type::Rn: drawRn(op.reg, op.read, op.write); break;
                case sh2::Operand::Type::AtRn:
                    drawRWSymbol("@", op.write);
                    ImGui::SameLine(0, 0);
                    drawRnRead(op.reg);
                    break;
                case sh2::Operand::Type::AtRnPlus:
                    drawRWSymbol("@", op.write);
                    ImGui::SameLine(0, 0);
                    drawRnReadWrite(op.reg);
                    ImGui::SameLine(0, 0);
                    drawPlus();
                    break;
                case sh2::Operand::Type::AtMinusRn:
                    drawRWSymbol("@", op.write);
                    ImGui::SameLine(0, 0);
                    drawMinus();
                    ImGui::SameLine(0, 0);
                    drawRnReadWrite(op.reg);
                    break;
                case sh2::Operand::Type::AtDispRn:
                    drawRWSymbol("@(", op.write);
                    ImGui::SameLine(0, 0);
                    drawImm(op.immDisp);
                    ImGui::SameLine(0, 0);
                    drawComma();
                    ImGui::SameLine(0, 0);
                    drawRnRead(op.reg);
                    ImGui::SameLine(0, 0);
                    drawRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtR0Rn:
                    drawRWSymbol("@(", op.write);
                    ImGui::SameLine(0, 0);
                    drawRnRead(0);
                    ImGui::SameLine(0, 0);
                    drawComma();
                    ImGui::SameLine(0, 0);
                    drawRnRead(op.reg);
                    ImGui::SameLine(0, 0);
                    drawRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtDispGBR:
                    drawRWSymbol("@(", op.write);
                    ImGui::SameLine(0, 0);
                    drawImm(op.immDisp);
                    ImGui::SameLine(0, 0);
                    drawComma();
                    ImGui::SameLine(0, 0);
                    drawRegRead("gbr");
                    ImGui::SameLine(0, 0);
                    drawRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtR0GBR:
                    drawRWSymbol("@(", op.write);
                    ImGui::SameLine(0, 0);
                    drawRnRead(0);
                    ImGui::SameLine(0, 0);
                    drawComma();
                    ImGui::SameLine(0, 0);
                    drawRegRead("gbr");
                    ImGui::SameLine(0, 0);
                    drawRWSymbol(")", op.write);
                    break;
                case sh2::Operand::Type::AtDispPC:
                    drawRWSymbol("@(", false);
                    ImGui::SameLine(0, 0);
                    drawImm(address + op.immDisp);
                    ImGui::SameLine(0, 0);
                    drawRWSymbol(")", false);
                    break;
                case sh2::Operand::Type::AtDispPCWordAlign:
                    drawRWSymbol("@(", false);
                    ImGui::SameLine(0, 0);
                    drawImm((address & ~3) + op.immDisp);
                    ImGui::SameLine(0, 0);
                    drawRWSymbol(")", false);
                    break;
                case sh2::Operand::Type::DispPC: drawImm(address + op.immDisp); break;
                case sh2::Operand::Type::RnPC: drawRnRead(op.reg); break;
                case sh2::Operand::Type::SR: drawReg("sr", op.read, op.write); break;
                case sh2::Operand::Type::GBR: drawReg("gbr", op.read, op.write); break;
                case sh2::Operand::Type::VBR: drawReg("vbr", op.read, op.write); break;
                case sh2::Operand::Type::MACH: drawReg("mach", op.read, op.write); break;
                case sh2::Operand::Type::MACL: drawReg("macl", op.read, op.write); break;
                case sh2::Operand::Type::PR: drawReg("pr", op.read, op.write); break;
                default: return false;
                }

                return true;
            };

            auto drawOp1 = [&] { return drawOp(disasm.op1); };
            auto drawOp2 = [&] { return drawOp(disasm.op2); };

            // -------------------------------------------------------------------------------------------------------------

            ImGui::BeginGroup();
            {
                drawHighlight();
                drawIcons();
                drawAddress();
                ImGui::SameLine(0.0f, m_style.disasmSpacing);
                drawOpcodeBytes();
                ImGui::SameLine(0.0f, m_style.disasmSpacing);
                drawOpcodeAscii();
                ImGui::SameLine(0.0f, m_style.disasmSpacing);
                drawInstruction();
                ImGui::SameLine(0, 0);
                drawOp1();
                if (disasm.op1.type != sh2::Operand::Type::None && disasm.op2.type != sh2::Operand::Type::None) {
                    ImGui::SameLine(0, 0);
                    ImGui::TextColored(m_colors.disasm.separator, ", ");
                }
                ImGui::SameLine(0, 0);
                drawOp2();
                // TODO: show short annotations
                ImGui::SameLine(0, 0);
                ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 0));
            }
            ImGui::EndGroup();

            if (ImGui::IsItemHovered()) {
                if (ImGui::BeginTooltip()) {
                    auto notImm = [](sh2::Operand::Type opType) {
                        return opType != sh2::Operand::Type::Imm && opType != sh2::Operand::Type::DispPC;
                    };
                    if (notImm(disasm.op1.type) && drawOp1()) {
                        ImGui::SameLine(0, 0);
                        ImGui::TextColored(m_colors.disasm.separator, " = ");
                        ImGui::SameLine(0, 0);
                        drawImm(getOp1());
                    }
                    if (notImm(disasm.op2.type) && drawOp2()) {
                        ImGui::SameLine(0, 0);
                        ImGui::TextColored(m_colors.disasm.separator, " = ");
                        ImGui::SameLine(0, 0);
                        drawImm(getOp2());
                    }
                    // TODO: show detailed annotations
                    ImGui::EndTooltip();
                }
            }
        }
        ImGui::PopFont();
    }
    ImGui::EndChild();
}

} // namespace app::ui

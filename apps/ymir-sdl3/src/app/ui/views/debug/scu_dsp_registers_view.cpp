#include "scu_dsp_registers_view.hpp"

namespace app::ui {

SCUDSPRegistersView::SCUDSPRegistersView(SharedContext &context)
    : m_context(context)
    , m_dsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPRegistersView::Display() {
    const float flagsSpacing = 4.0f;
    const float frameHeight = ImGui::GetFrameHeight();
    const float framePadding = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    if (ImGui::BeginTable("scu_dsp_regs", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("AC");
        }
        if (ImGui::TableNextColumn()) {
            uint64 ac = m_dsp.AC.u64;
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 12);
            if (ImGui::InputScalar("##reg_ac", ImGuiDataType_U64, &ac, nullptr, nullptr, "%012X",
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
                m_dsp.AC.u64 = ac;
            }
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("P");
        }
        if (ImGui::TableNextColumn()) {
            uint64 p = m_dsp.P.u64;
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 12);
            if (ImGui::InputScalar("##reg_p", ImGuiDataType_U64, &p, nullptr, nullptr, "%012X",
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
                m_dsp.P.u64 = p;
            }
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("RX");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 8);
            ImGui::InputScalar("##reg_rx", ImGuiDataType_S32, &m_dsp.RX, nullptr, nullptr, "%08X",
                               ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("RY");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 8);
            ImGui::InputScalar("##reg_ry", ImGuiDataType_S32, &m_dsp.RY, nullptr, nullptr, "%08X",
                               ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("PC");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 2);
            ImGui::InputScalar("##reg_pc", ImGuiDataType_U8, &m_dsp.PC, nullptr, nullptr, "%02X",
                               ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("LOP");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 3);
            uint16 lop = m_dsp.loopCount;
            if (ImGui::InputScalar("##reg_lop", ImGuiDataType_U16, &lop, nullptr, nullptr, "%03X",
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
                m_dsp.loopCount = lop & 0xFFF;
            }
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("TOP");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 2);
            ImGui::InputScalar("##reg_top", ImGuiDataType_U8, &m_dsp.loopTop, nullptr, nullptr, "%02X",
                               ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Flags");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, flagsSpacing);

            ImGui::BeginGroup();
            ImGui::Checkbox("##reg_S", &m_dsp.sign);
            ImGui::NewLine();
            ImGui::SameLine(0, (frameHeight - ImGui::CalcTextSize("S").x) / 2);
            ImGui::TextUnformatted("S");
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::Checkbox("##reg_Z", &m_dsp.zero);
            ImGui::NewLine();
            ImGui::SameLine(0, (frameHeight - ImGui::CalcTextSize("Z").x) / 2);
            ImGui::TextUnformatted("Z");
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::Checkbox("##reg_C", &m_dsp.carry);
            ImGui::NewLine();
            ImGui::SameLine(0, (frameHeight - ImGui::CalcTextSize("C").x) / 2);
            ImGui::TextUnformatted("C");
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::Checkbox("##reg_V", &m_dsp.overflow);
            ImGui::NewLine();
            ImGui::SameLine(0, (frameHeight - ImGui::CalcTextSize("V").x) / 2);
            ImGui::TextUnformatted("V");
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::Checkbox("##reg_T0", &m_dsp.dmaRun);
            ImGui::NewLine();
            ImGui::SameLine(0, (frameHeight - ImGui::CalcTextSize("T0").x) / 2);
            ImGui::TextUnformatted("T0");
            ImGui::EndGroup();

            ImGui::PopStyleVar();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("CT");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, flagsSpacing);
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            for (uint32 i = 0; i < 4; i++) {
                uint8 ct = m_dsp.CT.array[i];
                if (i > 0) {
                    ImGui::SameLine();
                }
                ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 2);
                if (ImGui::InputScalar(fmt::format("##reg_ct{}", i).c_str(), ImGuiDataType_U8, &ct, nullptr, nullptr,
                                       "%02X", ImGuiInputTextFlags_CharsHexadecimal)) {
                    m_dsp.CT.array[i] = ct & 0x3F;
                }
            }
            ImGui::PopFont();
            ImGui::PopStyleVar();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("RA0");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 7);
            if (ImGui::InputScalar("##reg_ra0", ImGuiDataType_S32, &m_dsp.dmaReadAddr, nullptr, nullptr, "%07X",
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
                m_dsp.dmaReadAddr &= 0x7FF'FFFC;
            }
            ImGui::PopFont();
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("WA0");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fonts.sizes.medium);
            ImGui::SetNextItemWidth(framePadding * 2 + hexCharWidth * 7);
            if (ImGui::InputScalar("##reg_wa0", ImGuiDataType_S32, &m_dsp.dmaWriteAddr, nullptr, nullptr, "%07X",
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
                m_dsp.dmaWriteAddr &= 0x7FF'FFFC;
            }
            ImGui::PopFont();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

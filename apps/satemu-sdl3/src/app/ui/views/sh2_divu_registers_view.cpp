#include "sh2_divu_registers_view.hpp"

using namespace satemu;

namespace app::ui {

SH2DivisionUnitRegistersView::SH2DivisionUnitRegistersView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2DivisionUnitRegistersView::Display() {
    auto &probe = m_sh2.GetProbe();
    // auto &divu = probe.DIVU();
    auto &intc = probe.INTC();

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText("Registers");

    uint32 dummy = 0; // TODO: use real register

    if (ImGui::BeginTable("divu_regs", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();

        auto drawReg = [&](uint32 &value, const char *name) {
            ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 8);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            ImGui::InputScalar(fmt::format("##{}", name).c_str(), ImGuiDataType_U32, &value, nullptr, nullptr, "%08X",
                               ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(name);
        };

        if (ImGui::TableNextColumn()) {
            drawReg(dummy, "DVDNTH");
            drawReg(dummy, "DVDNTUH");
        }

        if (ImGui::TableNextColumn()) {
            drawReg(dummy, "DVDNTL");
            drawReg(dummy, "DVDNTUL");
        }

        if (ImGui::TableNextColumn()) {
            drawReg(dummy, "DVDNT");
            drawReg(dummy, "DVSR");
        }

        if (ImGui::TableNextColumn()) {
            drawReg(dummy, "DVCR");
            bool dummyFlag = false;
            ImGui::Checkbox("OVF", &dummyFlag);
            ImGui::SameLine();
            ImGui::Checkbox("OVFIE", &dummyFlag);
        }

        ImGui::EndTable();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Interrupt:");

    ImGui::SameLine();

    uint8 vector = intc.GetVector(sh2::InterruptSource::DIVU_OVFI);
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::InputScalar("##vcrdiv", ImGuiDataType_U8, &vector, nullptr, nullptr, "%02X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        intc.SetVector(sh2::InterruptSource::DIVU_OVFI, vector);
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextUnformatted("vector (VCRDIV)");

    ImGui::SameLine();

    uint8 level = intc.GetLevel(sh2::InterruptSource::DIVU_OVFI);
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 1);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::InputScalar("##ipra_divuipn", ImGuiDataType_U8, &level, nullptr, nullptr, "%X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        intc.SetLevel(sh2::InterruptSource::DIVU_OVFI, std::min<uint8>(level, 0xF));
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextUnformatted("level (IPRA.DIVUIP3-0)");
}

} // namespace app::ui

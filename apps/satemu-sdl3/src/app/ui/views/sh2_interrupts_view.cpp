#include "sh2_interrupts_view.hpp"

#include <imgui.h>

namespace app {

SH2InterruptsView::SH2InterruptsView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2InterruptsView::Display() {
    using namespace satemu;

    auto &probe = m_sh2.GetProbe();
    auto &intc = probe.INTC();

    ImGui::PushFont(m_context.fonts.monospaceMedium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::BeginGroup();

    // --- INTC ----------------------------------------------------------------
    {
        ImGui::SeparatorText("INTC");
        ImGui::PushFont(m_context.fonts.monospaceMedium);
        ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 4);
        uint16 ICR = intc.ICR.Read();
        if (ImGui::InputScalar("ICR", ImGuiDataType_U16, &ICR, nullptr, nullptr, "%04X")) {
            intc.ICR.Write<true, true, true>(ICR);
        }
        ImGui::PopFont();

        ImGui::SameLine();

        ImGui::Checkbox("NMIL", &intc.ICR.NMIL);
        ImGui::SameLine();
        ImGui::Checkbox("NMIE", &intc.ICR.NMIE);
        ImGui::SameLine();
        ImGui::Checkbox("VECMD", &intc.ICR.VECMD);
    }

    // --- Interrupt signals ---------------------------------------------------
    {
        ImGui::SeparatorText("Interrupt signals");
    }

    ImGui::EndGroup();
}

} // namespace app

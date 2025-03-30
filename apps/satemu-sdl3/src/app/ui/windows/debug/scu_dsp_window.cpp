#include "scu_dsp_window.hpp"

namespace app::ui {

SCUDSPWindow::SCUDSPWindow(SharedContext &context)
    : WindowBase(context)
    , m_regsView(context)
    , m_dmaView(context) {

    m_windowConfig.name = "SCU DSP";
}

void SCUDSPWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(750, 368), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDSPWindow::DrawContents() {
    if (ImGui::BeginTable("scu_dsp", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Registers", ImGuiTableColumnFlags_WidthFixed, 250);
        ImGui::TableSetupColumn("Disassembly", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("DMA", ImGuiTableColumnFlags_WidthFixed, 350);

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("Registers");
            m_regsView.Display();
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("(placeholder for controls)");
            ImGui::SeparatorText("Disassembly");
            ImGui::TextUnformatted("(placeholder for disassembly)");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("DMA");
            m_dmaView.Display();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui
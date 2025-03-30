#include "scu_dsp_window.hpp"

namespace app::ui {

SCUDSPWindow::SCUDSPWindow(SharedContext &context)
    : WindowBase(context)
    , m_regsView(context)
    , m_disasmView(context)
    , m_dmaRegsView(context)
    , m_dmaTraceView(context) {

    m_windowConfig.name = "SCU DSP";
}

void SCUDSPWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(750, 368), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDSPWindow::DrawContents() {
    if (ImGui::BeginTable("scu_dsp", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Registers", ImGuiTableColumnFlags_WidthFixed, 170);
        ImGui::TableSetupColumn("Disassembly", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("DMA", ImGuiTableColumnFlags_WidthFixed, 300);

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("Registers");
            m_regsView.Display();
        }
        if (ImGui::TableNextColumn()) {
            // TODO
            ImGui::TextUnformatted("(placeholder for controls)");
            ImGui::SeparatorText("Disassembly");
            m_disasmView.Display();
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("DMA");
            m_dmaRegsView.Display();

            ImGui::SeparatorText("Trace");
            m_dmaTraceView.Display();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui
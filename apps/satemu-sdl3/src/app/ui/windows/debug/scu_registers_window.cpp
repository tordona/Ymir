#include "scu_registers_window.hpp"

#include <imgui.h>

namespace app::ui {

SCURegistersWindow::SCURegistersWindow(SharedContext &context)
    : WindowBase(context)
    , m_regsView(context)
    , m_intrView(context)
    , m_timersView(context)
    , m_intrTraceView(context) {

    m_windowConfig.name = "SCU registers";
}

void SCURegistersWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(690, 676), ImVec2(FLT_MAX, FLT_MAX));
}

void SCURegistersWindow::DrawContents() {
    if (ImGui::BeginTable("scu_regs", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 380);
        ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("Registers");
            m_regsView.Display();

            ImGui::SeparatorText("Interrupts");
            m_intrView.Display();

            ImGui::SeparatorText("Timers");
            m_timersView.Display();
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("Interrupt trace");
            m_intrTraceView.Display();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

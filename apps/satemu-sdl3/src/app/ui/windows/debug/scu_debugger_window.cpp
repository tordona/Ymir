#include "scu_debugger_window.hpp"

#include <imgui.h>

namespace app::ui {

SCUDebuggerWindow::SCUDebuggerWindow(SharedContext &context)
    : WindowBase(context)
    , m_intrView(context)
    , m_debugOutput(context) {

    m_windowConfig.name = "SCU";
}

void SCUDebuggerWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 200), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDebuggerWindow::DrawContents() {
    if (ImGui::BeginTable("root", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 390);
        ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        if (ImGui::TableNextColumn()) {
            m_intrView.Display();
        }
        if (ImGui::TableNextColumn()) {
            m_debugOutput.Display();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

#include "scu_debugger_window.hpp"

#include <imgui.h>

namespace app::ui {

SCUDebuggerWindow::SCUDebuggerWindow(SharedContext &context)
    : WindowBase(context)
    , m_regsView(context)
    , m_intrView(context)
    , m_timersView(context)
    , m_debugOutputView(context) {

    m_windowConfig.name = "SCU";
}

void SCUDebuggerWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 676), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDebuggerWindow::DrawContents() {
    if (ImGui::BeginTable("root", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 390);
        ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        if (ImGui::TableNextColumn()) {
            m_regsView.Display();
            m_intrView.Display();
            m_timersView.Display();
        }
        if (ImGui::TableNextColumn()) {
            m_debugOutputView.Display();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

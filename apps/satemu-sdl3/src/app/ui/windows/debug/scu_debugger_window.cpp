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
    ImGui::SetNextWindowSizeConstraints(ImVec2(780, 676), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDebuggerWindow::DrawContents() {
    if (ImGui::BeginTable("root", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 390);
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
            if (ImGui::BeginTabBar("##right_tabs")) {
                if (ImGui::BeginTabItem("DMA")) {
                    // TODO
                    ImGui::TextUnformatted("(placeholder text)");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Debug output")) {
                    m_debugOutputView.Display();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

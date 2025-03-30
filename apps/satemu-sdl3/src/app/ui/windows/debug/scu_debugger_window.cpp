#include "scu_debugger_window.hpp"

#include <imgui.h>

namespace app::ui {

SCUDebuggerWindow::SCUDebuggerWindow(SharedContext &context)
    : WindowBase(context)
    , m_regsView(context)
    , m_intrView(context)
    , m_timersView(context) {

    m_windowConfig.name = "SCU";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SCUDebuggerWindow::DrawContents() {
    ImGui::SeparatorText("Registers");
    m_regsView.Display();

    ImGui::SeparatorText("Interrupts");
    m_intrView.Display();

    ImGui::SeparatorText("Timers");
    m_timersView.Display();
}

} // namespace app::ui

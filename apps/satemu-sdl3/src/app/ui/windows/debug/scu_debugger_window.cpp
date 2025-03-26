#include "scu_debugger_window.hpp"

#include <imgui.h>

namespace app::ui {

SCUDebuggerWindow::SCUDebuggerWindow(SharedContext &context)
    : WindowBase(context)
    , m_scu(context.saturn.SCU) {

    m_windowConfig.name = "SCU";
}

void SCUDebuggerWindow::DrawContents() {
    ImGui::SeparatorText("Interrupts");
    ImGui::Text("%08X mask", m_scu.GetInterruptMask().u32);
    ImGui::Text("%08X status", m_scu.GetInterruptStatus().u32);
}

} // namespace app::ui

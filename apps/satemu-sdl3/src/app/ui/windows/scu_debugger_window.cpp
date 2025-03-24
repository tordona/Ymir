#include "scu_debugger_window.hpp"

#include <imgui.h>

namespace app::ui {

SCUDebuggerWindow::SCUDebuggerWindow(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUDebuggerWindow::Display() {
    if (!Open) {
        return;
    }

    if (ImGui::Begin("SCU", &Open)) {
        ImGui::TextUnformatted("Interrupts");
        ImGui::Text("%08X mask", m_scu.GetInterruptMask().u32);
        ImGui::Text("%08X status", m_scu.GetInterruptStatus().u32);
    }
    ImGui::End();
}

} // namespace app::ui

#include "scu_debugger.hpp"

#include <imgui.h>

namespace app {

SCUDebugger::SCUDebugger(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUDebugger::Display() {
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

} // namespace app

#include "scu_interrupts_view.hpp"

namespace app::ui {

SCUInterruptsView::SCUInterruptsView(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUInterruptsView::Display() {
    auto &probe = m_scu.GetProbe();

    ImGui::SeparatorText("Interrupts");
    ImGui::Text("%08X mask", probe.GetInterruptMask().u32);
    ImGui::Text("%08X status", probe.GetInterruptStatus().u32);
}

} // namespace app::ui

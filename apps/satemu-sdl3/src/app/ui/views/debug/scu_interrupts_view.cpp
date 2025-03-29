#include "scu_interrupts_view.hpp"

namespace app::ui {

SCUInterruptsView::SCUInterruptsView(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUInterruptsView::Display() {
    ImGui::SeparatorText("Interrupts");
    ImGui::Text("%08X mask", m_scu.GetInterruptMask().u32);
    ImGui::Text("%08X status", m_scu.GetInterruptStatus().u32);
}

} // namespace app::ui

#include "scu_dma_view.hpp"

namespace app::ui {

SCUDMAView::SCUDMAView(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUDMAView::Display() {
    auto &probe = m_scu.GetProbe();

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::TextUnformatted("(placeholder text)");
}

} // namespace app::ui

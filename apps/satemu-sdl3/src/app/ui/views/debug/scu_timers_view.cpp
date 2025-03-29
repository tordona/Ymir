#include "scu_timers_view.hpp"

namespace app::ui {

SCUTimersView::SCUTimersView(SharedContext &context)
    : m_context(context)
    , m_scu(context.saturn.SCU) {}

void SCUTimersView::Display() {
    ImGui::SeparatorText("Timers");

    ImGui::TextUnformatted("(placeholder text)");
}

} // namespace app::ui

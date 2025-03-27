#include "sh2_wdt_view.hpp"

using namespace satemu;

namespace app::ui {

SH2WatchdogTimerView::SH2WatchdogTimerView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2WatchdogTimerView::Display() {
    auto &probe = m_sh2.GetProbe();
    auto &cache = probe.GetCache();

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText("Watchdog timer");

    ImGui::TextUnformatted("(TODO)");
}

} // namespace app::ui

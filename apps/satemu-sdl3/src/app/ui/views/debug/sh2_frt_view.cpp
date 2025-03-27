#include "sh2_frt_view.hpp"

using namespace satemu;

namespace app::ui {

SH2FreeRunningTimerView::SH2FreeRunningTimerView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2FreeRunningTimerView::Display() {
    auto &probe = m_sh2.GetProbe();
    auto &cache = probe.GetCache();

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText("Free-running timer");

    ImGui::TextUnformatted("(TODO)");
}

} // namespace app::ui

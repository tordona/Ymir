#include "sh2_timers_window.hpp"

using namespace satemu;

namespace app::ui {

SH2TimersWindow::SH2TimersWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master)
/*, m_frtView(context, m_sh2)
, m_wdtView(context, m_sh2)*/
{

    m_windowConfig.name = fmt::format("{}SH2 timers", master ? 'M' : 'S');
}

void SH2TimersWindow::PrepareWindow() {
    // ImGui::SetNextWindowSizeConstraints(ImVec2(250, 300), ImVec2(600, FLT_MAX));
}

void SH2TimersWindow::DrawContents() {
    // m_frtView.Display();
    // m_wdtView.Display();
    ImGui::TextUnformatted("(placeholder)");
}

} // namespace app::ui

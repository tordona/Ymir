#include "scu_dsp_window.hpp"

namespace app::ui {

SCUDSPWindow::SCUDSPWindow(SharedContext &context)
    : WindowBase(context)
    , m_dspView(context) {

    m_windowConfig.name = "SCU DSP";
}

void SCUDSPWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 150), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDSPWindow::DrawContents() {
    m_dspView.Display();
}

} // namespace app::ui
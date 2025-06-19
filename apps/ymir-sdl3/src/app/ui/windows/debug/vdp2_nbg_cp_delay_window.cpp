#include "vdp2_nbg_cp_delay_window.hpp"

namespace app::ui {

VDP2NBGCharPatDelayWindow::VDP2NBGCharPatDelayWindow(SharedContext &context)
    : VDPWindowBase(context)
    , m_nbgDelayView(context, m_vdp) {

    m_windowConfig.name = "VDP2 NBG character pattern delay";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void VDP2NBGCharPatDelayWindow::DrawContents() {
    m_nbgDelayView.Display();
}

} // namespace app::ui

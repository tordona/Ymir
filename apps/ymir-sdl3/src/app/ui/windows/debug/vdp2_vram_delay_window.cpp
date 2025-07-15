#include "vdp2_vram_delay_window.hpp"

namespace app::ui {

VDP2VRAMDelayWindow::VDP2VRAMDelayWindow(SharedContext &context)
    : VDPWindowBase(context)
    , m_nbgDelayView(context, m_vdp) {

    m_windowConfig.name = "VDP2 VRAM access delay";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void VDP2VRAMDelayWindow::DrawContents() {
    m_nbgDelayView.Display();
}

} // namespace app::ui

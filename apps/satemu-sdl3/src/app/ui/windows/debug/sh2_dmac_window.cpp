#include "sh2_dmac_window.hpp"

using namespace satemu;

namespace app::ui {

SH2DMAControllerWindow::SH2DMAControllerWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master)
    , m_dmacRegsView(context, m_sh2) {

    m_windowConfig.name = fmt::format("{}SH2 DMA controller", master ? 'M' : 'S');
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SH2DMAControllerWindow::DrawContents() {
    m_dmacRegsView.Display();
}

} // namespace app::ui

#include "scu_dma_window.hpp"

namespace app::ui {

SCUDMAWindow::SCUDMAWindow(SharedContext &context)
    : WindowBase(context)
    , m_dmaView(context) {

    m_windowConfig.name = "SCU DMA";
}

void SCUDMAWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 150), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDMAWindow::DrawContents() {
    m_dmaView.Display();
}

} // namespace app::ui
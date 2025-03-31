#include "scu_dma_trace_window.hpp"

namespace app::ui {

SCUDMATraceWindow::SCUDMATraceWindow(SharedContext &context)
    : WindowBase(context)
    , m_dmaTraceView(context) {

    m_windowConfig.name = "SCU DMA trace";
}

void SCUDMATraceWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(425, 200), ImVec2(525, FLT_MAX));
}

void SCUDMATraceWindow::DrawContents() {
    m_dmaTraceView.Display();
}

} // namespace app::ui

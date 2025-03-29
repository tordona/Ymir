#include "sh2_dmac_trace_window.hpp"

namespace app::ui {

SH2DMAControllerTraceWindow::SH2DMAControllerTraceWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master) {

    m_windowConfig.name = fmt::format("{}SH2 DMA controller trace", master ? 'M' : 'S');
}

void SH2DMAControllerTraceWindow::PrepareWindow() {}

void SH2DMAControllerTraceWindow::DrawContents() {
    ImGui::TextUnformatted("(placeholder text)");
}

} // namespace app::ui

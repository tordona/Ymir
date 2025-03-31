#include "sh2_interrupt_trace_window.hpp"

using namespace satemu;

namespace app::ui {

SH2InterruptTraceWindow::SH2InterruptTraceWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master)
    , m_intrTraceView(context, m_tracer) {

    m_windowConfig.name = fmt::format("{}SH2 interrupt trace", master ? 'M' : 'S');
}

void SH2InterruptTraceWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 200), ImVec2(600, FLT_MAX));
}

void SH2InterruptTraceWindow::DrawContents() {
    m_intrTraceView.Display();
}

} // namespace app::ui

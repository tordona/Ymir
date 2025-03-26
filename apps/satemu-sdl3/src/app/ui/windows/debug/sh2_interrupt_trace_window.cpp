#include "sh2_interrupt_trace_window.hpp"

using namespace satemu;

namespace app::ui {

SH2InterruptTraceWindow::SH2InterruptTraceWindow(SharedContext &context, bool master)
    : WindowBase(context)
    , m_intrTraceView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2,
                      master ? context.tracers.masterSH2 : context.tracers.slaveSH2) {
    m_windowConfig.name = fmt::format("{}SH2 interrupt trace", master ? "M" : "S");
}

void SH2InterruptTraceWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 300), ImVec2(600, FLT_MAX));
}

void SH2InterruptTraceWindow::DrawContents() {
    m_intrTraceView.Display();
}

} // namespace app::ui

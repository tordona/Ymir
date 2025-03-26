#include "sh2_divu_window.hpp"

using namespace satemu;

namespace app::ui {

SH2DivisionUnitWindow::SH2DivisionUnitWindow(SharedContext &context, bool master)
    : WindowBase(context)
    , m_divuRegsView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2)
    , m_divuTraceView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2,
                      master ? context.tracers.masterSH2 : context.tracers.slaveSH2) {
    m_windowConfig.name = fmt::format("{}SH2 division unit (DIVU)", master ? "M" : "S");
}

void SH2DivisionUnitWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(570, 356), ImVec2(570, FLT_MAX));
}

void SH2DivisionUnitWindow::DrawContents() {
    m_divuRegsView.Display();
    m_divuTraceView.Display();
}

} // namespace app::ui

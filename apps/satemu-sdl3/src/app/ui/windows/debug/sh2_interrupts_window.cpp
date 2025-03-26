#include "sh2_interrupts_window.hpp"

using namespace satemu;

namespace app::ui {

SH2InterruptsWindow::SH2InterruptsWindow(SharedContext &context, bool master)
    : WindowBase(context)
    , m_intrView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2) {
    m_windowConfig.name = fmt::format("{}SH2 interrupts", master ? "M" : "S");
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SH2InterruptsWindow::DrawContents() {
    m_intrView.Display();
}

} // namespace app::ui

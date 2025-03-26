#include "sh2_debugger_window.hpp"

using namespace satemu;

namespace app::ui {

SH2DebuggerWindow::SH2DebuggerWindow(SharedContext &context, bool master)
    : WindowBase(context)
    , m_regsView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2, master)
    , m_disasmView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2) {
    m_windowConfig.name = fmt::format("{}SH2", master ? "M" : "S");
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SH2DebuggerWindow::DrawContents() {
    m_regsView.Display();
    ImGui::SameLine();
    m_disasmView.Display();
}

} // namespace app::ui

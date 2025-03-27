#include "sh2_debugger_window.hpp"

using namespace satemu;

namespace app::ui {

SH2DebuggerWindow::SH2DebuggerWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master)
    , m_regsView(context, m_sh2)
    , m_disasmView(context, m_sh2) {

    m_windowConfig.name = fmt::format("[WIP] {}SH2 debugger", master ? 'M' : 'S');
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SH2DebuggerWindow::DrawContents() {
    m_regsView.Display();
    ImGui::SameLine();
    m_disasmView.Display();
}

} // namespace app::ui

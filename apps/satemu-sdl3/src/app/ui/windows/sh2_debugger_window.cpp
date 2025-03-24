#include "sh2_debugger_window.hpp"

#include <imgui.h>

using namespace satemu;

namespace app::ui {

SH2DebuggerWindow::SH2DebuggerWindow(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_sh2(master ? context.saturn.masterSH2 : context.saturn.slaveSH2)
    , m_regsView(context, m_sh2, master)
    , m_disasmView(context, m_sh2) {}

void SH2DebuggerWindow::Display() {
    if (!Open) {
        return;
    }

    std::string name = fmt::format("{}SH2", m_master ? "M" : "S");
    if (ImGui::Begin(name.c_str(), &Open, ImGuiWindowFlags_AlwaysAutoResize)) {
        m_regsView.Display();

        ImGui::SameLine();

        m_disasmView.Display();
    }
    ImGui::End();
}

} // namespace app::ui

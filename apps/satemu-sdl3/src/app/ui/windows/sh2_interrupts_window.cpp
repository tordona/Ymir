#include "sh2_interrupts_window.hpp"

#include <imgui.h>

using namespace satemu;

namespace app::ui {

SH2InterruptsWindow::SH2InterruptsWindow(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_intrView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2) {}

void SH2InterruptsWindow::Display() {
    if (!Open) {
        return;
    }

    std::string name = fmt::format("{}SH2 interrupts", m_master ? "M" : "S");
    if (ImGui::Begin(name.c_str(), &Open, ImGuiWindowFlags_AlwaysAutoResize)) {
        m_intrView.Display();
    }
    ImGui::End();
}

} // namespace app::ui

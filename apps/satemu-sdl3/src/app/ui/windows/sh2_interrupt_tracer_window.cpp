#include "sh2_interrupt_tracer_window.hpp"

#include <imgui.h>

using namespace satemu;

namespace app::ui {

SH2InterruptTracerWindow::SH2InterruptTracerWindow(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_intrTracerView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2,
                       master ? context.tracers.masterSH2 : context.tracers.slaveSH2) {}

void SH2InterruptTracerWindow::Display() {
    if (!Open) {
        return;
    }

    std::string name = fmt::format("{}SH2 interrupt tracer", m_master ? "M" : "S");
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 300), ImVec2(600, FLT_MAX));
    if (ImGui::Begin(name.c_str(), &Open)) {
        m_intrTracerView.Display();
    }
    ImGui::End();
}

} // namespace app::ui

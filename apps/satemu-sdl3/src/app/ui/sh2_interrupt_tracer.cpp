#include "sh2_interrupt_tracer.hpp"

#include <imgui.h>

using namespace satemu;

namespace app {

SH2InterruptTracer::SH2InterruptTracer(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_intrTracerView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2,
                       master ? context.tracers.masterSH2 : context.tracers.slaveSH2) {}

void SH2InterruptTracer::Display() {
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

} // namespace app

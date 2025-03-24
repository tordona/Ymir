#include "sh2_division_unit_window.hpp"

#include <imgui.h>

using namespace satemu;

namespace app::ui {

SH2DivisionUnitWindow::SH2DivisionUnitWindow(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_divisionUnitTracesView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2,
                               master ? context.tracers.masterSH2 : context.tracers.slaveSH2) {}

void SH2DivisionUnitWindow::Display() {
    if (!Open) {
        return;
    }

    std::string name = fmt::format("{}SH2 division unit (DIVU)", m_master ? "M" : "S");
    ImGui::SetNextWindowSizeConstraints(ImVec2(1000, 300), ImVec2(1200, FLT_MAX));
    if (ImGui::Begin(name.c_str(), &Open)) {
        // TODO: registers view
        // auto &probe = m_sh2.GetProbe();
        // auto &divu = probe.DIVU();

        m_divisionUnitTracesView.Display();
    }
    ImGui::End();
}

} // namespace app::ui

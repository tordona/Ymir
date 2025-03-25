#include "sh2_divu_window.hpp"

#include <imgui.h>

using namespace satemu;

namespace app::ui {

SH2DivisionUnitWindow::SH2DivisionUnitWindow(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_divuRegsView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2)
    , m_divuTraceView(context, master ? context.saturn.masterSH2 : context.saturn.slaveSH2,
                      master ? context.tracers.masterSH2 : context.tracers.slaveSH2) {}

void SH2DivisionUnitWindow::Display() {
    if (!Open) {
        return;
    }

    std::string name = fmt::format("{}SH2 division unit (DIVU)", m_master ? "M" : "S");
    ImGui::SetNextWindowSizeConstraints(ImVec2(570, 356), ImVec2(570, FLT_MAX));
    if (ImGui::Begin(name.c_str(), &Open)) {
        m_divuRegsView.Display();
        m_divuTraceView.Display();
    }
    ImGui::End();
}

} // namespace app::ui

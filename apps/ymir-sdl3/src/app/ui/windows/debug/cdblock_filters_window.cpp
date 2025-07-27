#include "cdblock_filters_window.hpp"

namespace app::ui {

CDBlockFiltersWindow::CDBlockFiltersWindow(SharedContext &context)
    : CDBlockWindowBase(context)
    , m_filtersView(context) {

    m_windowConfig.name = "CD Block filters";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void CDBlockFiltersWindow::PrepareWindow() {
    /*ImGui::SetNextWindowSizeConstraints(ImVec2(450 * m_context.displayScale, 180 * m_context.displayScale),
                                        ImVec2(FLT_MAX, FLT_MAX));*/
}

void CDBlockFiltersWindow::DrawContents() {
    m_filtersView.Display();
}

} // namespace app::ui

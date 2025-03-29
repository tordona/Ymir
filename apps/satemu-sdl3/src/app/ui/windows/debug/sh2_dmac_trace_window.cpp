#include "sh2_dmac_trace_window.hpp"

namespace app::ui {

SH2DMAControllerTraceWindow::SH2DMAControllerTraceWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master)
    , m_dmac0TraceView(context, m_sh2.GetProbe().DMAC0(), 0)
    , m_dmac1TraceView(context, m_sh2.GetProbe().DMAC1(), 1) {

    m_windowConfig.name = fmt::format("{}SH2 DMA controller trace", master ? 'M' : 'S');
}

void SH2DMAControllerTraceWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 200), ImVec2(FLT_MAX, FLT_MAX));
}

void SH2DMAControllerTraceWindow::DrawContents() {
    if (ImGui::BeginTable("dmac_traces", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow();

        if (ImGui::TableNextColumn()) {
            m_dmac0TraceView.Display();
        }
        if (ImGui::TableNextColumn()) {
            m_dmac1TraceView.Display();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

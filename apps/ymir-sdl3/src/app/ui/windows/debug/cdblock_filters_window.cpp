#include "cdblock_filters_window.hpp"

using namespace ymir;

#include <ymir/hw/cdblock/cdblock.hpp>

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
    auto &probe = m_cdblock.GetProbe();
    const uint8 cdDeviceConnection = probe.GetCDDeviceConnection();
    if (cdDeviceConnection != cdblock::Filter::kDisconnected) {
        ImGui::Text("CD device connected to output %u", cdDeviceConnection);
    } else {
        ImGui::Text("CD device disconnected");
    }
    ImGui::Separator();
    m_filtersView.Display();
}

} // namespace app::ui

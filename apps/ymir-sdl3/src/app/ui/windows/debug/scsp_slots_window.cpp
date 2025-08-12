#include "scsp_slots_window.hpp"

namespace app::ui {

SCSPSlotsWindow::SCSPSlotsWindow(SharedContext &context)
    : SCSPWindowBase(context)
    , m_slotsView(context) {

    m_windowConfig.name = "SCSP slots";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SCSPSlotsWindow::DrawContents() {
    m_slotsView.Display();

    // TODO: move this to a view and into a separate window

    const float availWidth = ImGui::GetContentRegionAvail().x;
    const float height = 150.0f * m_context.displayScale;
    const float centerLineThickness = 1.0f * m_context.displayScale;
    const float waveThickness = 1.5f * m_context.displayScale;
    const auto pos = ImGui::GetCursorScreenPos();

    ImGui::Dummy(ImVec2(availWidth, height));

    const uint32 count = m_tracer.output.Count();
    std::vector<ImVec2> points{};
    points.resize(count);
    for (uint32 i = 0; i < count; ++i) {
        points[i].x = pos.x + static_cast<float>(i) / count * availWidth;
        points[i].y =
            pos.y + height - (static_cast<float>(m_tracer.output.Read(i).left) + 32768.0f) / 65535.0f * height;
    }

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->AddLine(ImVec2(pos.x, pos.y + height * 0.5f), ImVec2(pos.x + availWidth, pos.y + height * 0.5f),
                      0x7FFFFFFF, centerLineThickness);
    drawList->AddPolyline(points.data(), points.size(), 0xFFFFFFFF, ImDrawFlags_None, waveThickness);
}

} // namespace app::ui

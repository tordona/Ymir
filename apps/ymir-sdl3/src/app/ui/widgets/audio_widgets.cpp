#include "audio_widgets.hpp"

#include <imgui.h>

namespace app::ui::widgets {

void Oscilloscope(SharedContext &ctx, std::span<const float> waveform, ImVec2 size) {
    if (size.x == 0.0f) {
        size.x = ImGui::GetContentRegionAvail().x;
    }
    if (size.y == 0.0f) {
        size.y = 150.0f * ctx.displayScale;
    }

    // TODO: move all style and colors to the SharedContext
    const float centerLineThickness = 1.0f * ctx.displayScale;
    const float waveThickness = 1.5f * ctx.displayScale;

    const auto pos = ImGui::GetCursorScreenPos();

    std::vector<ImVec2> points{};
    points.resize(waveform.size());
    for (uint32 i = 0; i < waveform.size(); ++i) {
        const float wfVal = std::clamp(waveform[i], -1.0f, 1.0f);
        points[i].x = pos.x + static_cast<float>(i) / waveform.size() * size.x;
        points[i].y = pos.y + size.y - (wfVal + 1.0f) * 0.5f * size.y;
    }

    auto *drawList = ImGui::GetWindowDrawList();
    // TODO: reusable background/border drawing functions
    // TODO: background
    // TODO: improve efficiency
    drawList->AddLine(ImVec2(pos.x, pos.y + size.y * 0.5f), ImVec2(pos.x + size.x, pos.y + size.y * 0.5f), 0x7FFFFFFF,
                      centerLineThickness);
    drawList->AddPolyline(points.data(), points.size(), 0xFFFFFFFF, ImDrawFlags_None, waveThickness);
    // TODO: border

    ImGui::Dummy(size);
}

void Oscilloscope(SharedContext &ctx, std::span<const StereoSample> waveform, ImVec2 size) {
    if (size.x == 0.0f) {
        size.x = ImGui::GetContentRegionAvail().x;
    }
    if (size.y == 0.0f) {
        size.y = 150.0f * ctx.displayScale;
    }

    // TODO: move all style and colors to the SharedContext
    const float centerLineThickness = 1.0f * ctx.displayScale;
    const float waveThickness = 1.5f * ctx.displayScale;

    const auto pos = ImGui::GetCursorScreenPos();

    std::vector<ImVec2> pointsL{};
    std::vector<ImVec2> pointsR{};
    pointsL.resize(waveform.size());
    pointsR.resize(waveform.size());
    for (uint32 i = 0; i < waveform.size(); ++i) {
        const float wfValL = std::clamp(waveform[i].left, -1.0f, 1.0f);
        pointsL[i].x = pos.x + static_cast<float>(i) / waveform.size() * size.x;
        pointsL[i].y = pos.y + size.y - (wfValL + 1.0f) * 0.5f * size.y;

        const float wfValR = std::clamp(waveform[i].right, -1.0f, 1.0f);
        pointsR[i].x = pos.x + static_cast<float>(i) / waveform.size() * size.x;
        pointsR[i].y = pos.y + size.y - (wfValR + 1.0f) * 0.5f * size.y;
    }

    auto *drawList = ImGui::GetWindowDrawList();
    // TODO: reusable background/border drawing functions
    // TODO: background
    // TODO: improve efficiency
    drawList->AddLine(ImVec2(pos.x, pos.y + size.y * 0.5f), ImVec2(pos.x + size.x, pos.y + size.y * 0.5f), 0x45FFFFFF,
                      centerLineThickness);
    drawList->AddPolyline(pointsL.data(), pointsL.size(), 0xFF7FBFFF, ImDrawFlags_None, waveThickness);
    drawList->AddPolyline(pointsR.data(), pointsR.size(), 0xFFFFBF7F, ImDrawFlags_None, waveThickness);
    // TODO: border

    ImGui::Dummy(size);
}

} // namespace app::ui::widgets

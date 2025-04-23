#include "savestate_widgets.hpp"

#include <fmt/format.h>

#include <imgui.h>

namespace app::ui::widgets {

void RewindBar(SharedContext &context) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport->WorkPos;
    const ImVec2 workSize = viewport->WorkSize;

    // TODO: make several parameters configurable
    // - padding from window borders
    // - colors
    // TODO: use ImGui style variables for certain options
    // - rounding
    // - background alpha
    // TODO: global alpha, to allow for fade-out effects
    // TODO: mouse interaction

    const float padding = 10.0f;

    const ImVec2 windowPos{
        (workPos.x + workSize.x) * 0.5f,
        (workPos.y + workSize.y - padding),
    };
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2{0.5f, 1.0f});
    ImGui::SetNextWindowSize(ImVec2(workSize.x - padding * 2.0f, 50.0f));
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("Rewind buffer bar", nullptr, windowFlags)) {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        const auto cap = context.rewindBuffer.GetBufferCapacity();
        const auto curr = context.rewindBuffer.GetBufferSize();
        const auto endOffset = context.rewindBuffer.GetTotalFrames();
        const auto startOffset = endOffset - curr;
        const float pct = (float)curr / cap;

        const auto startFrame = startOffset % 60;
        const auto startSecond = startOffset / 60 % 60;
        const auto startMinute = startOffset / 60 / 60 % 60;
        const auto startHour = startOffset / 60 / 60 / 60 % 60;
        const auto startStr = fmt::format("{:d}:{:02d}:{:02d}.{:02d}", startHour, startMinute, startSecond, startFrame);

        const auto endFrame = endOffset % 60;
        const auto endSecond = endOffset / 60 % 60;
        const auto endMinute = endOffset / 60 / 60 % 60;
        const auto endHour = endOffset / 60 / 60 / 60 % 60;
        const auto endStr = fmt::format("{:d}:{:02d}:{:02d}.{:02d}", endHour, endMinute, endSecond, endFrame);

        ImGui::PushFont(context.fonts.monospace.small.bold);
        const float endWidth = ImGui::CalcTextSize(endStr.c_str()).x;
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                if (x == 0 && y == 0) {
                    continue;
                }
                drawList->AddText(ImVec2(pos.x + x, pos.y + y), 0xBB000000, startStr.c_str());
                drawList->AddText(ImVec2(pos.x + x + avail.x * pct - endWidth, pos.y + y), 0xBB000000, endStr.c_str());
            }
        }

        drawList->AddText(ImVec2(pos.x, pos.y), 0xF0FFF8C0, startStr.c_str());
        drawList->AddText(ImVec2(pos.x + avail.x * pct - endWidth, pos.y), 0xF0FFF8C0, endStr.c_str());
        ImGui::PopFont();

        const ImVec2 rectTopLeft = ImVec2(pos.x, pos.y + lineHeight);

        // Background
        drawList->AddRectFilled(rectTopLeft, ImVec2(pos.x + avail.x, pos.y + avail.y), 0xAA211F15, 2.0f,
                                ImDrawFlags_RoundCornersAll);

        // Progress bar
        drawList->AddRectFilled(rectTopLeft, ImVec2(pos.x + avail.x * pct, pos.y + avail.y), 0xC0FF7322, 2.0f,
                                ImDrawFlags_RoundCornersAll);

        // Border
        drawList->AddRect(rectTopLeft, ImVec2(pos.x + avail.x, pos.y + avail.y), 0xE0FF9A35, 2.0f,
                          ImDrawFlags_RoundCornersAll, 2.0f);
    }
    ImGui::End();
}

} // namespace app::ui::widgets

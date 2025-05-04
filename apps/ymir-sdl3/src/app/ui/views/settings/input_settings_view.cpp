#include "input_settings_view.hpp"

#include <app/ui/widgets/common_widgets.hpp>
#include <app/ui/widgets/peripheral_widgets.hpp>

#include <app/input/input_utils.hpp>

using namespace ymir;

namespace app::ui {

InputSettingsView::InputSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void InputSettingsView::Display() {
    if (ImGui::BeginTable("periph_ports", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
            ImGui::SeparatorText("Port 1");
            ImGui::PopFont();

            MakeDirty(widgets::Port1PeripheralSelector(m_context));
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
            ImGui::SeparatorText("Port 2");
            ImGui::PopFont();

            MakeDirty(widgets::Port2PeripheralSelector(m_context));
        }
        ImGui::EndTable();
    }
    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Gamepads");
    ImGui::PopFont();

    auto &settings = m_context.settings.input;

    if (ImGui::BeginTable("input_settings", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("Analog to digital sensitivity");
            widgets::ExplanationTooltip("Affects how far analog inputs must be pushed to trigger buttons.",
                                        m_context.displayScale);
        }
        if (ImGui::TableNextColumn()) {
            float sens = settings.gamepad.analogToDigitalSensitivity * 100.0f;
            ImGui::SetNextItemWidth(-1.0f);
            if (MakeDirty(ImGui::SliderFloat("##analog_to_digital_sens", &sens, 5.f, 90.f, "%.2f%%",
                                             ImGuiSliderFlags_AlwaysClamp))) {
                settings.gamepad.analogToDigitalSensitivity = sens / 100.0f;
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("Left stick deadzone");
            widgets::ExplanationTooltip("Adjusts the deadzone for the left stick.\n"
                                        "The active range is mapped linearly from 0 to 1.",
                                        m_context.displayScale);
        }
        if (ImGui::TableNextColumn()) {
            float dz = settings.gamepad.lsDeadzone * 100.0f;
            ImGui::SetNextItemWidth(-1.0f);
            if (MakeDirty(
                    ImGui::SliderFloat("##ls_deadzones", &dz, 5.f, 90.f, "%.2f%%", ImGuiSliderFlags_AlwaysClamp))) {
                settings.gamepad.lsDeadzone = dz / 100.0f;
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("Right stick deadzone");
            widgets::ExplanationTooltip("Adjusts the deadzone for the right stick.\n"
                                        "The active range is mapped linearly from 0 to 1.",
                                        m_context.displayScale);
        }
        if (ImGui::TableNextColumn()) {
            float dz = settings.gamepad.rsDeadzone * 100.0f;
            ImGui::SetNextItemWidth(-1.0f);
            if (MakeDirty(
                    ImGui::SliderFloat("##rs_deadzones", &dz, 5.f, 90.f, "%.2f%%", ImGuiSliderFlags_AlwaysClamp))) {
                settings.gamepad.rsDeadzone = dz / 100.0f;
            }
        }

        ImGui::EndTable();
    }

    auto *drawList = ImGui::GetWindowDrawList();

    auto &inputContext = m_context.inputContext;

    for (uint32 id : inputContext.GetConnectedGamepads()) {
        auto [lsx, lsy] = inputContext.GetAxis2D(id, input::GamepadAxis2D::LeftStick);
        auto [rsx, rsy] = inputContext.GetAxis2D(id, input::GamepadAxis2D::RightStick);
        auto lt = inputContext.GetAxis1D(id, input::GamepadAxis1D::LeftTrigger);
        auto rt = inputContext.GetAxis1D(id, input::GamepadAxis1D::RightTrigger);

        auto drawStick = [&](const char *name, float x, float y, float dz) {
            static constexpr float kWidgetSize = 100.0f;
            static constexpr float kArrowSize = 6.0f;
            static constexpr float kCircleRadius = (kWidgetSize - kArrowSize) * 0.5f;
            static constexpr ImU32 kCircleBorderColor = 0xE0F5D4C6;
            static constexpr ImU32 kCircleBackgroundColor = 0xAA401A0A;
            static constexpr ImU32 kDeadzoneBackgroundColor = 0xC02F2A69;
            static constexpr ImU32 kOrthoLineColor = 0xAAA89992;
            static constexpr ImU32 kStickLineColor = 0xE0BAD1DB;
            static constexpr ImU32 kStickPushedPointColor = 0xF05FF58F;
            static constexpr ImU32 kStickAtRestPointColor = 0xF08F5FF5;
            static constexpr ImU32 kStickAdjustedPointColor = 0xF0F58F5F;
            static constexpr ImU32 kArrowColor = 0xF05FF58F;

            const float arrowSize = kArrowSize * m_context.displayScale;
            const float circleRadius = kCircleRadius * m_context.displayScale;

            const ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

            const ImVec2 boxSize{kWidgetSize * m_context.displayScale, kWidgetSize * m_context.displayScale};
            const float lineSpacing = ImGui::GetStyle().ItemSpacing.y;
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            auto pos = ImGui::GetCursorScreenPos();

            const float circleBorderThickness = 1.5f * m_context.displayScale;
            const float orthoLineThickness = 1.0f * m_context.displayScale;
            const float stickLineThickness = 1.2f * m_context.displayScale;
            const float stickPointRadius = 2.0f * m_context.displayScale;

            const float left = pos.x;
            const float top = pos.y;
            const float right = pos.x + boxSize.x;
            const float bottom = pos.y + boxSize.y;
            const ImVec2 center{pos.x + boxSize.x * 0.5f, pos.y + boxSize.y * 0.5f};

            const auto [xAdj, yAdj] = input::ApplyDeadzone(x, y, dz);
            const bool zero = xAdj == 0.0f && yAdj == 0.0f;

            ImGui::Dummy(ImVec2(boxSize.x, boxSize.y + lineSpacing + lineHeight * 3));

            // Circle background, deadzone, center cross, border
            drawList->AddCircleFilled(center, circleRadius, kCircleBackgroundColor);
            drawList->AddCircleFilled(center, dz * circleRadius, kDeadzoneBackgroundColor);
            drawList->AddLine(ImVec2(center.x, top + arrowSize), ImVec2(center.x, bottom - arrowSize), kOrthoLineColor,
                              orthoLineThickness);
            drawList->AddLine(ImVec2(left + arrowSize, center.y), ImVec2(right - arrowSize, center.y), kOrthoLineColor,
                              orthoLineThickness);
            drawList->AddCircle(center, circleRadius, kCircleBorderColor, 0, circleBorderThickness);

            // Stick line
            const ImVec2 stickPos{center.x + x * circleRadius, center.y + y * circleRadius};
            drawList->AddLine(center, stickPos, kStickLineColor, stickLineThickness);
            drawList->AddCircleFilled(stickPos, stickPointRadius,
                                      zero ? kStickAtRestPointColor : kStickPushedPointColor);

            if (!zero) {
                const ImVec2 adjustedStickPos{center.x + xAdj * circleRadius, center.y + yAdj * circleRadius};
                drawList->AddCircle(adjustedStickPos, stickPointRadius, kStickAdjustedPointColor);
            }

            auto drawText = [&](const char *text, float lineNum, ImU32 color) {
                const float textWidth = ImGui::CalcTextSize(text).x;
                drawList->AddText(ImVec2(center.x - textWidth * 0.5f, bottom + lineSpacing + lineHeight * lineNum),
                                  color, text);
            };

            // Label and values
            drawText(name, 0, textColor);
            drawText(fmt::format("{:.2f}x{:.2f}", x, y).c_str(), 1,
                     zero ? kStickAtRestPointColor : kStickPushedPointColor);
            drawText(fmt::format("{:.2f}x{:.2f}", xAdj, yAdj).c_str(), 2, kStickAdjustedPointColor);

            // D-Pad arrows
            const float distSq = xAdj * xAdj + yAdj * yAdj;
            const float sens = settings.gamepad.analogToDigitalSensitivity;
            if (distSq > 0.0f && distSq >= sens * sens) {
                static constexpr uint32 kPosX = 1u << 0u;
                static constexpr uint32 kNegX = 1u << 1u;
                static constexpr uint32 kPosY = 1u << 2u;
                static constexpr uint32 kNegY = 1u << 3u;
                const uint32 out = input::AnalogToDigital2DAxis(x, y, sens, kPosX, kNegX, kPosY, kNegY);

                float digX = (out & kPosX) ? +1.0f : (out & kNegX) ? -1.0f : 0.0f;
                float digY = (out & kPosY) ? +1.0f : (out & kNegY) ? -1.0f : 0.0f;
                const float dist = sqrt(digX * digX + digY * digY);
                digX /= dist;
                digY /= dist;

                const ImVec2 arrowBase{center.x + digX * circleRadius, center.y + digY * circleRadius};
                const ImVec2 arrowHead{center.x + digX * (circleRadius + arrowSize),
                                       center.y + digY * (circleRadius + arrowSize)};

                const ImVec2 triangle[3] = {
                    arrowHead,
                    {arrowBase.x - digY * arrowSize, arrowBase.y + digX * arrowSize},
                    {arrowBase.x + digY * arrowSize, arrowBase.y - digX * arrowSize},
                };

                drawList->AddConvexPolyFilled(triangle, std::size(triangle), kArrowColor);
            }
        };

        ImGui::PushFont(m_context.fonts.sansSerif.medium.bold);
        ImGui::Text("Gamepad %u", id + 1);
        ImGui::PopFont();
        ImGui::PushID(id);
        drawStick("Left Stick", lsx, lsy, settings.gamepad.lsDeadzone);
        ImGui::SameLine();
        drawStick("Right Stick", rsx, rsy, settings.gamepad.rsDeadzone);
        ImGui::PopID();
    }
}

} // namespace app::ui

#include "gui_settings_view.hpp"

#include <app/ui/widgets/common_widgets.hpp>

namespace app::ui {

GUISettingsView::GUISettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void GUISettingsView::Display() {
    auto &settings = m_context.settings.gui;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Behavior");
    ImGui::PopFont();

    MakeDirty(ImGui::Checkbox("Remember window geometry", &settings.rememberWindowGeometry));
    widgets::ExplanationTooltip(
        "When enabled, the current window position and size will be restored the next time the application is started.",
        m_context.displayScale);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("On-screen display");
    ImGui::PopFont();

    MakeDirty(ImGui::Checkbox("Show messages", &settings.showMessages));
    widgets::ExplanationTooltip(
        "When enabled, notification messages are displayed on the top-left corner of the window.",
        m_context.displayScale);

    MakeDirty(ImGui::Checkbox("Show frame rate OSD", &settings.showFrameRateOSD));
    ImGui::Indent();
    auto frameRateOSDOption = [&](const char *name, Settings::GUI::FrameRateOSDPosition value) {
        if (MakeDirty(ImGui::RadioButton(name, settings.frameRateOSDPosition == value))) {
            settings.frameRateOSDPosition = value;
        }
    };
    frameRateOSDOption("Top left##fps_osd", Settings::GUI::FrameRateOSDPosition::TopLeft);
    ImGui::SameLine();
    frameRateOSDOption("Top right##fps_osd", Settings::GUI::FrameRateOSDPosition::TopRight);
    ImGui::SameLine();
    frameRateOSDOption("Bottom left##fps_osd", Settings::GUI::FrameRateOSDPosition::BottomLeft);
    ImGui::SameLine();
    frameRateOSDOption("Bottom right##fps_osd", Settings::GUI::FrameRateOSDPosition::BottomRight);
    ImGui::Unindent();

    MakeDirty(ImGui::Checkbox("Show speed indicators for modified speeds", &settings.showSpeedIndicatorForAllSpeeds));
    widgets::ExplanationTooltip(
        "When enabled, the speed indicator will be displayed for any emulation speed other than 100%.\n"
        "When disabled, the speed indicator is only displayed while running in turbo speed.",
        m_context.displayScale);
}

} // namespace app::ui

#include "video_settings_view.hpp"

#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>
#include <app/ui/widgets/settings_widgets.hpp>

namespace app::ui {

VideoSettingsView::VideoSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void VideoSettingsView::Display() {
    auto &settings = m_context.settings.video;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Display");
    ImGui::PopFont();

    MakeDirty(ImGui::Checkbox("Force integer scaling", &settings.forceIntegerScaling));
    MakeDirty(ImGui::Checkbox("Force aspect ratio", &settings.forceAspectRatio));
    widgets::ExplanationTooltip("If disabled, forces square pixels.", m_context.displayScale);
    ImGui::SameLine();
    if (MakeDirty(ImGui::Button("4:3"))) {
        settings.forcedAspect = 4.0 / 3.0;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::Button("3:2"))) {
        settings.forcedAspect = 3.0 / 2.0;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::Button("16:10"))) {
        settings.forcedAspect = 16.0 / 10.0;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::Button("16:9"))) {
        settings.forcedAspect = 16.0 / 9.0;
    }
    // TODO: aspect ratio selector? slider?

    widgets::settings::video::DisplayRotation(m_context);

    ImGui::Separator();

    MakeDirty(ImGui::Checkbox("Auto-fit window to screen", &settings.autoResizeWindow));
    widgets::ExplanationTooltip(
        "If forced aspect ratio is disabled, adjusts and recenters the window whenever the display "
        "resolution changes.",
        m_context.displayScale);
    ImGui::SameLine();
    if (settings.displayVideoOutputInWindow) {
        ImGui::BeginDisabled();
    }
    if (MakeDirty(ImGui::Button("Fit now"))) {
        m_context.EnqueueEvent(events::gui::FitWindowToScreen());
    }
    if (settings.displayVideoOutputInWindow) {
        ImGui::EndDisabled();
    }

    if (MakeDirty(ImGui::Checkbox("Windowed video output", &settings.displayVideoOutputInWindow))) {
        m_context.EnqueueEvent(events::gui::FitWindowToScreen());
    }
    widgets::ExplanationTooltip("Moves the display into a dedicated window.\n"
                                "Can be helpful when used in conjunction with the debugger windows.",
                                m_context.displayScale);

    bool fullScreen = settings.fullScreen.Get();
    if (MakeDirty(ImGui::Checkbox("Full screen", &fullScreen))) {
        settings.fullScreen = fullScreen;
    }

    MakeDirty(ImGui::Checkbox("Double-click to toggle full screen", &settings.doubleClickToFullScreen));

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Enhancements");
    ImGui::PopFont();

    widgets::settings::video::Deinterlace(m_context);
    widgets::settings::video::TransparentMeshes(m_context);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    // TODO: renderer backend options

    widgets::settings::video::ThreadedVDP(m_context);
}

} // namespace app::ui

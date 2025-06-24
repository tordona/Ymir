#include "video_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>
#include <app/ui/widgets/settings_widgets.hpp>

namespace app::ui {

VideoSettingsView::VideoSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void VideoSettingsView::Display() {
    auto &settings = m_context.settings.video;
    auto &config = m_context.saturn.configuration.video;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
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
    if (MakeDirty(ImGui::Button("16:9"))) {
        settings.forcedAspect = 16.0 / 9.0;
    }
    // TODO: aspect ratio selector? slider?

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

    ImGui::Separator();

    // Round scale to steps of 25% and clamp to 100%-200% range
    bool overrideUIScale = settings.overrideUIScale;
    double uiScale = overrideUIScale ? settings.uiScale.Get() : m_context.displayScale;
    uiScale = std::round(uiScale / 0.25) * 0.25;
    uiScale = std::clamp(uiScale, 1.00, 2.00);

    if (MakeDirty(ImGui::Checkbox(fmt::format("Override UI scale (current: {:.0f}%)", uiScale * 100.0).c_str(),
                                  &overrideUIScale))) {
        settings.overrideUIScale = overrideUIScale;
        // Use current DPI setting when enabling the override
        if (overrideUIScale) {
            settings.uiScale = uiScale;
        }
    }

    ImGui::Indent();
    if (!overrideUIScale) {
        ImGui::BeginDisabled();
    }
    if (MakeDirty(ImGui::RadioButton("100%##ui_scale", uiScale == 1.0))) {
        settings.uiScale = 1.00;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("125%##ui_scale", uiScale == 1.25))) {
        settings.uiScale = 1.25;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("150%##ui_scale", uiScale == 1.50))) {
        settings.uiScale = 1.50;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("175%##ui_scale", uiScale == 1.75))) {
        settings.uiScale = 1.75;
    }
    ImGui::SameLine();
    if (MakeDirty(ImGui::RadioButton("200%##ui_scale", uiScale == 2.00))) {
        settings.uiScale = 2.00;
    }
    if (!overrideUIScale) {
        ImGui::EndDisabled();
    }
    ImGui::Unindent();

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Enhancements");
    ImGui::PopFont();

    widgets::settings::video::Deinterlace(m_context);
    widgets::settings::video::TransparentMeshes(m_context);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    // TODO: renderer backend options

    widgets::settings::video::ThreadedVDP(m_context);
}

} // namespace app::ui

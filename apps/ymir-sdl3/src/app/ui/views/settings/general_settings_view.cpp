#include "general_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>

namespace app::ui {

GeneralSettingsView::GeneralSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void GeneralSettingsView::Display() {
    auto &settings = m_context.settings.general;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Boost process priority", &settings.boostProcessPriority))) {
        m_context.EnqueueEvent(events::gui::SetProcessPriority(settings.boostProcessPriority));
    }
    widgets::ExplanationTooltip("Increases the process's priority level, which may help reduce stuttering.",
                                m_context.displayScale);

    if (MakeDirty(ImGui::Checkbox("Boost emulator thread priority", &settings.boostEmuThreadPriority))) {
        m_context.EnqueueEvent(events::emu::SetThreadPriority(settings.boostEmuThreadPriority));
    }
    widgets::ExplanationTooltip("Increases the emulator thread's priority, which may help reduce jitter.",
                                m_context.displayScale);

    MakeDirty(ImGui::Checkbox("Preload disc images to RAM", &settings.preloadDiscImagesToRAM));
    widgets::ExplanationTooltip(
        "Preloads the entire disc image to memory.\n"
        "May help reduce stuttering if you're loading images from a slow disk or from the network.",
        m_context.displayScale);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Rewind buffer");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Enable rewind buffer", &settings.enableRewindBuffer))) {
        m_context.EnqueueEvent(events::gui::EnableRewindBuffer(settings.enableRewindBuffer));
    }
    widgets::ExplanationTooltip("Allows you to step back in time.\n"
                                "Increases memory usage and slightly reduces performance.",
                                m_context.displayScale);

    // TODO: rewind buffer size

    if (MakeDirty(ImGui::SliderInt("Compression level", &settings.rewindCompressionLevel, 0, 16, "%d",
                                   ImGuiSliderFlags_AlwaysClamp))) {
        m_context.rewindBuffer.LZ4Accel = 1 << (16 - settings.rewindCompressionLevel);
    }

    widgets::ExplanationTooltip("Adjust compression ratio vs. speed.\n"
                                "Higher values improve compression ratio, reducing memory usage.\n"
                                "Lower values increase compression speed and improve emulation performance.\n",
                                m_context.displayScale);
}

} // namespace app::ui

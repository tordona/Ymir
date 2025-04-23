#include "audio_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>

using namespace ymir;

namespace app::ui {

AudioSettingsView::AudioSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void AudioSettingsView::Display() {
    auto &settings = m_context.settings.audio;
    auto &config = m_context.saturn.configuration.audio;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("General");
    ImGui::PopFont();

    static constexpr float kMinVolume = 0.0f;
    static constexpr float kMaxVolume = 100.0f;
    float volumePct = settings.volume * 100.0f;
    if (MakeDirty(ImGui::SliderScalar("Volume", ImGuiDataType_Float, &volumePct, &kMinVolume, &kMaxVolume, "%.1lf%%",
                                      ImGuiSliderFlags_AlwaysClamp))) {
        settings.volume = volumePct * 0.01f;
    }
    bool mute = settings.mute;
    if (MakeDirty(ImGui::Checkbox("Mute", &mute))) {
        settings.mute = mute;
    }

    // -----------------------------------------------------------------------------------------------------------------

    using InterpMode = config::audio::SampleInterpolationMode;

    auto interpOption = [&](const char *name, InterpMode mode) {
        const std::string label = fmt::format("{}##sample_interp", name);
        ImGui::SameLine();
        if (MakeDirty(ImGui::RadioButton(label.c_str(), config.interpolation == mode))) {
            config.interpolation = mode;
        }
    };

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Quality");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Interpolation:");
    widgets::ExplanationTooltip("- Nearest neighbor: Cheapest option with grittier sounds.\n"
                                "- Linear: Hardware accurate option with softer sounds. (default)");
    interpOption("Nearest neighbor", InterpMode::NearestNeighbor);
    interpOption("Linear", InterpMode::Linear);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    bool threadedSCSP = config.threadedSCSP;
    if (MakeDirty(ImGui::Checkbox("Threaded SCSP and sound CPU", &threadedSCSP))) {
        m_context.EnqueueEvent(events::emu::EnableThreadedSCSP(threadedSCSP));
    }
    widgets::ExplanationTooltip("NOTE: This feature is currently unimplemented.\n\n"
                                "Runs the SCSP and MC68EC000 in a dedicated thread.\n"
                                "Improves performance at the cost of accuracy.\n"
                                "A few select games may break when this option is enabled.");
}

} // namespace app::ui

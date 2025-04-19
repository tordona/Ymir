#include "cdblock_settings_view.hpp"

#include <app/ui/widgets/common_widgets.hpp>

using namespace ymir;

namespace app::ui {

CDBlockSettingsView::CDBlockSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void CDBlockSettingsView::Display() {
    auto &config = m_context.saturn.configuration.cdblock;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Tweaks");
    ImGui::PopFont();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Read speed");
    widgets::ExplanationTooltip("Changes the maximum read speed of the emulated CD drive.\n"
                                "The default value is 2x, matching the real Saturn's CD drive speed.\n"
                                "Higher speeds decrease load times but may reduce compatibility.");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    static constexpr uint8 kMinReadSpeed = 2u;
    static constexpr uint8 kMaxReadSpeed = 200u;
    uint8 readSpeed = config.readSpeedFactor;
    if (MakeDirty(ImGui::SliderScalar("##read_speed", ImGuiDataType_U8, &readSpeed, &kMinReadSpeed, &kMaxReadSpeed,
                                      "%ux", ImGuiSliderFlags_AlwaysClamp))) {
        config.readSpeedFactor = readSpeed;
    }
}

} // namespace app::ui

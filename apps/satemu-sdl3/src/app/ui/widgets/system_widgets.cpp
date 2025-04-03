#include "system_widgets.hpp"

#include <app/events/emu_event_factory.hpp>

#include <imgui.h>

using namespace satemu;

namespace app::ui::widgets {

void VideoStandardSelector(SharedContext &ctx) {
    sys::VideoStandard videoStandard = ctx.saturn.GetVideoStandard();
    if (ImGui::RadioButton("NTSC", videoStandard == sys::VideoStandard::NTSC)) {
        ctx.EnqueueEvent(events::emu::SetVideoStandard(sys::VideoStandard::NTSC));
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("PAL", videoStandard == sys::VideoStandard::PAL)) {
        ctx.EnqueueEvent(events::emu::SetVideoStandard(sys::VideoStandard::PAL));
    }
}

void RegionSelector(SharedContext &ctx) {
    static constexpr struct {
        uint8 charCode;
        const char *name;
    } kRegions[] = {
        {/*0x0*/ '?', "Invalid"},       {/*0x1*/ 'J', "Japan"},
        {/*0x2*/ 'T', "Asia NTSC"},     {/*0x3*/ '?', "Invalid"},
        {/*0x4*/ 'U', "North America"}, {/*0x5*/ 'B', "Central/South America NTSC"},
        {/*0x6*/ 'K', "Korea"},         {/*0x7*/ '?', "Invalid"},
        {/*0x8*/ '?', "Invalid"},       {/*0x9*/ '?', "Invalid"},
        {/*0xA*/ 'A', "Asia PAL"},      {/*0xB*/ '?', "Invalid"},
        {/*0xC*/ 'E', "Europe PAL"},    {/*0xD*/ 'L', "Central/South America PAL"},
        {/*0xE*/ '?', "Invalid"},       {/*0xF*/ '?', "Invalid"},
    };

    auto fmtRegion = [](uint8 index) {
        index &= 0xF;
        return fmt::format("({:c}) {}", kRegions[index].charCode, kRegions[index].name);
    };

    uint8 areaCode = ctx.saturn.SMPC.GetAreaCode();
    if (ImGui::BeginCombo("##region", fmtRegion(areaCode).c_str(),
                          ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_HeightLargest)) {
        for (uint8 i = 0; i <= 0xF; i++) {
            if (kRegions[i].charCode == '?') {
                continue;
            }
            if (ImGui::Selectable(fmtRegion(i).c_str(), i == areaCode) && i != areaCode) {
                ctx.EnqueueEvent(events::emu::SetAreaCode(i));
                // TODO: optional?
                ctx.EnqueueEvent(events::emu::HardReset());
            }
        }

        ImGui::EndCombo();
    }
}

} // namespace app::ui::widgets

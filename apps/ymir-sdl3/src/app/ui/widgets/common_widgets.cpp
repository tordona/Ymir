#include "common_widgets.hpp"

#include <imgui.h>

namespace app::ui::widgets {

void ExplanationTooltip(const char *explanation, bool sameLine) {
    if (sameLine) {
        ImGui::SameLine();
    }
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(450.0f);
        ImGui::TextUnformatted(explanation);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

} // namespace app::ui::widgets
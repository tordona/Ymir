#include "input_settings_view.hpp"

using namespace satemu;

namespace app::ui {

InputSettingsView::InputSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void InputSettingsView::Display() {
    // TODO: two-column table with port 1 on the left and port 2 on the right

    /*auto &settings = m_context.settings.input;

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Bindings");
    ImGui::PopFont();*/
}

} // namespace app::ui

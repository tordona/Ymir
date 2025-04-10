#include "input_settings_view.hpp"

#include <app/ui/widgets/peripheral_widgets.hpp>

using namespace satemu;

namespace app::ui {

InputSettingsView::InputSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void InputSettingsView::Display() {
    if (ImGui::BeginTable("periph_ports", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY)) {
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
}

} // namespace app::ui

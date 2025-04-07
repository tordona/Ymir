#include "input_settings_view.hpp"

using namespace satemu;

namespace app::ui {

InputSettingsView::InputSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void InputSettingsView::Display() {
    const float spacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float configureTextWidth = ImGui::CalcTextSize("Configure").x;

    auto periphSelector = [&](int portIndex) {
        auto &port =
            portIndex == 1 ? m_context.saturn.SMPC.GetPeripheralPort1() : m_context.saturn.SMPC.GetPeripheralPort2();
        auto &periph = port.GetPeripheral();

        if (ImGui::BeginTable(fmt::format("periph_table_{}", portIndex).c_str(), 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("##input", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Slot 1:");
            }
            if (ImGui::TableNextColumn()) {
                // TODO: support more peripheral types
                static constexpr const char *kPeriphNames[] = {"None", "Standard Saturn Pad"};

                // TODO: support 6 slots for multitap
                for (uint32 i = 0; i < 1; i++) {

                    ImGui::SetNextItemWidth(-(configureTextWidth + paddingWidth * 2 + spacingWidth));
                    if (ImGui::BeginCombo(fmt::format("##periph_{}_{}", portIndex, i).c_str(),
                                          kPeriphNames[static_cast<size_t>(periph.GetType())])) {
                        for (uint32 j = 0; j < std::size(kPeriphNames); j++) {
                            if (ImGui::Selectable(kPeriphNames[j], static_cast<uint32>(periph.GetType()) == j)) {
                                // TODO: replace with specified peripheral type
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(fmt::format("Configure##{}_{}", portIndex, i).c_str())) {
                        // TODO: open keybindings for this peripheral
                    }
                }
            }

            ImGui::EndTable();
        }
    };

    if (ImGui::BeginTable("periph_ports", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
            ImGui::SeparatorText("Port 1");
            ImGui::PopFont();

            std::unique_lock lock{m_context.locks.peripherals};
            periphSelector(1);
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
            ImGui::SeparatorText("Port 2");
            ImGui::PopFont();

            std::unique_lock lock{m_context.locks.peripherals};
            periphSelector(2);
        }
        ImGui::EndTable();
    }

    /*auto &settings = m_context.settings.input;

    ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
    ImGui::SeparatorText("Bindings");
    ImGui::PopFont();*/
}

} // namespace app::ui

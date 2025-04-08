#include "input_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>

using namespace satemu;

namespace app::ui {

InputSettingsView::InputSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void InputSettingsView::Display() {
    const float spacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float configureTextWidth = ImGui::CalcTextSize("Configure").x;

    auto periphSelector = [&](int portIndex) {
        std::unique_lock lock{m_context.locks.peripherals};

        const bool isPort1 = portIndex == 1;
        auto &port = isPort1 ? m_context.saturn.SMPC.GetPeripheralPort1() : m_context.saturn.SMPC.GetPeripheralPort2();
        auto &periph = port.GetPeripheral();
        const bool isNone = periph.GetType() == peripheral::PeripheralType::None;

        if (ImGui::BeginTable(fmt::format("periph_table_{}", portIndex).c_str(), 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("##input", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                // TODO: multitap -- ImGui::TextUnformatted("Slot 1:");
                ImGui::TextUnformatted("Peripheral:");
            }
            if (ImGui::TableNextColumn()) {
                for (uint32 i = 0; i < 1; i++) {
                    ImGui::SetNextItemWidth(-(configureTextWidth + paddingWidth * 2 + spacingWidth));
                    if (ImGui::BeginCombo(fmt::format("##periph_{}_{}", portIndex, i).c_str(),
                                          periph.GetName().data())) {
                        for (auto type : peripheral::kTypes) {
                            if (MakeDirty(ImGui::Selectable(peripheral::GetPeripheralName(type).data(),
                                                            periph.GetType() == type))) {
                                m_context.EnqueueEvent(isPort1 ? events::emu::InsertPort1Peripheral(type)
                                                               : events::emu::InsertPort2Peripheral(type));
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (isNone) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::Button(fmt::format("Configure##{}_{}", portIndex, i).c_str())) {
                        // TODO: open keybindings for this peripheral
                    }
                    if (isNone) {
                        ImGui::EndDisabled();
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

            periphSelector(1);
        }
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
            ImGui::SeparatorText("Port 2");
            ImGui::PopFont();

            periphSelector(2);
        }
        ImGui::EndTable();
    }
}

} // namespace app::ui

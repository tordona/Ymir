#include "hotkeys_settings_view.hpp"

#include <app/events/gui_event_factory.hpp>

namespace app::ui {

HotkeysSettingsView::HotkeysSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void HotkeysSettingsView::Display() {
    if (ImGui::BeginTable("hotkeys", 2 + kNumBindsPerInput,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        for (size_t i = 0; i < kNumBindsPerInput; i++) {
            ImGui::TableSetupColumn(fmt::format("Hotkey {}", i + 1).c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableHeadersRow();

        auto drawRow = [&](const char *type, const char *cmdName, InputEventArray &inputs) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(type);
            }
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(cmdName);
            }
            for (uint32 i = 0; i < kNumBindsPerInput; i++) {
                if (ImGui::TableNextColumn()) {
                    const std::string bind = input::ToHumanString(inputs[i]);
                    const std::string label = fmt::format("{}##bind_{}_{}_{}", bind, type, cmdName, i);
                    const float availWidth = ImGui::GetContentRegionAvail().x;
                    if (ImGui::Button(label.c_str(), ImVec2(availWidth, 0))) {
                        // TODO: engage input binding system -> open popup
                        // - every keyboard key except modifier keys (ctrl, alt, shift and windows/command) are
                        // considered "terminal" keys, and so are all mouse and gamepad buttons
                        // - when any terminal key is pressed (SDL_EVENT_*_DOWN), save it along with all pressed
                        // modifier keys
                        //   - modifier keys apply to keyboard keys or mouse buttons, not to gamepad buttons
                        // - when a non-terminal (modifier) key is released (SDL_EVENT_*_UP), assign it and all pressed
                        // modifiers
                        // - ESC cancels the binding system (the key cannot/should not be mapped)

                        // TODO: rebind inputs
                        // TODO: consider only rebinding this action
                        // m_context.EnqueueEvent(events::gui::RebindInputs());
                    }
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        // TODO: erase binding
                        fmt::println("#### {} {} {}", type, cmdName, i);
                    }
                }
            }
        };

        auto &hotkeys = m_context.settings.hotkeys;

        drawRow("General", "Open settings", hotkeys.openSettings);
        drawRow("General", "Toggle windowed video output", hotkeys.toggleWindowedVideoOutput);

        drawRow("CD drive", "Load disc", hotkeys.loadDisc);
        drawRow("CD drive", "Eject disc", hotkeys.ejectDisc);
        drawRow("CD drive", "Open/close tray", hotkeys.openCloseTray);

        drawRow("System", "Hard reset", hotkeys.hardReset);
        drawRow("System", "Soft reset", hotkeys.softReset);
        drawRow("System", "Reset button", hotkeys.resetButton);

        drawRow("Emulation", "Pause/resume", hotkeys.pauseResume);
        drawRow("Emulation", "Frame step", hotkeys.frameStep);
        drawRow("Emulation", "Fast forward", hotkeys.fastForward);

        drawRow("Debugger", "Toggle tracing", hotkeys.toggleDebugTrace);
        drawRow("Debugger", "Dump all memory", hotkeys.dumpMemory);

        ImGui::EndTable();
    }
}

} // namespace app::ui

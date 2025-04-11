#include "hotkeys_settings_view.hpp"

#include <app/events/gui_event_factory.hpp>

namespace app::ui {

HotkeysSettingsView::HotkeysSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void HotkeysSettingsView::Display() {
    ImGui::TextUnformatted("Left-click a button to assign a hotkey. Right-click to clear.");
    if (ImGui::BeginTable("hotkeys", 2 + kNumBindsPerInput,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        for (size_t i = 0; i < kNumBindsPerInput; i++) {
            ImGui::TableSetupColumn(fmt::format("Hotkey {}", i + 1).c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableHeadersRow();

        auto drawRow = [&](const char *type, const char *cmdName, InputBind &bind) {
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
                    const std::string bindStr = input::ToHumanString(bind.events[i]);
                    const std::string label = fmt::format("{}##bind_{}_{}_{}", bindStr, type, cmdName, i);
                    const float availWidth = ImGui::GetContentRegionAvail().x;

                    // Left-click engages binding mode
                    if (ImGui::Button(label.c_str(), ImVec2(availWidth, 0))) {
                        ImGui::OpenPopup("input_capture");
                        m_context.inputCapturer.Capture([=, this, &bind](const input::InputEvent &event) {
                            bind.events[i] = event;
                            MakeDirty();
                            // TODO: rebind only the modified action
                            m_context.EnqueueEvent(events::gui::RebindInputs());
                            m_captured = true;
                        });
                    }

                    // Right-click erases a binding
                    if (MakeDirty(ImGui::IsItemClicked(ImGuiMouseButton_Right))) {
                        m_context.inputCapturer.CancelCapture();
                        bind.events[i] = {};
                        // TODO: rebind only the modified action
                        m_context.EnqueueEvent(events::gui::RebindInputs());
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

        if (ImGui::BeginPopup("input_capture")) {
            if (m_captured) {
                m_captured = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::TextUnformatted("Press any key, mouse button or gamepad button to map it.");
            ImGui::TextUnformatted("Press Escape or click outside of this popup to cancel.");
            ImGui::EndPopup();
        } else {
            m_context.inputCapturer.CancelCapture();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

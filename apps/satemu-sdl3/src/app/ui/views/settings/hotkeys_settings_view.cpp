#include "hotkeys_settings_view.hpp"

#include <app/events/gui_event_factory.hpp>

namespace app::ui {

HotkeysSettingsView::HotkeysSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void HotkeysSettingsView::Display() {
    if (ImGui::Button("Restore defaults")) {
        m_context.settings.ResetHotkeys();
        m_context.settings.RebindInputs();
        MakeDirty();
    }

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

                    // Left-click engages bind mode
                    if (ImGui::Button(label.c_str(), ImVec2(availWidth, 0))) {
                        ImGui::OpenPopup("input_capture");
                        m_context.inputCapturer.Capture([=, this, &bind](const input::InputEvent &event) {
                            bind.events[i] = event;
                            MakeDirty();
                            m_context.EnqueueEvent(events::gui::RebindAction(bind.action));
                            m_captured = true;
                        });
                    }

                    // Right-click erases a bind
                    if (MakeDirty(ImGui::IsItemClicked(ImGuiMouseButton_Right))) {
                        m_context.inputCapturer.CancelCapture();
                        bind.events[i] = {};
                        m_context.EnqueueEvent(events::gui::RebindAction(bind.action));
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

        drawRow("Emulation", "Turbo speed", hotkeys.turboSpeed);
        drawRow("Emulation", "Pause/resume", hotkeys.pauseResume);
        drawRow("Emulation", "Forward frame step", hotkeys.fwdFrameStep);
        drawRow("Emulation", "Reverse frame step", hotkeys.revFrameStep);
        drawRow("Emulation", "Toggle rewind buffer", hotkeys.toggleRewindBuffer);
        drawRow("Emulation", "Rewind", hotkeys.rewind);

        drawRow("Debugger", "Toggle tracing", hotkeys.toggleDebugTrace);
        drawRow("Debugger", "Dump all memory", hotkeys.dumpMemory);

        drawRow("Save states", "Quick load state", hotkeys.saveStates.quickLoad);
        drawRow("Save states", "Quick save state", hotkeys.saveStates.quickSave);

        drawRow("Save states", "Select state 1", hotkeys.saveStates.select1);
        drawRow("Save states", "Select state 2", hotkeys.saveStates.select2);
        drawRow("Save states", "Select state 3", hotkeys.saveStates.select3);
        drawRow("Save states", "Select state 4", hotkeys.saveStates.select4);
        drawRow("Save states", "Select state 5", hotkeys.saveStates.select5);
        drawRow("Save states", "Select state 6", hotkeys.saveStates.select6);
        drawRow("Save states", "Select state 7", hotkeys.saveStates.select7);
        drawRow("Save states", "Select state 8", hotkeys.saveStates.select8);
        drawRow("Save states", "Select state 9", hotkeys.saveStates.select9);
        drawRow("Save states", "Select state 10", hotkeys.saveStates.select10);

        drawRow("Save states", "Load state 1", hotkeys.saveStates.load1);
        drawRow("Save states", "Load state 2", hotkeys.saveStates.load2);
        drawRow("Save states", "Load state 3", hotkeys.saveStates.load3);
        drawRow("Save states", "Load state 4", hotkeys.saveStates.load4);
        drawRow("Save states", "Load state 5", hotkeys.saveStates.load5);
        drawRow("Save states", "Load state 6", hotkeys.saveStates.load6);
        drawRow("Save states", "Load state 7", hotkeys.saveStates.load7);
        drawRow("Save states", "Load state 8", hotkeys.saveStates.load8);
        drawRow("Save states", "Load state 9", hotkeys.saveStates.load9);
        drawRow("Save states", "Load state 10", hotkeys.saveStates.load10);

        drawRow("Save states", "Save state 1", hotkeys.saveStates.save1);
        drawRow("Save states", "Save state 2", hotkeys.saveStates.save2);
        drawRow("Save states", "Save state 3", hotkeys.saveStates.save3);
        drawRow("Save states", "Save state 4", hotkeys.saveStates.save4);
        drawRow("Save states", "Save state 5", hotkeys.saveStates.save5);
        drawRow("Save states", "Save state 6", hotkeys.saveStates.save6);
        drawRow("Save states", "Save state 7", hotkeys.saveStates.save7);
        drawRow("Save states", "Save state 8", hotkeys.saveStates.save8);
        drawRow("Save states", "Save state 9", hotkeys.saveStates.save9);
        drawRow("Save states", "Save state 10", hotkeys.saveStates.save10);

        if (ImGui::BeginPopup("input_capture")) {
            if (m_captured) {
                m_captured = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::TextUnformatted("Press any key, mouse button or gamepad button to map it.\n\n"
                                   "Press Escape or click outside of this popup to cancel.");
            ImGui::EndPopup();
        } else {
            m_context.inputCapturer.CancelCapture();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

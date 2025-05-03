#include "standard_pad_binds_view.hpp"

#include <app/events/gui_event_factory.hpp>

namespace app::ui {

StandardPadBindsView::StandardPadBindsView(SharedContext &context)
    : SettingsViewBase(context) {}

void StandardPadBindsView::Display(Settings::Input::Port::StandardPadBinds &binds) {
    if (ImGui::Button("Restore defaults")) {
        m_context.settings.ResetBinds(binds);
        MakeDirty();
    }

    ImGui::TextUnformatted("Left-click a button to assign a hotkey. Right-click to clear.");
    if (ImGui::BeginTable("hotkeys", 1 + kNumBindsPerInput,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        for (size_t i = 0; i < kNumBindsPerInput; i++) {
            ImGui::TableSetupColumn(fmt::format("Hotkey {}", i + 1).c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableHeadersRow();

        auto drawRow = [&](const char *btnName, InputBind &bind) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(btnName);
            }
            for (uint32 i = 0; i < kNumBindsPerInput; i++) {
                if (ImGui::TableNextColumn()) {
                    const std::string bindStr = input::ToHumanString(bind.elements[i]);
                    const std::string label = fmt::format("{}##bind_{}_{}", bindStr, btnName, i);
                    const float availWidth = ImGui::GetContentRegionAvail().x;

                    // Left-click engages bind mode
                    if (ImGui::Button(label.c_str(), ImVec2(availWidth, 0))) {
                        ImGui::OpenPopup("input_capture");
                        m_context.inputContext.Capture([=, this, &bind](const input::InputElement &element) {
                            if (!element.IsButton()) {
                                return false;
                            }
                            bind.elements[i] = element;
                            MakeDirty();
                            m_context.EnqueueEvent(events::gui::RebindAction(bind.action));
                            m_captured = true;
                            return true;
                        });
                    }

                    // Right-click erases a bind
                    if (MakeDirty(ImGui::IsItemClicked(ImGuiMouseButton_Right))) {
                        m_context.inputContext.CancelCapture();
                        bind.elements[i] = {};
                        m_context.EnqueueEvent(events::gui::RebindAction(bind.action));
                    }
                }
            }
        };

        drawRow("A", binds.a);
        drawRow("B", binds.b);
        drawRow("C", binds.c);
        drawRow("X", binds.x);
        drawRow("Y", binds.y);
        drawRow("Z", binds.z);
        drawRow("L", binds.l);
        drawRow("R", binds.r);
        drawRow("Start", binds.start);
        drawRow("Up", binds.up);
        drawRow("Left", binds.left);
        drawRow("Down", binds.down);
        drawRow("Right", binds.right);

        if (ImGui::BeginPopup("input_capture")) {
            if (m_captured) {
                m_captured = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::TextUnformatted("Press any key, mouse button or gamepad button to map it.\n\n"
                                   "Press Escape or click outside of this popup to cancel.");
            ImGui::EndPopup();
        } else {
            m_context.inputContext.CancelCapture();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

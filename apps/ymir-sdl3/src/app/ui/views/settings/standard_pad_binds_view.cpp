#include "standard_pad_binds_view.hpp"

#include <app/events/gui_event_factory.hpp>

namespace app::ui {

StandardPadBindsView::StandardPadBindsView(SharedContext &context)
    : SettingsViewBase(context)
    , m_inputCaptureWidget(context) {}

void StandardPadBindsView::Display(Settings::Input::Port::StandardPadBinds &binds) {
    if (ImGui::Button("Restore defaults")) {
        m_context.settings.ResetBinds(binds);
        MakeDirty();
    }

    ImGui::TextUnformatted("Left-click a button to assign a hotkey. Right-click to clear.");
    if (ImGui::BeginTable("hotkeys", 1 + input::kNumBindsPerInput,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        for (size_t i = 0; i < input::kNumBindsPerInput; i++) {
            ImGui::TableSetupColumn(fmt::format("Hotkey {}", i + 1).c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableHeadersRow();

        auto drawRow = [&](const char *btnName, input::InputBind &bind) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(btnName);
            }
            for (uint32 i = 0; i < input::kNumBindsPerInput; i++) {
                if (ImGui::TableNextColumn()) {
                    m_inputCaptureWidget.DrawInputBindButton(bind, i);
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
        drawRow("D-Pad", binds.dpad);

        m_inputCaptureWidget.DrawCapturePopup();

        ImGui::EndTable();
    }
}

} // namespace app::ui

#include "analog_pad_binds_view.hpp"

namespace app::ui {

AnalogPadBindsView::AnalogPadBindsView(SharedContext &context)
    : SettingsViewBase(context)
    , m_inputCaptureWidget(context, m_unboundActionsWidget)
    , m_unboundActionsWidget(context) {}

void AnalogPadBindsView::Display(Settings::Input::Port::AnalogPadBinds &binds, uint32 portIndex) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Mode:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Analog", m_context.analogPadInputs[portIndex].analogMode)) {
        m_context.analogPadInputs[portIndex].analogMode = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Digital", !m_context.analogPadInputs[portIndex].analogMode)) {
        m_context.analogPadInputs[portIndex].analogMode = true;
    }

    if (ImGui::Button("Restore defaults")) {
        m_unboundActionsWidget.Capture(m_context.settings.ResetBinds(binds));
        MakeDirty();
    }

    ImGui::TextUnformatted("Left-click a button to assign a hotkey. Right-click to clear.");
    m_unboundActionsWidget.Display();
    if (ImGui::BeginTable("hotkeys", 1 + input::kNumBindsPerInput,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, 85.0f * m_context.displayScale);
        for (size_t i = 0; i < input::kNumBindsPerInput; i++) {
            ImGui::TableSetupColumn(fmt::format("Hotkey {}", i + 1).c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableHeadersRow();

        auto drawRow = [&](input::InputBind &bind) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(bind.action.name);
            }
            for (uint32 i = 0; i < input::kNumBindsPerInput; i++) {
                if (ImGui::TableNextColumn()) {
                    m_inputCaptureWidget.DrawInputBindButton(bind, i, &m_context.analogPadInputs[portIndex]);
                }
            }
        };

        drawRow(binds.a);
        drawRow(binds.b);
        drawRow(binds.c);
        drawRow(binds.x);
        drawRow(binds.y);
        drawRow(binds.z);
        drawRow(binds.l);
        drawRow(binds.r);
        drawRow(binds.start);
        drawRow(binds.up);
        drawRow(binds.left);
        drawRow(binds.down);
        drawRow(binds.right);
        drawRow(binds.dpad);
        drawRow(binds.analogStick);
        drawRow(binds.analogL);
        drawRow(binds.analogR);
        drawRow(binds.switchMode);

        m_inputCaptureWidget.DrawCapturePopup();

        ImGui::EndTable();
    }
}

} // namespace app::ui

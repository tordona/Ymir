#include "scu_dsp_disassembly_view.hpp"

namespace app::ui {

SCUDSPDisassemblyView::SCUDSPDisassemblyView(SharedContext &context)
    : m_context(context)
    , m_dsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPDisassemblyView::Display() {
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::BeginGroup();

    if (ImGui::BeginTable("dsp_disasm", 3,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed, paddingWidth * 2 + hexCharWidth * 2);
        ImGui::TableSetupColumn("Opcode", ImGuiTableColumnFlags_WidthFixed, paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Instructions", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        for (uint32 pc = 0; pc <= 0xFF; pc++) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%02X", pc);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%08X", m_dsp.programRAM[pc]);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);

                ImGui::PopFont();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
}

} // namespace app::ui

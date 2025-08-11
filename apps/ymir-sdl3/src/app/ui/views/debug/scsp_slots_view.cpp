#include "scsp_slots_view.hpp"

#include <ymir/hw/scsp/scsp.hpp>

using namespace ymir;

namespace app::ui {

SCSPSlotsView::SCSPSlotsView(SharedContext &context)
    : m_context(context)
    , m_scsp(context.saturn.GetSCSP()) {}

void SCSPSlotsView::Display() {
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    auto &probe = m_scsp.GetProbe();
    const auto &slots = probe.GetSlots();

    ImGui::BeginGroup();

    if (ImGui::BeginTable("slots", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2 + paddingWidth * 2);
        ImGui::TableSetupColumn("SA", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 5 + paddingWidth * 2);
        ImGui::TableSetupColumn("LSA", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 4 + paddingWidth * 2);
        ImGui::TableSetupColumn("LEA", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 4 + paddingWidth * 2);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        for (uint32 i = 0; i < 32; ++i) {
            const auto &slot = slots[i];

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%02d", i);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%05X", slot.startAddress);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%04X", slot.loopStartAddress);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%04X", slot.loopEndAddress);
                ImGui::PopFont();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
}

} // namespace app::ui

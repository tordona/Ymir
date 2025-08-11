#include "scsp_slots_view.hpp"

#include <ymir/hw/scsp/scsp.hpp>

#include <app/ui/fonts/IconsMaterialSymbols.h>

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
    const float msCharWidth = ImGui::CalcTextSize(ICON_MS_KEYBOARD_TAB).x;

    auto &probe = m_scsp.GetProbe();
    const auto &slots = probe.GetSlots();

    ImGui::BeginGroup();

    if (ImGui::BeginTable("slots", 15, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2);
        ImGui::TableSetupColumn("KYONB", ImGuiTableColumnFlags_WidthFixed, msCharWidth);
        ImGui::TableSetupColumn("SA", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 5);
        ImGui::TableSetupColumn("LSA", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 4);
        ImGui::TableSetupColumn("LEA", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 4);
        ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, msCharWidth);
        ImGui::TableSetupColumn("Bits", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2);
        ImGui::TableSetupColumn("SBCTL", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 4);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 4 + paddingWidth);
        ImGui::TableSetupColumn("AR", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2);
        ImGui::TableSetupColumn("D1R", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2);
        ImGui::TableSetupColumn("D2R", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2);
        ImGui::TableSetupColumn("RR", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2);
        ImGui::TableSetupColumn("DL", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 2);
        ImGui::TableSetupColumn("KRS", ImGuiTableColumnFlags_WidthFixed, hexCharWidth * 1);
        // TODO: EGHOLD
        // TODO: LPSLNK
        // TODO: EGBYPASS + padding
        // TODO: MDL
        // TODO: MDXSL
        // TODO: MDYSL
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        for (uint32 i = 0; i < 32; ++i) {
            const auto &slot = slots[i];

            const bool disabled = (slot.egState == scsp::Slot::EGState::Release && slot.GetEGLevel() >= 0x3C0) ||
                                  (!slot.active && slot.soundSource == scsp::Slot::SoundSource::SoundRAM);

            if (disabled) {
                ImGui::BeginDisabled();
            }

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%02d", i);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                if (slot.keyOnBit) {
                    ImGui::TextUnformatted(ICON_MS_PLAY_ARROW);
                }
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
            if (ImGui::TableNextColumn()) {
                using enum scsp::Slot::LoopControl;
                switch (slot.loopControl) {
                case Off:
                    ImGui::TextUnformatted(ICON_MS_KEYBOARD_TAB);
                    ImGui::SetItemTooltip("No loop");
                    break;
                case Normal:
                    ImGui::TextUnformatted(ICON_MS_ARROW_RIGHT_ALT);
                    ImGui::SetItemTooltip("Forward");
                    break;
                case Reverse:
                    ImGui::TextUnformatted(ICON_MS_ARROW_LEFT_ALT);
                    ImGui::SetItemTooltip("Reverse");
                    break;
                case Alternate:
                    ImGui::TextUnformatted(ICON_MS_SWAP_HORIZ);
                    ImGui::SetItemTooltip("Alternate");
                    break;
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::TextUnformatted(slot.pcm8Bit ? " 8" : "16");
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%04X", slot.sampleXOR);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                using enum scsp::Slot::SoundSource;
                const char *soundSourceText = "????";
                const char *soundSourceHint = "Unknown";
                switch (slot.soundSource) {
                case SoundRAM:
                    soundSourceText = "SRAM";
                    soundSourceHint = "Sound RAM";
                    break;
                case Noise:
                    soundSourceText = "LFSR";
                    soundSourceHint = "Noise";
                    break;
                case Silence:
                    soundSourceText = "ZERO";
                    soundSourceHint = "Silence";
                    break;
                case Unknown:
                    soundSourceText = "????";
                    soundSourceHint = "Unknown";
                    break;
                }

                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::TextUnformatted(soundSourceText);
                ImGui::PopFont();
                ImGui::SetItemTooltip("%s", soundSourceHint);
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%02X", slot.attackRate);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%02X", slot.decay1Rate);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%02X", slot.decay2Rate);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%02X", slot.releaseRate);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%02X", slot.decayLevel);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.medium);
                ImGui::Text("%X", slot.keyRateScaling);
                ImGui::PopFont();
            }

            if (disabled) {
                ImGui::EndDisabled();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
}

} // namespace app::ui

#include "sh2_wdt_view.hpp"

using namespace satemu;

namespace app::ui {

SH2WatchdogTimerView::SH2WatchdogTimerView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2WatchdogTimerView::Display() {
    auto &probe = m_sh2.GetProbe();
    auto &wdt = probe.WDT();

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText("Watchdog timer");

    if (ImGui::BeginTable("regs", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            uint8 WTCSR = wdt.ReadWTCSR();

            ImGui::BeginGroup();
            ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            if (ImGui::InputScalar("##wtcsr", ImGuiDataType_U8, &WTCSR, nullptr, nullptr, "%02X",
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
                wdt.WriteWTCSR<true>(WTCSR);
            }
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("WTCSR");
            ImGui::EndGroup();
            ImGui::SetItemTooltip("Watchdog Timer Control/Status Register");
        }

        if (ImGui::TableNextColumn()) {
            ImGui::Checkbox("OVF##wtcsr", &wdt.WTCSR.OVF);
            ImGui::SetItemTooltip("Overflow Flag");

            ImGui::SameLine();
            ImGui::Checkbox("WT/!IT##wtcsr", &wdt.WTCSR.WT_nIT);
            ImGui::SetItemTooltip("Timer Mode Select");

            ImGui::SameLine();
            ImGui::Checkbox("TME##wtcsr", &wdt.WTCSR.TME);
            ImGui::SetItemTooltip("Timer Enable");

            static constexpr const char *kCKSValues[] = {"Phi/2",   "Phi/64",   "Phi/128",  "Phi/256",
                                                         "Phi/512", "Phi/1024", "Phi/4096", "Phi/8192"};

            ImGui::SameLine();
            if (ImGui::BeginCombo("##wtcsr_cksn", kCKSValues[wdt.WTCSR.CKSn], ImGuiComboFlags_WidthFitPreview)) {
                for (int i = 0; i < std::size(kCKSValues); i++) {
                    bool selected = i == wdt.WTCSR.CKSn;
                    if (ImGui::Selectable(kCKSValues[i], &selected)) {
                        wdt.WTCSR.CKSn = i;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SetItemTooltip("Overflow Interrupt Enable");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            uint8 RSTCSR = wdt.ReadRSTCSR();

            ImGui::BeginGroup();
            ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            if (ImGui::InputScalar("##rstcsr", ImGuiDataType_U8, &RSTCSR, nullptr, nullptr, "%02X",
                                   ImGuiInputTextFlags_CharsHexadecimal)) {
                wdt.WriteRSTCSR<true>(RSTCSR);
            }
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("RSTCSR");
            ImGui::EndGroup();
            ImGui::SetItemTooltip("Reset Control/Status Register");
        }

        if (ImGui::TableNextColumn()) {
            ImGui::Checkbox("WOVF##rstcsr", &wdt.RSTCSR.WOVF);
            ImGui::SetItemTooltip("Watchdog Timer Overflow Flag");

            ImGui::SameLine();
            ImGui::Checkbox("RSTE##rstcsr", &wdt.RSTCSR.RSTE);
            ImGui::SetItemTooltip("Reset Enable");

            ImGui::SameLine();
            ImGui::Checkbox("RSTS##rstcsr", &wdt.RSTCSR.RSTS);
            ImGui::SetItemTooltip("Reset Select");
        }

        ImGui::EndTable();
    }

    {
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        ImGui::InputScalar("##wtcnt", ImGuiDataType_U8, &wdt.WTCNT, nullptr, nullptr, "%02X",
                           ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("WTCNT");
        ImGui::EndGroup();
        ImGui::SetItemTooltip("Watchdog Timer Counter");
    }
}

} // namespace app::ui

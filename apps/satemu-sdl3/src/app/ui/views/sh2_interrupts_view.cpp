#include "sh2_interrupts_view.hpp"

#include <imgui.h>

namespace app {

SH2InterruptsView::SH2InterruptsView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2InterruptsView::Display() {
    using namespace satemu;

    auto &probe = m_sh2.GetProbe();
    auto &intc = probe.INTC();

    ImGui::PushFont(m_context.fonts.monospaceMedium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::BeginGroup();

    // --- INTC ----------------------------------------------------------------
    {
        ImGui::SeparatorText("INTC");
        ImGui::PushFont(m_context.fonts.monospaceMedium);
        ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 4);
        uint16 ICR = intc.ICR.Read();
        if (ImGui::InputScalar("ICR", ImGuiDataType_U16, &ICR, nullptr, nullptr, "%04X")) {
            intc.ICR.Write<true, true, true>(ICR);
        }
        ImGui::PopFont();

        ImGui::SameLine();

        ImGui::Checkbox("NMIL", &intc.ICR.NMIL);
        ImGui::SameLine();
        ImGui::Checkbox("NMIE", &intc.ICR.NMIE);
        ImGui::SameLine();
        ImGui::Checkbox("VECMD", &intc.ICR.VECMD);
    }

    // --- Interrupt signals ---------------------------------------------------
    {
        ImGui::SeparatorText("Interrupt signals");

        auto drawSignal = [&](sh2::InterruptSource source, const char *name) {
            bool state = probe.IsInterruptRaised(source);
            if (ImGui::Checkbox(name, &state)) {
                if (state) {
                    probe.RaiseInterrupt(source);
                } else {
                    probe.LowerInterrupt(source);
                }
            }
        };

        if (ImGui::BeginTable("intr_signals", 3, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Signal");
            ImGui::TableSetupColumn("Vector");
            ImGui::TableSetupColumn("Level");
            ImGui::TableHeadersRow();

            auto drawRow = [&](sh2::InterruptSource source, const char *name, bool editable) {
                ImGui::TableNextRow();

                if (ImGui::TableNextColumn()) {
                    bool state = probe.IsInterruptRaised(source);
                    if (ImGui::Checkbox(name, &state)) {
                        if (state) {
                            probe.RaiseInterrupt(source);
                        } else {
                            probe.LowerInterrupt(source);
                        }
                    }
                }
                if (ImGui::TableNextColumn()) {
                    if (!editable) {
                        ImGui::BeginDisabled();
                    }
                    uint8 vector = intc.GetVector(source);
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
                    if (ImGui::InputScalar(fmt::format("##{}_vector", name).c_str(), ImGuiDataType_U8, &vector, nullptr,
                                           nullptr, "%02X")) {
                        intc.SetVector(source, vector);
                    }
                    ImGui::PopFont();
                    if (!editable) {
                        ImGui::EndDisabled();
                    }
                }
                if (ImGui::TableNextColumn()) {
                    if (!editable) {
                        ImGui::BeginDisabled();
                    }
                    uint8 level = intc.GetLevel(source);
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
                    if (ImGui::InputScalar(fmt::format("##{}_level", name).c_str(), ImGuiDataType_U8, &level, nullptr,
                                           nullptr, "%X")) {
                        intc.SetLevel(source, std::min<uint8>(level, 0xF));
                    }
                    ImGui::PopFont();
                    if (!editable) {
                        ImGui::EndDisabled();
                    }
                }
            };

            drawRow(sh2::InterruptSource::NMI, "NMI", false);
            drawRow(sh2::InterruptSource::UserBreak, "UBC BRK", true);
            drawRow(sh2::InterruptSource::IRL, "IRL", true);
            drawRow(sh2::InterruptSource::DIVU_OVFI, "DIVU OVFI", true);
            drawRow(sh2::InterruptSource::DMAC0_XferEnd, "DMAC0 TE", true);
            drawRow(sh2::InterruptSource::DMAC1_XferEnd, "DMAC1 TE", true);
            drawRow(sh2::InterruptSource::WDT_ITI, "WDT ITI", true);
            drawRow(sh2::InterruptSource::BSC_REF_CMI, "BSC REF CMI", true);
            drawRow(sh2::InterruptSource::SCI_ERI, "SCI ERI", true);
            drawRow(sh2::InterruptSource::SCI_RXI, "SCI RXI", true);
            drawRow(sh2::InterruptSource::SCI_TXI, "SCI TXI", true);
            drawRow(sh2::InterruptSource::SCI_TEI, "SCI TEI", true);
            drawRow(sh2::InterruptSource::FRT_ICI, "FRT ICI", true);
            drawRow(sh2::InterruptSource::FRT_OCI, "FRT OCI", true);
            drawRow(sh2::InterruptSource::FRT_OVI, "FRT OVI", true);

            // TODO: display and sync linked levels:
            // - DMAC0 + DMAC1
            // - WDT + BSC
            // - all SCI
            // - all FRT

            ImGui::EndTable();
        }
    }

    ImGui::EndGroup();
}

} // namespace app

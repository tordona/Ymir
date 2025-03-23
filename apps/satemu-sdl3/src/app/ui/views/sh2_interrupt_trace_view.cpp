#include "sh2_interrupt_trace_view.hpp"

namespace app {

SH2InterruptTraceView::SH2InterruptTraceView(SharedContext &context, satemu::sh2::SH2 &sh2, SH2Tracer &tracer)
    : m_context(context)
    , m_sh2(sh2)
    , m_tracer(tracer) {}

void SH2InterruptTraceView::Display() {
    using namespace satemu;

    auto &probe = m_sh2.GetProbe();
    auto &intc = probe.INTC();

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospaceMedium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::BeginGroup();

    // --- Trace ---------------------------------------------------------------
    {
        ImGui::SeparatorText("Trace");

        ImGui::Checkbox("Enable", &m_tracer.traceInterrupts);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted("You must also enable tracing in Debug > Enable tracing (F11)");
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##trace")) {
            m_tracer.interrupts.Clear();
        }

        if (ImGui::BeginTable("intr_trace", 5,
                              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_PreferSortDescending);
            ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                    paddingWidth * 2 + hexCharWidth * 8);
            ImGui::TableSetupColumn("Vec", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                    paddingWidth * 2 + hexCharWidth * 2);
            ImGui::TableSetupColumn("Lv", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                    paddingWidth * 2 + hexCharWidth * 2);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort);
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();

            const size_t count = m_tracer.interrupts.Count();
            for (size_t i = 0; i < count; i++) {
                auto *sort = ImGui::TableGetSortSpecs();
                bool reverse = false;
                if (sort != nullptr && sort->SpecsCount == 1) {
                    reverse = sort->Specs[0].SortDirection == ImGuiSortDirection_Descending;
                }

                auto trace = reverse ? m_tracer.interrupts.ReadReverse(i) : m_tracer.interrupts.Read(i);

                ImGui::TableNextRow();
                if (ImGui::TableNextColumn()) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%u", trace.counter);
                    ImGui::PopFont();
                }
                if (ImGui::TableNextColumn()) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%08X", trace.pc);
                    ImGui::PopFont();
                }
                if (ImGui::TableNextColumn()) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%02X", trace.vecNum);
                    ImGui::PopFont();
                }
                if (ImGui::TableNextColumn()) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%X", trace.level);
                    ImGui::PopFont();
                }
                if (ImGui::TableNextColumn()) {
                    switch (trace.source) {
                    case sh2::InterruptSource::None: ImGui::TextUnformatted("(none)"); break;
                    case sh2::InterruptSource::FRT_OVI: ImGui::TextUnformatted("FRT OVI"); break;
                    case sh2::InterruptSource::FRT_OCI: ImGui::TextUnformatted("FRT OCI"); break;
                    case sh2::InterruptSource::FRT_ICI: ImGui::TextUnformatted("FRT ICI"); break;
                    case sh2::InterruptSource::SCI_TEI: ImGui::TextUnformatted("SCI TEI"); break;
                    case sh2::InterruptSource::SCI_TXI: ImGui::TextUnformatted("SCI TXI"); break;
                    case sh2::InterruptSource::SCI_RXI: ImGui::TextUnformatted("SCI RXI"); break;
                    case sh2::InterruptSource::SCI_ERI: ImGui::TextUnformatted("SCI ERI"); break;
                    case sh2::InterruptSource::BSC_REF_CMI: ImGui::TextUnformatted("BSC REF CMI"); break;
                    case sh2::InterruptSource::WDT_ITI: ImGui::TextUnformatted("WDT ITI"); break;
                    case sh2::InterruptSource::DMAC1_XferEnd: ImGui::TextUnformatted("DMAC1 TE"); break;
                    case sh2::InterruptSource::DMAC0_XferEnd: ImGui::TextUnformatted("DMAC0 TE"); break;
                    case sh2::InterruptSource::DIVU_OVFI: ImGui::TextUnformatted("DIVU OVFI"); break;
                    case sh2::InterruptSource::IRL: ImGui::TextUnformatted("IRL"); break;
                    case sh2::InterruptSource::UserBreak: ImGui::TextUnformatted("UBC BRK"); break;
                    case sh2::InterruptSource::NMI: ImGui::TextUnformatted("NMI"); break;
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::EndGroup();
}

} // namespace app

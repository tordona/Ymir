#include "scu_dsp_dma_trace_view.hpp"

namespace app::ui {

SCUDSPDMATraceView::SCUDSPDMATraceView(SharedContext &context)
    : m_context(context)
    , m_tracer(context.tracers.SCU) {}

void SCUDSPDMATraceView::Display() {
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::BeginGroup();

    /*ImGui::Checkbox("Enable", &m_tracer.traceDSPDMA);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::TextUnformatted("You must also enable tracing in Debug > Enable tracing (F11)");
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_tracer.dspDmaTransfers.Clear();
        m_tracer.dspDmaStats.Clear();
        m_tracer.ResetDSPDMACounter();
    }*/

    if (ImGui::BeginTable("dsp_dma_trace", 5,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_PreferSortDescending);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 10);
        ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 10);
        ImGui::TableSetupColumn("Len", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 3);
        ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 2);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        const size_t count = 4; // m_tracer.dspDmaTransfers.Count();
        for (size_t i = 0; i < count; i++) {
            auto *sort = ImGui::TableGetSortSpecs();
            bool reverse = false;
            if (sort != nullptr && sort->SpecsCount == 1) {
                reverse = sort->Specs[0].SortDirection == ImGuiSortDirection_Descending;
            }

            // auto trace = reverse ? m_tracer.dspDmaTransfers.ReadReverse(i) : m_tracer.dspDmaTransfers.Read(i);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                // ImGui::Text("%u", trace.counter);
                ImGui::Text("%zu", i);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                if (i & 1 /*trace.toD0*/) {
                    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                    // ImGui::Text("%07X", trace.srcAddress);
                    ImGui::Text("%07X", 0x1234567);
                    ImGui::PopFont();
                    ImGui::SameLine();
                    ImGui::PushFont(m_context.fonts.monospace.small.regular);
                    ImGui::TextDisabled("+4");
                    /*if (trace.srcInc > 0) {
                        ImGui::TextDisabled("+%d", trace.srcInc);
                    } else if (trace.srcInc < 0) {
                        ImGui::TextDisabled("-%d", -trace.srcInc);
                    }*/
                    ImGui::PopFont();
                } else {
                    ImGui::TextUnformatted("Data RAM 1");
                    /*switch (trace.src) {
                    case 0 ... 3: ImGui::Text("Data RAM %u", trace.src); break;
                    default: ImGui::Text("Invalid (%u)", trace.src); break;
                    }*/
                }
            }
            if (ImGui::TableNextColumn()) {
                if (i & 1 /*trace.toD0*/) {
                    ImGui::TextUnformatted("Program RAM");
                    /*switch (trace.dst) {
                    case 0 ... 3: ImGui::Text("Data RAM %u", trace.dst); break;
                    case 4: ImGui::TextUnformatted("Program RAM"); break;
                    default: ImGui::Text("Invalid (%u)", trace.dst); break;
                    }*/
                } else {
                    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                    // ImGui::Text("%07X", trace.dstAddress);
                    ImGui::Text("%07X", 0x7654321);
                    ImGui::PopFont();
                    ImGui::SameLine();
                    ImGui::PushFont(m_context.fonts.monospace.small.regular);
                    ImGui::TextDisabled("+4");
                    /*if (trace.dstInc > 0) {
                        ImGui::TextDisabled("+%d", trace.dstInc);
                    } else if (trace.dstInc < 0) {
                        ImGui::TextDisabled("-%d", -trace.dstInc);
                    }*/
                    ImGui::PopFont();
                }
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                // ImGui::Text("%X", trace.count == 0 ? 0x100 : trace.count);
                ImGui::TextUnformatted("100");
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                // ImGui::Text("%X", trace.count);
                ImGui::Text("%02zX", i);
                ImGui::PopFont();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
}

} // namespace app::ui

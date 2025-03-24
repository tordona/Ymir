#include "sh2_division_unit_trace_view.hpp"

namespace app::ui {

SH2DivisionUnitTracesView::SH2DivisionUnitTracesView(SharedContext &context, satemu::sh2::SH2 &sh2, SH2Tracer &tracer)
    : m_context(context)
    , m_sh2(sh2)
    , m_tracer(tracer) {}

void SH2DivisionUnitTracesView::Display() {
    using namespace satemu;

    ImGui::BeginGroup();

    DisplayTrace();

    ImGui::EndGroup();
}

void SH2DivisionUnitTracesView::DisplayTrace() {
    ImGui::SeparatorText("Division trace");

    ImGui::Checkbox("Enable", &m_tracer.traceDivisions);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::TextUnformatted("You must also enable tracing in Debug > Enable tracing (F11)");
        ImGui::EndTooltip();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Display numbers in hexadecimal", &m_showHex);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        m_tracer.divisions.Clear();
        m_tracer.divStats.Clear();
        m_tracer.ResetDivisionCounter();
    }

    ImGui::PushStyleVarX(ImGuiStyleVar_CellPadding, 8.0f);
    if (ImGui::BeginTable("div_stats", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();

        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.medium.bold);
            ImGui::Text("%llu", m_tracer.divStats.div32s);
            ImGui::PopFont();
            ImGui::TextUnformatted("32x32 divisions");
        }

        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.medium.bold);
            ImGui::Text("%llu", m_tracer.divStats.div64s);
            ImGui::PopFont();
            ImGui::TextUnformatted("64x32 divisions");
        }

        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.medium.bold);
            ImGui::Text("%llu", m_tracer.divStats.overflows);
            ImGui::PopFont();
            ImGui::TextUnformatted("overflows");
        }

        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.sansSerif.medium.bold);
            ImGui::Text("%llu", m_tracer.divStats.interrupts);
            ImGui::PopFont();
            ImGui::TextUnformatted("interrupts");
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    if (ImGui::BeginTable("divu_trace", 7,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_PreferSortDescending);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
        ImGui::TableSetupColumn("Dividend", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 16);
        ImGui::TableSetupColumn("Divisor", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Quotient", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Remainder", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Overflow", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        const size_t count = m_tracer.divisions.Count();
        for (size_t i = 0; i < count; i++) {
            auto *sort = ImGui::TableGetSortSpecs();
            bool reverse = false;
            if (sort != nullptr && sort->SpecsCount == 1) {
                reverse = sort->Specs[0].SortDirection == ImGuiSortDirection_Descending;
            }

            auto trace = reverse ? m_tracer.divisions.ReadReverse(i) : m_tracer.divisions.Read(i);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%u", trace.counter);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::Text("%dx32", trace.div64 ? 64 : 32);
            }
            if (ImGui::TableNextColumn()) {
                if (m_showHex) {
                    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                    if (trace.div64) {
                        ImGui::Text("%016llX", trace.dividend);
                    } else {
                        ImGui::Text("%08X", (sint32)trace.dividend);
                    }
                    ImGui::PopFont();
                } else {
                    ImGui::Text("%lld", trace.dividend);
                }
            }
            if (ImGui::TableNextColumn()) {
                if (m_showHex) {
                    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                    ImGui::Text("%08X", trace.divisor);
                    ImGui::PopFont();
                } else {
                    ImGui::Text("%d", trace.divisor);
                }
            }
            if (ImGui::TableNextColumn()) {
                if (trace.finished) {
                    if (m_showHex) {
                        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                        ImGui::Text("%08X", trace.quotient);
                        ImGui::PopFont();
                    } else {
                        ImGui::Text("%d", trace.quotient);
                    }
                }
            }
            if (ImGui::TableNextColumn()) {
                if (trace.finished) {
                    if (m_showHex) {
                        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                        ImGui::Text("%08X", trace.remainder);
                        ImGui::PopFont();
                    } else {
                        ImGui::Text("%d", trace.remainder);
                    }
                }
            }
            if (ImGui::TableNextColumn()) {
                if (trace.overflow) {
                    if (trace.overflowIntrEnable) {
                        ImGui::TextUnformatted("yes, interrupt raised");
                    } else {
                        ImGui::TextUnformatted("yes");
                    }
                }
            }
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

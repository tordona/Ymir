#include "sh2_division_unit_traces_view.hpp"

namespace app::ui {

SH2DivisionUnitTracesView::SH2DivisionUnitTracesView(SharedContext &context, satemu::sh2::SH2 &sh2, SH2Tracer &tracer)
    : m_context(context)
    , m_sh2(sh2)
    , m_tracer(tracer) {}

void SH2DivisionUnitTracesView::Display() {
    using namespace satemu;

    ImGui::BeginGroup();

    DisplayTraces();

    ImGui::EndGroup();
}

void SH2DivisionUnitTracesView::DisplayTraces() {
    ImGui::SeparatorText("Division traces");

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
    if (ImGui::Button("Clear all")) {
        m_tracer.divisions32.Clear();
        m_tracer.divisions64.Clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear 32x32##clear_32")) {
        m_tracer.divisions32.Clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear 64x32##64")) {
        m_tracer.divisions64.Clear();
    }

    if (ImGui::BeginTable("##divu_main", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 32.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 36.0f);

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            Display32x32Trace();
        }
        if (ImGui::TableNextColumn()) {
            Display64x32Trace();
        }

        ImGui::EndTable();
    }
}

void SH2DivisionUnitTracesView::Display32x32Trace() {
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospaceMedium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText("32x32 divisions");

    if (ImGui::BeginTable("divu_trace_32", 6,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_PreferSortDescending);
        ImGui::TableSetupColumn("Dividend", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Divisor", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Quotient", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Remainder", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                paddingWidth * 2 + hexCharWidth * 8);
        ImGui::TableSetupColumn("Overflow", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoSort);
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableHeadersRow();

        const size_t count = m_tracer.divisions32.Count();
        for (size_t i = 0; i < count; i++) {
            auto *sort = ImGui::TableGetSortSpecs();
            bool reverse = false;
            if (sort != nullptr && sort->SpecsCount == 1) {
                reverse = sort->Specs[0].SortDirection == ImGuiSortDirection_Descending;
            }

            auto trace = reverse ? m_tracer.divisions32.ReadReverse(i) : m_tracer.divisions32.Read(i);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospaceMedium);
                ImGui::Text("%u", trace.counter);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                if (m_showHex) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%08X", trace.dividend);
                    ImGui::PopFont();
                } else {
                    ImGui::Text("%d", trace.dividend);
                }
            }
            if (ImGui::TableNextColumn()) {
                if (m_showHex) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%08X", trace.divisor);
                    ImGui::PopFont();
                } else {
                    ImGui::Text("%d", trace.divisor);
                }
            }
            if (ImGui::TableNextColumn()) {
                if (trace.finished) {
                    if (m_showHex) {
                        ImGui::PushFont(m_context.fonts.monospaceMedium);
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
                        ImGui::PushFont(m_context.fonts.monospaceMedium);
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

void SH2DivisionUnitTracesView::Display64x32Trace() {
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospaceMedium);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText("64x32 divisions");

    if (ImGui::BeginTable("divu_trace_64", 6,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_PreferSortDescending);
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

        const size_t count = m_tracer.divisions64.Count();
        for (size_t i = 0; i < count; i++) {
            auto *sort = ImGui::TableGetSortSpecs();
            bool reverse = false;
            if (sort != nullptr && sort->SpecsCount == 1) {
                reverse = sort->Specs[0].SortDirection == ImGuiSortDirection_Descending;
            }

            auto trace = reverse ? m_tracer.divisions64.ReadReverse(i) : m_tracer.divisions64.Read(i);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospaceMedium);
                ImGui::Text("%u", trace.counter);
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                if (m_showHex) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%016llX", trace.dividend);
                    ImGui::PopFont();
                } else {
                    ImGui::Text("%lld", trace.dividend);
                }
            }
            if (ImGui::TableNextColumn()) {
                if (m_showHex) {
                    ImGui::PushFont(m_context.fonts.monospaceMedium);
                    ImGui::Text("%08X", trace.divisor);
                    ImGui::PopFont();
                } else {
                    ImGui::Text("%d", trace.divisor);
                }
            }
            if (ImGui::TableNextColumn()) {
                if (trace.finished) {
                    if (m_showHex) {
                        ImGui::PushFont(m_context.fonts.monospaceMedium);
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
                        ImGui::PushFont(m_context.fonts.monospaceMedium);
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

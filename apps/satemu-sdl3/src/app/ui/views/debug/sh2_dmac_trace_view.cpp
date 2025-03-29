#include "sh2_dmac_trace_view.hpp"

using namespace satemu;

namespace app::ui {

SH2DMAControllerChannelTraceView::SH2DMAControllerChannelTraceView(SharedContext &context,
                                                                   satemu::sh2::DMAChannel &channel, int index)
    : m_context(context)
    , m_channel(channel)
    , m_index(index) {}

void SH2DMAControllerChannelTraceView::Display() {
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText(fmt::format("Channel {} trace", m_index).c_str());

    ImGui::TextUnformatted("(placeholder text)");
}

} // namespace app::ui

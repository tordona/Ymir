#include "sh2_dmac_channel_view.hpp"

namespace app::ui {

SH2DMAControllerChannelView::SH2DMAControllerChannelView(SharedContext &context, satemu::sh2::DMAChannel &channel,
                                                         int index)
    : m_context(context)
    , m_channel(channel)
    , m_index(index) {}

void SH2DMAControllerChannelView::Display() {
    ImGui::SeparatorText(fmt::format("Channel {}", m_index).c_str());

    ImGui::TextUnformatted("(placeholder text)");
}

} // namespace app::ui

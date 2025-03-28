#include "sh2_dmac_channel_view.hpp"

namespace app::ui {

SH2DMAControllerChannelView::SH2DMAControllerChannelView(SharedContext &context, satemu::sh2::DMAChannel &channel,
                                                         int index)
    : m_context(context)
    , m_channel(channel)
    , m_index(index) {}

void SH2DMAControllerChannelView::Display() {
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText(fmt::format("Channel {}", m_index).c_str());

    auto calcSpacing = [&](const char *label) {
        const float len0 = ImGui::CalcTextSize(fmt::format("{}0", label).c_str()).x;
        const float len1 = ImGui::CalcTextSize(fmt::format("{}1", label).c_str()).x;
        const float currLen = m_index == 0 ? len0 : len1;
        const float maxLen = std::max(len0, len1);
        return maxLen - currLen + ImGui::GetStyle().ItemSpacing.x;
    };

    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 8);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    ImGui::InputScalar(fmt::format("##sar{}", m_index).c_str(), ImGuiDataType_U32, &m_channel.srcAddress, nullptr,
                       nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("SAR%d", m_index);
    ImGui::EndGroup();
    ImGui::SetItemTooltip("DMA Source Address Register");

    ImGui::SameLine(0, calcSpacing("SAR"));

    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 8);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    ImGui::InputScalar(fmt::format("##dar{}", m_index).c_str(), ImGuiDataType_U32, &m_channel.dstAddress, nullptr,
                       nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("DAR%d", m_index);
    ImGui::EndGroup();
    ImGui::SetItemTooltip("DMA Destination Address Register");

    ImGui::SameLine(0, calcSpacing("DAR"));

    uint32 xferCount = m_channel.xferCount;
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 6);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::InputScalar(fmt::format("##tcr{}", m_index).c_str(), ImGuiDataType_U32, &xferCount, nullptr, nullptr,
                           "%06X", ImGuiInputTextFlags_CharsHexadecimal)) {
        m_channel.xferCount = xferCount;
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("TCR%d", m_index);
    ImGui::EndGroup();
    ImGui::SetItemTooltip("DMA Transfer Count Register");

    ImGui::SameLine(0, calcSpacing("TCR"));

    uint32 CHCR = m_channel.ReadCHCR();
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 8);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::InputScalar(fmt::format("##chcr{}", m_index).c_str(), ImGuiDataType_U32, &CHCR, nullptr, nullptr, "%08X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        m_channel.WriteCHCR<true>(CHCR);
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("CHCR%d", m_index);
    ImGui::EndGroup();
    ImGui::SetItemTooltip("DMA Channel Control Register");

    ImGui::SameLine(0, calcSpacing("CHCR"));

    uint8 DRCR = m_channel.ReadDRCR();
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::InputScalar(fmt::format("##drcr{}", m_index).c_str(), ImGuiDataType_U8, &DRCR, nullptr, nullptr, "%02X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        m_channel.WriteDRCR(DRCR);
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("DRCR%d", m_index);
    ImGui::EndGroup();
    ImGui::SetItemTooltip("DMA Request/Response Selection Control Register");
}

} // namespace app::ui

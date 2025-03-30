#include "scu_dma_registers_view.hpp"

namespace app::ui {

SCUDMARegistersView::SCUDMARegistersView(SharedContext &context, uint8 channel)
    : m_context(context)
    , m_scu(context.saturn.SCU)
    , m_channel(channel) {}

void SCUDMARegistersView::Display() {
    const float frameHeight = ImGui::GetFrameHeight();
    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    auto &probe = m_scu.GetProbe();

    ImGui::BeginGroup();

    bool enabled = probe.IsDMAEnabled(m_channel);
    if (ImGui::Checkbox(fmt::format("Enabled##{}", m_channel).c_str(), &enabled)) {
        probe.SetDMAEnabled(m_channel, enabled);
    }
    ImGui::SameLine();
    bool indirect = probe.IsDMAIndirect(m_channel);
    if (ImGui::Checkbox(fmt::format("Indirect transfer##{}", m_channel).c_str(), &indirect)) {
        probe.SetDMAIndirect(m_channel, indirect);
    }

    if (ImGui::BeginTable(fmt::format("addrs_{}", m_channel).c_str(), 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, paddingWidth * 2 + hexCharWidth * 7);
        ImGui::TableSetupColumn("Update", ImGuiTableColumnFlags_WidthFixed, frameHeight);
        ImGui::TableSetupColumn("Increment", ImGuiTableColumnFlags_WidthFixed, paddingWidth * 2 + hexCharWidth * 2);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            uint32 srcAddr = probe.GetDMASourceAddress(m_channel);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            ImGui::SetNextItemWidth(paddingWidth * 2 + hexCharWidth * 7);
            if (ImGui::InputScalar(fmt::format("##srcAddr_{}", m_channel).c_str(), ImGuiDataType_U32, &srcAddr, nullptr,
                                   nullptr, "%07X", ImGuiInputTextFlags_CharsHexadecimal)) {
                probe.SetDMASourceAddress(m_channel, srcAddr);
            }
            ImGui::PopFont();
        }
        if (ImGui::TableNextColumn()) {
            bool srcAddrInc = probe.IsDMAUpdateSourceAddress(m_channel);
            if (ImGui::Checkbox(fmt::format("##srcAddrInc_{}", m_channel).c_str(), &srcAddrInc)) {
                probe.SetDMAUpdateSourceAddress(m_channel, srcAddrInc);
            }
        }
        if (ImGui::TableNextColumn()) {
            uint32 srcAddrIncAmount = probe.GetDMASourceAddressIncrement(m_channel);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            ImGui::SetNextItemWidth(paddingWidth * 2 + hexCharWidth * 2);
            if (ImGui::InputScalar(fmt::format("##srcAddrIncAmount_{}", m_channel).c_str(), ImGuiDataType_U32,
                                   &srcAddrIncAmount, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsHexadecimal)) {
                probe.SetDMASourceAddressIncrement(m_channel, srcAddrIncAmount);
            }
            ImGui::PopFont();
        }
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Source");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            uint32 dstAddr = probe.GetDMADestinationAddress(m_channel);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            ImGui::SetNextItemWidth(paddingWidth * 2 + hexCharWidth * 7);
            if (ImGui::InputScalar(fmt::format("##dstAddr_{}", m_channel).c_str(), ImGuiDataType_U32, &dstAddr, nullptr,
                                   nullptr, "%07X", ImGuiInputTextFlags_CharsHexadecimal)) {
                probe.SetDMADestinationAddress(m_channel, dstAddr);
            }
            ImGui::PopFont();
        }
        if (ImGui::TableNextColumn()) {
            bool dstAddrInc = probe.IsDMAUpdateDestinationAddress(m_channel);
            if (ImGui::Checkbox(fmt::format("##dstAddrInc_{}", m_channel).c_str(), &dstAddrInc)) {
                probe.SetDMAUpdateDestinationAddress(m_channel, dstAddrInc);
            }
        }
        if (ImGui::TableNextColumn()) {
            uint32 dstAddrIncAmount = probe.GetDMADestinationAddressIncrement(m_channel);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            ImGui::SetNextItemWidth(paddingWidth * 2 + hexCharWidth * 2);
            if (ImGui::InputScalar(fmt::format("##dstAddrIncAmount_{}", m_channel).c_str(), ImGuiDataType_U32,
                                   &dstAddrIncAmount, nullptr, nullptr, "%u", ImGuiInputTextFlags_CharsHexadecimal)) {
                probe.SetDMADestinationAddressIncrement(m_channel, dstAddrIncAmount);
            }
            ImGui::PopFont();
        }
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Destination");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            uint32 xferLen = probe.GetDMATransferCount(m_channel);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            ImGui::SetNextItemWidth(paddingWidth * 2 + hexCharWidth * 7); // only 4 for channels 1 and 2
            if (ImGui::InputScalar(fmt::format("##xferLen_{}", m_channel).c_str(), ImGuiDataType_U32, &xferLen, nullptr,
                                   nullptr, "%u", ImGuiInputTextFlags_CharsHexadecimal)) {
                probe.SetDMATransferCount(m_channel, xferLen);
            }
            ImGui::PopFont();
        }
        if (ImGui::TableNextColumn()) {
            // empty
        }
        if (ImGui::TableNextColumn()) {
            // empty
        }
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Length");
        }

        ImGui::EndTable();
    }

    /*
    DMATrigger GetDMATrigger(uint8 channel) const;
    void SetDMATrigger(uint8 channel, DMATrigger trigger);

    // ---------------------------------------------------------------------
    // DMA state

    bool IsDMATransferActive(uint8 channel) const;
    uint32 GetCurrentDMASourceAddress(uint8 channel) const;
    uint32 GetCurrentDMADestinationAddress(uint8 channel) const;
    uint32 GetCurrentDMATransferCount(uint8 channel) const;
    uint32 GetCurrentDMASourceAddressIncrement(uint8 channel) const;
    uint32 GetCurrentDMADestinationAddressIncrement(uint8 channel) const;
    uint32 GetCurrentDMAIndirectSourceAddress(uint8 channel) const;
    */

    ImGui::EndGroup();
}

} // namespace app::ui

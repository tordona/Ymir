#include "scu_dsp_dma_view.hpp"

namespace app::ui {

SCUDSPDMAView::SCUDSPDMAView(SharedContext &context)
    : m_context(context)
    , m_dsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPDMAView::Display() {
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    auto fmtRAMOp = [&](uint8 value) {
        switch (value) {
        case 0: return "Data RAM 0";
        case 1: return "Data RAM 1";
        case 2: return "Data RAM 2";
        case 3: return "Data RAM 3";
        case 4: return "Program RAM";
        default: return "Unknown";
        }
    };

    ImGui::AlignTextToFramePadding();
    if (m_dsp.dmaToD0) {
        ImGui::BeginGroup();
        ImGui::TextUnformatted("From");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##src", fmtRAMOp(m_dsp.dmaSrc), ImGuiComboFlags_WidthFitPreview)) {
            for (uint32 i = 0; i < 4; i++) {
                if (ImGui::Selectable(fmtRAMOp(i), m_dsp.dmaSrc == i)) {
                    m_dsp.dmaSrc = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextUnformatted("to");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 7);
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        if (ImGui::InputScalar("##dst", ImGuiDataType_U32, &m_dsp.dmaWriteAddr, nullptr, nullptr, "%07X",
                               ImGuiInputTextFlags_CharsHexadecimal)) {
            m_dsp.dmaWriteAddr &= 0x7FF'FFFC;
        }
        ImGui::PopFont();
        ImGui::EndGroup();
    } else {
        ImGui::BeginGroup();
        ImGui::TextUnformatted("From");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 7);
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        if (ImGui::InputScalar("##src", ImGuiDataType_U32, &m_dsp.dmaReadAddr, nullptr, nullptr, "%07X",
                               ImGuiInputTextFlags_CharsHexadecimal)) {
            m_dsp.dmaReadAddr &= 0x7FF'FFFC;
        }
        ImGui::PopFont();
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextUnformatted("to");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##dst", fmtRAMOp(m_dsp.dmaDst), ImGuiComboFlags_WidthFitPreview)) {
            for (uint32 i = 0; i < 5; i++) {
                if (ImGui::Selectable(fmtRAMOp(i), m_dsp.dmaDst == i)) {
                    m_dsp.dmaDst = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndGroup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Swap")) {
        m_dsp.dmaToD0 = !m_dsp.dmaToD0;
    }
    // uint8 dmaCount;      // DMA transfer length
    // uint32 dmaAddrInc;   // DMA address increment
    // bool dmaHold;        // DMA transfer hold address
    // bool dmaRun;         // DMA transfer in progress (T0)
}

} // namespace app::ui

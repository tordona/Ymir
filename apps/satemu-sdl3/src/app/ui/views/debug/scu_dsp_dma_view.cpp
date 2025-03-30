#include "scu_dsp_dma_view.hpp"

namespace app::ui {

SCUDSPDMAView::SCUDSPDMAView(SharedContext &context)
    : m_context(context)
    , m_dsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPDMAView::Display() {
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    auto fmtRAMOp = [&](uint8 value, bool allowProgramRAM) {
        switch (value) {
        case 0: return "Data RAM 0";
        case 1: return "Data RAM 1";
        case 2: return "Data RAM 2";
        case 3: return "Data RAM 3";
        case 4: return allowProgramRAM ? "Program RAM" : "Invalid (4)";
        case 5: return "Invalid (5)";
        case 6: return "Invalid (6)";
        case 7: return "Invalid (7)";
        default: return "Invalid";
        }
    };

    if (m_dsp.dmaToD0) {
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("From");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##src", fmtRAMOp(m_dsp.dmaSrc, false), ImGuiComboFlags_WidthFitPreview)) {
            for (uint32 i = 0; i < 4; i++) {
                if (ImGui::Selectable(fmtRAMOp(i, false), m_dsp.dmaSrc == i)) {
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
        ImGui::AlignTextToFramePadding();
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
        if (ImGui::BeginCombo("##dst", fmtRAMOp(m_dsp.dmaDst, true), ImGuiComboFlags_WidthFitPreview)) {
            for (uint32 i = 0; i < 5; i++) {
                if (ImGui::Selectable(fmtRAMOp(i, true), m_dsp.dmaDst == i)) {
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
        if (!m_dsp.dmaToD0) {
            m_dsp.dmaAddrInc = std::min(m_dsp.dmaAddrInc, 4u);
        }
    }

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Count:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    ImGui::InputScalar("##count", ImGuiDataType_U8, &m_dsp.dmaCount, nullptr, nullptr, "%02X",
                       ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextUnformatted("longwords");
    ImGui::EndGroup();

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Increment address by");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##addr_inc", fmt::format("{}", m_dsp.dmaAddrInc).c_str(), ImGuiComboFlags_WidthFitPreview)) {
        if (m_dsp.dmaToD0) {
            for (uint32 i = 0; i < 8; i++) {
                const uint32 inc = (1u << i) >> 1u;
                if (ImGui::Selectable(fmt::format("{}", inc).c_str(), m_dsp.dmaAddrInc == inc)) {
                    m_dsp.dmaAddrInc = inc;
                }
            }
        } else {
            for (uint32 i = 0; i < 2; i++) {
                const uint32 inc = (1u << (i * 2u)) & ~1u;
                if (ImGui::Selectable(fmt::format("{}", inc).c_str(), m_dsp.dmaAddrInc == inc)) {
                    m_dsp.dmaAddrInc = inc;
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::Checkbox("Hold address", &m_dsp.dmaHold);

    ImGui::Checkbox("Transfer running (T0)", &m_dsp.dmaRun);
}

} // namespace app::ui

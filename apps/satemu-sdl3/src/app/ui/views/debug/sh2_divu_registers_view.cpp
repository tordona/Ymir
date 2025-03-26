#include "sh2_divu_registers_view.hpp"

using namespace satemu;

namespace app::ui {

SH2DivisionUnitRegistersView::SH2DivisionUnitRegistersView(SharedContext &context, satemu::sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2DivisionUnitRegistersView::Display() {
    auto &probe = m_sh2.GetProbe();
    auto &divu = probe.DIVU();
    auto &intc = probe.INTC();

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::SeparatorText("Registers");

    if (ImGui::BeginTable("divu_regs", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();

        auto drawReg = [&](uint32 &value, const char *name) {
            ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 8);
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            const bool changed = ImGui::InputScalar(fmt::format("##{}", name).c_str(), ImGuiDataType_U32, &value,
                                                    nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(name);
            return changed;
        };

        if (ImGui::TableNextColumn()) {
            drawReg(divu.DVDNTH, "DVDNTH");
            drawReg(divu.DVDNTUH, "DVDNTUH");
        }

        if (ImGui::TableNextColumn()) {
            drawReg(divu.DVDNTL, "DVDNTL");
            drawReg(divu.DVDNTUL, "DVDNTUL");
        }

        if (ImGui::TableNextColumn()) {
            drawReg(divu.DVDNT, "DVDNT");
            drawReg(divu.DVSR, "DVSR");
        }

        if (ImGui::TableNextColumn()) {
            uint32 dvcr = divu.DVCR.Read();
            if (drawReg(dvcr, "DVCR")) {
                divu.DVCR.Write(dvcr);
            }
            ImGui::Checkbox("OVF", &divu.DVCR.OVF);
            ImGui::SameLine();
            ImGui::Checkbox("OVFIE", &divu.DVCR.OVFIE);
        }

        ImGui::EndTable();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Interrupt:");

    ImGui::SameLine();

    uint8 vector = intc.GetVector(sh2::InterruptSource::DIVU_OVFI);
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 2);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::InputScalar("##vcrdiv", ImGuiDataType_U8, &vector, nullptr, nullptr, "%02X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        intc.SetVector(sh2::InterruptSource::DIVU_OVFI, vector);
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextUnformatted("vector (VCRDIV)");

    ImGui::SameLine();

    uint8 level = intc.GetLevel(sh2::InterruptSource::DIVU_OVFI);
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 1);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::InputScalar("##ipra_divuipn", ImGuiDataType_U8, &level, nullptr, nullptr, "%X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        intc.SetLevel(sh2::InterruptSource::DIVU_OVFI, std::min<uint8>(level, 0xF));
    }
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextUnformatted("level (IPRA.DIVUIP3-0)");

    ImGui::SameLine(0, 15.0f);
    ImGui::TextUnformatted("Calculate:");
    ImGui::SameLine();
    if (ImGui::Button("32x32")) {
        m_context.eventQueues.emulator.enqueue(EmuEvent::DebugDivide(false, m_sh2.IsMaster()));
    }
    ImGui::SameLine();
    if (ImGui::Button("64x32")) {
        m_context.eventQueues.emulator.enqueue(EmuEvent::DebugDivide(true, m_sh2.IsMaster()));
    }
}

} // namespace app::ui

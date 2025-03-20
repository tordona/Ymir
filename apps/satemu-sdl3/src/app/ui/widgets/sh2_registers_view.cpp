#include "sh2_registers_view.hpp"

#include <imgui.h>

using namespace satemu;

namespace app {

SH2RegistersView::SH2RegistersView(SharedContext &context, sh2::SH2 &sh2, bool master)
    : m_context(context)
    , m_sh2(sh2)
    , m_master(master) {}

void SH2RegistersView::Display() {
    ImGui::BeginGroup();

    if (m_master || m_context.saturn.slaveSH2Enabled) {
        // TODO: improve layout - two compact columns

        ImGui::PushFont(m_context.fonts.monospaceMedium);
        const float hexCharWidth = ImGui::CalcTextSize("F").x;
        ImGui::PopFont();

        auto &regs = m_sh2.GetGPRs();

        auto drawReg32 = [&](std::string name, uint32 value) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(name.c_str());

            ImGui::SameLine(50.0f);

            ImGui::PushFont(m_context.fonts.monospaceMedium);
            ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 8);
            const std::string lblField = fmt::format("##input_{}", name);
            ImGui::InputScalar(lblField.c_str(), ImGuiDataType_U32, &value, nullptr, nullptr, "%08X",
                               ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopFont();
        };

        for (uint32 i = 0; i < 16; i++) {
            drawReg32(fmt::format("R{}", i), regs[i]);
        }

        drawReg32("PC", m_sh2.GetPC());
        drawReg32("PR", m_sh2.GetPR());

        auto mac = m_sh2.GetMAC();
        drawReg32("MACH", mac.H);
        drawReg32("MACL", mac.L);

        drawReg32("GBR", m_sh2.GetGBR());
        drawReg32("VBR", m_sh2.GetVBR());

        auto sr = m_sh2.GetSR();
        drawReg32("SR", sr.u32);
        bool M = sr.M;
        bool Q = sr.Q;
        bool S = sr.S;
        bool T = sr.T;
        uint8 ILevel = sr.ILevel;

        ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 4.0f);
        ImGui::BeginGroup();
        ImGui::Checkbox("##M", &M);
        ImGui::TextUnformatted("M");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Checkbox("##Q", &Q);
        ImGui::TextUnformatted("Q");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Checkbox("##S", &S);
        ImGui::TextUnformatted("S");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Checkbox("##T", &T);
        ImGui::TextUnformatted("T");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::PushFont(m_context.fonts.monospaceMedium);
        ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 1);
        ImGui::InputScalar("##input_SR_ILevel", ImGuiDataType_U8, &ILevel, nullptr, nullptr, "%X",
                           ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopFont();
        ImGui::TextUnformatted("I");
        ImGui::EndGroup();
        ImGui::PopStyleVar();

        /*if (debugTrace) {
            drawText(x, y + 280, "vec lv");
            for (size_t i = 0; i < tracer.interruptsCount; i++) {
                const size_t pos =
                    (tracer.interruptsPos - tracer.interruptsCount + i) % tracer.interrupts.size();
                const auto &intr = tracer.interrupts[pos];
                drawText(x, y + 290 + i * 10, fmt::format("{:02X}  {:02X}", intr.vecNum, intr.level));
            }
        } else {
            ImGui::TextUnformatted("(trace disabled)");
        }*/
    } else {
        ImGui::TextUnformatted("(disabled)");
    }

    ImGui::EndGroup();
}

} // namespace app

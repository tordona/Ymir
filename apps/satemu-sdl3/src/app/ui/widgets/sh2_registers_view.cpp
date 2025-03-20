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

        auto &regs = m_sh2.GetGPRs();

        auto drawReg32 = [&](std::string name, uint32 value) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(name.c_str());

            ImGui::SameLine(50.0f);

            ImGui::PushFont(m_context.fonts.monospaceMedium);
            float charWidth = ImGui::CalcTextSize("F").x;
            ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + charWidth * 8);
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

        auto sr = m_sh2.GetSR();
        drawReg32("SR", sr.u32);
        bool M = sr.M;
        bool Q = sr.Q;
        bool S = sr.S;
        bool T = sr.T;

        ImGui::BeginGroup();
        ImGui::Checkbox("##M", &M);
        ImGui::Text("M");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Checkbox("##Q", &Q);
        ImGui::Text("Q");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Checkbox("##S", &S);
        ImGui::Text("S");
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::Checkbox("##T", &T);
        ImGui::Text("T");
        ImGui::EndGroup();

        // TODO: make this editable
        ImGui::Text("I=%X", (uint8)sr.ILevel);

        drawReg32("GBR", m_sh2.GetGBR());
        drawReg32("VBR", m_sh2.GetVBR());

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

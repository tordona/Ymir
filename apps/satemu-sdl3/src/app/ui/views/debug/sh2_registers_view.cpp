#include "sh2_registers_view.hpp"

#include <imgui.h>

using namespace satemu;

namespace app::ui {

SH2RegistersView::SH2RegistersView(SharedContext &context, sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2RegistersView::Display() {
    ImGui::BeginGroup();

    const bool master = m_sh2.IsMaster();
    const bool enabled = master || m_context.saturn.slaveSH2Enabled;

    if (!master) {
        ImGui::Checkbox("Enabled", &m_context.saturn.slaveSH2Enabled);
    }

    // TODO: improve layout - two compact columns
    if (!enabled) {
        ImGui::BeginDisabled();
    }

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    auto &probe = m_sh2.GetProbe();

    auto drawReg32 = [&](std::string name, uint32 &value) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(name.c_str());

        ImGui::SameLine(50.0f);

        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 8);
        const std::string lblField = fmt::format("##input_{}", name);
        ImGui::InputScalar(lblField.c_str(), ImGuiDataType_U32, &value, nullptr, nullptr, "%08X",
                           ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopFont();
    };

    for (uint32 i = 0; i < 16; i++) {
        drawReg32(fmt::format("R{}", i), probe.R(i));
    }

    drawReg32("PC", probe.PC());
    drawReg32("PR", probe.PR());

    sh2::RegMAC &mac = probe.MAC();
    drawReg32("MACH", mac.H);
    drawReg32("MACL", mac.L);

    drawReg32("GBR", probe.GBR());
    drawReg32("VBR", probe.VBR());

    sh2::RegSR &sr = probe.SR();
    drawReg32("SR", sr.u32);

    ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 4.0f);

    ImGui::BeginGroup();
    bool M = sr.M;
    if (ImGui::Checkbox("##M", &M)) {
        sr.M = M;
    }
    ImGui::TextUnformatted("M");
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    bool Q = sr.Q;
    if (ImGui::Checkbox("##Q", &Q)) {
        sr.Q = Q;
    }
    ImGui::TextUnformatted("Q");
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    bool S = sr.S;
    if (ImGui::Checkbox("##S", &S)) {
        sr.S = S;
    }
    ImGui::TextUnformatted("S");
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    bool T = sr.T;
    if (ImGui::Checkbox("##T", &T)) {
        sr.T = T;
    }
    ImGui::TextUnformatted("T");
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    ImGui::SetNextItemWidth(ImGui::GetStyle().FramePadding.x * 2 + hexCharWidth * 1);
    uint8 ILevel = sr.ILevel;
    if (ImGui::InputScalar("##input_SR_ILevel", ImGuiDataType_U8, &ILevel, nullptr, nullptr, "%X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        sr.ILevel = std::min<uint8>(ILevel, 0xFu);
    }
    ImGui::PopFont();
    ImGui::TextUnformatted("I");
    ImGui::EndGroup();
    ImGui::PopStyleVar();

    if (!enabled) {
        ImGui::EndDisabled();
    }

    ImGui::EndGroup();
}

} // namespace app::ui

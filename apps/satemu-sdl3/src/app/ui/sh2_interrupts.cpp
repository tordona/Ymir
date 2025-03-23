#include "sh2_interrupts.hpp"

#include <imgui.h>

using namespace satemu;

namespace app {

SH2Interrupts::SH2Interrupts(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_sh2(master ? context.saturn.masterSH2 : context.saturn.slaveSH2)
    , m_intrView(context, m_sh2)
    , m_intrTraceView(context, m_sh2, master ? context.tracers.masterSH2 : context.tracers.slaveSH2) {}

void SH2Interrupts::Display() {
    if (!Open) {
        return;
    }

    std::string name = fmt::format("{}SH2 interrupts", m_master ? "M" : "S");
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 598), ImVec2(580, FLT_MAX));
    if (ImGui::Begin(name.c_str(), &Open)) {
        if (ImGui::BeginTable("intr_view_base", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();

            if (ImGui::TableNextColumn()) {
                m_intrView.Display();
            }
            if (ImGui::TableNextColumn()) {
                m_intrTraceView.Display();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

} // namespace app

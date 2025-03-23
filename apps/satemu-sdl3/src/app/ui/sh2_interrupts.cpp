#include "sh2_interrupts.hpp"

#include <imgui.h>

using namespace satemu;

namespace app {

SH2Interrupts::SH2Interrupts(SharedContext &context, bool master)
    : m_context(context)
    , m_master(master)
    , m_sh2(master ? context.saturn.masterSH2 : context.saturn.slaveSH2)
    , m_intrView(context, m_sh2) {}

void SH2Interrupts::Display() {
    if (!Open) {
        return;
    }

    std::string name = fmt::format("{}SH2 interrupts", m_master ? "M" : "S");
    if (ImGui::Begin(name.c_str(), &Open, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::BeginTable("intr_view_base", 1, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextRow();

            if (ImGui::TableNextColumn()) {
                m_intrView.Display();
            }
            /*if (ImGui::TableNextColumn()) {
                ImGui::TextUnformatted("room for traces");
            }*/
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

} // namespace app

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
        m_intrView.Display();
    }
    ImGui::End();
}

} // namespace app

#include "scu_dsp_view.hpp"

namespace app::ui {

SCUDSPView::SCUDSPView(SharedContext &context)
    : m_context(context)
    , m_scudsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPView::Display() {
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::TextUnformatted("(placeholder text)");
    ImGui::Text("PC = %02X", m_scudsp.PC);
}

} // namespace app::ui

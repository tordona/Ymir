#include "scu_dsp_view.hpp"

namespace app::ui {

SCUDSPView::SCUDSPView(SharedContext &context)
    : m_context(context)
/*, m_scudsp(context.saturn.SCU)*/ {}

void SCUDSPView::Display() {
    /*auto &probe = m_scu.GetProbe();*/

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::TextUnformatted("(placeholder text)");
}

} // namespace app::ui

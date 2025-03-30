#include "scu_dsp_disassembly_view.hpp"

namespace app::ui {

SCUDSPDisassemblyView::SCUDSPDisassemblyView(SharedContext &context)
    : m_context(context)
    , m_dsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPDisassemblyView::Display() {
    ImGui::TextUnformatted("(placeholder for DSP disassembly)");
}

} // namespace app::ui

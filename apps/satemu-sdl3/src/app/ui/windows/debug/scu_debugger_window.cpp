#include "scu_debugger_window.hpp"

#include <imgui.h>

namespace app::ui {

SCUDebuggerWindow::SCUDebuggerWindow(SharedContext &context)
    : WindowBase(context)
    , m_intrView(context) {

    m_windowConfig.name = "SCU";
}

void SCUDebuggerWindow::DrawContents() {
    m_intrView.Display();
}

} // namespace app::ui

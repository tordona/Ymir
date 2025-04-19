#include "window_base.hpp"

namespace app::ui {

WindowBase::WindowBase(SharedContext &context)
    : m_context(context) {}

void WindowBase::Display() {
    if (!Open) {
        return;
    }

    if (m_focusRequested) {
        m_focusRequested = false;
        ImGui::SetNextWindowFocus();
    }

    PrepareWindow();
    if (ImGui::Begin(m_windowConfig.name.c_str(), &Open, m_windowConfig.flags)) {
        DrawContents();
    }
    ImGui::End();
}

void WindowBase::RequestFocus() {
    if (Open) {
        m_focusRequested = true;
    }
}

} // namespace app::ui

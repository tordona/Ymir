#include "about_window.hpp"

namespace app::ui {

AboutWindow::AboutWindow(SharedContext &context)
    : m_context(context) {}

void AboutWindow::Display() {
    if (!Open) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 240), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("About", &Open)) {
        ImGui::PushFont(m_context.fonts.display);
        ImGui::TextUnformatted("satemu"); // TODO: placeholder name
        ImGui::PopFont();

        ImGui::PushFont(m_context.fonts.sansSerif.large.bold);
        ImGui::TextUnformatted("A Sega Saturn emulator");
        ImGui::PopFont();

        ImGui::NewLine();

        ImGui::TextUnformatted("Version " satemu_VERSION);
    }
    ImGui::End();
}

} // namespace app::ui

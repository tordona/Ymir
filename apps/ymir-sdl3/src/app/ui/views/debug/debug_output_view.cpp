#include "debug_output_view.hpp"

namespace app::ui {

DebugOutputView::DebugOutputView(SharedContext &context)
    : m_context(context)
    , m_tracer(context.tracers.SCU) {}

void DebugOutputView::Display() {
    if (ImGui::Button("Clear##debug_output")) {
        m_tracer.ClearDebugMessages();
    }

    if (ImGui::BeginChild("##scu_debug_output")) {
        ImGui::PushFont(m_context.fonts.monospace.regular, m_context.fontSizes.small);
        const size_t count = m_tracer.debugMessages.Count();
        for (size_t i = 0; i < count; i++) {
            ImGui::Text("%s", m_tracer.debugMessages.Read(i).c_str());
        }
        ImGui::Text("%s", m_tracer.GetDebugMessageBuffer().data());
        ImGui::PopFont();
    }
    ImGui::EndChild();
}

} // namespace app::ui

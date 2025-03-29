#include "scu_debug_output_view.hpp"

namespace app::ui {

SCUDebugOutputView::SCUDebugOutputView(SharedContext &context)
    : m_context(context)
    , m_tracer(context.tracers.SCU) {}

void SCUDebugOutputView::Display() {
    // TODO: should check if tracer is attached instead
    if (!m_context.saturn.IsDebugTracingEnabled()) {
        ImGui::TextUnformatted("Tracing is disabled -- no debug output will be captured.");
        ImGui::TextUnformatted("Enable tracing under Debug > Enable tracing (F11).");
    }
    if (ImGui::Button("Clear##debug_output")) {
        m_tracer.ClearDebugMessages();
    }

    if (ImGui::BeginChild("##scu_debug_output")) {
        ImGui::PushFont(m_context.fonts.monospace.small.regular);
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

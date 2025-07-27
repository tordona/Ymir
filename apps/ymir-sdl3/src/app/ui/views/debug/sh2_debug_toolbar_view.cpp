#include "sh2_debug_toolbar_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>

#include <imgui.h>

using namespace ymir;

namespace app::ui {

SH2DebugToolbarView::SH2DebugToolbarView(SharedContext &context, sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2DebugToolbarView::Display() {
    ImGui::BeginGroup();

    if (!m_context.saturn.IsDebugTracingEnabled()) {
        ImGui::TextColored(m_context.colors.warn, "Debug tracing is disabled. Some features will not work.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Enable##debug_tracing")) {
            m_context.EnqueueEvent(events::emu::SetDebugTrace(true));
        }
    }
    const bool master = m_sh2.IsMaster();
    const bool enabled = master || m_context.saturn.slaveSH2Enabled;

    if (!master) {
        ImGui::Checkbox("Enabled", &m_context.saturn.slaveSH2Enabled);
        ImGui::SameLine();
    }

    if (!m_context.saturn.IsDebugTracingEnabled()) {
        ImGui::BeginDisabled();
    }
    bool suspended = m_sh2.IsCPUSuspended();
    if (ImGui::Checkbox("Suspended", &suspended)) {
        m_sh2.SetCPUSuspended(suspended);
    }
    widgets::ExplanationTooltip("Disables the CPU while in debug mode.", m_context.displayScale);
    if (!m_context.saturn.IsDebugTracingEnabled()) {
        ImGui::EndDisabled();
    }

    ImGui::BeginDisabled(!enabled);
    {
        if (ImGui::Button("Step")) {
            m_context.EnqueueEvent(master ? events::emu::StepMSH2() : events::emu::StepSSH2());
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(m_context.paused);
        if (ImGui::Button("Pause")) {
            m_context.EnqueueEvent(events::emu::SetPaused(true));
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_context.paused);
        if (ImGui::Button("Resume")) {
            m_context.EnqueueEvent(events::emu::SetPaused(false));
        }
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Breakpoints")) {
        m_context.EnqueueEvent(events::gui::OpenSH2BreakpointsWindow(m_sh2.IsMaster()));
    }

    ImGui::EndGroup();
}

} // namespace app::ui

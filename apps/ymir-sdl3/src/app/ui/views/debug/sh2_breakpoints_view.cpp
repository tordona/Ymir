#include "sh2_breakpoints_view.hpp"

#include <app/events/emu_event_factory.hpp>

#include <imgui.h>

using namespace ymir;

namespace app::ui {

SH2BreakpointsView::SH2BreakpointsView(SharedContext &context, sh2::SH2 &sh2)
    : m_context(context)
    , m_sh2(sh2) {}

void SH2BreakpointsView::Display() {
    const float fontSize = m_context.fonts.sizes.medium;
    ImGui::PushFont(m_context.fonts.monospace.regular, fontSize);
    const float hexCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();
    const float framePadding = ImGui::GetStyle().FramePadding.x;
    const float vecFieldWidth = framePadding * 2 + hexCharWidth * 8;

    auto drawHex32 = [&](auto id, uint32 &value) {
        ImGui::PushFont(m_context.fonts.monospace.regular, fontSize);
        ImGui::SetNextItemWidth(vecFieldWidth);
        ImGui::InputScalar(fmt::format("##input_{}", id).c_str(), ImGuiDataType_U32, &value, nullptr, nullptr, "%08X",
                           ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopFont();
        return ImGui::IsItemDeactivated();
    };

    ImGui::BeginGroup();

    if (!m_context.saturn.IsDebugTracingEnabled()) {
        ImGui::TextColored(m_context.colors.warn, "Debug tracing is disabled.");
        ImGui::TextColored(m_context.colors.warn, "Breakpoints will not work.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Enable##debug_tracing")) {
            m_context.EnqueueEvent(events::emu::SetDebugTrace(true));
        }
    }

    if (drawHex32("addr", m_address)) {
        m_address &= ~1u;
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)) {

            std::unique_lock lock{m_context.locks.breakpoints};
            m_sh2.AddBreakpoint(m_address);
            m_context.debuggers.MakeDirty();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        std::unique_lock lock{m_context.locks.breakpoints};
        m_sh2.AddBreakpoint(m_address);
        m_context.debuggers.MakeDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
        std::unique_lock lock{m_context.locks.breakpoints};
        m_sh2.RemoveBreakpoint(m_address);
        m_context.debuggers.MakeDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        std::unique_lock lock{m_context.locks.breakpoints};
        m_sh2.ClearBreakpoints();
        m_context.debuggers.MakeDirty();
    }

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.medium);
    ImGui::SeparatorText("Active breakpoints");
    ImGui::PopFont();

    std::vector<uint32> breakpoints{};
    {
        std::unique_lock lock{m_context.locks.breakpoints};
        auto &currBreakpoints = m_sh2.GetBreakpoints();
        breakpoints.insert(breakpoints.end(), currBreakpoints.begin(), currBreakpoints.end());
    }

    if (ImGui::BeginTable("bkpts", 2, ImGuiTableFlags_SizingFixedFit)) {
        for (size_t i = 0; i < breakpoints.size(); ++i) {
            uint32 address = breakpoints[i];
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                const uint32 prevAddress = address;
                if (drawHex32(i, address)) {
                    std::unique_lock lock{m_context.locks.breakpoints};
                    m_sh2.RemoveBreakpoint(prevAddress);
                    m_sh2.AddBreakpoint(address);
                    m_context.debuggers.MakeDirty();
                }
            }
            if (ImGui::TableNextColumn()) {
                if (ImGui::Button(fmt::format("Remove##{}", i).c_str())) {
                    std::unique_lock lock{m_context.locks.breakpoints};
                    m_sh2.RemoveBreakpoint(address);
                    m_context.debuggers.MakeDirty();
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndGroup();
}

} // namespace app::ui

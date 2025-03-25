#include "memory_viewer_window.hpp"

namespace app::ui {

uint32 MemoryViewerWindow::s_index = 0;

MemoryViewerWindow::MemoryViewerWindow(SharedContext &context)
    : m_sharedCtx(context)
    , m_index(s_index++) {

    m_context.reset(new Context(context));

    m_context->selectedRegion = &kRegionGroups[0].regions[0];
    m_context->memoryEditor.ReadFn = m_context->selectedRegion->readFn;
    m_context->memoryEditor.WriteFn = m_context->selectedRegion->writeFn;
    m_context->memoryEditor.BgColorFn = m_context->selectedRegion->bgColorFn;
    m_context->memoryEditor.Open = false;
    m_context->memoryEditor.UserData = m_context.get();
}

void MemoryViewerWindow::Display() {
    if (!Open) {
        return;
    }

    if (m_requestFocus) {
        ImGui::SetNextWindowFocus();
        m_requestFocus = false;
    }

    MemoryEditor::Sizes sizes{};
    m_context->memoryEditor.CalcSizes(sizes, 0x8000000, 0x0);

    ImGui::SetNextWindowSizeConstraints(ImVec2(sizes.WindowWidth, 245), ImVec2(sizes.WindowWidth, FLT_MAX));
    if (ImGui::Begin(fmt::format("Memory viewer #{}", m_index + 1).c_str(), &Open, ImGuiWindowFlags_NoScrollbar)) {
        const Region *nextRegion = m_context->selectedRegion;
        auto &currRegion = *nextRegion;
        if (currRegion.paramsFn) {
            currRegion.paramsFn(m_context.get());
        }

        ImGui::PushFont(m_sharedCtx.fonts.monospace.medium.regular);
        if (ImGui::BeginCombo("Region", currRegion.ToString().c_str(),
                              ImGuiComboFlags_HeightLarge | ImGuiComboFlags_WidthFitPreview)) {
            for (auto &group : kRegionGroups) {
                ImGui::SeparatorText(group.name);
                for (auto &region : group.regions) {
                    bool selected = &region == &currRegion;
                    if (ImGui::Selectable(region.ToString().c_str(), &selected)) {
                        nextRegion = &region;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopFont();

        ImGui::Checkbox("Enable side-effects", &m_context->enableSideEffects);
        ImGui::Separator();
        ImGui::PushFont(m_sharedCtx.fonts.monospace.medium.regular);
        m_context->memoryEditor.DrawContents(this, currRegion.size, currRegion.baseAddress);
        if (m_context->memoryEditor.MouseHovered) {
            // TODO: use this to display additional info on specific addresses
            if (ImGui::BeginTooltip()) {
                const uint32 address = currRegion.baseAddress + m_context->memoryEditor.MouseHoveredAddr;
                ImGui::Text("Address: %08X", address);
                ImGui::EndTooltip();
            }
        }

        if (nextRegion != m_context->selectedRegion) {
            m_context->selectedRegion = nextRegion;
            m_context->memoryEditor.ReadFn = m_context->selectedRegion->readFn;
            m_context->memoryEditor.WriteFn = m_context->selectedRegion->writeFn;
            m_context->memoryEditor.BgColorFn = m_context->selectedRegion->bgColorFn;
        }

        ImGui::PopFont();
    }
    ImGui::End();
}

void MemoryViewerWindow::RequestFocus() {
    m_requestFocus = true;
}

} // namespace app::ui

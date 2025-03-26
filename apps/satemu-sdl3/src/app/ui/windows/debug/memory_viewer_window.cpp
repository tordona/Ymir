#include "memory_viewer_window.hpp"

#include "memory_viewer_region_defs.hpp"

namespace app::ui {

uint32 MemoryViewerWindow::s_index = 0;

MemoryViewerWindow::MemoryViewerWindow(SharedContext &context)
    : WindowBase(context)
    , m_index(s_index++) {

    m_windowConfig.name = fmt::format("Memory viewer #{}", m_index + 1);
    m_windowConfig.flags = ImGuiWindowFlags_NoScrollbar;

    m_memViewCtx.reset(new Context(context));

    m_memViewCtx->selectedRegion = &RegionDefs::kRegionGroups[0].regions[0];
    m_memViewCtx->memoryEditor.ReadFn = m_memViewCtx->selectedRegion->readFn;
    m_memViewCtx->memoryEditor.WriteFn = m_memViewCtx->selectedRegion->writeFn;
    m_memViewCtx->memoryEditor.BgColorFn = m_memViewCtx->selectedRegion->bgColorFn;
    m_memViewCtx->memoryEditor.Open = false;
    m_memViewCtx->memoryEditor.UserData = m_memViewCtx.get();
}

void MemoryViewerWindow::PrepareWindow() {
    MemoryEditor::Sizes sizes{};
    m_memViewCtx->memoryEditor.CalcSizes(sizes, 0x8000000, 0x0);

    ImGui::SetNextWindowSizeConstraints(ImVec2(sizes.WindowWidth, 245), ImVec2(sizes.WindowWidth, FLT_MAX));
}

void MemoryViewerWindow::DrawContents() {
    const Region *nextRegion = m_memViewCtx->selectedRegion;
    auto &currRegion = *nextRegion;
    if (currRegion.paramsFn) {
        currRegion.paramsFn(m_memViewCtx.get());
    }

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    if (ImGui::BeginCombo("Region", currRegion.ToString().c_str(),
                          ImGuiComboFlags_HeightLarge | ImGuiComboFlags_WidthFitPreview)) {
        for (auto &group : RegionDefs::kRegionGroups) {
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

    ImGui::Checkbox("Enable side-effects", &m_memViewCtx->enableSideEffects);
    ImGui::Separator();
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    m_memViewCtx->memoryEditor.DrawContents(this, currRegion.size, currRegion.baseAddress);
    if (m_memViewCtx->memoryEditor.MouseHovered) {
        // TODO: use this to display additional info on specific addresses
        if (ImGui::BeginTooltip()) {
            const uint32 address = currRegion.baseAddress + m_memViewCtx->memoryEditor.MouseHoveredAddr;
            ImGui::Text("Address: %08X", address);
            ImGui::EndTooltip();
        }
    }

    if (nextRegion != m_memViewCtx->selectedRegion) {
        m_memViewCtx->selectedRegion = nextRegion;
        m_memViewCtx->memoryEditor.ReadFn = m_memViewCtx->selectedRegion->readFn;
        m_memViewCtx->memoryEditor.WriteFn = m_memViewCtx->selectedRegion->writeFn;
        m_memViewCtx->memoryEditor.BgColorFn = m_memViewCtx->selectedRegion->bgColorFn;
    }

    ImGui::PopFont();
}

} // namespace app::ui

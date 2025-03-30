#include "scu_dma_window.hpp"

namespace app::ui {

SCUDMAWindow::SCUDMAWindow(SharedContext &context)
    : WindowBase(context)
    , m_dmaRegsView(context)
    , m_dmaStateView(context) {

    m_windowConfig.name = "SCU DMA";
}

void SCUDMAWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 760), ImVec2(FLT_MAX, FLT_MAX));
}

void SCUDMAWindow::DrawContents() {
    if (ImGui::BeginTable("scu_dma", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 230);
        ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            for (uint32 i = 0; i < 3; i++) {
                ImGui::SeparatorText(fmt::format("Channel {}", i).c_str());

                m_dmaRegsView.Display(i);
                ImGui::Separator();
                m_dmaStateView.Display(i);
            }
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("Trace");
            ImGui::TextUnformatted("(placeholder for DMA trace)");
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui
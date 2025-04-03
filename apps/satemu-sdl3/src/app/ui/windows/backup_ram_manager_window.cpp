#include "backup_ram_manager_window.hpp"

#include <imgui.h>

namespace app::ui {

BackupMemoryManagerWindow::BackupMemoryManagerWindow(SharedContext &context)
    : WindowBase(context) {

    m_windowConfig.name = "Backup memory manager";
}

void BackupMemoryManagerWindow::DrawContents() {
    if (ImGui::Button("Open image...")) {
        // TODO: open image in a new window
        // - but only if it's not the system or cartridge memory or one of the extra windows
        // - disable if there are too many open windows
    }

    // TODO: support drag and drop
    // TODO: support multi-selection
    // TODO: build view for backup memory contents
    // TODO: buttons to easily copy/move between System and Cartridge memory

    if (ImGui::BeginTable("bup_mgr", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("(System backup memory placeholder)");

            ImGui::Button("Import##sys");
            ImGui::SameLine();
            ImGui::Button("Export##sys");
            ImGui::SameLine();
            ImGui::Button("Delete##sys");
            ImGui::SameLine();
            ImGui::Button("Format##sys");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("(Cartridge backup memory placeholder)");

            ImGui::Button("Import##cart");
            ImGui::SameLine();
            ImGui::Button("Export##cart");
            ImGui::SameLine();
            ImGui::Button("Delete##cart");
            ImGui::SameLine();
            ImGui::Button("Format##cart");
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

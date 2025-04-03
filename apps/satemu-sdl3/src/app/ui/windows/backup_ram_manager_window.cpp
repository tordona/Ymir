#include "backup_ram_manager_window.hpp"

#include <satemu/hw/cart/cart.hpp>

#include <imgui.h>

using namespace satemu;

namespace app::ui {

BackupMemoryManagerWindow::BackupMemoryManagerWindow(SharedContext &context)
    : WindowBase(context)
    , m_bupView(context) {

    m_windowConfig.name = "Backup memory manager";
}

void BackupMemoryManagerWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(1025, 340), ImVec2(1025, FLT_MAX));
}

void BackupMemoryManagerWindow::DrawContents() {
    if (ImGui::Button("Open image...")) {
        // TODO: open image in a new window
        // - but only if it's not the system or cartridge memory image or already opened in one of the extra windows
        // - disable if there are too many open windows
    }

    // TODO: buttons to easily copy/move between System and Cartridge memory

    if (ImGui::BeginTable("bup_mgr", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("System memory");
            ImGui::PushID("sys_bup");
            m_bupView.Display(&m_context.saturn.mem.GetInternalBackupRAM());
            ImGui::PopID();
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("Cartridge memory");

            ImGui::PushID("cart_bup");
            std::unique_lock lock{m_context.locks.cart};
            if (auto *bupCart = cart::As<cart::CartType::BackupMemory>(m_context.saturn.GetCartridge())) {
                m_bupView.Display(&bupCart->GetBackupMemory());
            } else {
                m_bupView.Display(nullptr);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

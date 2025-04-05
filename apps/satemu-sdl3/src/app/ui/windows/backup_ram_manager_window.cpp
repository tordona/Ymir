#include "backup_ram_manager_window.hpp"

#include <satemu/hw/cart/cart.hpp>

#include <imgui.h>

using namespace satemu;

namespace app::ui {

BackupMemoryManagerWindow::BackupMemoryManagerWindow(SharedContext &context)
    : WindowBase(context)
    , m_sysBupView(context, "System memory", false)
    , m_cartBupView(context, "Cartridge memory", true) {

    m_sysBupView.SetBackupMemory(&m_context.saturn.mem.GetInternalBackupRAM());

    m_windowConfig.name = "Backup memory manager";
}

void BackupMemoryManagerWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(1100, 340), ImVec2(1100, FLT_MAX));
}

void BackupMemoryManagerWindow::DrawContents() {
    // TODO: buttons to easily copy/move between System and Cartridge memory

    if (ImGui::BeginTable("bup_mgr", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("System memory");
            ImGui::PushID("sys_bup");
            m_sysBupView.Display();
            ImGui::PopID();
        }
        if (ImGui::TableNextColumn()) {
            ImGui::SeparatorText("Cartridge memory");

            ImGui::PushID("cart_bup");
            std::unique_lock lock{m_context.locks.cart};
            if (auto *bupCart = m_context.saturn.GetCartridge().As<cart::CartType::BackupMemory>()) {
                m_cartBupView.SetBackupMemory(&bupCart->GetBackupMemory());
            } else {
                m_cartBupView.SetBackupMemory(nullptr);
            }
            m_cartBupView.Display();
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

} // namespace app::ui

#include "backup_memory_view.hpp"

using namespace satemu;

namespace app::ui {

BackupMemoryView::BackupMemoryView(SharedContext &context)
    : m_context(context) {}

void BackupMemoryView::Display(bup::IBackupMemory *bup) {
    if (bup == nullptr) {
        ImGui::BeginDisabled();

        ImGui::TextUnformatted("Unavailable");
    } else {
        ImGui::Text("%u KiB capacity, %u of %u blocks used", bup->Size() / 1024u, bup->GetUsedBlocks(),
                    bup->GetTotalBlocks());
    }

    ImGui::Button("Import");
    ImGui::SameLine();
    ImGui::Button("Export");
    ImGui::SameLine();
    ImGui::Button("Delete");
    ImGui::SameLine();
    ImGui::Button("Format");

    if (bup == nullptr) {
        ImGui::EndDisabled();
    }
}

} // namespace app::ui

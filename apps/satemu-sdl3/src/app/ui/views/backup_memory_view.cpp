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

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float monoCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    auto files = bup->List();

    // TODO: support drag and drop
    // TODO: support multi-selection
    if (ImGui::BeginTable("bup_files_list", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("File name", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 13);
        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 12);
        ImGui::TableSetupColumn("Language", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 9);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 7);
        ImGui::TableSetupColumn("Blocks", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 5);
        ImGui::TableSetupColumn("Date/time", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto &file : files) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%s", file.header.filename.c_str());
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                ImGui::Text("%s", file.header.comment.c_str());
                ImGui::PopFont();
            }
            if (ImGui::TableNextColumn()) {
                // TODO: convert to text
                ImGui::Text("%u", file.header.language);
            }
            if (ImGui::TableNextColumn()) {
                ImGui::Text("%u", file.size);
            }
            if (ImGui::TableNextColumn()) {
                ImGui::Text("%u", file.numBlocks);
            }
            if (ImGui::TableNextColumn()) {
                const uint32 min = file.header.date % 60;
                const uint32 hour = file.header.date / 60 % 24;
                const uint32 day = file.header.date / 60 / 24;
                // TODO: compute day/month/year with days since 1/1/1980
                ImGui::Text("%u %u:%u", day, hour % 24, min);
            }
        }

        ImGui::EndTable();
    }

    // TODO: fit these buttons below the table
    /*ImGui::Button("Import");
    ImGui::SameLine();
    ImGui::Button("Export");
    ImGui::SameLine();
    ImGui::Button("Delete");
    ImGui::SameLine();
    ImGui::Button("Format");*/

    if (bup == nullptr) {
        ImGui::EndDisabled();
    }
}

} // namespace app::ui

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

    // Make room for buttons below the table
    auto avail = ImGui::GetContentRegionAvail();
    avail.y -= ImGui::GetFrameHeightWithSpacing();

    if (ImGui::BeginChild("##bup_files_table", avail)) {
        // TODO: support drag and drop
        // TODO: support multi-selection
        if (ImGui::BeginTable("bup_files_list", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("File name", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 12.5f);
            ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 11.5f);
            ImGui::TableSetupColumn("Language", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 9);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 6.5f);
            ImGui::TableSetupColumn("Blks", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 4);
            ImGui::TableSetupColumn("Date/time", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            if (bup != nullptr) {
                for (auto &file : bup->List()) {
                    ImGui::TableNextRow();
                    if (ImGui::TableNextColumn()) {
                        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                        bool selected = false; // TODO: check if selected
                        if (ImGui::Selectable(file.header.filename.c_str(), &selected,
                                              ImGuiSelectableFlags_AllowOverlap |
                                                  ImGuiSelectableFlags_SpanAllColumns)) {
                            // TODO: select/deselect
                        }
                        ImGui::PopFont();
                    }
                    if (ImGui::TableNextColumn()) {
                        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                        ImGui::Text("%s", file.header.comment.c_str());
                        ImGui::PopFont();
                    }
                    if (ImGui::TableNextColumn()) {
                        static constexpr const char *kLanguages[] = {"Japanese", "English", "French",
                                                                     "German",   "Spanish", "Italian"};
                        const auto langIndex = static_cast<uint8>(file.header.language);
                        if (langIndex < std::size(kLanguages)) {
                            ImGui::Text("%s", kLanguages[langIndex]);
                        } else {
                            ImGui::Text("<%X>", langIndex);
                        }
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
                        //ImGui::Text("%u %02u:%02u", day, hour % 24, min);
                        ImGui::Text("01/01/1980 %02u:%02u", hour % 24, min);
                    }
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    // TODO: fit these buttons below the table
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

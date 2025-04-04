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
                auto files = bup->List();

                ImGuiMultiSelectIO *msio =
                    ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ClearOnEscape |
                                            ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_BoxSelect1d);
                ApplyRequests(msio, files);

                for (uint32 i = 0; i < files.size(); i++) {
                    auto &file = files[i];

                    ImGui::TableNextRow();
                    if (ImGui::TableNextColumn()) {
                        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                        bool selected = m_selected.contains(i);
                        ImGui::SetNextItemSelectionUserData(i);
                        ImGui::Selectable(file.header.filename.c_str(), selected,
                                          ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns);
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
                        // ImGui::Text("%u %02u:%02u", day, hour % 24, min);
                        ImGui::Text("01/01/1980 %02u:%02u", hour % 24, min);
                    }
                }

                msio = ImGui::EndMultiSelect();
                ApplyRequests(msio, files);
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::Button("Import");
    ImGui::SameLine();
    if (m_selected.empty()) {
        ImGui::BeginDisabled();
    }
    ImGui::Button("Export");
    ImGui::SameLine();
    ImGui::Button("Delete");
    if (m_selected.empty()) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::Button("Format");

    if (bup == nullptr) {
        ImGui::EndDisabled();
    }
}

void BackupMemoryView::ApplyRequests(ImGuiMultiSelectIO *msio, std::vector<satemu::bup::BackupFileInfo> &files) {
    for (ImGuiSelectionRequest &req : msio->Requests) {
        switch (req.Type) {
        case ImGuiSelectionRequestType_None: break;
        case ImGuiSelectionRequestType_SetAll:
            if (req.Selected) {
                for (uint32 i = 0; i < files.size(); i++) {
                    m_selected.insert(i);
                }
            } else {
                m_selected.clear();
            }
            break;
        case ImGuiSelectionRequestType_SetRange:
            for (uint32 i = req.RangeFirstItem; i <= req.RangeLastItem; i++) {
                if (req.Selected) {
                    m_selected.insert(i);
                } else {
                    m_selected.erase(i);
                }
            }
            break;
        }
    }
}

} // namespace app::ui

#include "backup_memory_view.hpp"

using namespace satemu;

namespace app::ui {

BackupMemoryView::BackupMemoryView(SharedContext &context, std::string_view name)
    : m_context(context)
    , m_name(name) {}

void BackupMemoryView::Display(bup::IBackupMemory *bup) {
    const bool hasBup = bup != nullptr;

    std::vector<bup::BackupFileInfo> files{};

    if (hasBup) {
        ImGui::Text("%u KiB capacity, %u of %u blocks used", bup->Size() / 1024u, bup->GetUsedBlocks(),
                    bup->GetTotalBlocks());
        files = bup->List();
    } else {
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("Unavailable");
    }

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float monoCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    // Make room for buttons below the table
    auto avail = ImGui::GetContentRegionAvail();
    avail.y -= ImGui::GetFrameHeightWithSpacing();

    auto drawFileTableRow = [&](const bup::BackupFileInfo &file, uint32 index = 0, bool selectable = false) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
            if (selectable) {
                bool selected = m_selected.contains(index);
                ImGui::SetNextItemSelectionUserData(index);
                ImGui::Selectable(file.header.filename.c_str(), selected,
                                  ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAllColumns);
            } else {
                ImGui::Text("%s", file.header.filename.c_str());
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
            const uint32 daysSince1980 = file.header.date / 60 / 24;
            // TODO: compute day/month/year with days since 1/1/1980
            // ImGui::Text("%u %02u:%02u", daysSince1980, hour % 24, min);
            ImGui::Text("01/01/1980 %02u:%02u", hour % 24, min);
        }
    };

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
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            if (hasBup) {
                ImGuiMultiSelectIO *msio =
                    ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ClearOnEscape |
                                            ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_BoxSelect1d);
                ApplyRequests(msio, files);

                for (uint32 i = 0; i < files.size(); i++) {
                    auto &file = files[i];
                    drawFileTableRow(file, i, true);
                }

                msio = ImGui::EndMultiSelect();
                ApplyRequests(msio, files);
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Import")) {
        // TODO: open file dialog to import one or more backup files
    }
    ImGui::SameLine();
    if (m_selected.empty()) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Export")) {
        // TODO: open file dialog to export selected backup files
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        ImGui::OpenPopup("Confirm deletion");
    }
    if (m_selected.empty()) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Format")) {
        ImGui::OpenPopup("Confirm format");
    }

    // Align to the right
    const float loadImageWidth = ImGui::CalcTextSize("Load image...").x + ImGui::GetStyle().FramePadding.x * 2;
    const float sameLineSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float saveImageWidth = ImGui::CalcTextSize("Save image...").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(avail.x - loadImageWidth - sameLineSpacing - saveImageWidth);
    if (ImGui::Button("Load image...")) {
        // TODO: open file dialog to select a backup memory image file to load; must match current size
    }
    ImGui::SameLine();
    if (ImGui::Button("Save image...")) {
        // TODO: open file dialog to select location to save backup memory image file; ask for overwrite confirmation
    }

    if (!hasBup) {
        ImGui::EndDisabled();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Confirm deletion modal popup

    if (ImGui::BeginPopupModal("Confirm deletion", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The following files will be deleted from %s:", m_name.c_str());

        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        if (ImGui::BeginChild("##files_to_delete", ImVec2(510, lineHeight * 10))) {
            if (ImGui::BeginTable("bup_files_list", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("File name", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 12.5f);
                ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 11.5f);
                ImGui::TableSetupColumn("Language", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 9);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 6.5f);
                ImGui::TableSetupColumn("Blks", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 4);
                ImGui::TableSetupColumn("Date/time", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                for (uint32 item : m_selected) {
                    auto &file = files[item];
                    drawFileTableRow(file);
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        ImGui::TextUnformatted("This operation cannot be undone!");

        /*
        static bool dont_ask_me_next_time = false;
        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Checkbox("Don't ask me next time", &dont_ask_me_next_time);
        ImGui::PopStyleVar();*/

        if (ImGui::Button("OK", ImVec2(80, 0))) {
            // TODO: delete selected files
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Confirm format modal popup

    if (ImGui::BeginPopupModal("Confirm format", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s will be formatted. All files will be erased.", m_name.c_str());
        ImGui::TextUnformatted("This operation cannot be undone!\n");
        ImGui::Text("Are you sure you want to format %s?", m_name.c_str());

        if (ImGui::Button("Yes", ImVec2(80, 0))) {
            // TODO: format backup memory
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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

#include "backup_memory_view.hpp"

#include <app/events/gui_event_factory.hpp>

#include <util/sdl_file_dialog.hpp>

#include <satemu/util/backup_datetime.hpp>

#include <cassert>

using namespace satemu;

namespace app::ui {

static constexpr const char *kConfirmDeletionTitle = "Confirm deletion";
static constexpr const char *kConfirmFormatTitle = "Confirm format";

BackupMemoryView::BackupMemoryView(SharedContext &context, std::string_view name)
    : m_context(context)
    , m_name(name) {}

void BackupMemoryView::SetBackupMemory(satemu::bup::IBackupMemory *bup) {
    if (m_bup != bup) {
        m_bup = bup;
        m_selected.clear();
    }
}

void BackupMemoryView::Display() {
    const bool hasBup = m_bup != nullptr;

    std::vector<bup::BackupFileInfo> files{};

    if (hasBup) {
        ImGui::Text("%u KiB capacity, %u of %u blocks used", m_bup->Size() / 1024u, m_bup->GetUsedBlocks(),
                    m_bup->GetTotalBlocks());
        files = m_bup->List();
    } else {
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("Unavailable");
    }

    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float monoCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    // Make room for buttons below the table
    auto avail = ImGui::GetContentRegionAvail();
    avail.y -= ImGui::GetTextLineHeightWithSpacing(); // selection stats
    avail.y -= ImGui::GetFrameHeightWithSpacing();    // actions

    if (ImGui::BeginChild("##bup_files_table", avail)) {
        // TODO: support drag and drop
        if (ImGui::BeginTable("bup_files_list", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
            DrawFileTableHeader();

            if (hasBup) {
                ImGuiMultiSelectIO *msio =
                    ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ClearOnEscape |
                                            ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_BoxSelect1d);
                ApplyRequests(msio, files);

                for (uint32 i = 0; i < files.size(); i++) {
                    auto &file = files[i];
                    DrawFileTableRow(file, i, true);
                }

                msio = ImGui::EndMultiSelect();
                ApplyRequests(msio, files);
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    // Show selection statistics
    {
        uint32 selCount = m_selected.size();
        uint32 selBlocks = 0;
        uint32 selSize = 0;
        for (uint32 item : m_selected) {
            auto &file = files[item];
            selBlocks += file.numBlocks;
            selSize += file.size;
        }

        auto plural = [](uint32 count) { return count == 1 ? "" : "s"; };

        ImGui::Text("%u file%s selected - %u block%s, %u byte%s", selCount, plural(selCount), selBlocks,
                    plural(selBlocks), selSize, plural(selSize));
    }

    if (ImGui::Button("Import")) {
        // TODO: open file dialog to import one or more backup files
        // - validate input files
        //   - show list of invalid files and ignore them
        // - check for existing files
        // - if there are matching files in the backup memory, show a modal asking if the user wants to replace them
        //   - show table with:
        //     - filename
        //     - comments
        //     - language
        //     - current size, blocks, date/time, ( ) keep original
        //     - imported size, blocks, ( ) replace with imported
        // - if importing fails because of running out of space, show which files could not be imported
    }
    ImGui::SameLine();
    if (m_selected.empty()) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Export")) {
        assert(m_bup != nullptr);

        // Export files from backup memory into a list
        m_exportFiles.clear();
        for (uint32 item : m_selected) {
            auto &fileInfo = files[item];
            auto optFile = m_bup->Export(fileInfo.header.filename);
            if (optFile) {
                m_exportFiles.push_back(*optFile);
            }
        }

        // Open file dialog to export selected backup files
        if (m_exportFiles.size() == 1) {
            // Single file -> allow user to pick location and file name

            auto &filename = m_exportFiles[0].header.filename;
            util::BackupDateTime bupDate{m_exportFiles[0].header.date};

            // TODO (folder manager): default to the exported saves folder
            SaveFileParams params{};
            params.dialogTitle = fmt::format("Export {} from {}", filename, m_name);
            params.defaultPath = fmt::format("{}_{:04d}{:02d}{:02d}_{:02d}{:02d}.bup", filename, bupDate.year,
                                             bupDate.month, bupDate.day, bupDate.hour, bupDate.minute);
            params.filters.push_back({"Backup file", "bup"});
            params.filters.push_back({"All files", "*"});
            params.userdata = this;
            params.callback = util::WrapSingleSelectionCallback<&BackupMemoryView::ProcessSingleFileExport>;

            m_context.EnqueueEvent(events::gui::SaveFile(std::move(params)));
        } else if (!m_exportFiles.empty()) {
            // Multiple files -> allow user to pick location only

            // TODO (folder manager): default to the exported saves folder
            SelectDirectoryParams params{};
            params.dialogTitle = fmt::format("Export {} files from {}", m_exportFiles.size(), m_name);
            params.userdata = this;
            params.callback = util::WrapSingleSelectionCallback<&BackupMemoryView::ProcessMultiFileExport>;

            m_context.EnqueueEvent(events::gui::SelectDirectory(std::move(params)));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        ImGui::OpenPopup(kConfirmDeletionTitle);
    }
    if (m_selected.empty()) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Format")) {
        ImGui::OpenPopup(kConfirmFormatTitle);
    }

    // Align to the right
    const float loadImageWidth = ImGui::CalcTextSize("Load image...").x + ImGui::GetStyle().FramePadding.x * 2;
    const float sameLineSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float saveImageWidth = ImGui::CalcTextSize("Save image...").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(avail.x - loadImageWidth - sameLineSpacing - saveImageWidth);
    if (ImGui::Button("Load image...")) {
        // TODO: open file dialog to select a backup memory image file to load
        // - for system memory, file must be 32 KiB
        // - for cartridge memory, file must be 512 KiB, 1 MiB, 2 MiB or 4 MiB
        //   - cartridge must be "reinserted" with the new backup memory
    }
    ImGui::SameLine();
    if (ImGui::Button("Save image...")) {
        // TODO: open file dialog to select location to save backup memory image file; ask for overwrite confirmation
        // - default names:
        //   - system memory: bup-int.bin
        //   - cartridge memory: bup-ext-<size>.bin
        //     <size> is 4M, 8M, 16M or 32M based on backup memory size (in Mbits)
    }

    if (!hasBup) {
        ImGui::EndDisabled();
    }

    DisplayConfirmDeleteModal(files);
    DisplayConfirmFormatModal();
}

void BackupMemoryView::ProcessSingleFileExport(void *userdata, const char *file, int filter) {
    // TODO: export to file
}

void BackupMemoryView::ProcessMultiFileExport(void *userdata, const char *dir, int filter) {
    // TODO: export all to directory
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

void BackupMemoryView::DrawFileTableHeader() {
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    const float monoCharWidth = ImGui::CalcTextSize("F").x;
    ImGui::PopFont();

    ImGui::TableSetupColumn("File name", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 12.5f);
    ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 11.5f);
    ImGui::TableSetupColumn("Language", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 9);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 6.5f);
    ImGui::TableSetupColumn("Blks", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 4);
    ImGui::TableSetupColumn("Date/time", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
}

void BackupMemoryView::DrawFileTableRow(const bup::BackupFileInfo &file, uint32 index, bool selectable) {
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
        static constexpr const char *kLanguages[] = {"Japanese", "English", "French", "German", "Spanish", "Italian"};
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
        util::BackupDateTime bupDate{file.header.date};
        ImGui::Text("%04u/%02u/%02u %02u:%02u", bupDate.year, bupDate.month, bupDate.day, bupDate.hour, bupDate.minute);
    }
}

void BackupMemoryView::DisplayConfirmDeleteModal(std::span<bup::BackupFileInfo> files) {
    if (ImGui::BeginPopupModal(kConfirmDeletionTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The following files will be deleted from %s:", m_name.c_str());

        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        if (ImGui::BeginChild("##files_to_delete", ImVec2(550, lineHeight * 10))) {
            if (ImGui::BeginTable("bup_files_list", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
                DrawFileTableHeader();

                for (uint32 item : m_selected) {
                    auto &file = files[item];
                    DrawFileTableRow(file);
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
            assert(m_bup != nullptr);
            for (uint32 item : m_selected) {
                auto &file = files[item];
                m_bup->Delete(file.header.filename);
            }
            m_selected.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void BackupMemoryView::DisplayConfirmFormatModal() {
    if (ImGui::BeginPopupModal(kConfirmFormatTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s will be formatted. All files will be erased.", m_name.c_str());
        ImGui::TextUnformatted("This operation cannot be undone!\n");
        ImGui::Text("Are you sure you want to format %s?", m_name.c_str());

        if (ImGui::Button("Yes", ImVec2(80, 0))) {
            assert(m_bup != nullptr);
            m_bup->Format();
            m_selected.clear();
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

} // namespace app::ui

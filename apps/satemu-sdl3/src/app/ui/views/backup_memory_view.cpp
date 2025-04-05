// This absolutely disgusting hacky mess somehow manages to work.
//
// It seems like the current design of the application makes it extremely hard to do anything more complex that just
// displaying state or doing very simple interactions. Multi-step operations take a *ton* of effort to build and
// requires storing a bunch of state in a hacky manner, on top of having to be thread-aware -- most of this code runs on
// the main thread, but file dialog operations run on an unspecified SDL thread (which could be the main thread or not).
// Anything that needs access to the cartridge also needs to lock the cartridges mutex (which is done by the window, but
// not by the SDL thread, so you'll find locks in both places).
//
// I *really* hate frontend development... *sigh*
//
// - StrikerX3

#include "backup_memory_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <util/sdl_file_dialog.hpp>

#include <satemu/util/backup_datetime.hpp>
#include <satemu/util/bit_ops.hpp>
#include <satemu/util/size_ops.hpp>

#include <cassert>
#include <fstream>
#include <iostream>
#include <optional>
#include <string_view>

using namespace satemu;

namespace app::ui {

static constexpr const char *kConfirmDeletionTitle = "Confirm deletion";
static constexpr const char *kConfirmFormatTitle = "Confirm format";

BackupMemoryView::BackupMemoryView(SharedContext &context, std::string_view name, bool external)
    : m_context(context)
    , m_name(name)
    , m_external(external) {}

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

        auto plural = [](uint32 count, const char *singular, const char *plural) {
            return count == 1 ? singular : plural;
        };

        if (selCount == 0) {
            ImGui::TextUnformatted("No files selected");
        } else {
            ImGui::Text("%u %s selected - %u %s, %u %s", selCount, plural(selCount, "file", "files"), selBlocks,
                        plural(selBlocks, "block", "blocks"), selSize, plural(selSize, "byte", "bytes"));
        }
    }

    if (ImGui::Button("Import")) {
        // Open file dialog to select backup files to load
        // TODO (folder manager): default to the exported saves folder
        FileDialogParams params{};
        params.dialogTitle = fmt::format("Import backup files to {}", m_name);
        params.filters.push_back({"Backup files", "bup"});
        params.filters.push_back({"All files", "*"});
        params.userdata = this;
        params.callback = util::WrapMultiSelectionCallback<&BackupMemoryView::ProcessFileImport,
                                                           &BackupMemoryView::ProcessCancelFileImport,
                                                           &BackupMemoryView::ProcessFileImportError>;

        m_context.EnqueueEvent(events::gui::OpenManyFiles(std::move(params)));
    }
    ImGui::SameLine();
    if (m_selected.empty()) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Export")) {
        assert(m_bup != nullptr);

        // Export files from backup memory into a list
        m_filesToExport.clear();
        for (uint32 item : m_selected) {
            auto &fileInfo = files[item];
            auto optFile = m_bup->Export(fileInfo.header.filename);
            if (optFile) {
                m_filesToExport.push_back(*optFile);
            }
        }

        // Open file dialog to export selected backup files
        if (m_filesToExport.size() == 1) {
            // Single file -> allow user to pick location and file name

            auto &filename = m_filesToExport[0].header.filename;
            util::BackupDateTime bupDate{m_filesToExport[0].header.date};

            // TODO (folder manager): default to the exported saves folder
            FileDialogParams params{};
            params.dialogTitle = fmt::format("Export {} from {}", filename, m_name);
            params.defaultPath = fmt::format("{}_{:04d}{:02d}{:02d}_{:02d}{:02d}.bup", filename, bupDate.year,
                                             bupDate.month, bupDate.day, bupDate.hour, bupDate.minute);
            params.filters.push_back({"Backup file", "bup"});
            params.filters.push_back({"All files", "*"});
            params.userdata = this;
            params.callback = util::WrapSingleSelectionCallback<&BackupMemoryView::ProcessSingleFileExport,
                                                                &BackupMemoryView::ProcessCancelFileExport,
                                                                &BackupMemoryView::ProcessFileExportError>;

            m_context.EnqueueEvent(events::gui::SaveFile(std::move(params)));
        } else if (!m_filesToExport.empty()) {
            // Multiple files -> allow user to pick location only

            // TODO (folder manager): default to the exported saves folder
            FolderDialogParams params{};
            params.dialogTitle = fmt::format("Export {} files from {}", m_filesToExport.size(), m_name);
            params.userdata = this;
            params.callback = util::WrapSingleSelectionCallback<&BackupMemoryView::ProcessMultiFileExport,
                                                                &BackupMemoryView::ProcessCancelFileExport,
                                                                &BackupMemoryView::ProcessFileExportError>;

            m_context.EnqueueEvent(events::gui::SelectFolder(std::move(params)));
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
        // Open file dialog to select backup memory image to load
        // TODO (folder manager): default to the exported saves folder
        FileDialogParams params{};
        params.dialogTitle = fmt::format("Load {} image", m_name);
        params.defaultPath =
            m_external ? fmt::format("bup-ext-{}M.bin", m_bup->Size() * 8 / 1024 / 1024) : "bup-int.bin";
        params.filters.push_back({"Backup memory image file", "bin"});
        params.filters.push_back({"All files", "*"});
        params.userdata = this;
        params.callback = util::WrapSingleSelectionCallback<&BackupMemoryView::ProcessImageImport,
                                                            &BackupMemoryView::ProcessCancelImageImport,
                                                            &BackupMemoryView::ProcessImageImportError>;

        m_context.EnqueueEvent(events::gui::OpenFile(std::move(params)));
    }
    ImGui::SameLine();
    if (ImGui::Button("Save image...")) {
        m_imageToSave = m_bup->ReadAll();

        // Open file dialog to select backup memory image to save
        // TODO (folder manager): default to the exported saves folder
        FileDialogParams params{};
        params.dialogTitle = fmt::format("Save {} image", m_name);
        params.defaultPath =
            m_external ? fmt::format("bup-ext-{}M.bin", m_bup->Size() * 8 / 1024 / 1024) : "bup-int.bin";
        params.filters.push_back({"Backup memory image file", "bin"});
        params.filters.push_back({"All files", "*"});
        params.userdata = this;
        params.callback = util::WrapSingleSelectionCallback<&BackupMemoryView::ProcessImageExport,
                                                            &BackupMemoryView::ProcessCancelImageExport,
                                                            &BackupMemoryView::ProcessImageExportError>;

        m_context.EnqueueEvent(events::gui::SaveFile(std::move(params)));
    }

    if (!hasBup) {
        ImGui::EndDisabled();
    }

    DisplayConfirmDeleteModal(files);
    DisplayConfirmFormatModal();
    DisplayFileImportOverwriteModal(files);
    DisplayFileImportResultModal();
    DisplayFilesExportSuccessfulModal();
    DisplayImageImportSuccessfulModal();
    DisplayImageExportSuccessfulModal();
    DisplayErrorModal();
}

bool BackupMemoryView::HasSelection() const {
    return !m_selected.empty();
}

std::vector<bup::BackupFile> BackupMemoryView::ExportAll() const {
    if (m_bup != nullptr) {
        return m_bup->ExportAll();
    } else {
        return {};
    }
}

std::vector<bup::BackupFile> BackupMemoryView::ExportSelected() const {
    if (m_bup != nullptr) {
        std::vector<bup::BackupFile> files{};
        auto bupFiles = m_bup->List();
        for (auto item : m_selected) {
            if (item >= bupFiles.size()) {
                continue;
            }
            if (auto file = m_bup->Export(bupFiles[item].header.filename)) {
                files.push_back(*file);
            }
        }
        return files;
    } else {
        return {};
    }
}

void BackupMemoryView::ImportAll(std::span<const bup::BackupFile> files) {
    m_importBad.clear();
    m_importFailed.clear();
    m_importOverwrite.clear();
    for (auto &file : files) {
        switch (m_bup->Import(file, true)) {
        case bup::BackupFileImportResult::Imported: break;
        case bup::BackupFileImportResult::Overwritten: break;
        case bup::BackupFileImportResult::NoSpace:
            m_importFailed.push_back({file.header, "Not enough space in memory"});
            break;
        default: m_importFailed.push_back({file.header, "Unspecified error"}); break;
        }
    }
    if (!m_importFailed.empty()) {
        OpenFileImportResultModal();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// File table drawing

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

// ---------------------------------------------------------------------------------------------------------------------
// Popups and modals

void BackupMemoryView::OpenFileImportOverwriteModal() {
    m_openFileImportOverwriteModal = true;
}

void BackupMemoryView::OpenFileImportResultModal() {
    m_openFileImportResultModal = true;
}

void BackupMemoryView::OpenFilesExportSuccessfulModal(uint32 exportCount) {
    m_openFilesExportSuccessfulModal = true;
    m_filesExportCount = exportCount;
}

void BackupMemoryView::OpenImageImportSuccessfulModal() {
    m_openImageImportSuccessfulModal = true;
}

void BackupMemoryView::OpenImageExportSuccessfulModal() {
    m_openImageExportSuccessfulModal = true;
}

void BackupMemoryView::OpenErrorModal(std::string errorMessage) {
    m_openErrorModal = true;
    m_errorModalMessage = errorMessage;
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
            for (uint32 item : m_selected) {
                auto &file = files[item];
                m_context.EnqueueEvent(events::emu::DeleteBackupFile(file.header.filename, m_external));
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
            m_context.EnqueueEvent(events::emu::FormatBackupMemory(m_external));
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

void BackupMemoryView::DisplayFileImportOverwriteModal(std::span<bup::BackupFileInfo> files) {
    static constexpr const char *kTitle = "Resolve imported file conflicts";

    if (m_openFileImportOverwriteModal) {
        ImGui::OpenPopup(kTitle);
        m_openFileImportOverwriteModal = false;
    }

    if (ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The following files already exist in %s:", m_name.c_str());

        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        const float monoCharWidth = ImGui::CalcTextSize("F").x;
        ImGui::PopFont();

        auto avail = ImGui::GetContentRegionAvail();
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        if (ImGui::BeginChild("##bup_files_table", ImVec2(550, lineHeight * 20))) {
            if (ImGui::BeginTable("bup_files_overwrite_list", 6,
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("File name", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 12.5f);
                ImGui::TableSetupColumn("Original\nSize", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 7);
                ImGui::TableSetupColumn("Original\nDate/time", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 14);
                ImGui::TableSetupColumn("Imported\nSize", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 7);
                ImGui::TableSetupColumn("Imported\nDate/time", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 14);
                ImGui::TableSetupColumn("Overwrite", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                auto findOgInfo = [&](std::string_view filename) -> std::optional<bup::BackupFileInfo> {
                    for (auto &file : files) {
                        if (file.header.filename == filename) {
                            return file;
                        }
                    }
                    return std::nullopt;
                };

                int index = 0;
                for (auto &ovFile : m_importOverwrite) {
                    auto ogInfo = findOgInfo(ovFile.file.header.filename);

                    if (!ogInfo.has_value()) {
                        // Might've been erased by the emulated system in the meantime; "overwrite" and move on
                        ovFile.overwrite = true;
                        continue;
                    }

                    ImGui::TableNextRow();
                    // filename
                    if (ImGui::TableNextColumn()) {
                        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                        ImGui::Text("%s", ovFile.file.header.filename.c_str());
                        ImGui::PopFont();
                    }

                    if (ovFile.overwrite) {
                        ImGui::BeginDisabled();
                    }
                    // original size
                    if (ImGui::TableNextColumn()) {
                        ImGui::Text("%u", ogInfo->size);
                    }
                    // original date/time
                    if (ImGui::TableNextColumn()) {
                        util::BackupDateTime bupDate{ogInfo->header.date};
                        ImGui::Text("%04u/%02u/%02u %02u:%02u", bupDate.year, bupDate.month, bupDate.day, bupDate.hour,
                                    bupDate.minute);
                    }
                    if (ovFile.overwrite) {
                        ImGui::EndDisabled();
                    }

                    if (!ovFile.overwrite) {
                        ImGui::BeginDisabled();
                    }
                    // imported size
                    if (ImGui::TableNextColumn()) {
                        ImGui::Text("%u", (uint32)ovFile.file.data.size());
                    }
                    // imported date/time
                    if (ImGui::TableNextColumn()) {
                        util::BackupDateTime bupDate{ovFile.file.header.date};
                        ImGui::Text("%04u/%02u/%02u %02u:%02u", bupDate.year, bupDate.month, bupDate.day, bupDate.hour,
                                    bupDate.minute);
                    }
                    if (!ovFile.overwrite) {
                        ImGui::EndDisabled();
                    }

                    // overwrite checkbox
                    if (ImGui::TableNextColumn()) {
                        ImGui::Checkbox(fmt::format("##overwrite_{}_{}", ovFile.file.header.filename, index++).c_str(),
                                        &ovFile.overwrite);
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        bool execute = false;

        if (ImGui::Button("Import", ImVec2(100, 0))) {
            execute = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Overwrite all", ImVec2(100, 0))) {
            for (auto &ovFile : m_importOverwrite) {
                ovFile.overwrite = true;
                break;
            }
            execute = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Ignore all", ImVec2(100, 0))) {
            execute = false;
            ImGui::CloseCurrentPopup();
            OpenFileImportResultModal();
        }

        if (execute) {
            for (auto &ovFile : m_importOverwrite) {
                // Skip files not selected to be overwritten
                if (!ovFile.overwrite) {
                    continue;
                }

                // TODO: should do this in the emulator thread; but needs two-way communication
                // - std::future/std::promise?
                // Attempt to overwrite files
                switch (m_bup->Import(ovFile.file, true)) {
                case bup::BackupFileImportResult::Imported: // fallthrough
                case bup::BackupFileImportResult::Overwritten: m_importSuccess.push_back(ovFile.file.header); break;
                case bup::BackupFileImportResult::NoSpace:
                    m_importFailed.push_back({ovFile.file.header, "Not enough space in memory"});
                    break;
                default: m_importFailed.push_back({ovFile.file.header, "Unspecified error"}); break;
                }
            }
            m_importOverwrite.clear();

            ImGui::CloseCurrentPopup();
            OpenFileImportResultModal();
        }

        ImGui::EndPopup();
    }
}

void BackupMemoryView::DisplayFileImportResultModal() {
    static constexpr const char *kTitle = "Backup file import summary";

    if (m_openFileImportResultModal) {
        ImGui::OpenPopup(kTitle);
        m_openFileImportResultModal = false;
    }

    if (ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        const float monoCharWidth = ImGui::CalcTextSize("F").x;
        ImGui::PopFont();

        if (!m_importSuccess.empty()) {
            ImGui::Text("%zu file%s imported successfully.", m_importSuccess.size(),
                        (m_importSuccess.size() == 1 ? "" : "s"));
        }

        if (!m_importFailed.empty()) {
            ImGui::Text("The following file%s could not be imported:", (m_importFailed.size() == 1 ? "" : "s"));

            auto avail = ImGui::GetContentRegionAvail();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            if (ImGui::BeginChild("##bup_failed_table", ImVec2(550, lineHeight * 10))) {
                if (ImGui::BeginTable("bup_files_failed_list", 2,
                                      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupColumn("File name", ImGuiTableColumnFlags_WidthFixed, monoCharWidth * 12.5f);
                    ImGui::TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    for (auto &file : m_importFailed) {
                        ImGui::TableNextRow();
                        // filename
                        if (ImGui::TableNextColumn()) {
                            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                            ImGui::Text("%s", file.file.filename.c_str());
                            ImGui::PopFont();
                        }
                        // reason
                        if (ImGui::TableNextColumn()) {
                            ImGui::Text("%s", file.errorMessage.c_str());
                        }
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
        }

        if (!m_importBad.empty()) {
            ImGui::Text("The following file%s could not be loaded:", (m_importBad.size() == 1 ? "" : "s"));

            auto avail = ImGui::GetContentRegionAvail();
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            if (ImGui::BeginChild("##bup_bad_table", ImVec2(550, lineHeight * 10))) {
                if (ImGui::BeginTable("bup_files_bad_list", 2,
                                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupColumn("Path");
                    ImGui::TableSetupColumn("Reason");
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    for (auto &file : m_importBad) {
                        ImGui::TableNextRow();
                        // path
                        if (ImGui::TableNextColumn()) {
                            ImGui::PushFont(m_context.fonts.monospace.medium.regular);
                            ImGui::Text("%s", file.file.string().c_str());
                            ImGui::PopFont();
                        }
                        // reason
                        if (ImGui::TableNextColumn()) {
                            ImGui::Text("%s", file.errorMessage.c_str());
                        }
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
        }

        if (ImGui::Button("OK", ImVec2(80, 0))) {
            m_importBad.clear();
            m_importFailed.clear();
            m_importOverwrite.clear();
            m_importSuccess.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void BackupMemoryView::DisplayFilesExportSuccessfulModal() {
    static constexpr const char *kTitle = "Files export successful";

    if (m_openFilesExportSuccessfulModal) {
        ImGui::OpenPopup(kTitle);
        m_openFilesExportSuccessfulModal = false;
    }

    if (ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%u file%s exported successfully.", m_filesExportCount, (m_filesExportCount == 1 ? "" : "s"));

        if (ImGui::Button("OK", ImVec2(80, 0))) {
            m_filesExportCount = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void BackupMemoryView::DisplayImageImportSuccessfulModal() {
    static constexpr const char *kTitle = "Image import successful";

    if (m_openImageImportSuccessfulModal) {
        ImGui::OpenPopup(kTitle);
        m_openImageImportSuccessfulModal = false;
    }

    if (ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s image imported successfully.", m_name.c_str());

        if (ImGui::Button("OK", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void BackupMemoryView::DisplayImageExportSuccessfulModal() {
    static constexpr const char *kTitle = "Image export successful";

    if (m_openImageExportSuccessfulModal) {
        ImGui::OpenPopup(kTitle);
        m_openImageExportSuccessfulModal = false;
    }

    if (ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s image exported successfully.", m_name.c_str());

        if (ImGui::Button("OK", ImVec2(80, 0))) {
            m_filesExportCount = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void BackupMemoryView::DisplayErrorModal() {
    static constexpr const char *kTitle = "Error";

    if (m_openErrorModal) {
        ImGui::OpenPopup(kTitle);
        m_openErrorModal = false;
    }

    if (ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(450.0f);
        ImGui::Text("%s", m_errorModalMessage.c_str());
        ImGui::PopTextWrapPos();

        if (ImGui::Button("OK", ImVec2(80, 0))) {
            m_filesExportCount = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// File import action

void BackupMemoryView::ProcessFileImport(void *userdata, std::span<std::filesystem::path> files, int filter) {
    static_cast<BackupMemoryView *>(userdata)->ImportFiles(files);
}

void BackupMemoryView::ProcessCancelFileImport(void *userdata, int filter) {
    static_cast<BackupMemoryView *>(userdata)->CancelFileImport();
}

void BackupMemoryView::ProcessFileImportError(void *userdata, const char *errorMessage, int filter) {
    static_cast<BackupMemoryView *>(userdata)->FileImportError(errorMessage);
}

void BackupMemoryView::ImportFiles(std::span<std::filesystem::path> files) {
    bup::BackupFile bupFile{};
    std::error_code error{};
    for (auto &file : files) {
        switch (ImportFile(file, bupFile, error)) {
        case ImportFileResult::Success:
            // TODO: should do this in the emulator thread; but needs two-way communication
            // - std::future/std::promise?
            // Attempt to import file without overwriting
            switch (m_bup->Import(bupFile, false)) {
            case bup::BackupFileImportResult::Imported: m_importSuccess.push_back(bupFile.header); break;
            case bup::BackupFileImportResult::NoSpace:
                m_importFailed.push_back({bupFile.header, "Not enough space in memory"});
                break;
            case bup::BackupFileImportResult::FileExists: m_importOverwrite.push_back({bupFile}); break;
            default: m_importFailed.push_back({bupFile.header, "Unspecified error"}); break;
            }
            break;
        case ImportFileResult::FilesystemError: m_importBad.push_back({file, error.message()}); break;
        case ImportFileResult::FileTruncated: m_importBad.push_back({file, "Backup file truncated"}); break;
        case ImportFileResult::BadMagic: m_importBad.push_back({file, "Not a valid backup file"}); break;
        default: m_importBad.push_back({file, "Unspecified error"}); break;
        }
    }

    if (m_importOverwrite.empty()) {
        // Show final import result
        OpenFileImportResultModal();
    } else {
        // Show list of files that would be overwritten
        OpenFileImportOverwriteModal();
    }
}

void BackupMemoryView::CancelFileImport() {
    // nothing to do
}

void BackupMemoryView::FileImportError(const char *errorMessage) {
    OpenErrorModal(fmt::format("File import failed: {}", errorMessage));
}

BackupMemoryView::ImportFileResult BackupMemoryView::ImportFile(std::filesystem::path path, bup::BackupFile &out,
                                                                std::error_code &error) {
    // 00..03  char[4]   magic: "YmBP"
    // 04..0E  char[11]  filename
    //     0F  uint8     language
    // 10..19  char[10]  comment
    // 1A..1D  uint32le  date/time (minutes since 01/01/1980)
    // 1E..21  uint32le  data size (in bytes)
    // 22....  uint8...  data

    error.clear();

#define CHECK_INPUT_ERROR                                 \
    do {                                                  \
        if (!in) {                                        \
            error.assign(errno, std::generic_category()); \
            return ImportFileResult::FilesystemError;     \
        }                                                 \
    } while (0)

    std::ifstream in{path, std::ios::binary};
    std::array<char, 11> buf{};

    CHECK_INPUT_ERROR;

    const size_t fileSize = std::filesystem::file_size(path);
    if (fileSize < 0x22) {
        return ImportFileResult::FileTruncated;
    }

    // magic
    static constexpr std::string_view kMagic = "YmBP";
    in.read(buf.data(), 4);
    CHECK_INPUT_ERROR;
    if (std::string_view(buf.data(), 4) != kMagic) {
        return ImportFileResult::BadMagic;
    }

    // filename
    in.read(buf.data(), 11);
    CHECK_INPUT_ERROR;
    out.header.filename.assign(buf.begin(), buf.end());

    // language
    out.header.language = static_cast<bup::Language>(in.get());
    CHECK_INPUT_ERROR;

    // comment
    in.read(buf.data(), 10);
    CHECK_INPUT_ERROR;
    out.header.comment.assign(buf.begin(), buf.end());

    // date/time
    in.read((char *)&out.header.date, sizeof(out.header.date));
    CHECK_INPUT_ERROR;

    // data size
    uint32 size{};
    in.read((char *)&size, sizeof(size));
    CHECK_INPUT_ERROR;
    if ((size_t)size + 0x22 > fileSize) {
        return ImportFileResult::FileTruncated;
    }

    // data
    out.data.resize(size);
    in.read((char *)out.data.data(), out.data.size());
    CHECK_INPUT_ERROR;

    return ImportFileResult::Success;
}

// ---------------------------------------------------------------------------------------------------------------------
// File export action

void BackupMemoryView::ProcessSingleFileExport(void *userdata, std::filesystem::path file, int filter) {
    static_cast<BackupMemoryView *>(userdata)->ExportSingleFile(file);
}

void BackupMemoryView::ProcessMultiFileExport(void *userdata, std::filesystem::path dir, int filter) {
    static_cast<BackupMemoryView *>(userdata)->ExportMultiFile(dir);
}

void BackupMemoryView::ProcessCancelFileExport(void *userdata, int filter) {
    static_cast<BackupMemoryView *>(userdata)->CancelFileExport();
}

void BackupMemoryView::ProcessFileExportError(void *userdata, const char *errorMessage, int filter) {
    static_cast<BackupMemoryView *>(userdata)->FileExportError(errorMessage);
}

void BackupMemoryView::ExportSingleFile(std::filesystem::path file) {
    assert(m_filesToExport.size() == 1);

    std::filesystem::create_directories(file.parent_path());

    ExportFile(file, m_filesToExport[0]);
    OpenFilesExportSuccessfulModal(m_filesToExport.size());
    m_filesToExport.clear();
}

void BackupMemoryView::ExportMultiFile(std::filesystem::path dir) {
    std::filesystem::create_directories(dir);

    for (auto &file : m_filesToExport) {
        util::BackupDateTime bupDate{file.header.date};
        std::string filename = fmt::format("{}_{:04d}{:02d}{:02d}_{:02d}{:02d}.bup", file.header.filename, bupDate.year,
                                           bupDate.month, bupDate.day, bupDate.hour, bupDate.minute);
        ExportFile(dir / filename, file);
    }
    OpenFilesExportSuccessfulModal(m_filesToExport.size());
    m_filesToExport.clear();
}

void BackupMemoryView::CancelFileExport() {
    m_filesToExport.clear();
}

void BackupMemoryView::FileExportError(const char *errorMessage) {
    OpenErrorModal(fmt::format("File export failed: {}", errorMessage));
    m_filesToExport.clear();
}

void BackupMemoryView::ExportFile(std::filesystem::path path, const satemu::bup::BackupFile &bupFile) {
    // 00..03  char[4]   magic: "YmBP"
    // 04..0E  char[11]  filename
    //     0F  uint8     language
    // 10..19  char[10]  comment
    // 1A..1D  uint32le  date/time (minutes since 01/01/1980)
    // 1E..21  uint32le  data size (in bytes)
    // 22....  uint8...  data

    std::ofstream out{path, std::ios::binary};
    std::array<char, 11> buf{};

    // magic
    static constexpr std::string_view kMagic = "YmBP";
    out.write(kMagic.data(), kMagic.size());

    // filename
    buf.fill(0);
    std::copy(bupFile.header.filename.begin(), bupFile.header.filename.end(), buf.begin());
    out.write(&buf[0], 11);

    // language
    out.put(static_cast<char>(bupFile.header.language));

    // comment
    buf.fill(0);
    std::copy(bupFile.header.comment.begin(), bupFile.header.comment.end(), buf.begin());
    out.write(&buf[0], 10);

    // date/time
    const uint32 date = bupFile.header.date;
    out.write((const char *)&date, sizeof(date));

    // data size
    const uint32 size = static_cast<uint32>(bupFile.data.size());
    out.write((const char *)&size, sizeof(size));

    // data
    out.write((const char *)bupFile.data.data(), bupFile.data.size());
}

// ---------------------------------------------------------------------------------------------------------------------
// Load image action

void BackupMemoryView::ProcessImageImport(void *userdata, std::filesystem::path file, int filter) {
    static_cast<BackupMemoryView *>(userdata)->ImportImage(file);
}

void BackupMemoryView::ProcessCancelImageImport(void *userdata, int filter) {
    static_cast<BackupMemoryView *>(userdata)->CancelImageImport();
}

void BackupMemoryView::ProcessImageImportError(void *userdata, const char *errorMessage, int filter) {
    static_cast<BackupMemoryView *>(userdata)->ImageImportError(errorMessage);
}

void BackupMemoryView::ImportImage(std::filesystem::path file) {
    // Try to load image
    bup::BackupMemory bupMem{};
    std::error_code error{};
    auto result = bupMem.LoadFrom(file, error);
    switch (result) {
    case bup::BackupMemoryImageLoadResult::Success: break;
    case bup::BackupMemoryImageLoadResult::FilesystemError:
        OpenErrorModal(fmt::format("Could not import {} as {}: {}", file.string(), m_name, error.message()));
        return;
    case bup::BackupMemoryImageLoadResult::InvalidSize:
        OpenErrorModal(fmt::format("Could not import {} as {}: invalid image size", file.string(), m_name));
        return;
    }

    // Check file size - must match expected size for the device
    // - for system memory, file must be 32 KiB
    // - for cartridge memory, file must be 512 KiB, 1 MiB, 2 MiB or 4 MiB
    auto size = bupMem.Size();
    if ((!m_external && size != 32_KiB) || (m_external && (size < 512_KiB || size > 4_MiB))) {
        OpenErrorModal(fmt::format("Could not import {} as {}: invalid image size", file.string(), m_name));
        return;
    }

    // Replace backup memory instances
    if (m_external) {
        m_context.EnqueueEvent(events::emu::ReplaceExternalBackupMemory(std::move(bupMem)));
    } else {
        m_context.EnqueueEvent(events::emu::ReplaceInternalBackupMemory(std::move(bupMem)));
    }

    OpenImageImportSuccessfulModal();
}

void BackupMemoryView::CancelImageImport() {
    // nothing to do
}

void BackupMemoryView::ImageImportError(const char *errorMessage) {
    OpenErrorModal(fmt::format("{} image import failed: {}", m_name, errorMessage));
}

// ---------------------------------------------------------------------------------------------------------------------
// Save image action

void BackupMemoryView::ProcessImageExport(void *userdata, std::filesystem::path file, int filter) {
    static_cast<BackupMemoryView *>(userdata)->ExportImage(file);
}

void BackupMemoryView::ProcessCancelImageExport(void *userdata, int filter) {
    static_cast<BackupMemoryView *>(userdata)->CancelImageExport();
}

void BackupMemoryView::ProcessImageExportError(void *userdata, const char *errorMessage, int filter) {
    static_cast<BackupMemoryView *>(userdata)->ImageExportError(errorMessage);
}

void BackupMemoryView::ExportImage(std::filesystem::path file) {
    std::ofstream out{file, std::ios::binary};
    out.write((const char *)m_imageToSave.data(), m_imageToSave.size());
    m_imageToSave.clear();
    OpenImageExportSuccessfulModal();
}

void BackupMemoryView::CancelImageExport() {
    m_imageToSave.clear();
}

void BackupMemoryView::ImageExportError(const char *errorMessage) {
    OpenErrorModal(fmt::format("{} image export failed: {}", m_name, errorMessage));
    m_imageToSave.clear();
}

} // namespace app::ui

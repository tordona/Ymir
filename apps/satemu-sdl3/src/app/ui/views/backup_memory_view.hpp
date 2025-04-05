#pragma once

#include <app/shared_context.hpp>

#include <satemu/sys/backup_ram_defs.hpp>

#include <imgui.h>

#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace app::ui {

class BackupMemoryView {
public:
    BackupMemoryView(SharedContext &context, std::string_view name, bool external);

    // NOTE: for external backup memory, the lock must be held by the caller
    // Pass nullptr to display an empty/disabled/unavailable backup memory list
    void SetBackupMemory(satemu::bup::IBackupMemory *bup);

    void Display();

private:
    SharedContext &m_context;
    std::string m_name;
    bool m_external;

    std::set<uint32> m_selected;
    satemu::bup::IBackupMemory *m_bup;

    // -----------------------------------------------------------------------------------------------------------------
    // File table drawing

    void ApplyRequests(ImGuiMultiSelectIO *msio, std::vector<satemu::bup::BackupFileInfo> &files);

    void DrawFileTableHeader();
    void DrawFileTableRow(const satemu::bup::BackupFileInfo &file, uint32 index = 0, bool selectable = false);

    // -----------------------------------------------------------------------------------------------------------------
    // Popups and modals

    void OpenFilesExportSuccessfulModal(uint32 exportCount);
    void OpenImageExportSuccessfulModal();
    void OpenErrorModal(std::string errorMessage);

    void DisplayConfirmDeleteModal(std::span<satemu::bup::BackupFileInfo> files);
    void DisplayConfirmFormatModal();
    void DisplayFilesExportSuccessfulModal();
    void DisplayImageExportSuccessfulModal();
    void DisplayErrorModal();

    bool m_openFilesExportSuccessfulPopup = false;
    uint32 m_filesExportCount;

    bool m_openImageExportSuccessfulPopup = false;

    bool m_openErrorModal = false;
    std::string m_errorModalMessage;

    // -----------------------------------------------------------------------------------------------------------------
    // File export action

    std::vector<satemu::bup::BackupFile> m_filesToExport;

    static void ProcessSingleFileExport(void *userdata, std::filesystem::path file, int filter);
    static void ProcessMultiFileExport(void *userdata, std::filesystem::path dir, int filter);
    static void ProcessCancelFileExport(void *userdata, int filter);
    static void ProcessFileExportError(void *userdata, const char *errorMessage, int filter);

    void ExportSingleFile(std::filesystem::path file);
    void ExportMultiFile(std::filesystem::path dir);
    void CancelFileExport();
    void FileExportError(const char *errorMessage);

    void ExportFile(std::filesystem::path path, const satemu::bup::BackupFile &bupFile);

    // -----------------------------------------------------------------------------------------------------------------
    // Save Image action

    std::vector<uint8> m_imageToSave;

    static void ProcessImageExport(void *userdata, std::filesystem::path file, int filter);
    static void ProcessCancelImageExport(void *userdata, int filter);
    static void ProcessImageExportError(void *userdata, const char *errorMessage, int filter);

    void ExportImage(std::filesystem::path file);
    void CancelImageExport();
    void ImageExportError(const char *errorMessage);
};

} // namespace app::ui

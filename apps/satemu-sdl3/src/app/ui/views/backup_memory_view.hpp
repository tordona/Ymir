#pragma once

#include <app/shared_context.hpp>

#include <satemu/sys/backup_ram_defs.hpp>

#include <imgui.h>

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace app::ui {

class BackupMemoryView {
public:
    BackupMemoryView(SharedContext &context, std::string_view name);

    // NOTE: for external backup memory, the lock must be held by the caller
    // Pass nullptr to display an empty/disabled/unavailable backup memory list
    void SetBackupMemory(satemu::bup::IBackupMemory *bup);

    void Display();

private:
    SharedContext &m_context;
    std::string m_name;

    std::set<uint32> m_selected;
    satemu::bup::IBackupMemory *m_bup;

    std::vector<satemu::bup::BackupFile> m_exportFiles;

    static void ProcessSingleFileExport(void *userdata, const char *file, int filter);
    static void ProcessMultiFileExport(void *userdata, const char *dir, int filter);

    void ApplyRequests(ImGuiMultiSelectIO *msio, std::vector<satemu::bup::BackupFileInfo> &files);

    void DrawFileTableHeader();
    void DrawFileTableRow(const satemu::bup::BackupFileInfo &file, uint32 index = 0, bool selectable = false);

    void DisplayConfirmDeleteModal(std::span<satemu::bup::BackupFileInfo> files);
    void DisplayConfirmFormatModal();
};

} // namespace app::ui

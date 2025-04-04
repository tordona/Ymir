#pragma once

#include <app/shared_context.hpp>

#include <satemu/sys/backup_ram_defs.hpp>

#include <imgui.h>

#include <set>

namespace app::ui {

class BackupMemoryView {
public:
    BackupMemoryView(SharedContext &context);

    // NOTE: for external backup memory, the lock must be held by the caller
    // Pass nullptr to display an empty/disabled/unavailable backup memory list
    void Display(satemu::bup::IBackupMemory *bup);

private:
    SharedContext &m_context;

    std::set<uint32> m_selected;

    void ApplyRequests(ImGuiMultiSelectIO *msio, std::vector<satemu::bup::BackupFileInfo> &files);
};

} // namespace app::ui

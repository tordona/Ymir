#pragma once

#include <app/shared_context.hpp>

#include <satemu/sys/backup_ram_defs.hpp>

namespace app::ui {

class BackupMemoryView {
public:
    BackupMemoryView(SharedContext &context);

    // NOTE: for external backup memory, the lock must be held by the caller
    // Pass nullptr to display an empty/disabled/unavailable backup memory list
    void Display(satemu::bup::IBackupMemory *bup);

private:
    SharedContext &m_context;
};

} // namespace app::ui

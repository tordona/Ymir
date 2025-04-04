#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/backup_memory_view.hpp>

namespace app::ui {

class BackupMemoryManagerWindow : public WindowBase {
public:
    BackupMemoryManagerWindow(SharedContext &context);

protected:
    void PrepareWindow() final;
    void DrawContents() final;

private:
    // TODO: system backup memory view
    // TODO: cartridge backup memory view
    // TODO: additional windows for backup memory views from files

    BackupMemoryView m_sysBupView;
    BackupMemoryView m_cartBupView;
};

} // namespace app::ui

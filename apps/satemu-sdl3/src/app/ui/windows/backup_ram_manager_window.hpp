#pragma once

#include <app/ui/window_base.hpp>

namespace app::ui {

class BackupMemoryManagerWindow : public WindowBase {
public:
    BackupMemoryManagerWindow(SharedContext &context);

protected:
    void DrawContents() final;

private:
    // TODO: system backup memory view
    // TODO: cartridge backup memory view
    // TODO: additional windows for backup memory views from files
};

} // namespace app::ui

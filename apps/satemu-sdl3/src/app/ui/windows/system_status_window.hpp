#pragma once

#include <app/ui/window_base.hpp>

namespace app::ui {

class SystemStatusWindow : public WindowBase {
public:
    SystemStatusWindow(SharedContext &context);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    void DrawParameters();
    void DrawClocks();
    void DrawRealTimeClock();
    void DrawScreen();
    void DrawCDDrive();
    void DrawCartridge();
    void DrawPeripherals();
    void DrawActions();
};

} // namespace app::ui

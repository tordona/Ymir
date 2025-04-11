#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/settings/standard_pad_binds_view.hpp>

namespace app::ui {

class PeripheralBindsWindow : public WindowBase {
public:
    PeripheralBindsWindow(SharedContext &context);

    void Open(uint32 portIndex, uint32 slotIndex);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    uint32 m_portIndex;
    uint32 m_slotIndex;

    StandardPadBindsView m_standardPadView;
};

} // namespace app::ui

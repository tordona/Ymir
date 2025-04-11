#pragma once

#include <satemu/hw/smpc/peripheral/peripheral_impl_standard_pad.hpp>

#include <app/ui/window_base.hpp>

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

    void DrawBindsTable(satemu::peripheral::StandardPad &periph);
};

} // namespace app::ui

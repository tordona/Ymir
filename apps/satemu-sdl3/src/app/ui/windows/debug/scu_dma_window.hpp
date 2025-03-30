#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/scu_dma_registers_view.hpp>

#include <array>

namespace app::ui {

class SCUDMAWindow : public WindowBase {
public:
    SCUDMAWindow(SharedContext &context);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    std::array<SCUDMARegistersView, 3> m_dmaRegsViews;
};

} // namespace app::ui

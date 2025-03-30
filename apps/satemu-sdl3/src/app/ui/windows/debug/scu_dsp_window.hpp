#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/scu_dsp_dma_view.hpp>
#include <app/ui/views/debug/scu_dsp_registers_view.hpp>

namespace app::ui {

class SCUDSPWindow : public WindowBase {
public:
    SCUDSPWindow(SharedContext &context);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SCUDSPRegistersView m_regsView;
    SCUDSPDMAView m_dmaView;
};

} // namespace app::ui

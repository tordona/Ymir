#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/scu_dma_view.hpp>

namespace app::ui {

class SCUDMAWindow : public WindowBase {
public:
    SCUDMAWindow(SharedContext &context);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SCUDMAView m_dmaView;
};

} // namespace app::ui

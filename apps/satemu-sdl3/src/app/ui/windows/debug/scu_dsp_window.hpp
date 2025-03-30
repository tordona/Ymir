#pragma once

#include <app/ui/window_base.hpp>

#include <app/ui/views/debug/scu_dsp_view.hpp>

namespace app::ui {

class SCUDSPWindow : public WindowBase {
public:
    SCUDSPWindow(SharedContext &context);

protected:
    void PrepareWindow() override;
    void DrawContents() override;

private:
    SCUDSPView m_dspView;
};

} // namespace app::ui

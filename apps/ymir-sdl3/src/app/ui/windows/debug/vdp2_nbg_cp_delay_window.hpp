#pragma once

#include "vdp_window_base.hpp"

#include <app/ui/views/debug/vdp2_nbg_cp_delay_view.hpp>

namespace app::ui {

class VDP2NBGCharPatDelayWindow : public VDPWindowBase {
public:
    VDP2NBGCharPatDelayWindow(SharedContext &context);

protected:
    void DrawContents() override;

private:
    VDP2NBGCharPatDelayView m_nbgDelayView;
};

} // namespace app::ui

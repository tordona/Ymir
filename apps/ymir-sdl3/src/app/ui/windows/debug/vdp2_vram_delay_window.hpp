#pragma once

#include "vdp_window_base.hpp"

#include <app/ui/views/debug/vdp2_vram_delay_view.hpp>

namespace app::ui {

class VDP2VRAMDelayWindow : public VDPWindowBase {
public:
    VDP2VRAMDelayWindow(SharedContext &context);

protected:
    void DrawContents() override;

private:
    VDP2VRAMDelayView m_nbgDelayView;
};

} // namespace app::ui

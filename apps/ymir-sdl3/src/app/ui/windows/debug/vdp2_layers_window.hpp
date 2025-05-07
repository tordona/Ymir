#pragma once

#include "vdp_window_base.hpp"

#include <app/ui/views/debug/vdp2_layers_enable_view.hpp>

namespace app::ui {

class VDP2LayersWindow : public VDPWindowBase {
public:
    VDP2LayersWindow(SharedContext &context);

protected:
    void DrawContents() override;

private:
    VDP2LayersEnableView m_layersEnableView;
};

} // namespace app::ui
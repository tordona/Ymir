#include "vdp2_layers_window.hpp"

namespace app::ui {

VDP2LayersWindow::VDP2LayersWindow(SharedContext &context)
    : VDPWindowBase(context)
    , m_layersEnableView(context, m_vdp) {

    m_windowConfig.name = "VDP2 layers";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void VDP2LayersWindow::DrawContents() {
    m_layersEnableView.Display();
}

} // namespace app::ui

#include "sh2_cache_window.hpp"

namespace app::ui {

SH2CacheWindow::SH2CacheWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master) {

    m_windowConfig.name = fmt::format("{}SH2 cache", master ? 'M' : 'S');
}

void SH2CacheWindow::DrawContents() {
    ImGui::TextUnformatted("(placeholder)");
}

} // namespace app::ui

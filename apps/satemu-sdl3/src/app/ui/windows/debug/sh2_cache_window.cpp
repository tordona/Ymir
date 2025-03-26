#include "sh2_cache_window.hpp"

namespace app::ui {

SH2CacheWindow::SH2CacheWindow(SharedContext &context, bool master)
    : SH2WindowBase(context, master)
    , m_cacheRegView(context, m_sh2) {

    m_windowConfig.name = fmt::format("{}SH2 cache", master ? 'M' : 'S');
}

void SH2CacheWindow::DrawContents() {
    m_cacheRegView.Display();
}

} // namespace app::ui

#pragma once

#include "sh2_window_base.hpp"

#include <app/ui/views/debug/sh2_cache_register_view.hpp>

namespace app::ui {

class SH2CacheWindow : public SH2WindowBase {
public:
    SH2CacheWindow(SharedContext &context, bool master);

protected:
    void DrawContents();

private:
    SH2CacheRegisterView m_cacheRegView;
};

} // namespace app::ui

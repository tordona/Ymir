#pragma once

#include "sh2_window_base.hpp"

#include <satemu/hw/sh2/sh2.hpp>

namespace app::ui {

class SH2CacheWindow : public SH2WindowBase {
public:
    SH2CacheWindow(SharedContext &context, bool master);

protected:
    void DrawContents();
};

} // namespace app::ui

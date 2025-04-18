#pragma once

#include "settings_view_base.hpp"

namespace app::ui {

class CDBlockSettingsView : public SettingsViewBase {
public:
    CDBlockSettingsView(SharedContext &context);

    void Display();
};

} // namespace app::ui

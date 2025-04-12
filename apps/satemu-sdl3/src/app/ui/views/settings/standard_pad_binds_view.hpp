#pragma once

#include "settings_view_base.hpp"

namespace app::ui {

class StandardPadBindsView : public SettingsViewBase {
public:
    StandardPadBindsView(SharedContext &context);

    void Display(Settings::Input::Port::StandardPadBinds &binds);

private:
    bool m_captured = false;
};

} // namespace app::ui

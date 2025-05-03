#pragma once

#include "settings_view_base.hpp"

#include <app/ui/widgets/input_widgets.hpp>

namespace app::ui {

class StandardPadBindsView : public SettingsViewBase {
public:
    StandardPadBindsView(SharedContext &context);

    void Display(Settings::Input::Port::StandardPadBinds &binds);

private:
    widgets::InputCaptureWidget m_inputCaptureWidget;
};

} // namespace app::ui

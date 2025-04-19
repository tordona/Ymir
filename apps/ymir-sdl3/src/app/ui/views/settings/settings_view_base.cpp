#include "settings_view_base.hpp"

namespace app::ui {

SettingsViewBase::SettingsViewBase(SharedContext &context)
    : m_context(context) {}

void SettingsViewBase::MakeDirty() {
    m_context.settings.MakeDirty();
}

bool SettingsViewBase::MakeDirty(bool value) {
    if (value) {
        MakeDirty();
    }
    return value;
}

} // namespace app::ui

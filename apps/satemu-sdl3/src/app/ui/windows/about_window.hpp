#pragma once

#include <app/shared_context.hpp>

namespace app::ui {

class AboutWindow {
public:
    AboutWindow(SharedContext &context);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;
};

} // namespace app::ui

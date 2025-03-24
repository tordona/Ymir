#pragma once

#include <app/shared_context.hpp>

#include <vector>

namespace app::ui {

struct License {
    const char *name;
    const char *url;
};

struct FontDesc {
    const char *name;
    const License &license;
    const char *url;
    ImFont *&font;
};

class AboutWindow {
public:
    AboutWindow(SharedContext &context);

    void Display();

    bool Open = false;

private:
    SharedContext &m_context;

    std::vector<FontDesc> m_fontDescs;
};

} // namespace app::ui

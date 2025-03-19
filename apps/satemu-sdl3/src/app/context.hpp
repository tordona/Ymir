#pragma once

#include <satemu/satemu.hpp>

#include <imgui.h>

namespace app {

struct Context {
    satemu::Saturn saturn;

    struct Fonts {
        ImFont *sansSerifMedium = nullptr;
        ImFont *sansSerifBold = nullptr;
        ImFont *sansSerifMediumMedium = nullptr;
        ImFont *sansSerifMediumBold = nullptr;
        ImFont *sansSerifLargeBold = nullptr;
        ImFont *monospaceMedium = nullptr;
        ImFont *monospaceBold = nullptr;
        ImFont *monospaceMediumMedium = nullptr;
        ImFont *monospaceMediumBold = nullptr;
        ImFont *display = nullptr;
    } fonts;
};

} // namespace app

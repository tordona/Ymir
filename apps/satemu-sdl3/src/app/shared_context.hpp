#pragma once

#include <satemu/satemu.hpp>

#include <app/debug/scu_tracer.hpp>
#include <app/debug/sh2_tracer.hpp>

#include <imgui.h>

namespace app {

struct SharedContext {
    satemu::Saturn saturn;

    struct Tracers {
        SH2Tracer masterSH2;
        SH2Tracer slaveSH2;
        SCUTracer SCU;
    } tracers;

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

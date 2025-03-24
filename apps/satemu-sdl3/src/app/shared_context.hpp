#pragma once

#include <satemu/satemu.hpp>

#include <app/debug/scu_tracer.hpp>
#include <app/debug/sh2_tracer.hpp>

#include <app/events/emu_events.hpp>

#include <imgui.h>

#include <blockingconcurrentqueue.h>

namespace app {

struct SharedContext {
    satemu::Saturn saturn;

    struct Tracers {
        SH2Tracer masterSH2;
        SH2Tracer slaveSH2;
        SCUTracer SCU;
    } tracers;

    struct Fonts {
        struct {
            // 14 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } small;

            // 16 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } medium;

            // 20 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } large;

            // 28 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } xlarge;
        } sansSerif;

        struct {
            // 14 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } small;

            // 16 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } medium;

            // 20 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } large;

            // 28 pt
            struct {
                ImFont *regular = nullptr;
                ImFont *bold = nullptr;
            } xlarge;
        } monospace;

        ImFont *display = nullptr;
    } fonts;

    struct EventQueues {
        moodycamel::BlockingConcurrentQueue<EmuEvent> emulator;
    } eventQueues;
};

} // namespace app

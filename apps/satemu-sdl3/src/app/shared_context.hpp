#pragma once

#include <satemu/satemu.hpp>

#include <app/profile.hpp>
#include <app/rewind_buffer.hpp>
#include <app/settings.hpp>

#include <app/input/input_capturer.hpp>
#include <app/input/input_context.hpp>

#include <app/debug/scu_tracer.hpp>
#include <app/debug/sh2_tracer.hpp>

#include <app/events/emu_event.hpp>
#include <app/events/gui_event.hpp>

#include <imgui.h>

#include <blockingconcurrentqueue.h>

#include <filesystem>
#include <memory>
#include <mutex>

namespace app {

struct SharedContext {
    satemu::Saturn saturn;

    input::InputContext inputContext;
    input::InputCapturer inputCapturer;

    std::array<satemu::peripheral::StandardPadButton, 2> standardPadButtons{
        satemu::peripheral::StandardPadButton::Default, satemu::peripheral::StandardPadButton::Default};

    Profile profile;
    Settings settings{*this};

    std::array<std::unique_ptr<satemu::state::State>, 10> saveStates;
    size_t currSaveStateSlot = 0;

    RewindBuffer rewindBuffer;
    bool rewinding = false;

    // Certain GUI interactions requires synchronization with the emulator thread, specifically when dealing with
    // dynamic objects:
    // - Cartridges
    // - Discs
    // - Peripherals
    // - Save states
    // These locks must be held by the emulator thread whenever the object instances are to be replaced.
    // The GUI must hold these locks when accessing these objects to ensure the emulator thread doesn't destroy them.
    struct Locks {
        std::mutex cart;
        std::mutex disc;
        std::mutex peripherals;
        std::array<std::mutex, 10> saveStates;
    } locks;

    struct State {
        std::filesystem::path loadedDiscImagePath;
    } state;

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

        struct {
            ImFont *small = nullptr;
            ImFont *large = nullptr;
        } display;
    } fonts;

    struct EventQueues {
        moodycamel::BlockingConcurrentQueue<EmuEvent> emulator;
        moodycamel::BlockingConcurrentQueue<GUIEvent> gui;
    } eventQueues;

    // -----------------------------------------------------------------------------------------------------------------
    // Convenience methods

    void EnqueueEvent(EmuEvent &&event) {
        eventQueues.emulator.enqueue(std::move(event));
    }

    void EnqueueEvent(GUIEvent &&event) {
        eventQueues.gui.enqueue(std::move(event));
    }
};

} // namespace app

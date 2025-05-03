#pragma once

#include <ymir/ymir.hpp>

#include <app/ipl_rom_manager.hpp>
#include <app/profile.hpp>
#include <app/rewind_buffer.hpp>
#include <app/settings.hpp>

#include <app/input/input_context.hpp>

#include <app/debug/scu_tracer.hpp>
#include <app/debug/sh2_tracer.hpp>

#include <app/events/emu_event.hpp>
#include <app/events/gui_event.hpp>

#include <imgui.h>

#include <SDL3/SDL_render.h>

#include <blockingconcurrentqueue.h>

#include <filesystem>
#include <memory>
#include <mutex>

namespace app {

struct SharedContext {
    ymir::Saturn saturn;

    float displayScale = 1.0f;

    input::InputContext inputContext;

    struct StandardPadInput {
        ymir::peripheral::StandardPadButton buttons = ymir::peripheral::StandardPadButton::Default;

        struct DPad {
            float x, y;
        };
        std::unordered_map<input::InputElement, DPad> dpad2DInputs;

        void UpdateDPadInput(float sensitivity) {
            using Button = ymir::peripheral::StandardPadButton;

            // Aggregate all D-Pad inputs
            float x = 0.0f;
            float y = 0.0f;
            for (auto &[_, inputs] : dpad2DInputs) {
                x += inputs.x;
                y += inputs.y;
            }

            // Clamp to -1.0..1.0
            x = std::clamp(x, -1.0f, 1.0f);
            y = std::clamp(y, -1.0f, 1.0f);

            // Reset D-Pad button states
            buttons |= Button::Left | Button::Right | Button::Up | Button::Down;

            // Convert aggregate input into button presses
            const float distSq = x * x + y * y;
            if (distSq >= sensitivity * sensitivity && distSq > 0.0f) {
                // Constraint to one quadrant
                float ax = abs(x);
                float ay = abs(y);

                // Normalize vector
                const float dist = sqrt(distSq);
                ax /= dist;
                ay /= dist;

                // Dot product with normalized diagonal vector: (1/sqrt(2), 1/sqrt(2))
                // dot = x*1/sqrt(2) + y*1/sqrt(2)
                // dot = (x+y)/sqrt(2)
                static constexpr float kInvSqrt2 = 0.70710678f;
                const float dot = (ax + ay) * kInvSqrt2;

                // If dot product is above threshold, the vector is closer to the diagonal.
                // Otherwise, it is closer to orthogonal.
                static constexpr float kThreshold = 0.92387953f; // cos(45deg / 2)
                const bool diagonal = dot >= kThreshold;

                // Determine buttons to press
                const bool sx = signbit(x);
                const bool sy = signbit(y);
                const Button btnX = sx ? Button::Left : Button::Right;
                const Button btnY = sy ? Button::Up : Button::Down;

                // Press both buttons on diagonal, otherwise press the button of the longest orthogonal axis
                if (diagonal) {
                    buttons &= ~(btnX | btnY);
                } else if (ax >= ay) {
                    buttons &= ~btnX;
                } else {
                    buttons &= ~btnY;
                }
            }
        }
    };

    std::array<StandardPadInput, 2> standardPadInputs;

    Profile profile;
    Settings settings{*this};

    IPLROMManager iplRomManager;
    std::filesystem::path iplRomPath;

    std::array<std::unique_ptr<ymir::state::State>, 10> saveStates;
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

    struct Images {
        struct Image {
            SDL_Texture *texture = nullptr;
            ImVec2 size;
        };

        Image ymirLogo;
    } images;

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

#include "system_status_window.hpp"

using namespace satemu;

namespace app::ui {

inline constexpr float kWindowWidth = 350.0f;

SystemStatusWindow::SystemStatusWindow(SharedContext &context)
    : WindowBase(context) {

    m_windowConfig.name = "System status";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SystemStatusWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(kWindowWidth, 0), ImVec2(kWindowWidth, FLT_MAX));
}

void SystemStatusWindow::DrawContents() {
    ImGui::BeginGroup();

    ImGui::SeparatorText("Parameters");
    DrawParameters();

    ImGui::SeparatorText("State");
    DrawScreen();
    DrawRealTimeClock();
    DrawClocks();

    ImGui::SeparatorText("CD drive");
    DrawCDDrive();

    ImGui::SeparatorText("Cartridge");
    DrawCartridge();

    ImGui::SeparatorText("Peripherals");
    DrawPeripherals();

    ImGui::SeparatorText("Actions");
    DrawActions();

    ImGui::EndGroup();
}

void SystemStatusWindow::DrawParameters() {
    sys::ClockSpeed clockSpeed = m_context.saturn.GetClockSpeed();
    sys::VideoStandard videoStandard = m_context.saturn.GetVideoStandard();

    if (ImGui::BeginTable("sys_params", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Clock speed");
        }
        if (ImGui::TableNextColumn()) {
            if (ImGui::RadioButton("320 pixels", clockSpeed == sys::ClockSpeed::_320)) {
                // TODO: m_context.eventQueues.emulator.enqueue(EmuEvent::SetClockSpeed(sys::ClockSpeed::_320));
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("352 pixels", clockSpeed == sys::ClockSpeed::_352)) {
                // TODO: m_context.eventQueues.emulator.enqueue(EmuEvent::SetClockSpeed(sys::ClockSpeed::_352));
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Video standard");
        }
        if (ImGui::TableNextColumn()) {
            if (ImGui::RadioButton("NTSC", videoStandard == sys::VideoStandard::NTSC)) {
                // TODO: m_context.eventQueues.emulator.enqueue(EmuEvent::SetVideoStandard(sys::VideoStandard::NTSC));
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("PAL", videoStandard == sys::VideoStandard::PAL)) {
                // TODO: m_context.eventQueues.emulator.enqueue(EmuEvent::SetVideoStandard(sys::VideoStandard::PAL));
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Region");
        }
        if (ImGui::TableNextColumn()) {
            // TODO: uint8 areaCode = m_context.saturn.SMPC.GetAreaCode();
            // TODO: comboxbox with options
            if (ImGui::BeginCombo("##region", "North America")) {
                //   0x1: (J) Japan
                //   0x2: (T) Asia NTSC
                //   0x4: (U) North America
                //   0x5: (B) Central/South America NTSC
                //   0x6: (K) Korea
                //   0xA: (A) Asia PAL
                //   0xC: (E) Europe PAL
                //   0xD: (L) Central/South America PAL
                ImGui::Selectable("Japan");
                ImGui::Selectable("Asia NTSC");
                ImGui::Selectable("North America");
                ImGui::Selectable("Central/South America NTSC");
                ImGui::Selectable("Korea");
                ImGui::Selectable("Asia PAL");
                ImGui::Selectable("Europe PAL");
                ImGui::Selectable("Central/South America PAL");

                ImGui::EndCombo();
            }
        }

        ImGui::EndTable();
    }
}

void SystemStatusWindow::DrawClocks() {
    if (ImGui::BeginTable("sys_clocks", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Components");
        ImGui::TableSetupColumn("Clock");
        ImGui::TableHeadersRow();

        // TODO: calculate clocks from ratios

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SH-2, SCU and VDPs");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("28.63636 MHz");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SCU DSP");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("14.31818 MHz");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("Pixel clock");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("7.15909 MHz");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SCSP");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("22.57920 MHz");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("CD Block SH-1");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("20.00000 MHz");
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SMPC MCU");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("4.00000 MHz");
        }

        ImGui::EndTable();
    }
}

void SystemStatusWindow::DrawRealTimeClock() {
    // TODO: current date-time (editable)
    ImGui::TextUnformatted("Current date/time:");
    ImGui::SameLine();
    ImGui::TextUnformatted("01/02/2003 12:34:56 AM");
}

void SystemStatusWindow::DrawScreen() {
    // TODO: resolution
    // TODO: interlace mode
    ImGui::TextUnformatted("Resolution:");
    ImGui::SameLine();
    ImGui::TextUnformatted("352x224 progressive");
}

void SystemStatusWindow::DrawCDDrive() {
    if (ImGui::Button("Open tray")) {
        // TODO
    }
    ImGui::SameLine();
    if (ImGui::Button("Load disc")) {
        // TODO
    }
    ImGui::SameLine();
    if (ImGui::Button("Eject disc")) {
        // TODO
    }

    ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x);
    ImGui::TextUnformatted("Image from "
                           "D:\\mocked_path\\extremely_long_path\\file_name_that_is_really_long_to_purposefully_break_"
                           "the_window_layout (J).cue");
    ImGui::PopTextWrapPos();
    ImGui::TextUnformatted("Playing track 2 - CDDA - 2:31.67");
    ImGui::TextUnformatted("Frame address ");
    ImGui::SameLine(0, 0);
    ImGui::PushFont(m_context.fonts.monospace.medium.regular);
    ImGui::TextUnformatted("A1F63");
    ImGui::PopFont();
}

void SystemStatusWindow::DrawCartridge() {
    if (ImGui::Button("Insert...")) {
        // TODO: open cartridge selector
    }
    ImGui::SameLine();
    if (ImGui::Button("Eject")) {
        // TODO
    }
    ImGui::SameLine();
    // TODO: display dynamically according to loaded cartridge type
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("32 Mbit Backup RAM");
    if (ImGui::Button("Open backup manager")) {
        // TODO: open cartridge window (e.g. Action Replay codes, backup memory manager)
    }
    // e.g., for DRAM cartridges, show a button to open the memory viewer on its comtents
}

void SystemStatusWindow::DrawPeripherals() {
    // TODO: fill in dynamically
    if (ImGui::BeginTable("sys_peripherals", 3, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Port 1:");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Standard Saturn pad");
        }
        if (ImGui::TableNextColumn()) {
            if (ImGui::Button("Keybindings...##port_1")) {
                // TODO
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Port 2:");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Standard Saturn pad");
        }
        if (ImGui::TableNextColumn()) {
            if (ImGui::Button("Keybindings...##port_2")) {
                // TODO
            }
        }

        ImGui::EndTable();
    }
    // TODO: show connected peripherals on both ports
    // TODO: allow inserting/removing peripherals
    // TODO: button to open peripheral keybindings
}

void SystemStatusWindow::DrawActions() {
    if (ImGui::Button("Hard reset")) {
        // TODO
    }
    ImGui::SameLine();
    if (ImGui::Button("Soft reset")) {
        // TODO
    }
    ImGui::SameLine();
    if (ImGui::Button("Factory reset")) {
        // TODO
    }
}

} // namespace app::ui

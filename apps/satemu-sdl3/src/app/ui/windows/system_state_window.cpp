#include "system_state_window.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

using namespace satemu;

namespace app::ui {

inline constexpr float kWindowWidth = 350.0f;

SystemStateWindow::SystemStateWindow(SharedContext &context)
    : WindowBase(context) {

    m_windowConfig.name = "System state";
    m_windowConfig.flags = ImGuiWindowFlags_AlwaysAutoResize;
}

void SystemStateWindow::PrepareWindow() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(kWindowWidth, 0), ImVec2(kWindowWidth, FLT_MAX));
}

void SystemStateWindow::DrawContents() {
    ImGui::BeginGroup();

    ImGui::SeparatorText("Parameters");
    DrawParameters();

    ImGui::SeparatorText("State");
    DrawScreen();
    DrawRealTimeClock();
    DrawClocks();

    ImGui::SeparatorText("CD drive");
    DrawCDDrive();

    ImGui::SeparatorText("Backup memory");
    DrawBackupMemory();

    ImGui::SeparatorText("Cartridge");
    DrawCartridge();

    ImGui::SeparatorText("Peripherals");
    DrawPeripherals();

    ImGui::SeparatorText("Actions");
    DrawActions();

    ImGui::EndGroup();
}

void SystemStateWindow::DrawParameters() {
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
                m_context.EnqueueEvent(events::emu::SetClockSpeed(sys::ClockSpeed::_320));
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("352 pixels", clockSpeed == sys::ClockSpeed::_352)) {
                m_context.EnqueueEvent(events::emu::SetClockSpeed(sys::ClockSpeed::_352));
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Video standard");
        }
        if (ImGui::TableNextColumn()) {
            if (ImGui::RadioButton("NTSC", videoStandard == sys::VideoStandard::NTSC)) {
                m_context.EnqueueEvent(events::emu::SetVideoStandard(sys::VideoStandard::NTSC));
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("PAL", videoStandard == sys::VideoStandard::PAL)) {
                m_context.EnqueueEvent(events::emu::SetVideoStandard(sys::VideoStandard::PAL));
            }
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Region");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::BeginItemTooltip()) {
                ImGui::TextUnformatted("Changing this option will cause a hard reset");
                ImGui::EndTooltip();
            }
        }
        if (ImGui::TableNextColumn()) {
            static constexpr struct {
                uint8 charCode;
                const char *name;
            } kRegions[] = {
                {/*0x0*/ '?', "Invalid"},       {/*0x1*/ 'J', "Japan"},
                {/*0x2*/ 'T', "Asia NTSC"},     {/*0x3*/ '?', "Invalid"},
                {/*0x4*/ 'U', "North America"}, {/*0x5*/ 'B', "Central/South America NTSC"},
                {/*0x6*/ 'K', "Korea"},         {/*0x7*/ '?', "Invalid"},
                {/*0x8*/ '?', "Invalid"},       {/*0x9*/ '?', "Invalid"},
                {/*0xA*/ 'A', "Asia PAL"},      {/*0xB*/ '?', "Invalid"},
                {/*0xC*/ 'E', "Europe PAL"},    {/*0xD*/ 'L', "Central/South America PAL"},
                {/*0xE*/ '?', "Invalid"},       {/*0xF*/ '?', "Invalid"},
            };

            auto fmtRegion = [](uint8 index) {
                index &= 0xF;
                return fmt::format("({:c}) {}", kRegions[index].charCode, kRegions[index].name);
            };

            uint8 areaCode = m_context.saturn.SMPC.GetAreaCode();
            if (ImGui::BeginCombo("##region", fmtRegion(areaCode).c_str(),
                                  ImGuiComboFlags_WidthFitPreview | ImGuiComboFlags_HeightLargest)) {
                for (uint8 i = 0; i <= 0xF; i++) {
                    if (kRegions[i].charCode == '?') {
                        continue;
                    }
                    if (ImGui::Selectable(fmtRegion(i).c_str(), i == areaCode) && i != areaCode) {
                        m_context.EnqueueEvent(events::emu::SetAreaCode(i));
                        // TODO: optional?
                        m_context.EnqueueEvent(events::emu::HardReset());
                    }
                }

                ImGui::EndCombo();
            }
        }

        ImGui::EndTable();
    }
}

void SystemStateWindow::DrawScreen() {
    auto &probe = m_context.saturn.VDP.GetProbe();
    auto [width, height] = probe.GetResolution();
    auto interlaceMode = probe.GetInterlaceMode();

    static constexpr const char *kInterlaceNames[]{"progressive", "(invalid)", "single-density interlace",
                                                   "double-density interlace"};

    ImGui::TextUnformatted("Resolution:");
    ImGui::SameLine();
    ImGui::Text("%ux%u %s", width, height, kInterlaceNames[static_cast<uint8>(interlaceMode)]);
}

void SystemStateWindow::DrawRealTimeClock() {
    const auto dt = m_context.saturn.SMPC.GetProbe().GetRTCDateTime();

    static constexpr const char *kWeekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

    ImGui::Text("Current date/time: %s %04u/%02u/%02u %02u:%02u:%02u", kWeekdays[dt.weekday], dt.year, dt.month, dt.day,
                dt.hour, dt.minute, dt.second);
    // TODO: make it adjustable, sync to host
}

void SystemStateWindow::DrawClocks() {
    if (ImGui::BeginTable("sys_clocks", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Components");
        ImGui::TableSetupColumn("Clock");
        ImGui::TableHeadersRow();

        const sys::ClockRatios &clockRatios = m_context.saturn.GetClockRatios();

        const double masterClock =
            (double)clockRatios.masterClock * clockRatios.masterClockNum / clockRatios.masterClockDen / 1000000.0;
        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SH-2, SCU and VDPs");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::Text("%.5lf MHz", masterClock);
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SCU DSP");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::Text("%.5lf MHz", masterClock * 0.5);
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("Pixel clock");
        }
        if (ImGui::TableNextColumn()) {
            // Account for double-resolution
            auto &probe = m_context.saturn.VDP.GetProbe();
            auto resolution = probe.GetResolution();
            const double factor = resolution.width >= 640 ? 0.5 : 0.25;
            ImGui::Text("%.5lf MHz", masterClock * factor);
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SCSP");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::Text("%.5lf MHz", masterClock * clockRatios.SCSPNum / clockRatios.SCSPDen);
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("MC68EC000");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::Text("%.5lf MHz", masterClock * clockRatios.SCSPNum / clockRatios.SCSPDen * 0.5);
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("CD Block SH-1");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::Text("%.5lf MHz", masterClock * clockRatios.CDBlockNum / clockRatios.CDBlockDen);
        }

        ImGui::TableNextRow();
        if (ImGui::TableNextColumn()) {
            ImGui::TextUnformatted("SMPC MCU");
        }
        if (ImGui::TableNextColumn()) {
            ImGui::Text("%.5lf MHz", masterClock * clockRatios.SMPCNum / clockRatios.SMPCDen);
        }

        ImGui::EndTable();
    }
}

void SystemStateWindow::DrawCDDrive() {
    auto &probe = m_context.saturn.CDBlock.GetProbe();

    const uint8 status = probe.GetCurrentStatusCode();

    if (ImGui::Button(m_context.saturn.CDBlock.IsTrayOpen() ? "Close tray" : "Open tray")) {
        m_context.EnqueueEvent(events::emu::OpenCloseTray());
    }
    ImGui::SameLine();
    if (ImGui::Button("Load disc...")) {
        m_context.EnqueueEvent(events::gui::LoadDisc());
    }
    ImGui::SameLine();
    if (ImGui::Button("Eject disc")) {
        m_context.EnqueueEvent(events::emu::EjectDisc());
    }

    ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x);
    if (m_context.state.loadedDiscImagePath.empty()) {
        ImGui::TextUnformatted("No image loaded");
    } else {
        ImGui::Text("Image from %s", m_context.state.loadedDiscImagePath.string().c_str());
    }
    ImGui::PopTextWrapPos();
    switch (status) {
    case cdblock::kStatusCodeBusy: ImGui::TextUnformatted("Busy"); break;
    case cdblock::kStatusCodePause: ImGui::TextUnformatted("Paused"); break;
    case cdblock::kStatusCodeStandby: ImGui::TextUnformatted("Standby"); break;
    case cdblock::kStatusCodePlay:
        ImGui::Text("Playing track %u, index %u (%s)", probe.GetCurrentTrack(), probe.GetCurrentIndex(),
                    (probe.GetCurrentControlADRBits() == 0x01 ? "CDDA" : "Data"));
        break;
    case cdblock::kStatusCodeSeek: ImGui::TextUnformatted("Seeking"); break;
    case cdblock::kStatusCodeScan:
        ImGui::Text("Scanning track %u, index %u (%s)", probe.GetCurrentTrack(), probe.GetCurrentIndex(),
                    (probe.GetCurrentControlADRBits() == 0x01 ? "CDDA" : "Data"));
        break;
    case cdblock::kStatusCodeOpen: ImGui::TextUnformatted("Tray open"); break;
    case cdblock::kStatusCodeNoDisc: ImGui::TextUnformatted("No disc"); break;
    case cdblock::kStatusCodeRetry: ImGui::TextUnformatted("Retrying"); break;
    case cdblock::kStatusCodeError: ImGui::TextUnformatted("Error"); break;
    case cdblock::kStatusCodeFatal: ImGui::TextUnformatted("Fatal error"); break;
    }

    ImGui::Text("Read speed: %ux", probe.GetReadSpeed());

    const uint32 fad = probe.GetCurrentFrameAddress();
    const uint8 repeat = probe.GetCurrentRepeatCount();
    const uint8 maxRepeat = probe.GetMaxRepeatCount();
    const cdblock::MSF msf = cdblock::FADToMSF(fad);

    if (status == cdblock::kStatusCodePlay || status == cdblock::kStatusCodeScan) {
        ImGui::BeginGroup();
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        if (msf.m == 0) {
            ImGui::TextDisabled("00");
        } else {
            if (msf.m < 10) {
                ImGui::TextDisabled("0");
                ImGui::SameLine(0, 0);
            }
            ImGui::Text("%u", msf.m);
        }
        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(":");
        ImGui::SameLine(0, 0);
        if (msf.m == 0 && msf.s == 0) {
            ImGui::TextDisabled("00");
        } else {
            if (msf.m == 0 && msf.s < 10) {
                ImGui::TextDisabled("0");
                ImGui::SameLine(0, 0);
                ImGui::Text("%u", msf.s);
            } else {
                ImGui::Text("%02u", msf.s);
            }
        }
        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(".");
        ImGui::SameLine(0, 0);
        if (msf.m == 0 && msf.s == 0 && msf.f == 0) {
            ImGui::TextDisabled("00");
        } else {
            if (msf.m == 0 && msf.s == 0 && msf.f < 10) {
                ImGui::TextDisabled("0");
                ImGui::SameLine(0, 0);
                ImGui::Text("%u", msf.f);
            } else {
                ImGui::Text("%02u", msf.f);
            }
        }
        ImGui::PopFont();
        ImGui::EndGroup();
        ImGui::SetItemTooltip("MM:SS.FF\nMinutes, seconds and frames\n(75 frames per second)");

        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(" :: ");
        ImGui::SameLine(0, 0);

        ImGui::BeginGroup();
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        const uint32 numZeros = std::countl_zero(fad) / 4 - 2; // FAD is 24 bits
        ImGui::TextDisabled("%0*u", numZeros, 0);
        ImGui::SameLine(0, 0);
        ImGui::Text("%X", fad);
        ImGui::PopFont();
        ImGui::EndGroup();
        ImGui::SetItemTooltip("Frame address (FAD)");
    } else {
        ImGui::BeginGroup();
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        ImGui::TextDisabled("--");
        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(":");
        ImGui::SameLine(0, 0);
        ImGui::TextDisabled("--");
        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(".");
        ImGui::SameLine(0, 0);
        ImGui::TextDisabled("--");
        ImGui::PopFont();
        ImGui::EndGroup();
        ImGui::SetItemTooltip("MM:SS.FF\nMinutes, seconds and frames\n(75 frames per second)");

        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(" :: ");
        ImGui::SameLine(0, 0);

        ImGui::BeginGroup();
        ImGui::PushFont(m_context.fonts.monospace.medium.regular);
        ImGui::TextDisabled("------");
        ImGui::PopFont();
        ImGui::EndGroup();
        ImGui::SetItemTooltip("Frame address (FAD)");
    }

    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(" :: ");
    ImGui::SameLine(0, 0);

    if (maxRepeat == 0xF) {
        ImGui::TextUnformatted("Repeat forever");
    } else if (maxRepeat > 0) {
        ImGui::Text("Repeat %u of %u", repeat, maxRepeat);
    } else {
        ImGui::TextUnformatted("No repeat");
    }
}

void SystemStateWindow::DrawBackupMemory() {
    if (ImGui::BeginTable("bup_info", 3, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Device");
        ImGui::TableSetupColumn("Capacity");
        ImGui::TableSetupColumn("Blocks used");
        ImGui::TableHeadersRow();

        auto drawBup = [&](const char *name, bup::IBackupMemory *bup) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::TextUnformatted(name);
            }
            if (bup) {
                if (ImGui::TableNextColumn()) {
                    ImGui::Text("%u KiB", bup->Size() / 1024u);
                }
                if (ImGui::TableNextColumn()) {
                    if (bup->IsHeaderValid()) {
                        ImGui::Text("%u of %u", bup->GetUsedBlocks(), bup->GetTotalBlocks());
                    } else {
                        ImGui::TextUnformatted("Invalid");
                    }
                }
            } else {
                if (ImGui::TableNextColumn()) {
                    ImGui::TextUnformatted("-");
                }
                if (ImGui::TableNextColumn()) {
                    ImGui::TextUnformatted("-");
                }
            }
        };

        drawBup("Internal", &m_context.saturn.mem.GetInternalBackupRAM());
        {
            std::unique_lock lock{m_context.locks.cart};
            auto *bupCart = cart::As<cart::CartType::BackupMemory>(&m_context.saturn.GetCartridge());
            if (bupCart != nullptr) {
                drawBup("External", &bupCart->GetBackupMemory());
            } else {
                drawBup("External", nullptr);
            }
        }

        ImGui::EndTable();
    }

    if (ImGui::Button("Open backup memory manager")) {
        // TODO: open backup memory manager
    }
}

void SystemStateWindow::DrawCartridge() {
    ImGui::Button("Insert...");
    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
        if (ImGui::MenuItem("Backup RAM")) {
            // TODO: open popup to select backup image
            // m_context.EnqueueEvent(events::gui::OpenBackupMemoryCartFileDialog());
        }
        if (ImGui::MenuItem("8 Mbit DRAM")) {
            // TODO: insert cartridge
            // m_context.EnqueueEvent(events::emu::Insert8MbitDRAMCartridge());
        }
        if (ImGui::MenuItem("32 Mbit DRAM")) {
            // TODO: insert cartridge
            // m_context.EnqueueEvent(events::emu::Insert32MbitDRAMCartridge());
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Eject")) {
        m_context.EnqueueEvent(events::emu::EjectCartridge());
    }
    ImGui::SameLine();

    std::unique_lock lock{m_context.locks.cart};
    auto &cart = m_context.saturn.GetCartridge();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("[ID %02X] ", cart.GetID());
    ImGui::SameLine(0, 0);
    switch (cart.GetType()) {
    case cart::CartType::None: ImGui::TextUnformatted("No cartridge inserted"); break;
    case cart::CartType::BackupMemory: //
    {
        auto &bupCart = *cart::As<cart::CartType::BackupMemory>(&cart);
        ImGui::Text("%u Mbit Backup RAM", bupCart.GetBackupMemory().Size() * 8u / 1024u / 1024u);
        break;
    }
    case cart::CartType::DRAM8Mbit: ImGui::TextUnformatted("8 Mbit DRAM"); break;
    case cart::CartType::DRAM32Mbit: ImGui::TextUnformatted("32 Mbit DRAM"); break;
    }
}

void SystemStateWindow::DrawPeripherals() {
    // TODO: fill in dynamically
    // TODO: allow inserting/removing peripherals
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
            if (ImGui::Button("Configure...##port_1")) {
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
            if (ImGui::Button("Configure...##port_2")) {
                // TODO
            }
        }

        ImGui::EndTable();
    }
}

void SystemStateWindow::DrawActions() {
    if (ImGui::Button("Hard reset")) {
        m_context.EnqueueEvent(events::emu::HardReset());
    }
    ImGui::SameLine();
    if (ImGui::Button("Soft reset")) {
        m_context.EnqueueEvent(events::emu::SoftReset());
    }
    // TODO: Let's not make it that easy to accidentally wipe system settings
    /*ImGui::SameLine();
    if (ImGui::Button("Factory reset")) {
        m_context.EnqueueEvent(events::emu::FactoryReset());
    }*/
}

} // namespace app::ui

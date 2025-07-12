#include "general_settings_view.hpp"

#include <app/events/emu_event_factory.hpp>
#include <app/events/gui_event_factory.hpp>

#include <app/ui/widgets/common_widgets.hpp>

#include <misc/cpp/imgui_stdlib.h>

#include <util/sdl_file_dialog.hpp>

#include <fmt/std.h>

namespace app::ui {

GeneralSettingsView::GeneralSettingsView(SharedContext &context)
    : SettingsViewBase(context) {}

void GeneralSettingsView::Display() {
    auto &settings = m_context.settings.general;
    auto &profile = m_context.profile;

    const float paddingWidth = ImGui::GetStyle().FramePadding.x;
    const float itemSpacingWidth = ImGui::GetStyle().ItemSpacing.x;
    const float fileSelectorButtonWidth = ImGui::CalcTextSize("...").x + paddingWidth * 2;
    const float clearButtonWidth = ImGui::CalcTextSize("Clear").x + paddingWidth * 2;

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Performance");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Boost process priority", &settings.boostProcessPriority))) {
        m_context.EnqueueEvent(events::gui::SetProcessPriority(settings.boostProcessPriority));
    }
    widgets::ExplanationTooltip("Increases the process's priority level, which may help reduce stuttering.",
                                m_context.displayScale);

    if (MakeDirty(ImGui::Checkbox("Boost emulator thread priority", &settings.boostEmuThreadPriority))) {
        m_context.EnqueueEvent(events::emu::SetThreadPriority(settings.boostEmuThreadPriority));
    }
    widgets::ExplanationTooltip("Increases the emulator thread's priority, which may help reduce jitter.",
                                m_context.displayScale);

    MakeDirty(ImGui::Checkbox("Preload disc images to RAM", &settings.preloadDiscImagesToRAM));
    widgets::ExplanationTooltip(
        "Preloads the entire disc image to memory.\n"
        "May help reduce stuttering if you're loading images from a slow disk or from the network.",
        m_context.displayScale);

    MakeDirty(ImGui::Checkbox("Show frame rate OSD", &settings.showFrameRateOSD));
    ImGui::Indent();
    auto osdOption = [&](const char *name, Settings::General::FrameRateOSDPosition value) {
        if (MakeDirty(ImGui::RadioButton(name, settings.frameRateOSDPosition == value))) {
            settings.frameRateOSDPosition = value;
        }
    };
    osdOption("Top left##fps_osd", Settings::General::FrameRateOSDPosition::TopLeft);
    ImGui::SameLine();
    osdOption("Top right##fps_osd", Settings::General::FrameRateOSDPosition::TopRight);
    ImGui::SameLine();
    osdOption("Bottom left##fps_osd", Settings::General::FrameRateOSDPosition::BottomLeft);
    ImGui::SameLine();
    osdOption("Bottom right##fps_osd", Settings::General::FrameRateOSDPosition::BottomRight);
    ImGui::Unindent();

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Behavior");
    ImGui::PopFont();

    MakeDirty(ImGui::Checkbox("Pause when unfocused", &settings.pauseWhenUnfocused));
    widgets::ExplanationTooltip(
        "The emulator will pause when the window loses focus and resume when it regains focus.\n"
        "Does not affect the behavior of manual pauses - they persist through focus changes.",
        m_context.displayScale);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Rewind buffer");
    ImGui::PopFont();

    if (MakeDirty(ImGui::Checkbox("Enable rewind buffer", &settings.enableRewindBuffer))) {
        m_context.EnqueueEvent(events::gui::EnableRewindBuffer(settings.enableRewindBuffer));
    }
    widgets::ExplanationTooltip("Allows you to step back in time.\n"
                                "Increases memory usage and slightly reduces performance.",
                                m_context.displayScale);

    // TODO: rewind buffer size

    if (MakeDirty(ImGui::SliderInt("Compression level", &settings.rewindCompressionLevel, 0, 16, "%d",
                                   ImGuiSliderFlags_AlwaysClamp))) {
        m_context.rewindBuffer.LZ4Accel = 1 << (16 - settings.rewindCompressionLevel);
    }

    widgets::ExplanationTooltip("Adjust compression ratio vs. speed.\n"
                                "Higher values improve compression ratio, reducing memory usage.\n"
                                "Lower values increase compression speed and improve emulation performance.\n",
                                m_context.displayScale);

    // -----------------------------------------------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fonts.sizes.large);
    ImGui::SeparatorText("Profile paths");
    ImGui::PopFont();

    ImGui::TextUnformatted("Override profile paths");

    if (ImGui::BeginTable("path_overrides", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto drawRow = [&](const char *name, ProfilePath profPath) {
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name);
            }
            if (ImGui::TableNextColumn()) {
                std::string label = fmt::format("##prof_path_override_{}", static_cast<uint32>(profPath));
                std::string imagePath = fmt::format("{}", profile.GetPathOverride(profPath));
                std::string currPath = fmt::format("{}", profile.GetPath(profPath));

                ImGui::SetNextItemWidth(-(fileSelectorButtonWidth + clearButtonWidth + itemSpacingWidth * 2));
                if (MakeDirty(ImGui::InputTextWithHint(label.c_str(), currPath.c_str(), &imagePath,
                                                       ImGuiInputTextFlags_ElideLeft))) {
                    profile.SetPathOverride(profPath, std::u8string{imagePath.begin(), imagePath.end()});
                }
                ImGui::SameLine();
                if (ImGui::Button(fmt::format("...{}", label).c_str())) {
                    m_selectedProfPath = profPath;
                    m_context.EnqueueEvent(events::gui::SelectFolder({
                        .dialogTitle = fmt::format("Select {} directory", name),
                        .defaultPath = m_context.profile.GetPath(profPath),
                        .userdata = this,
                        .callback =
                            util::WrapSingleSelectionCallback<&GeneralSettingsView::ProcessPathOverrideSelection,
                                                              &util::NoopCancelFileDialogCallback,
                                                              &GeneralSettingsView::ProcessPathOverrideSelectionError>,
                    }));
                }
                ImGui::SameLine();
                if (ImGui::Button(fmt::format("Clear{}", label).c_str())) {
                    profile.ClearOverridePath(profPath);
                }
            }
        };

        drawRow("IPL ROM images", ProfilePath::IPLROMImages);
        drawRow("Cartridge ROM images", ProfilePath::ROMCartImages);
        drawRow("Backup memory", ProfilePath::BackupMemory);
        drawRow("Exported backup files", ProfilePath::ExportedBackups);
        drawRow("Persistent state", ProfilePath::PersistentState);
        drawRow("Save states", ProfilePath::SaveStates);
        drawRow("Dumps", ProfilePath::Dumps);

        ImGui::EndTable();
    }
}

void GeneralSettingsView::ProcessPathOverrideSelection(void *userdata, std::filesystem::path file, int filter) {
    static_cast<GeneralSettingsView *>(userdata)->SelectPathOverride(file);
}

void GeneralSettingsView::ProcessPathOverrideSelectionError(void *userdata, const char *message, int filter) {
    static_cast<GeneralSettingsView *>(userdata)->ShowPathOverrideSelectionError(message);
}

void GeneralSettingsView::SelectPathOverride(std::filesystem::path file) {
    if (std::filesystem::is_directory(file)) {
        m_context.profile.SetPathOverride(m_selectedProfPath, file);
        MakeDirty();
    }
}

void GeneralSettingsView::ShowPathOverrideSelectionError(const char *message) {
    m_context.EnqueueEvent(events::gui::ShowError(fmt::format("Could not open directory: {}", message)));
}

} // namespace app::ui

#include "settings_widgets.hpp"

#include "common_widgets.hpp"

#include <app/events/emu_event_factory.hpp>

#include <fmt/format.h>

namespace app::ui::widgets {

namespace settings::system {

    void EmulateSH2Cache(SharedContext &ctx) {
        auto &sysConfig = ctx.saturn.configuration.system;
        bool emulateSH2Cache = sysConfig.emulateSH2Cache;
        if (ctx.settings.MakeDirty(ImGui::Checkbox("Emulate SH-2 cache", &emulateSH2Cache))) {
            ctx.EnqueueEvent(events::emu::SetEmulateSH2Cache(emulateSH2Cache));
        }
        widgets::ExplanationTooltip("Enables emulation of the SH-2 cache.\n"
                                    "A few games require this to work properly.\n"
                                    "Reduces emulation performance by about 10%.\n\n"
                                    "Upon enabling this option, both SH-2 CPUs' caches will be flushed.",
                                    ctx.displayScale);
    }

} // namespace settings::system

namespace settings::video {

    void Deinterlace(SharedContext &ctx) {
        auto &settings = ctx.settings.video;
        bool deinterlace = settings.deinterlace.Get();
        if (ctx.settings.MakeDirty(ImGui::Checkbox("Deinterlace video", &deinterlace))) {
            settings.deinterlace = deinterlace;
        }
        widgets::ExplanationTooltip(
            "When enabled, high-resolution modes will be rendered in progressive mode instead of interlaced.\n"
            "Significantly impacts performance in those modes when enabled.",
            ctx.displayScale);
    }

    void TransparentMeshes(SharedContext &ctx) {
        auto &settings = ctx.settings.video;
        bool transparentMeshes = settings.transparentMeshes.Get();
        if (ctx.settings.MakeDirty(ImGui::Checkbox("Transparent meshes", &transparentMeshes))) {
            settings.transparentMeshes = transparentMeshes;
        }
        widgets::ExplanationTooltip(
            "When enabled, meshes (checkerboard patterns) will be rendered as transparent polygons instead.",
            ctx.displayScale);
    }

    void ThreadedVDP(SharedContext &ctx) {
        auto &config = ctx.saturn.configuration.video;
        bool threadedVDP = config.threadedVDP;
        if (ctx.settings.MakeDirty(ImGui::Checkbox("Threaded VDP2 renderer", &threadedVDP))) {
            ctx.EnqueueEvent(events::emu::EnableThreadedVDP(threadedVDP));
        }
        widgets::ExplanationTooltip(
            "Runs the software VDP2 renderer in a dedicated thread.\n"
            "Greatly improves performance and seems to cause no issues to games.\n"
            "When disabled, VDP2 rendering is done on the emulator thread.\n"
            "\n"
            "It is HIGHLY recommended to leave this option enabled as there are no known drawbacks.",
            ctx.displayScale);

        if (!threadedVDP) {
            ImGui::BeginDisabled();
        }
        ImGui::Indent();
        bool includeVDP1InRenderThread = config.includeVDP1InRenderThread;
        if (ctx.settings.MakeDirty(
                ImGui::Checkbox("Include VDP1 rendering in VDP2 renderer thread", &includeVDP1InRenderThread))) {
            ctx.EnqueueEvent(events::emu::IncludeVDP1InVDPRenderThread(includeVDP1InRenderThread));
        }
        widgets::ExplanationTooltip(
            "If VDP2 rendering is running on a dedicated thread, move the software VDP1 renderer to that thread.\n"
            "Improves performance by about 10% at the cost of accuracy.\n"
            "A few select games may freeze or refuse to start when this option is enabled.\n"
            "When this option or Threaded VDP2 renderer is disabled, VDP1 rendering is done on the emulator thread.\n"
            "\n"
            "Try enabling this option if you need to squeeze a bit more performance.",
            ctx.displayScale);
        ImGui::Unindent();
        if (!threadedVDP) {
            ImGui::EndDisabled();
        }
    }

} // namespace settings::video

namespace settings::audio {

    void InterpolationMode(SharedContext &ctx) {
        auto &config = ctx.saturn.configuration.audio;

        using InterpMode = ymir::core::config::audio::SampleInterpolationMode;

        auto interpOption = [&](const char *name, InterpMode mode) {
            const std::string label = fmt::format("{}##sample_interp", name);
            ImGui::SameLine();
            if (ctx.settings.MakeDirty(ImGui::RadioButton(label.c_str(), config.interpolation == mode))) {
                config.interpolation = mode;
            }
        };

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Interpolation:");
        widgets::ExplanationTooltip("- Nearest neighbor: Cheapest option with grittier sounds.\n"
                                    "- Linear: Hardware accurate option with softer sounds. (default)",
                                    ctx.displayScale);
        interpOption("Nearest neighbor", InterpMode::NearestNeighbor);
        interpOption("Linear", InterpMode::Linear);
    }

    std::string StepGranularityToString(uint32 stepGranularity) {
        const uint32 numSteps = 32u >> stepGranularity;
        return fmt::format("{} {}{}", numSteps, (numSteps != 1 ? "slots" : "slot"),
                           (numSteps == 32 ? " (1 sample)" : ""));
    }

    void StepGranularity(SharedContext &ctx) {
        auto &settings = ctx.settings.audio;
        int stepGranularity = settings.stepGranularity;

        if (ImGui::BeginTable("scsp_step_granularity", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Emulation step granularity");
                widgets::ExplanationTooltip(
                    "WARNING: This setting is very performance-hungry!\n"
                    "\n"
                    "Increasing this setting causes the SCSP to be emulated in smaller timeslices (up to 32 times as "
                    "often as sample-level processing), significantly lowering performance in exchange for a higher "
                    "level of accuracy that doesn't benefit the vast majority of commercial games.\n"
                    "\n"
                    "This option might be of interest to homebrew developers who need extra accuracy in some way.",
                    ctx.displayScale);
            }
            if (ImGui::TableNextColumn()) {
                ImGui::SetNextItemWidth(-1.0f);
                if (ctx.settings.MakeDirty(ImGui::SliderInt("##scsp_step_granularity", &stepGranularity, 0, 5, "%d",
                                                            ImGuiSliderFlags_AlwaysClamp))) {
                    settings.stepGranularity = stepGranularity;
                }
            }
            ImGui::TableNextRow();
            if (ImGui::TableNextColumn()) {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Step size: %s", StepGranularityToString(stepGranularity).c_str());
            }
            if (ImGui::TableNextColumn()) {
                static constexpr ImU32 kGraphBackgroundColor = 0xAA253840;
                static constexpr ImU32 kGraphSliceFillColor = 0xE04AC3F7;
                static constexpr ImU32 kGraphSliceFillColorAlt = 0xE02193C4;
                static constexpr ImU32 kGraphSlotSeparatorColor = 0xE02A6F8C;

                const auto initBasePos = ImGui::GetCursorScreenPos();
                const auto totalAvail = ImGui::GetContentRegionAvail();
                const auto cellPadding = ImGui::GetStyle().CellPadding;
                const auto basePos = ImVec2(initBasePos.x, initBasePos.y + cellPadding.y);
                const auto avail = ImVec2(totalAvail.x, totalAvail.y - cellPadding.y * 2.0f);
                const float graphWidth = avail.x;
                const float graphHeight = ImGui::GetFrameHeight();
                const float sliceWidth = graphWidth / (1 << stepGranularity);
                const float slotWidth = graphWidth / 32.0f;
                const float sepThickness = 1.5f * ctx.displayScale;

                auto *drawList = ImGui::GetWindowDrawList();

                ImGui::Dummy(ImVec2(graphWidth, graphHeight));
                drawList->AddRectFilled(basePos, ImVec2(basePos.x + graphWidth, basePos.y + graphHeight),
                                        kGraphBackgroundColor);
                for (uint32 i = 0; i < (1 << stepGranularity); ++i) {
                    const float xStart = basePos.x + i * sliceWidth;
                    const float xEnd = xStart + sliceWidth;
                    drawList->AddRectFilled(ImVec2(xStart, basePos.y), ImVec2(xEnd, basePos.y + graphHeight),
                                            (i & 1) ? kGraphSliceFillColorAlt : kGraphSliceFillColor);
                }
                for (uint32 i = 1; i < 32; ++i) {
                    const float x = basePos.x + i * slotWidth;
                    drawList->AddLine(ImVec2(x, basePos.y), ImVec2(x, basePos.y + graphHeight),
                                      kGraphSlotSeparatorColor, sepThickness);
                }
            }

            ImGui::EndTable();
        }
    }

} // namespace settings::audio

namespace settings::cdblock {

    void CDReadSpeed(SharedContext &ctx) {
        auto &config = ctx.saturn.configuration.cdblock;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("CD read speed");
        widgets::ExplanationTooltip("Changes the maximum read speed of the emulated CD drive.\n"
                                    "The default value is 2x, matching the real Saturn's CD drive speed.\n"
                                    "Higher speeds decrease load times but may reduce compatibility.",
                                    ctx.displayScale);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        static constexpr uint8 kMinReadSpeed = 2u;
        static constexpr uint8 kMaxReadSpeed = 200u;
        uint8 readSpeed = config.readSpeedFactor;
        if (ctx.settings.MakeDirty(ImGui::SliderScalar("##read_speed", ImGuiDataType_U8, &readSpeed, &kMinReadSpeed,
                                                       &kMaxReadSpeed, "%ux", ImGuiSliderFlags_AlwaysClamp))) {
            config.readSpeedFactor = readSpeed;
        }
    }

} // namespace settings::cdblock

} // namespace app::ui::widgets

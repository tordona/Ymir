#pragma once

#include <app/shared_context.hpp>

namespace app::ui::widgets {

namespace settings::system {

    void EmulateSH2Cache(SharedContext &ctx);

} // namespace settings::system

namespace settings::video {

    void Deinterlace(SharedContext &ctx);
    void TransparentMeshes(SharedContext &ctx);
    void ThreadedVDP(SharedContext &ctx);

} // namespace settings::video

namespace settings::audio {

    void InterpolationMode(SharedContext &ctx);
    void StepGranularity(SharedContext &ctx);

    std::string StepGranularityToString(uint32 stepGranularity);

} // namespace settings::audio

namespace settings::cdblock {

    void CDReadSpeed(SharedContext &ctx);

}

} // namespace app::ui::widgets

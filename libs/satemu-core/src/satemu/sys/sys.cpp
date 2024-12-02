#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(sysVideo.VDP1, sysVideo.VDP2, SCSP, CDBlock, SH2)
    , SMPC(SCU)
    , sysVideo(SCU) {

    SCU.PostConstructInit();

    Reset(true);
}

void Saturn::Reset(bool hard) {
    SH2.Reset(hard);
    SCU.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);

    sysVideo.Reset(hard);
}

void Saturn::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    SH2.LoadIPL(ipl);
}

void Saturn::Step() {
    SH2.Step();

    // TODO: proper timings
    // TODO: replace with scheduler events
    sysVideo.Advance(1);
}

} // namespace satemu

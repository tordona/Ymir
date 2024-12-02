#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(VDP1, VDP2, SCSP, CDBlock, SH2)
    , VDP2(SCU)
    , SMPC(SCU) {

    SCU.PostConstructInit();

    Reset(true);
}

void Saturn::Reset(bool hard) {
    SH2.Reset(hard);
    SCU.Reset(hard);
    VDP1.Reset(hard);
    VDP2.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);
}

void Saturn::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    SH2.LoadIPL(ipl);
}

void Saturn::Step() {
    SH2.Step();

    // TODO: proper timings
    // TODO: replace with scheduler events
    VDP2.Advance(1);
}

} // namespace satemu

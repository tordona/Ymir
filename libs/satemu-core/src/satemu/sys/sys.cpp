#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(VDP1, VDP2, SCSP, CDBlock, SH2.master)
    , VDP2(SCU)
    , SMPC(SCU, SCSP)
    , SCSP(SCU)
    , CDBlock(SCU) {

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

void Saturn::LoadIPL(std::span<uint8, sh2::kIPLSize> ipl) {
    SH2.bus.LoadIPL(ipl);
}

void Saturn::Step() {
    SH2.master.Step();
    // TODO: step slave SH2 if enabled
    // SH2.slave.Step();

    // TODO: proper timings
    // TODO: replace with scheduler events
    VDP2.Advance(1);

    // TODO: proper timings
    SCSP.Advance(1);
}

} // namespace satemu

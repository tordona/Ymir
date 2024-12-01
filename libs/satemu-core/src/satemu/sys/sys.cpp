#include <satemu/sys/sys.hpp>

namespace satemu {

// TODO: remove these reference-passing constructors
Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(VDP1, VDP2, SCSP, CDBlock)
    , sysSH2(SH2)
    , sysVideo(VDP1, VDP2, SCU, SH2) {
    Reset(true);
}

void Saturn::Reset(bool hard) {
    SCU.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);

    sysSH2.Reset(hard);
    sysVideo.Reset(hard);
}

void Saturn::Step() {
    sysSH2.Step();

    // TODO: proper timings
    // TODO: replace with scheduler events
    sysVideo.Advance(1);
}

} // namespace satemu

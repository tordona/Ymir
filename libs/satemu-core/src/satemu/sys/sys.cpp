#include <satemu/sys/sys.hpp>

namespace satemu {

// TODO: remove these reference-passing constructors
Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(VDP1, VDP2, SCSP, CDBlock)
    , VDP2(SCU)
    , sysSH2(SH2) {
    Reset(true);
}

void Saturn::Reset(bool hard) {
    SCU.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);
    VDP1.Reset(hard);
    VDP2.Reset(hard);

    sysSH2.Reset(hard);
}

void Saturn::Step() {
    sysSH2.Step();

    // TODO: proper timings
    // TODO: remove when using a scheduler
    VDP2.Advance(1);
}

} // namespace satemu

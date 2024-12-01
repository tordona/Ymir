#include <satemu/sys/sys.hpp>

namespace satemu {

// TODO: remove these reference-passing constructors
Saturn::Saturn()
    : sh2bus(SCU, SMPC)
    , masterSH2(sh2bus, true)
    , slaveSH2(sh2bus, false)
    , SCU(VDP1, VDP2, SCSP, CDBlock)
    , VDP2(SCU) {
    Reset(true);
}

void Saturn::Reset(bool hard) {
    sh2bus.Reset(hard);
    masterSH2.Reset(hard);
    slaveSH2.Reset(hard);
    SCU.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);
    VDP1.Reset(hard);
    VDP2.Reset(hard);
}

void Saturn::Step() {
    masterSH2.Step();

    // TODO: proper timings
    // TODO: remove when using a scheduler
    VDP2.Advance(1);
}

} // namespace satemu

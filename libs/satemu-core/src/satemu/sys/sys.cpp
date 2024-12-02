#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : SMPC(sysSCU)
    , sysSH2(sysSCU.SCU, SMPC)
    , sysSCU(sysVideo.VDP1, sysVideo.VDP2, SCSP, CDBlock, sysSH2)
    , sysVideo(sysSCU) {
    Reset(true);
}

void Saturn::Reset(bool hard) {
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);

    sysSH2.Reset(hard);
    sysSCU.Reset(hard);
    sysVideo.Reset(hard);
}

void Saturn::LoadIPL(std::span<uint8, kIPLSize> ipl) {
    sysSH2.SH2.bus.LoadIPL(ipl);
}

void Saturn::Step() {
    sysSH2.Step();

    // TODO: proper timings
    // TODO: replace with scheduler events
    sysVideo.Advance(1);
}

} // namespace satemu

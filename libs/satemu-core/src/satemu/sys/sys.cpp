#include <satemu/sys/sys.hpp>

namespace satemu {

Saturn::Saturn()
    : SH2(SCU, SMPC)
    , SCU(m_scheduler, VDP, SCSP, CDBlock, SH2)
    , VDP(m_scheduler, SCU)
    , SMPC(m_scheduler, *this)
    , SCSP(m_scheduler, SCU)
    , CDBlock(m_scheduler, SCU, SCSP) {

    Reset(true);
}

void Saturn::Reset(bool hard) {
    m_scheduler.Reset();

    SH2.Reset(hard);
    SCU.Reset(hard);
    VDP.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    CDBlock.Reset(hard);

    m_scspCycles = 0;
}

void Saturn::LoadIPL(std::span<uint8, sh2::kIPLSize> ipl) {
    SH2.bus.LoadIPL(ipl);
}

void Saturn::LoadDisc(media::Disc &&disc) {
    CDBlock.LoadDisc(std::move(disc));
}

void Saturn::EjectDisc() {
    CDBlock.EjectDisc();
}

void Saturn::OpenTray() {
    CDBlock.OpenTray();
}

void Saturn::CloseTray() {
    CDBlock.CloseTray();
}

void Saturn::RunFrame() {
    // Run until VDP exits top blanking phase
    while (VDP.InTopBlankingPhase()) {
        Step();
    }

    // Run until VDP enters top blanking phase
    while (!VDP.InTopBlankingPhase()) {
        Step();
    }
}

void Saturn::Step() {
    // Clock speeds:
    // - SH2:
    //   - 320 mode: 26.846591 MHz (NTSC) / 26.660156 MHz (PAL)
    //   - 352 mode: 28.636364 MHz (NTSC) / 28.437500 MHz (PAL)
    // - VDP1, VDP2, SCU share the SH2 clock
    //   - VDP pixel clock is 1/2 on hi-res modes or 1/4 at lo-res modes
    //   - SCU DSP runs at 1/2 clock speed
    // - SCSP: 22.579200 MHz (44100 * 512)
    //   - MC68EC000 runs at 1/2 SCSP clock
    // - CD Block SH1: 20.000000 MHz
    // - SMPC MCU: 4.000000 MHz

    // Subcycle counting is needed to accurately emulate the timings between these components.
    // We use the fastest clocks as a reference, incrementing the cycle counts of the other clocks by the same amount.
    // When a counter reaches the threshold, a full cycle is completed on that clock.
    //
    // Exact clock ratios are listed below for each mode:
    //
    // NTSC system at clock 352 mode:
    //   Clock rate       Step   Actual clock rate
    //   28,636,363.64    2464   28,636,363.64
    //   22,579,200.00    3125   22,579,200.00
    //   20,000,000.00    3528   20,000,000.00
    //    4,000,000.00   17640    4,000,000.00
    //  (64-bit counter overflows in over 3.9 million years at 59.97 fps)
    //
    // NTSC system at clock 320 mode:
    //   Clock rate       Step   Actual clock rate
    //   26,846,590.91   39424   26,846,590.91
    //   22,579,200.00   46875   22,579,200.00
    //   20,000,000.00   52920   20,000,000.00
    //    4,000,000.00  264600    4,000,000.00
    //  (64-bit counter overflows in over 240 thousand years at 59.97 fps)
    //
    // PAL system at clock 352 mode:
    //   Clock rate       Step   Actual clock rate
    //   28,437,500.00   32256   28,437,500.00
    //   22,579,200.00   40625   22,579,200.00
    //   20,000,000.00   45864   20,000,000.00
    //    4,000,000.00  229320    4,000,000.00
    //  (64-bit counter overflows in over 360 thousand years at 50 fps)
    //
    // PAL system at clock 320 mode:
    //   Clock rate        Step   Actual clock rate
    //   26,660,156.25   172032   26,660,156.25
    //   22,579,200.00   203125   22,579,200.00
    //   20,000,000.00   229320   20,000,000.00
    //    4,000,000.00  1146600    4,000,000.00
    //  (64-bit counter overflows in over 67 thousand years at 50 fps)

    static constexpr uint64 kMaxStep = 64;

    const uint64 cycles = std::min<uint64>(m_scheduler.RemainingCount(), kMaxStep);

    SH2.master.Advance(cycles);
    if (SH2.slaveEnabled) {
        SH2.slave.Advance(cycles);
    }
    SCU.Advance(cycles);
    VDP.Advance(cycles);

    // SCSP+M68K and CD block are ticked by the scheduler

    // TODO: advance SMPC
    /*m_smpcCycles += cycles * 2464;
    const uint64 smpcCycleCount = m_smpcCycles / 17640;
    if (smpcCycleCount > 0) {
        m_smpcCycles -= smpcCycleCount * 17640;
        SMPC.Advance(smpcCycleCount);
    }*/

    m_scheduler.Advance(cycles);
}

} // namespace satemu

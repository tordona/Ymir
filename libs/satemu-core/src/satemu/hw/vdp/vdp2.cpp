#include <satemu/hw/vdp/vdp2.hpp>

#include <fmt/format.h>

namespace satemu::vdp2 {

VDP2::VDP2() {
    Reset(true);
}

void VDP2::Reset(bool hard) {
    m_VRAM.fill(0);
    m_CRAM.fill(0);

    TVMD.u16 = 0x0;
    TVSTAT.u16 = 0x0; // TODO: preserve PAL flag
    VCNT = 0x0;
    RAMCTL.u16 = 0x0;
    VRSIZE.u16 = 0x0;
    CYCA0.u32 = 0x0;
    CYCA1.u32 = 0x0;
    CYCB0.u32 = 0x0;
    CYCB1.u32 = 0x0;
    BGON.u16 = 0x0;
    MZCTL.u16 = 0x0;
    SFSEL.u16 = 0x0;
    SFCODE.u16 = 0x0;
    CHCTLA.u16 = 0x0;
    CHCTLB.u16 = 0x0;
    BMPNA.u16 = 0x0;
    BMPNB.u16 = 0x0;
    PNCN0.u16 = 0x0;
    PNCN1.u16 = 0x0;
    PNCN2.u16 = 0x0;
    PNCN3.u16 = 0x0;
    PNCR.u16 = 0x0;
    MPOFN.u16 = 0x0;
    MPN0.u32 = 0x0;
    MPN1.u32 = 0x0;
    MPN2.u32 = 0x0;
    MPN3.u32 = 0x0;
    MPRA.ABCDEFGH.u64 = 0x0;
    MPRA.IJKLMNOP.u64 = 0x0;
    MPRB.ABCDEFGH.u64 = 0x0;
    MPRB.IJKLMNOP.u64 = 0x0;
    SCN0.u64 = 0x0;
    ZMN0.u64 = 0x0;
    SCN1.u64 = 0x0;
    ZMN1.u64 = 0x0;
    SCN2.u32 = 0x0;
    SCN3.u32 = 0x0;
    ZMCTL.u16 = 0x0;
    SCRCTL.u16 = 0x0;
    VCSTA.u32 = 0x0;
    LSTA0.u32 = 0x0;
    LSTA1.u32 = 0x0;
    LCTA.u32 = 0x0;
    RPMD.u16 = 0x0;
    RPRCTL.u16 = 0x0;
    KTCTL.u16 = 0x0;
    KTAOF.u16 = 0x0;
    OVPNRA = 0x0;
    OVPNRB = 0x0;
    RPTA.u32 = 0x0;
    WPXY0.u64 = 0x0;
    WPXY1.u64 = 0x0;
    WCTL.u64 = 0x0;
    LWTA0.u32 = 0x0;
    LWTA1.u32 = 0x0;
    SPCTL.u16 = 0x0;
    SDCTL.u16 = 0x0;
    CRAOFA.u16 = 0x0;
    CRAOFB.u16 = 0x0;
    LNCLEN.u16 = 0x0;
    SFPRMD.u16 = 0x0;
    CCCTL.u16 = 0x0;
    SFCCMD.u16 = 0x0;
    PRISA.u16 = 0x0;
    PRISB.u16 = 0x0;
    PRISC.u16 = 0x0;
    PRISD.u16 = 0x0;
    PRINA.u16 = 0x0;
    PRINB.u16 = 0x0;
    PRIR.u16 = 0x0;
    CCRSA.u16 = 0x0;
    CCRSB.u16 = 0x0;
    CCRSC.u16 = 0x0;
    CCRSD.u16 = 0x0;
    CCRNA.u16 = 0x0;
    CCRNB.u16 = 0x0;
    CCRR.u16 = 0x0;
    CCRLB.u16 = 0x0;
    CLOFEN.u16 = 0x0;
    CLOFSL.u16 = 0x0;
    COAR.u16 = 0x0;
    COAG.u16 = 0x0;
    COAB.u16 = 0x0;
    COBR.u16 = 0x0;
    COBG.u16 = 0x0;
    COBB.u16 = 0x0;

    m_HPhase = HorizontalPhase::Active;
    m_VPhase = VerticalPhase::Active;
    m_currCycles = 0;

    UpdateResolution();
}

void VDP2::Advance(uint64 cycles) {
    // Update timings and fire events
    // TODO: use scheduler

    m_currCycles += cycles;
    while (m_currCycles >= m_HTimings[static_cast<uint32>(m_HPhase)]) {
        auto nextPhase = static_cast<uint32>(m_HPhase) + 1;
        if (nextPhase == 4) {
            m_currCycles -= m_HTimings[3];
            nextPhase = 0;
        }

        m_HPhase = static_cast<HorizontalPhase>(nextPhase);
        switch (m_HPhase) {
        case HorizontalPhase::Active: BeginHPhaseActiveDisplay(); break;
        case HorizontalPhase::RightBorder: BeginHPhaseRightBorder(); break;
        case HorizontalPhase::HorizontalSync: BeginHPhaseHorizontalSync(); break;
        case HorizontalPhase::LeftBorder: BeginHPhaseLeftBorder(); break;
        }
    }
}

void VDP2::UpdateResolution() {
    // Horizontal/vertical resolution tables
    // NTSC uses the first two vRes entries, PAL uses the full table, and exclusive monitors use 480 lines
    static constexpr uint32 hRes[] = {320, 352, 640, 704};
    static constexpr uint32 vRes[] = {224, 240, 256, 256};

    // TODO: check for NTSC, PAL or exclusive monitor; assuming NTSC for now
    // TODO: exclusive monitor: even hRes entries are valid for 31 KHz monitors, odd are for Hi-Vision
    m_HRes = hRes[TVMD.HRESOn];
    m_VRes = vRes[TVMD.VRESOn & 1]; // for PAL, use & 3 (or omit it)
    if (TVMD.LSMDn == 3) {
        // Double-density interlace doubles the vertical resolution
        m_VRes *= 2;
    }

    fmt::println("VDP2: screen resolution set to {}x{}", m_HRes, m_VRes);

    // Timing tables
    // NOTE: the timings indicate when the specified phase begins

    // Horizontal phase timings
    //   RBd = Right Border
    //   HSy = Horizontal Sync
    //   LBd = Left Border
    //   ADp = Active Display
    static constexpr std::array<std::array<uint32, 4>, 4> hTimings{{
        // RBd, HSy, LBd, ADp
        {320, 347, 400, 427},
        {352, 375, 432, 455},
        {640, 694, 800, 854},
        {704, 375, 864, 910},
    }};
    m_HTimings = hTimings[TVMD.HRESOn];

    // Vertical phase timings
    //   BBd = Bottom Border
    //   BBl = Bottom Blanking
    //   VSy = Vertical Sync
    //   TBl = Top Blanking
    //   TBd = Top Border
    //   ADp = Active Display
    static constexpr std::array<std::array<std::array<uint32, 6>, 4>, 2> vTimings{{
        // NTSC
        {{
            // BBd, BBl, VSy, TBl, TBd, ADp
            {224, 232, 237, 240, 255, 263},
            {240, 240, 245, 248, 263, 263},
            {224, 232, 237, 240, 255, 263},
            {240, 240, 245, 248, 263, 263},
        }},
        // PAL
        {{
            // BBd, BBl, VSy, TBl, TBd, ADp
            {224, 256, 259, 262, 281, 313},
            {240, 264, 267, 270, 289, 313},
            {256, 272, 275, 278, 297, 313},
            {256, 272, 275, 278, 297, 313},
        }},
    }};
    // TODO: first index is NTSC/PAL selector
    m_VTimings = vTimings[0][TVMD.VRESOn];

    // Adjust for dot clock
    const uint32 dotClockMult = (TVMD.HRESOn & 2) ? 2 : 4;
    for (auto &timing : m_HTimings) {
        timing *= dotClockMult;
    }
    m_dotClockMult = dotClockMult;

    fmt::println("VDP2: dot clock mult = {}", dotClockMult);
    fmt::println("VDP2: display {}", TVMD.DISP ? "ON" : "OFF");
}

void VDP2::IncrementVCounter() {
    ++VCNT;
    while (VCNT >= m_VTimings[static_cast<uint32>(m_VPhase)]) {
        auto nextPhase = static_cast<uint32>(m_VPhase) + 1;
        if (nextPhase == 6) {
            VCNT = 0;
            nextPhase = 0;
        }

        m_VPhase = static_cast<VerticalPhase>(nextPhase);
        switch (m_VPhase) {
        case VerticalPhase::Active: BeginVPhaseActiveDisplay(); break;
        case VerticalPhase::BottomBorder: BeginVPhaseBottomBorder(); break;
        case VerticalPhase::BottomBlanking: BeginVPhaseBottomBlanking(); break;
        case VerticalPhase::VerticalSync: BeginVPhaseVerticalSync(); break;
        case VerticalPhase::TopBlanking: BeginVPhaseTopBlanking(); break;
        case VerticalPhase::TopBorder: BeginVPhaseTopBorder(); break;
        }
    }
}

// ----

void VDP2::BeginHPhaseActiveDisplay() {
    fmt::println("VDP2: (VCNT = {:3d})  entering horizontal active display phase", VCNT);
}

void VDP2::BeginHPhaseRightBorder() {
    fmt::println("VDP2: (VCNT = {:3d})  entering right border phase", VCNT);
}

void VDP2::BeginHPhaseHorizontalSync() {
    IncrementVCounter();
    fmt::println("VDP2: (VCNT = {:3d})  entering horizontal sync phase", VCNT);
    TVSTAT.HBLANK = 1;
}

void VDP2::BeginHPhaseLeftBorder() {
    fmt::println("VDP2: (VCNT = {:3d})  entering left border phase", VCNT);
    TVSTAT.HBLANK = 0;
}

// ----

void VDP2::BeginVPhaseActiveDisplay() {
    fmt::println("VDP2: (VCNT = {:3d})  entering vertical active display phase", VCNT);
    if (TVMD.LSMDn != 0) {
        TVSTAT.ODD ^= 1;
    } else {
        TVSTAT.ODD = 1;
    }
}

void VDP2::BeginVPhaseBottomBorder() {
    fmt::println("VDP2: (VCNT = {:3d})  entering bottom border phase", VCNT);
}

void VDP2::BeginVPhaseBottomBlanking() {
    fmt::println("VDP2: (VCNT = {:3d})  entering bottom blanking phase", VCNT);
}

void VDP2::BeginVPhaseVerticalSync() {
    fmt::println("VDP2: (VCNT = {:3d})  entering vertical sync phase", VCNT);
    TVSTAT.VBLANK = 1;
}

void VDP2::BeginVPhaseTopBlanking() {
    fmt::println("VDP2: (VCNT = {:3d})  entering top blanking phase", VCNT);
    TVSTAT.VBLANK = 0;
}

void VDP2::BeginVPhaseTopBorder() {
    fmt::println("VDP2: (VCNT = {:3d})  entering top border phase", VCNT);
}

} // namespace satemu::vdp2

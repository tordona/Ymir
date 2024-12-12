#include <satemu/hw/vdp/vdp.hpp>

#include <satemu/hw/scu/scu.hpp>

#include <satemu/util/constexpr_for.hpp>

#include <fmt/format.h>

#include <cassert>

namespace satemu::vdp {

VDP::VDP(scu::SCU &scu)
    : m_SCU(scu) {
    // TODO: set PAL flag
    Reset(true);
}

void VDP::Reset(bool hard) {
    m_VRAM1.fill(0);
    m_VRAM2.fill(0);
    m_CRAM.fill(0);
    for (auto &fb : m_spriteFB) {
        fb.fill(0);
    }
    m_drawFB = 0;

    m_VDP2regs.TVMD.u16 = 0x0;
    m_VDP2regs.TVSTAT.u16 &= 0xFFFE; // Preserve PAL flag
    m_VDP2regs.HCNT = 0x0;
    m_VDP2regs.VCNT = 0x0;
    m_VDP2regs.RAMCTL.u16 = 0x0;
    m_VDP2regs.VRSIZE.u16 = 0x0;
    m_VDP2regs.CYCA0.u32 = 0x0;
    m_VDP2regs.CYCA1.u32 = 0x0;
    m_VDP2regs.CYCB0.u32 = 0x0;
    m_VDP2regs.CYCB1.u32 = 0x0;
    m_VDP2regs.MZCTL.u16 = 0x0;
    m_VDP2regs.ZMCTL.u16 = 0x0;
    m_VDP2regs.SCRCTL.u16 = 0x0;
    m_VDP2regs.VCSTA.u32 = 0x0;
    m_VDP2regs.LSTA0.u32 = 0x0;
    m_VDP2regs.LSTA1.u32 = 0x0;
    m_VDP2regs.LCTA.u32 = 0x0;
    m_VDP2regs.RPMD.u16 = 0x0;
    m_VDP2regs.RPRCTL.u16 = 0x0;
    m_VDP2regs.KTCTL.u16 = 0x0;
    m_VDP2regs.KTAOF.u16 = 0x0;
    m_VDP2regs.OVPNRA = 0x0;
    m_VDP2regs.OVPNRB = 0x0;
    m_VDP2regs.RPTA.u32 = 0x0;
    m_VDP2regs.WPXY0.u64 = 0x0;
    m_VDP2regs.WPXY1.u64 = 0x0;
    m_VDP2regs.WCTL.u64 = 0x0;
    m_VDP2regs.LWTA0.u32 = 0x0;
    m_VDP2regs.LWTA1.u32 = 0x0;
    m_VDP2regs.SPCTL.u16 = 0x0;
    m_VDP2regs.SDCTL.u16 = 0x0;
    m_VDP2regs.CCCTL.u16 = 0x0;
    m_VDP2regs.SFCCMD.u16 = 0x0;
    m_VDP2regs.PRISA.u16 = 0x0;
    m_VDP2regs.PRISB.u16 = 0x0;
    m_VDP2regs.PRISC.u16 = 0x0;
    m_VDP2regs.PRISD.u16 = 0x0;
    m_VDP2regs.CCRSA.u16 = 0x0;
    m_VDP2regs.CCRSB.u16 = 0x0;
    m_VDP2regs.CCRSC.u16 = 0x0;
    m_VDP2regs.CCRSD.u16 = 0x0;
    m_VDP2regs.CCRNA.u16 = 0x0;
    m_VDP2regs.CCRNB.u16 = 0x0;
    m_VDP2regs.CCRR.u16 = 0x0;
    m_VDP2regs.CCRLB.u16 = 0x0;
    m_VDP2regs.CLOFEN.u16 = 0x0;
    m_VDP2regs.CLOFSL.u16 = 0x0;
    m_VDP2regs.COAR.u16 = 0x0;
    m_VDP2regs.COAG.u16 = 0x0;
    m_VDP2regs.COAB.u16 = 0x0;
    m_VDP2regs.COBR.u16 = 0x0;
    m_VDP2regs.COBG.u16 = 0x0;
    m_VDP2regs.COBB.u16 = 0x0;

    for (auto &bg : m_VDP2regs.m_NormBGParams) {
        bg.Reset();
    }
    for (auto &bg : m_VDP2regs.m_RotBGParams) {
        bg.Reset();
    }
    for (auto &sp : m_VDP2regs.m_specialFunctionCodes) {
        sp.Reset();
    }

    m_HPhase = HorizontalPhase::Active;
    m_VPhase = VerticalPhase::Active;
    m_currCycles = 0;
    m_dotClockMult = 2;
    m_VCounter = 0;
    m_HRes = 320;
    m_VRes = 224;
    m_VDP2regs.m_TVMDDirty = true;

    BeginHPhaseActiveDisplay();
    BeginVPhaseActiveDisplay();

    UpdateResolution();
}

void VDP::Advance(uint64 cycles) {
    // Update timings and fire events
    // TODO: use scheduler events

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

void VDP::UpdateResolution() {
    if (!m_VDP2regs.m_TVMDDirty) {
        return;
    }

    m_VDP2regs.m_TVMDDirty = false;

    // Horizontal/vertical resolution tables
    // NTSC uses the first two vRes entries, PAL uses the full table, and exclusive monitors use 480 lines
    static constexpr uint32 hRes[] = {320, 352, 640, 704};
    static constexpr uint32 vRes[] = {224, 240, 256, 256};

    // TODO: wait until next frame to update timings and etc.
    // - not sure how the hardware behaves if TVMODE is changed mid-frame
    // TODO: check for NTSC, PAL or exclusive monitor; assuming NTSC for now
    // TODO: exclusive monitor: even hRes entries are valid for 31 KHz monitors, odd are for Hi-Vision
    m_HRes = hRes[m_VDP2regs.TVMD.HRESOn];
    m_VRes = vRes[m_VDP2regs.TVMD.VRESOn & (m_VDP2regs.TVSTAT.PAL ? 3 : 1)];
    if (m_VDP2regs.TVMD.LSMDn == 3) {
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
    m_HTimings = hTimings[m_VDP2regs.TVMD.HRESOn];

    // Vertical phase timings
    //   BBd = Bottom Border
    //   BBl = Bottom Blanking
    //   VSy = Vertical Sync
    //   TBl = Top Blanking
    //   TBd = Top Border
    //   LLn = Last Line
    //   ADp = Active Display
    static constexpr std::array<std::array<std::array<uint32, 7>, 4>, 2> vTimings{{
        // NTSC
        {{
            // BBd, BBl, VSy, TBl, TBd, LLn, ADp
            {224, 232, 237, 240, 255, 262, 263},
            {240, 240, 245, 248, 263, 262, 263},
            {224, 232, 237, 240, 255, 262, 263},
            {240, 240, 245, 248, 263, 262, 263},
        }},
        // PAL
        {{
            // BBd, BBl, VSy, TBl, TBd, ADp
            {224, 256, 259, 262, 281, 312, 313},
            {240, 264, 267, 270, 289, 312, 313},
            {256, 272, 275, 278, 297, 312, 313},
            {256, 272, 275, 278, 297, 312, 313},
        }},
    }};
    m_VTimings = vTimings[m_VDP2regs.TVSTAT.PAL][m_VDP2regs.TVMD.VRESOn];

    // Adjust for dot clock
    const uint32 dotClockMult = (m_VDP2regs.TVMD.HRESOn & 2) ? 2 : 4;
    for (auto &timing : m_HTimings) {
        timing *= dotClockMult;
    }
    m_dotClockMult = dotClockMult;

    fmt::println("VDP2: dot clock mult = {}", dotClockMult);
    fmt::println("VDP2: display {}", m_VDP2regs.TVMD.DISP ? "ON" : "OFF");
}

FORCE_INLINE void VDP::IncrementVCounter() {
    ++m_VCounter;
    while (m_VCounter >= m_VTimings[static_cast<uint32>(m_VPhase)]) {
        auto nextPhase = static_cast<uint32>(m_VPhase) + 1;
        if (nextPhase == 7) {
            m_VCounter = 0;
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
        case VerticalPhase::LastLine: BeginVPhaseLastLine(); break;
        }
    }
}

// ----

void VDP::BeginHPhaseActiveDisplay() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering horizontal active display phase", m_VCounter);
    if (m_VPhase == VerticalPhase::Active) {
        if (m_VCounter == 0) {
            m_framebuffer = m_cbRequestFramebuffer(m_HRes, m_VRes);
        }
        VDP2DrawLine();
    }
}

void VDP::BeginHPhaseRightBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering right border phase", m_VCounter);
}

void VDP::BeginHPhaseHorizontalSync() {
    IncrementVCounter();
    // fmt::println("VDP2: (VCNT = {:3d})  entering horizontal sync phase", m_VCounter);

    m_VDP2regs.TVSTAT.HBLANK = 1;
    m_SCU.TriggerHBlankIN();
}

void VDP::BeginHPhaseLeftBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering left border phase", m_VCounter);
    m_VDP2regs.TVSTAT.HBLANK = 0;
}

// ----

void VDP::BeginVPhaseActiveDisplay() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering vertical active display phase", m_VCounter);
    if (m_VDP2regs.TVMD.LSMDn != 0) {
        m_VDP2regs.TVSTAT.ODD ^= 1;
    } else {
        m_VDP2regs.TVSTAT.ODD = 1;
    }
}

void VDP::BeginVPhaseBottomBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering bottom border phase", m_VCounter);
}

void VDP::BeginVPhaseBottomBlanking() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering bottom blanking phase", m_VCounter);
}

void VDP::BeginVPhaseVerticalSync() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering vertical sync phase", m_VCounter);
    m_VDP2regs.TVSTAT.VBLANK = 1;
    m_SCU.TriggerVBlankIN();
}

void VDP::BeginVPhaseTopBlanking() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering top blanking phase", m_VCounter);

    // TODO: end frame
    m_cbFrameComplete(m_framebuffer, m_HRes, m_VRes);

    UpdateResolution();
}

void VDP::BeginVPhaseTopBorder() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering top border phase", m_VCounter);
}

void VDP::BeginVPhaseLastLine() {
    // fmt::println("VDP2: (VCNT = {:3d})  entering last line phase", m_VCounter);

    m_VDP2regs.TVSTAT.VBLANK = 0;
    m_SCU.TriggerVBlankOUT();
}

// ----
// Renderer

void VDP::VDP2DrawLine() {
    // fmt::println("VDP2: drawing line {}", m_VCounter);

    using FnDrawScrollNBG = void (VDP::*)(const NormBGParams &, BGRenderContext &);
    using FnDrawBitmapNBG = void (VDP::*)(const NormBGParams &, BGRenderContext &);

    // Lookup table of DrawNormalScrollBG functions
    // [twoWordChar][fourCellChar][wideChar][colorFormat][colorMode]
    static constexpr auto fnDrawScrollNBGs = [] {
        std::array<std::array<std::array<std::array<std::array<FnDrawScrollNBG, 4>, 8>, 2>, 2>, 2> arr{};

        util::constexpr_for<2 * 2 * 2 * 8 * 4>([&](auto index) {
            const uint32 twcIndex = bit::extract<0>(index());
            const uint32 fccIndex = bit::extract<1>(index());
            const uint32 wcIndex = bit::extract<2>(index());
            const uint32 cfIndex = bit::extract<3, 5>(index());
            const uint32 cmIndex = bit::extract<6, 7>(index());

            const bool twoWordChar = twcIndex;
            const bool fourCellChar = fccIndex;
            const bool wideChar = wcIndex;
            const ColorFormat colorFormat = static_cast<ColorFormat>(cfIndex <= 4 ? cfIndex : 4);
            const uint32 colorMode = cmIndex <= 2 ? cmIndex : 2;
            arr[twcIndex][fccIndex][wcIndex][cfIndex][cmIndex] =
                &VDP::DrawNormalScrollBG<twoWordChar, fourCellChar, wideChar, colorFormat, colorMode>;
        });

        return arr;
    }();

    // Lookup table of DrawNormalBitmapBG functions
    // [colorFormat][colorMode]
    static constexpr auto fnDrawBitmapNBGs = [] {
        std::array<std::array<FnDrawBitmapNBG, 4>, 8> arr{};

        util::constexpr_for<8 * 4>([&](auto index) {
            const uint32 cfIndex = bit::extract<0, 2>(index());
            const uint32 cmIndex = bit::extract<3, 4>(index());

            const ColorFormat colorFormat = static_cast<ColorFormat>(cfIndex <= 4 ? cfIndex : 4);
            const size_t colorMode = cmIndex <= 2 ? cmIndex : 2;
            arr[cfIndex][cmIndex] = &VDP::DrawNormalBitmapBG<colorFormat, colorMode>;
        });

        return arr;
    }();

    // TODO: optimize
    // - RBG1 replaces NBG0
    // - when RBG0 and RBG1 are both enabled, NBG0-3 are disabled
    // - this means we can use less render contexts (4 max it seems)

    // Draw normal BGs
    int i = 0;
    for (const auto &bg : m_VDP2regs.m_NormBGParams) {
        auto &rctx = m_renderContexts[i++];
        if (bg.enabled) {
            rctx.cramOffset = bg.caos << (m_VDP2regs.RAMCTL.CRMDn == 1 ? 10 : 9);

            const uint32 colorFormat = static_cast<uint32>(bg.colorFormat);
            const uint32 colorMode = m_VDP2regs.RAMCTL.CRMDn;
            if (bg.bitmap) {
                (this->*fnDrawBitmapNBGs[colorFormat][colorMode])(bg, rctx);
            } else {
                const bool twoWordChar = bg.twoWordChar;
                const bool fourCellChar = bg.cellSizeShift;
                const bool wideChar = bg.wideChar;
                (this->*fnDrawScrollNBGs[twoWordChar][fourCellChar][wideChar][colorFormat][colorMode])(bg, rctx);
            }
        } else {
            // TODO: optimize -- fill these when the BG is disabled
            rctx.colorData.fill(0);
            rctx.colors.fill({0});
            rctx.priorities.fill(0);
        }
    }

    // Draw rotation BGs
    for (const auto &bg : m_VDP2regs.m_RotBGParams) {
        auto &rctx = m_renderContexts[i++];
        if (bg.enabled) {
            rctx.cramOffset = bg.caos << (m_VDP2regs.RAMCTL.CRMDn == 1 ? 10 : 9);

            // TODO: implement
            if (bg.bitmap) {
                // (this->*fnDrawBitmapRBGs[...])(bg, rctx);
            } else {
                // (this->*fnDrawScrollRBGs[...])(bg, rctx);
            }
        } else {
            // TODO: optimize -- fill these when the BG is disabled
            rctx.colorData.fill(0);
            rctx.colors.fill({0});
            rctx.priorities.fill(0);
        }
    }

    // Compose image
    const uint32 y = m_VCounter;
    for (uint32 x = 0; x < m_HRes; x++) {
        // TODO: handle priorities
        // - sort layers per pixel
        //   - priority == 0 and special priority in use -> transparent pixel
        //   - transparent pixels are ignored
        // - keep two topmost layers
        //   - add one if LNCL insertion happened
        //   - add one if second screen color calculation is enabled (extended color calculation)
        // - use BACK when all layers are transparent
        // TODO: handle color calculations

        // HACK: for now, use the top priority of each layer
        if (m_framebuffer != nullptr) {
            uint32 prio = 0;
            for (int i = 0; i < 6; i++) {
                const auto &rctx = m_renderContexts[i];
                if (rctx.colors[x].msb && rctx.priorities[x] >= prio) {
                    prio = rctx.priorities[x];
                    m_framebuffer[x + y * m_HRes] = rctx.colors[x].u32;
                }
            }
            // TODO: if no layers are visible, draw BACK screen
        }
    }
}

template <bool twoWordChar, bool fourCellChar, bool wideChar, ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::DrawNormalScrollBG(const NormBGParams &bgParams, BGRenderContext &rctx) {
    //          Map
    // +---------+---------+
    // |         |         |   Normal BGs always have 4 planes named A,B,C,D in this exact configuration.
    // | Plane A | Plane B |   Each plane can point to different portions of VRAM through a combination of bits
    // |         |         |   from the Map Register (per BG) and the Map Offset Register (per BG and plane):
    // +---------+---------+     bits 5-0 of the address come from the Map Register (MPxxN#)
    // |         |         |     bits 8-6 of the address come from the Map Offset Register (MPOFN)
    // | Plane C | Plane D |
    // |         |         |
    // +---------+---------+
    //
    //         Plane
    // +---------+---------+   Each plane is composed of 1x1, 2x1 or 2x2 pages, determined by Plane Size in the
    // |         |         |   Plane Size Register (PLSZ).
    // | Page 1  | Page 2  |   Pages are stored sequentially in VRAM left to right, top to bottom, as shown.
    // |         |         |
    // +---------+---------+
    // |         |         |
    // | Page 3  | Page 4  |
    // |         |         |
    // +---------+---------+
    //
    //           Page
    // +----+----+..+----+----+   Pages contain 32x32 or 64x64 character patterns, which are groups of 1x1 or 2x2 cells,
    // |CP 1|CP 2|  |CP63|CP64|   determined by Character Size in the Character Control Register (CHCTLA-B).
    // +----+----+..+----+----+   Pages always contain a total of 64x64 cells - a grid of 64x64 1x1 character patterns
    // |  65|  66|  | 127| 128|   or 32x32 2x2 character patterns. Because of this, pages always have 512x512 dots.
    // +----+----+..+----+----+
    // :    :    :  :    :    :   Character patterns in a page are stored sequentially in VRAM left to right, top to
    // +----+----+..+----+----+   bottom, as shown. The figure to the left illustrates a 64x64 page; a 32x32 page would
    // |3969|3970|  |4031|4032|   have 1024 character patterns in total instead of 4096.
    // +----+----+..+----+----+
    // |4033|4034|  |4095|4096|
    // +----+----+..+----+----+
    //
    //   Character Pattern
    // +---------+---------+   Character patterns are groups of 1x1 or 2x2 cells, determined by Character Size in the
    // |         |         |   Character Control Register (CHCTLA-B).
    // | Cell 1  | Cell 2  |   Cells are stored sequentially in VRAM left to right, top to bottom, as shown.
    // |         |         |   Character patterns contain a character number (15 bits), a palette number (7 bits, only
    // +---------+---------+   used with 16 or 256 color palette modes), two special function bits (Special Priority and
    // |         |         |   Special Color Calculation) and two flip bits (horizontal and vertical).
    // | Cell 3  | Cell 4  |   Character patterns can be one or two words long, as defined by Pattern Name Data Size
    // |         |         |   in the Pattern Name Control Register (PNCN0-3, PNCR).
    // +---------+---------+   When using one word characters, some of the data comes from supplementary registers.
    //
    //           Cell
    // +--+--+--+--+--+--+--+--+   Cells contain 8x8 dots (pixels) in one of the following color formats:
    // | 1| 2| 3| 4| 5| 6| 7| 8|     - 16 color palette
    // +--+--+--+--+--+--+--+--+     - 256 color palette
    // | 9|10|11|12|13|14|15|16|     - 1024 or 2048 color palette (depending on Color Mode)
    // +--+--+--+--+--+--+--+--+     - 5:5:5 RGB (32768 colors)
    // |17|18|19|20|21|22|23|24|     - 8:8:8 RGB (16777216 colors)
    // +--+--+--+--+--+--+--+--+
    // |25|26|27|28|29|30|31|32|
    // +--+--+--+--+--+--+--+--+
    // |33|34|35|36|37|38|39|40|
    // +--+--+--+--+--+--+--+--+
    // |41|42|43|44|45|46|47|48|
    // +--+--+--+--+--+--+--+--+
    // |49|50|51|52|53|54|55|56|
    // +--+--+--+--+--+--+--+--+
    // |57|58|59|60|61|62|63|64|
    // +--+--+--+--+--+--+--+--+

    const auto &specialFunctionCodes = m_VDP2regs.m_specialFunctionCodes[bgParams.specialFunctionSelect];

    // TODO: implement mosaic

    const uint32 y = m_VCounter;

    // TODO: precompute fracScrollY at start of frame and increment per Y
    uint32 fracScrollX = bgParams.scrollAmountH;
    uint32 fracScrollY = bgParams.scrollAmountV + y * bgParams.scrollIncV;
    for (uint32 x = 0; x < m_HRes; x++) {
        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 8u;
        const uint32 scrollY = fracScrollY >> 8u;

        // Determine plane index from the scroll coordinate
        const uint32 planeX = bit::extract<9>(scrollX) >> bgParams.pageShiftH;
        const uint32 planeY = bit::extract<9>(scrollY) >> bgParams.pageShiftV;
        const uint32 plane = planeX + planeY * 2u;

        // Determine page index from the scroll coordinate
        const uint32 pageX = bit::extract<9>(scrollX) & bgParams.pageShiftH;
        const uint32 pageY = bit::extract<9>(scrollY) & bgParams.pageShiftV;
        const uint32 page = pageX + pageY * 2u;

        // Determine character pattern from the scroll coordinate
        const uint32 charPatX = bit::extract<3, 8>(scrollX) >> fourCellChar;
        const uint32 charPatY = bit::extract<3, 8>(scrollY) >> fourCellChar;
        const uint32 charIndex = charPatX + charPatY * (64u >> fourCellChar);

        // Determine cell index from the scroll coordinate
        const uint32 cellX = bit::extract<3>(scrollX) & fourCellChar;
        const uint32 cellY = bit::extract<3>(scrollY) & fourCellChar;
        const uint32 cellIndex = cellX + cellY * 2u;

        // Determine dot coordinates
        const uint32 dotX = bit::extract<0, 2>(scrollX);
        const uint32 dotY = bit::extract<0, 2>(scrollY);

        // Fetch character
        const uint32 pageBaseAddress = bgParams.pageBaseAddresses[plane];
        const uint32 pageOffset = page * kPageSizes[fourCellChar][twoWordChar];
        const uint32 pageAddress = pageBaseAddress + pageOffset;
        constexpr bool largePalette = colorFormat != ColorFormat::Palette16;
        const Character ch =
            twoWordChar ? FetchTwoWordCharacter(pageAddress, charIndex)
                        : FetchOneWordCharacter<fourCellChar, largePalette, wideChar>(bgParams, pageAddress, charIndex);

        // Fetch dot color using character data
        rctx.colors[x] =
            FetchCharacterColor<colorFormat, colorMode>(rctx.cramOffset, rctx.colorData[x], ch, dotX, dotY, cellIndex);

        // Compute priority
        rctx.priorities[x] = bgParams.priorityNumber;
        if (bgParams.priorityMode == PriorityMode::PerCharacter) {
            rctx.priorities[x] &= ~1;
            rctx.priorities[x] |= ch.specPriority;
        } else if (bgParams.priorityMode == PriorityMode::PerDot) {
            if constexpr (colorFormat == ColorFormat::Palette16 || colorFormat == ColorFormat::Palette256 ||
                          colorFormat == ColorFormat::Palette2048) {
                rctx.priorities[x] &= ~1;
                if (ch.specPriority && specialFunctionCodes.colorMatches[rctx.colorData[x]]) {
                    rctx.priorities[x] |= 1;
                }
            }
        }

        // Increment horizontal coordinate
        fracScrollX += bgParams.scrollIncH;
    }
}

template <ColorFormat colorFormat, uint32 colorMode>
NO_INLINE void VDP::DrawNormalBitmapBG(const NormBGParams &bgParams, BGRenderContext &rctx) {
    const uint32 y = m_VCounter;

    // TODO: precompute fracScrollY at start of frame and increment per Y
    uint32 fracScrollX = bgParams.scrollAmountH;
    uint32 fracScrollY = bgParams.scrollAmountV + y * bgParams.scrollIncV;

    for (uint32 x = 0; x < m_HRes; x++) {
        // Get integer scroll screen coordinates
        const uint32 scrollX = fracScrollX >> 8u;
        const uint32 scrollY = fracScrollY >> 8u;

        // Fetch dot color from bitmap
        rctx.colors[x] = FetchBitmapColor<colorFormat, colorMode>(bgParams, rctx.cramOffset, scrollX, scrollY);

        // Compute priority
        rctx.priorities[x] = bgParams.priorityNumber;
        if (bgParams.priorityMode == PriorityMode::PerCharacter || bgParams.priorityMode == PriorityMode::PerDot) {
            rctx.priorities[x] &= ~1;
            rctx.priorities[x] |= bgParams.supplBitmapSpecialPriority;
        }

        // Increment horizontal coordinate
        fracScrollX += bgParams.scrollIncH;
    }
}

FORCE_INLINE VDP::Character VDP::FetchTwoWordCharacter(uint32 pageBaseAddress, uint32 charIndex) {
    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint32);
    const uint32 charData = util::ReadBE<uint32>(&m_VRAM2[charAddress]);

    Character ch{};
    ch.charNum = bit::extract<0, 14>(charData);
    ch.palNum = bit::extract<16, 22>(charData);
    ch.specColorCalc = bit::extract<28>(charData);
    ch.specPriority = bit::extract<29>(charData);
    ch.flipH = bit::extract<30>(charData);
    ch.flipV = bit::extract<31>(charData);
    return ch;
}

template <bool fourCellChar, bool largePalette, bool wideChar>
FORCE_INLINE VDP::Character VDP::FetchOneWordCharacter(const NormBGParams &bgParams, uint32 pageBaseAddress,
                                                       uint32 charIndex) {
    const uint32 charAddress = pageBaseAddress + charIndex * sizeof(uint16);
    const uint16 charData = util::ReadBE<uint16>(&m_VRAM2[charAddress]);

    /*
    Contents of 1 word character patterns vary based on Character Size, Character Color Count and Auxiliary Mode:
        Character Size        = CHCTLA/CHCTLB.xxCHSZ  = !fourCellChar = !FCC
        Character Color Count = CHCTLA/CHCTLB.xxCHCNn = largePalette  = LP
        Auxiliary Mode        = PNCN0/PNCR.xxCNSM     = wideChar      = WC
                ---------------- Character data ----------------    Supplement in Pattern Name Control Register
    FCC LP  WC  |15 14 13 12 11 10 9  8  7  6  5  4  3  2  1  0|    | 9  8  7  6  5  4  3  2  1  0|
     F   F   F  |palnum 3-0 |VF|HF| character number 9-0       |    |PR|CC| PN 6-4 |charnum 14-10 |
     F   T   F  |--| PN 6-4 |VF|HF| character number 9-0       |    |PR|CC|--------|charnum 14-10 |
     T   F   F  |palnum 3-0 |VF|HF| character number 11-2      |    |PR|CC| PN 6-4 |CN 14-12|CN1-0|
     T   T   F  |--| PN 6-4 |VF|HF| character number 11-2      |    |PR|CC|--------|CN 14-12|CN1-0|
     F   F   T  |palnum 3-0 |       character number 11-0      |    |PR|CC| PN 6-4 |CN 14-12|-----|
     F   T   T  |--| PN 6-4 |       character number 11-0      |    |PR|CC|--------|CN 14-12|-----|
     T   F   T  |palnum 3-0 |       character number 13-2      |    |PR|CC| PN 6-4 |xx|-----|CN1-0|   xx=CN14
     T   T   T  |--| PN 6-4 |       character number 13-2      |    |PR|CC|--------|xx|-----|CN1-0|   xx=CN14
    */

    // Character number bit range from the 1-word character pattern data (charData)
    static constexpr uint32 charNumStart = 2 * fourCellChar;
    static constexpr uint32 charNumEnd = charNumStart + 9 + 2 * wideChar;

    // Upper character number bit range from the supplementary character number (bgParams.supplCharNum)
    static constexpr uint32 supplCharNumStartUpper = 2 * fourCellChar + 2 * wideChar;
    static constexpr uint32 supplCharNumEndUpper = 4;
    // The lower bits are always in range 0..1 and only used if fourCellChar == true

    const uint32 baseCharNum = bit::extract<charNumStart, charNumEnd>(charData);
    const uint32 supplCharNum = bit::extract<supplCharNumStartUpper, supplCharNumEndUpper>(bgParams.supplScrollCharNum);

    Character ch{};
    ch.charNum = (baseCharNum << charNumStart) | (supplCharNum << (10 + supplCharNumStartUpper));
    if constexpr (fourCellChar) {
        ch.charNum |= bit::extract<0, 1>(bgParams.supplScrollCharNum);
    }
    if constexpr (largePalette) {
        ch.palNum = bit::extract<12, 14>(charData) << 4u;
    } else {
        ch.palNum = bit::extract<12, 15>(charData) | bgParams.supplScrollPalNum;
    }
    ch.specColorCalc = bgParams.supplScrollSpecialColorCalc;
    ch.specPriority = bgParams.supplScrollSpecialPriority;
    ch.flipH = !wideChar && bit::extract<10>(charData);
    ch.flipV = !wideChar && bit::extract<11>(charData);
    return ch;
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE vdp::Color888 VDP::FetchCharacterColor(uint32 cramOffset, uint8 &colorData, Character ch, uint8 dotX,
                                                    uint8 dotY, uint32 cellIndex) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");
    assert(dotX < 8);
    assert(dotY < 8);

    // Cell addressing uses a fixed offset of 32 bytes
    const uint32 cellAddress = (ch.charNum + cellIndex) * 0x20;
    const uint32 dotOffset = dotX + dotY * 8;
    const uint32 dotBaseAddress = cellAddress + dotOffset;

    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = dotBaseAddress >> 1u;
        const uint8 dotData = (m_VRAM2[dotAddress & 0x7FFFF] >> ((dotX & 1) * 4)) & 0xF;
        const uint32 colorIndex = (ch.palNum << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        return FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = dotBaseAddress;
        const uint8 dotData = m_VRAM2[dotAddress & 0x7FFFF];
        const uint32 colorIndex = ((ch.palNum & 0x70) << 4u) | dotData;
        colorData = bit::extract<1, 3>(dotData);
        return FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = dotBaseAddress * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        const uint32 colorIndex = dotData & 0x7FF;
        colorData = bit::extract<1, 3>(dotData);
        return FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = dotBaseAddress * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        return ConvertRGB555to888(Color555{.u16 = dotData});
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = dotBaseAddress * sizeof(uint32);
        const uint32 dotData = util::ReadBE<uint32>(&m_VRAM2[dotAddress & 0x7FFFF]);
        return Color888{.u32 = dotData};
    }
}

template <ColorFormat colorFormat, uint32 colorMode>
FORCE_INLINE vdp::Color888 VDP::FetchBitmapColor(const NormBGParams &bgParams, uint32 cramOffset, uint8 dotX,
                                                 uint8 dotY) {
    static_assert(static_cast<uint32>(colorFormat) <= 4, "Invalid xxCHCN value");

    // Bitmap data wraps around infinitely
    dotX &= ~(bgParams.bitmapSizeH - 1);
    dotY &= ~(bgParams.bitmapSizeV - 1);

    // Bitmap addressing uses a fixed offset of 0x20000 bytes which is precalculated when MPOFN/MPOFR is written to
    const uint32 bitmapBaseAddress = bgParams.bitmapBaseAddress;
    const uint32 dotOffset = dotX + dotY * bgParams.bitmapSizeH;
    const uint32 dotBaseAddress = bitmapBaseAddress + dotOffset;
    const uint32 palNum = bgParams.supplBitmapPalNum;

    if constexpr (colorFormat == ColorFormat::Palette16) {
        const uint32 dotAddress = dotBaseAddress >> 1u;
        const uint8 dotData = (m_VRAM2[dotAddress & 0x7FFFF] >> ((dotX & 1) * 4)) & 0xF;
        const uint32 colorIndex = palNum | dotData;
        return FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette256) {
        const uint32 dotAddress = dotBaseAddress;
        const uint8 dotData = m_VRAM2[dotAddress & 0x7FFFF];
        const uint32 colorIndex = palNum | dotData;
        return FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::Palette2048) {
        const uint32 dotAddress = dotBaseAddress * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        const uint32 colorIndex = dotData & 0x7FF;
        return FetchCRAMColor<colorMode>(cramOffset, colorIndex);
    } else if constexpr (colorFormat == ColorFormat::RGB555) {
        const uint32 dotAddress = dotBaseAddress * sizeof(uint16);
        const uint16 dotData = util::ReadBE<uint16>(&m_VRAM2[dotAddress & 0x7FFFF]);
        return ConvertRGB555to888(Color555{.u16 = dotData});
    } else if constexpr (colorFormat == ColorFormat::RGB888) {
        const uint32 dotAddress = dotBaseAddress * sizeof(uint32);
        const uint32 dotData = util::ReadBE<uint32>(&m_VRAM2[dotAddress & 0x7FFFF]);
        return Color888{.u32 = dotData};
    }
}

template <uint32 colorMode>
FORCE_INLINE Color888 VDP::FetchCRAMColor(uint32 cramOffset, uint32 colorIndex) {
    static_assert(colorMode <= 2, "Invalid CRMD value");

    if constexpr (colorMode == 0) {
        // RGB 5:5:5, 1024 words
        const uint32 address = (cramOffset + colorIndex * sizeof(uint16)) & 0x7FF;
        const uint16 data = util::ReadBE<uint16>(&m_CRAM[address]);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else if constexpr (colorMode == 1) {
        // RGB 5:5:5, 2048 words
        const uint32 address = (cramOffset + colorIndex * sizeof(uint16)) & 0xFFF;
        const uint16 data = util::ReadBE<uint16>(&m_CRAM[address]);
        Color555 clr555{.u16 = data};
        return ConvertRGB555to888(clr555);
    } else { // colorMode == 2
        // RGB 8:8:8, 1024 words
        const uint32 address = (cramOffset + colorIndex * sizeof(uint32)) & 0xFFF;
        const uint32 data = util::ReadBE<uint32>(&m_CRAM[address]);
        return Color888{.u32 = data};
    }
}

} // namespace satemu::vdp

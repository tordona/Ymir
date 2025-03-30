#include "scu_dsp_dma_view.hpp"

namespace app::ui {

SCUDSPDMAView::SCUDSPDMAView(SharedContext &context)
    : m_context(context)
    , m_dsp(context.saturn.SCU.GetDSP()) {}

void SCUDSPDMAView::Display() {
    ImGui::TextUnformatted("(placeholder - SCU DSP DMA)");
    // TODO: show registers and state
    /*
    bool dmaRun;         // DMA transfer in progress (T0)
    bool dmaToD0;        // DMA transfer direction: false=D0 to DSP, true=DSP to D0
    bool dmaHold;        // DMA transfer hold address
    uint8 dmaCount;      // DMA transfer length
    uint8 dmaSrc;        // DMA source register (CT0-3 or program RAM)
    uint8 dmaDst;        // DMA destination register (CT0-3 or program RAM)
    uint32 dmaReadAddr;  // DMA read address (RA0)
    uint32 dmaWriteAddr; // DMA write address (WA0)
    uint32 dmaAddrInc;   // DMA address increment
    */
}

} // namespace app::ui

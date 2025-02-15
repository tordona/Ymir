#include <satemu/hw/smpc/peripheral/peripheral_impl_standard_pad.hpp>

#include <satemu/util/bit_ops.hpp>

namespace satemu::peripheral {

StandardPad::StandardPad()
    : BasePeripheral(0x0, 2)
    , m_buttons(Button::All | Button::_bit2) {}

void StandardPad::PressButton(Button button) {
    m_buttons &= ~(button & Button::All);
}

void StandardPad::ReleaseButton(Button button) {
    m_buttons |= button & Button::All;
}

void StandardPad::Read(std::span<uint8> out) {
    assert(out.size() == 2);

    // [0] 7-0 = left, right, down, up, start, A, C, B
    // [1] 7-3 = R, X, Y, Z, L; 2-0 = nothing
    const uint16 btnValue = static_cast<uint16>(m_buttons);
    out[0] = bit::extract<8, 15>(btnValue);
    out[1] = (bit::extract<3, 7>(btnValue) << 3) | 0x7;
}

uint8 StandardPad::WritePDR(uint8 ddr, uint8 value) {
    const uint16 btnValue = static_cast<uint16>(m_buttons);

    switch (ddr & 0x7F) {
    case 0x40: // TH control mode
        if (value & 0x40) {
            return 0x70 | bit::extract<0, 3>(btnValue);
        } else {
            return 0x30 | bit::extract<12, 15>(btnValue);
        }
        break;
    case 0x60: // TH/TR control mode
        switch (value & 0x60) {
        case 0x00: // R X Y Z
            return 0x10 | bit::extract<4, 7>(btnValue);
            break;
        case 0x20: // right left down up
            return 0x30 | bit::extract<12, 15>(btnValue);
            break;
        case 0x40: // start A C B
            return 0x50 | bit::extract<8, 11>(btnValue);
            break;
        case 0x60: // L 1 0 0
            return 0x70 | bit::extract<0, 3>(btnValue);
        }
    }
}

} // namespace satemu::peripheral

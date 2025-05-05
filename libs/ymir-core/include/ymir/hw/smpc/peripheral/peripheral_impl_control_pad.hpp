#pragma once

#include "peripheral_base.hpp"

#include "peripheral_state_control_pad.hpp"

namespace ymir::peripheral {

class ControlPad final : public BasePeripheral {
public:
    ControlPad(CBPeripheralReport callback);

    void Read(std::span<uint8> out) final;

    uint8 WritePDR(uint8 ddr, uint8 value) final;

private:
    ControlPadButton ReadButtons();
};

} // namespace ymir::peripheral

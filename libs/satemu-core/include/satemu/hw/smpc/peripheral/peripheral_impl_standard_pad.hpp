#pragma once

#include "peripheral_base.hpp"

#include "peripheral_state_standard_pad.hpp"

namespace satemu::peripheral {

class StandardPad final : public BasePeripheral {
public:
    StandardPad(CBPeripheralReport callback);

    void Read(std::span<uint8> out) final;

    uint8 WritePDR(uint8 ddr, uint8 value) final;

private:
    StandardPadButton ReadButtons();
};

} // namespace satemu::peripheral

#pragma once

#include "peripheral_base.hpp"

namespace satemu::peripheral {

class NullPeripheral final : public BasePeripheral {
public:
    NullPeripheral()
        : BasePeripheral(PeripheralType::None, 0x0, 0) {}

    void Read(std::span<uint8> out) final {}

    uint8 WritePDR(uint8 ddr, uint8 value) final {
        return 0;
    }
};

} // namespace satemu::peripheral

#pragma once

#include "peripheral_base.hpp"

#include <satemu/core/types.hpp>

#include <satemu/util/bitmask_enum.hpp>

namespace satemu::peripheral {

class StandardPad final : public BasePeripheral {
public:
    enum class Button : uint16 {
        None = 0,

        Right = (1u << 15u),
        Left = (1u << 14u),
        Down = (1u << 13u),
        Up = (1u << 12u),
        Start = (1u << 11u),
        A = (1u << 10u),
        C = (1u << 9u),
        B = (1u << 8u),
        R = (1u << 7u),
        X = (1u << 6u),
        Y = (1u << 5u),
        Z = (1u << 4u),
        L = (1u << 3u),

        _bit2 = (1u << 2u), // always one; should never be used

        All = Right | Left | Down | Up | Start | A | C | B | R | X | Y | Z | L,
    };

    StandardPad();

    void PressButton(Button button);
    void ReleaseButton(Button button);

    void Read(std::span<uint8> out) final;

    uint8 WritePDR(uint8 ddr, uint8 value) final;

private:
    Button m_buttons; // NOTE: button state is inverted
};

} // namespace satemu::peripheral

ENABLE_BITMASK_OPERATORS(satemu::peripheral::StandardPad::Button);

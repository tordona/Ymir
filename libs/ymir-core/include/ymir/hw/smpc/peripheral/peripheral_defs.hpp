#pragma once

#include <string_view>

namespace ymir::peripheral {

enum class PeripheralType { None, ControlPad };

inline std::string_view GetPeripheralName(PeripheralType type) {
    switch (type) {
    case PeripheralType::None: return "None";
    case PeripheralType::ControlPad: return "Saturn Control Pad";
    default: return "Invalid";
    }
}

inline constexpr PeripheralType kTypes[] = {PeripheralType::None, PeripheralType::ControlPad};

class NullPeripheral;
class ControlPad;

namespace detail {

    template <PeripheralType type>
    struct PeripheralTypeMeta {};

    template <>
    struct PeripheralTypeMeta<PeripheralType::None> {
        using type = NullPeripheral;
    };

    template <>
    struct PeripheralTypeMeta<PeripheralType::ControlPad> {
        using type = ControlPad;
    };

    template <PeripheralType type>
    using PeripheralType_t = typename PeripheralTypeMeta<type>::type;

} // namespace detail

} // namespace ymir::peripheral

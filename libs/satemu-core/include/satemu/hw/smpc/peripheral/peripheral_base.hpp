#pragma once

#include "peripheral_defs.hpp"

#include <satemu/core/types.hpp>

#include <satemu/util/inline.hpp>

#include <span>

namespace satemu::peripheral {

class BasePeripheral {
public:
    BasePeripheral(PeripheralType type, uint8 typeCode, uint8 reportLength)
        : m_type(type)
        , m_typeCode(typeCode)
        , m_reportLength(reportLength) {}

    virtual ~BasePeripheral() = default;

    // If this peripheral object has the specified PeripheralType, casts it to the corresponding concrete type.
    // Returns nullptr otherwise.
    template <PeripheralType type>
    FORCE_INLINE typename detail::PeripheralType_t<type> *As() {
        if (m_type == type) {
            return static_cast<detail::PeripheralType_t<type> *>(this);
        } else {
            return nullptr;
        }
    }

    // If this peripheral object has the specified PeripheralType, casts it to the corresponding concrete type.
    // Returns nullptr otherwise.
    template <PeripheralType type>
    FORCE_INLINE const typename detail::PeripheralType_t<type> *As() const {
        if (m_type == type) {
            return static_cast<detail::PeripheralType_t<type> *>(this);
        } else {
            return nullptr;
        }
    }

    std::string_view GetName() const {
        return GetPeripheralName(m_type);
    }

    PeripheralType GetType() const {
        return m_type;
    }

    uint8 GetTypeCode() const {
        return m_typeCode;
    }

    uint8 GetReportLength() const {
        return m_reportLength;
    }

    bool IsConnected() const {
        return m_reportLength > 0;
    }

    virtual void Read(std::span<uint8> out) = 0;

    virtual uint8 WritePDR(uint8 ddr, uint8 value) = 0;

private:
    const PeripheralType m_type;
    uint8 m_typeCode;
    uint8 m_reportLength;
};

} // namespace satemu::peripheral

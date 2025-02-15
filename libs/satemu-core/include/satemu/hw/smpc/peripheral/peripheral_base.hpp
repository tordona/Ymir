#pragma once

#include <satemu/core/types.hpp>

#include <span>

namespace satemu::peripheral {

class BasePeripheral {
public:
    BasePeripheral(uint8 type, uint8 reportLength)
        : m_type(type)
        , m_reportLength(reportLength) {}

    virtual ~BasePeripheral() = default;

    uint8 GetType() const {
        return m_type;
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
    uint8 m_type;
    uint8 m_reportLength;
};

} // namespace satemu::peripheral

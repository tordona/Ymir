#pragma once

#include "peripheral_base.hpp"
#include "peripheral_impl_null.hpp"
#include "peripheral_impl_standard_pad.hpp"

#include <satemu/core/types.hpp>

#include <memory>
#include <span>
#include <utility>

// -----------------------------------------------------------------------------
// Forward declarations

namespace satemu::smpc {

class SMPC;

} // namespace satemu::smpc

// -----------------------------------------------------------------------------

namespace satemu::peripheral {

class PeripheralPort {
public:
    PeripheralPort() {
        DisconnectPeripherals();
    }

    StandardPad *ConnectStandardPad() {
        return ConnectPeripheral<StandardPad>();
    }

    void DisconnectPeripherals() {
        ConnectPeripheral<NullPeripheral>();
    }

private:
    // TODO: implement multi-tap as an array of peripherals
    std::unique_ptr<BasePeripheral> m_peripheral;

    template <typename T, typename... Args>
        requires std::derived_from<T, BasePeripheral>
    T *ConnectPeripheral(Args &&...args) {
        T *peripheral = new T(std::forward<Args>(args)...);
        m_peripheral.reset(peripheral);
        return peripheral;
    }

    friend class smpc::SMPC;

    uint32 GetReportLength() const {
        if (m_peripheral->IsConnected()) {
            return 2 + m_peripheral->GetReportLength();
        } else {
            return 1;
        }
    }

    void Read(std::span<uint8> out) const {
        assert(out.size() == GetReportLength());

        if (m_peripheral->IsConnected() && m_peripheral->GetReportLength() <= 15) {
            // TODO: support multi-tap
            // TODO: support report lengths longer than 15
            // [0] 0xF1 -> 7-4 = F=no multitap/device directly connected; 3-0 = 1 device
            // [1] 0x02 -> 7-4 = 0=standard pad; 3-0 = 2 data bytes
            // [2..N]   -> peripheral-specific report
            out[0] = 0xF1;
            out[1] = (m_peripheral->GetType() << 4u) | m_peripheral->GetReportLength();
            m_peripheral->Read(out.subspan(2));
        } else {
            // [0] 0xF0 -> 7-4 = F=no multitap/device directly connected; 3-0 = 0 devices
            out[0] = 0xF0;
        }
    }

    uint8 WritePDR(uint8 ddr, uint8 value) {
        return m_peripheral->WritePDR(ddr, value);
    }
};

} // namespace satemu::peripheral

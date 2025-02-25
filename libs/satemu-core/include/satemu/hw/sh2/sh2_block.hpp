#pragma once

#include "sh2.hpp"

#include <satemu/sys/bus.hpp>

namespace satemu::sh2 {

class SH2Block {
public:
    SH2Block()
        : master(bus, true)
        , slave(bus, false) {}

    sys::Bus bus;
    SH2 master;
    SH2 slave;

    bool slaveEnabled;

    void Reset(bool hard) {
        master.Reset(hard);
        slave.Reset(hard);

        slaveEnabled = false;
    }
};

} // namespace satemu::sh2

#pragma once

#include "sh2.hpp"
#include "sh2_bus.hpp"

namespace satemu::sh2 {

class SH2Block {
public:
    SH2Block()
        : bus(master, slave)
        , master(bus, true)
        , slave(bus, false) {}

    SH2Bus bus;
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

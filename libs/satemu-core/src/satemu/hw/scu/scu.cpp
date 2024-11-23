#include <satemu/hw/scu/scu.hpp>

namespace satemu {

SCU::SCU() {
    Reset(true);
}

void SCU::Reset(bool hard) {}

} // namespace satemu

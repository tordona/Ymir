#pragma once

namespace satemu::config {

// true = run the MC68EC000 on the scheduled SCSP::Tick() function.
// false = run the MC68EC000 on the Saturn::Step() function.
//
// Panzer Dragoon Saga breaks when the CPU ticks with the SCSP.
inline constexpr bool runM68KOnSCSPTick = true;

} // namespace satemu::config

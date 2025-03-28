#pragma once

namespace satemu::sys {

struct SystemFeatures {
    bool enableDebugTracing = false;
    bool emulateSH2Cache = false;
};

} // namespace satemu::sys

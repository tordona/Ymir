#pragma once

#include <filesystem>
#include <string>

// ---------------------------------------------------------------------------------------------------------------------
// Forward declarations

namespace ymir {
struct Saturn;
}

// ---------------------------------------------------------------------------------------------------------------------

namespace util {

struct IPLROMLoadResult {
    bool succeeded;
    std::string errorMessage;

    static IPLROMLoadResult Success() {
        return {true};
    }

    static IPLROMLoadResult Fail(std::string message) {
        return {false, message};
    }
};

IPLROMLoadResult LoadIPLROM(std::filesystem::path path, ymir::Saturn &saturn);

} // namespace util

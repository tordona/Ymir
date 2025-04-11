#pragma once

#include <filesystem>
#include <string>

// ---------------------------------------------------------------------------------------------------------------------
// Forward declarations

namespace satemu {
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

IPLROMLoadResult LoadIPLROM(std::filesystem::path path, satemu::Saturn &saturn);

} // namespace util

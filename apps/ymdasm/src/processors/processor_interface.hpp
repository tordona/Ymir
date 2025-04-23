#pragma once

#include <string_view>

class ICommandProcessor {
public:
    // Processes a named option with an argument.
    // Returns true if successful, false if failed.
    virtual bool Option(std::string_view name, std::string_view args) = 0;

    // Processes a named flag.
    // Returns true if successful, false if failed.
    virtual bool Flag(std::string_view name, bool value) = 0;

    // Processes an argument.
    // Returns true if successful, false if failed.
    virtual bool Argument(std::string_view arg) = 0;
};

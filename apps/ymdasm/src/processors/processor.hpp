#pragma once

#include "option_info.hpp"

#include <span>
#include <string_view>
#include <unordered_map>

class CommandProcessor {
public:
    virtual ~CommandProcessor() = default;

    // Gets all options supported by the command.
    virtual const std::span<const OptionInfo *> GetOptions() const = 0;

    // Gets information about a long option.
    // Returns nullptr if the option is not accepted by the command.
    virtual const OptionInfo *LongOptionInfo(std::string_view name) const = 0;

    // Gets information about a short option.
    // Returns nullptr if the option is not accepted by the command.
    virtual const OptionInfo *ShortOptionInfo(char name) const = 0;

    // Processes a long option with an argument.
    // Returns true if successful, false if failed.
    virtual bool LongOption(std::string_view name, std::string_view value) = 0;

    // Processes a long flag.
    // Returns true if successful, false if failed.
    virtual bool LongFlag(std::string_view name, bool value) = 0;

    // Processes a short option with an argument.
    // Returns true if successful, false if failed.
    virtual bool ShortOption(char name, std::string_view value) = 0;

    // Processes a short flag.
    // Returns true if successful, false if failed.
    virtual bool ShortFlag(char name, bool value) = 0;

    // Processes an argument.
    // Returns true if successful, false if failed.
    virtual bool Argument(std::string_view arg) = 0;
};

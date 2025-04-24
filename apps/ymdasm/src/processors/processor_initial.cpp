#include "processor_initial.hpp"

#include <fmt/format.h>

#include <unordered_map>

// RegisterOption('a', "abc", "value", "option A");
// RegisterOption('x', "xyz", "flag B");

const std::span<const OptionInfo *> InitialCommandProcessor::GetOptions() const {
    return {};
}

const OptionInfo *InitialCommandProcessor::LongOptionInfo(std::string_view name) const {
    return nullptr;
}

const OptionInfo *InitialCommandProcessor::ShortOptionInfo(char name) const {
    return nullptr;
}

bool InitialCommandProcessor::LongOption(std::string_view name, std::string_view value) {
    fmt::println("Received long option {} = {}", name, value);
    return true;
}

bool InitialCommandProcessor::LongFlag(std::string_view name, bool value) {
    fmt::println("Received long flag {} = {}", name, value);
    return true;
}

bool InitialCommandProcessor::ShortOption(char name, std::string_view value) {
    fmt::println("Received short option {} = {}", name, value);
    return true;
}

bool InitialCommandProcessor::ShortFlag(char name, bool value) {
    fmt::println("Received short flag {} = {}", name, value);
    return true;
}

bool InitialCommandProcessor::Argument(std::string_view arg) {
    fmt::println("Received argument {}", arg);
    return true;
}

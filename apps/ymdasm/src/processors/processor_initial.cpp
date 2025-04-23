#include "processor_initial.hpp"

#include <fmt/format.h>

bool InitialCommandProcessor::Option(std::string_view name, std::string_view args) {
    fmt::println("Received option {} = {}", name, args);
    return false;
}

bool InitialCommandProcessor::Flag(std::string_view name, bool value) {
    fmt::println("Received flag {} = {}", name, value);
    return false;
}

bool InitialCommandProcessor::Argument(std::string_view arg) {
    fmt::println("Received argument {}", arg);
    return false;
}

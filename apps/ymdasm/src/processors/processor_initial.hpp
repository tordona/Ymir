#include "processor_interface.hpp"

class InitialCommandProcessor : public ICommandProcessor {
public:
    bool Option(std::string_view name, std::string_view args) final;
    bool Flag(std::string_view name, bool value) final;
    bool Argument(std::string_view arg) final;
};

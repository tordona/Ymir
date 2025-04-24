#include "processor.hpp"

class InitialCommandProcessor : public CommandProcessor {
public:
    const std::span<const OptionInfo *> GetOptions() const final;
    const OptionInfo *LongOptionInfo(std::string_view name) const final;
    const OptionInfo *ShortOptionInfo(char name) const final;

    bool LongOption(std::string_view name, std::string_view value) final;
    bool LongFlag(std::string_view name, bool value) final;

    bool ShortOption(char name, std::string_view value) final;
    bool ShortFlag(char name, bool value) final;

    bool Argument(std::string_view arg) final;
};

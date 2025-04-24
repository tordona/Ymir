#pragma once

struct OptionInfo {
    const char shortName;
    const char *longName;
    const char *argument; // if nullptr, is a flag
    const char *description;
};

#pragma once

/**
@file
@brief Game database.

Contains information about specific games that require special handling.
*/

#include <string_view>

namespace ymir::db {

/// @brief The cartridge required for the game to work
enum class Cartridge { None, DRAM8Mbit, DRAM32Mbit, ROM_KOF95, ROM_Ultraman };

/// @brief Information about a game in the database.
struct GameInfo {
    Cartridge cartridge = Cartridge::None; ///< Cartridge required for the game to work
    bool sh2Cache = false;                 ///< SH-2 cache emulation required for the game to work
};

/// @brief Retrieves information about a game image given its product code.
///
/// Returns `nullptr` if there is no information for the given product code.
///
/// @param[in] productCode the product code to check
/// @return a pointer to `GameInfo` containing information about the game, or `nullptr` if no matching information was
/// found
const GameInfo *GetGameInfo(std::string_view productCode);

} // namespace ymir::db

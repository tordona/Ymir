#include <ymir/db/game_db.hpp>

namespace ymir::db {

// Table adapted from Mednafen
// https://github.com/libretro-mirrors/mednafen-git/blob/master/src/ss/db.cpp

// clang-format off
static const std::unordered_map<std::string_view, GameInfo> kGameInfos = {
    {"MK-81088", {Cartridge::ROM_KOF95}},    // King of Fighters '95, The (Europe)
    {"T-3101G",  {Cartridge::ROM_KOF95}},    // King of Fighters '95, The (Japan)
    {"T-13308G", {Cartridge::ROM_Ultraman}}, // Ultraman - Hikari no Kyojin Densetsu (Japan)
    
    {"T-1521G",    {Cartridge::DRAM8Mbit}}, // Astra Superstars (Japan)
    {"T-9904G",    {Cartridge::DRAM8Mbit}}, // Cotton 2 (Japan)
    {"T-1217G",    {Cartridge::DRAM8Mbit}}, // Cyberbots (Japan)
    {"GS-9107",    {Cartridge::DRAM8Mbit}}, // Fighter's History Dynamite (Japan)
    {"T-20109G",   {Cartridge::DRAM8Mbit}}, // Friends (Japan)
    {"T-14411G",   {Cartridge::DRAM8Mbit}}, // Groove on Fight (Japan)
    {"T-7032H-50", {Cartridge::DRAM8Mbit}}, // Marvel Super Heroes (Europe)
    {"T-1215G",    {Cartridge::DRAM8Mbit}}, // Marvel Super Heroes (Japan)
    {"T-3111G",    {Cartridge::DRAM8Mbit}}, // Metal Slug (Japan)
    {"T-22205G",   {Cartridge::DRAM8Mbit}}, // NOël 3 (Japan)
    {"T-20114G",   {Cartridge::DRAM8Mbit}}, // Pia Carrot e Youkoso!! 2 (Japan)
    {"T-3105G",    {Cartridge::DRAM8Mbit}}, // Real Bout Garou Densetsu (Japan)
    {"T-99901G",   {Cartridge::DRAM8Mbit}}, // Real Bout Garou Densetsu Demo (Japan)
    {"T-3119G",    {Cartridge::DRAM8Mbit}}, // Real Bout Garou Densetsu Special (Japan)
    {"T-3116G",    {Cartridge::DRAM8Mbit}}, // Samurai Spirits - Amakusa Kourin (Japan)
    {"T-3104G",    {Cartridge::DRAM8Mbit}}, // Samurai Spirits - Zankurou Musouken (Japan)
    {"610636008",  {Cartridge::DRAM8Mbit}}, // "Tech Saturn 1997.6 (Japan)
    {"T-16509G",   {Cartridge::DRAM8Mbit}}, // Super Real Mahjong P7 (Japan)
    {"T-16510G",   {Cartridge::DRAM8Mbit}}, // Super Real Mahjong P7 (Japan)
    {"T-3108G",    {Cartridge::DRAM8Mbit}}, // The King of Fighters '96 (Japan)
    {"T-3121G",    {Cartridge::DRAM8Mbit}}, // The King of Fighters '97 (Japan)
    {"T-1515G",    {Cartridge::DRAM8Mbit}}, // Waku Waku 7 (Japan)

    {"T-1245G", {Cartridge::DRAM32Mbit}}, // Dungeons and Dragons Collection (Japan)
    {"T-1248G", {Cartridge::DRAM32Mbit}}, // Final Fight Revenge (Japan)
    {"T-1238G", {Cartridge::DRAM32Mbit}}, // Marvel Super Heroes vs. Street Fighter (Japan)
    {"T-1230G", {Cartridge::DRAM32Mbit}}, // Pocket Fighter (Japan)
    {"T-1246G", {Cartridge::DRAM32Mbit}}, // Street Fighter Zero 3 (Japan)
    {"T-1229G", {Cartridge::DRAM32Mbit}}, // Vampire Savior (Japan)
    {"T-1226G", {Cartridge::DRAM32Mbit}}, // X-Men vs. Street Fighter (Japan)
};
// clang-format on

const GameInfo *GetGameInfo(std::string_view productCode) {
    if (kGameInfos.contains(productCode)) {
        return &kGameInfos.at(productCode);
    } else {
        return nullptr;
    }
}

} // namespace ymir::db

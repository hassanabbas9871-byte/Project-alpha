#pragma once

// Everything in this file is plain C++ data. The story, combat, save files and
// tests share these types without needing a graphics window or raylib.
#include <array>
#include <string>
#include <vector>

namespace veil {
inline constexpr int BoardWidth = 12, BoardHeight = 8;
inline constexpr int HeroCount = 3, SkillCount = 18, MissionCount = 32;
struct Pos {
    int x{}, y{};
    bool operator==(const Pos&) const = default;
};
enum class Terrain { Floor, Wall, Hazard, Sigil, Exit, Cover };
enum class Objective { Hunt, Survive, Seal, Escort };
enum class Ai { Melee, Ranged, Medic, Brute, Stalker, Summoner, Artillery, Leech, Guardian, Trickster };
enum class Slot { Weapon, Armor, Charm, Boots };
enum class WeaponClass { Blade, Revolver, Cane, Charm, Rifle, Gauntlet };
enum class Perk { None, Bleed, Drain, Stun, Focus, Guard, Regen };
enum class SkillKind { Strike, Heal, Guard, Blink, Poison, Pull, Drain, Stun, Restore };
enum class Outcome { Ongoing, Victory, Defeat };
enum class Page { Title, Map, Story, Battle, Reward, Inventory, Skills, Journal, Ending };

struct EnemyDef {
    int id{};
    std::string name, description;
    Ai ai{};
    int hp{30}, attack{7}, defense{1}, range{1}, move{3}, speed{8};
    bool boss{};
};
struct ItemDef {
    int id{};
    std::string name, description;
    Slot slot{};
    WeaponClass weaponClass{};
    int attack{}, defense{}, hp{}, focus{}, move{}, price{20}, unlockChapter{};
    Perk perk{};
};
struct SkillDef {
    int id{};
    std::string name, description;
    int hero{}, focusCost{3}, range{3}, radius{}, power{100}, cooldown{2}, unlockLevel{1};
    SkillKind kind{SkillKind::Strike};
};
struct Mission {
    int id{}, chapter{};
    std::string name, location, summary;
    int biome{}, level{1};
    Objective objective{};
    std::array<std::vector<int>, 2> enemies;
    int gold{40}, xp{35};
    std::vector<std::string> intro, outro;
    std::array<std::string, 2> choices;
    std::string clueQuestion;
    std::array<std::string, 3> clueOptions;
    int clueAnswer{};
    std::string clueExplanation;
};

// Progress is the persistent part of play. Combat makes temporary Unit copies
// from it; equipment and learned abilities survive between cases.
struct Progress {
    int mission{}, wave{}, level{1}, xp{}, gold{90}, skillPoints{3}, compassion{}, clues{}, deaths{};
    bool campaignComplete{};
    double playSeconds{};
    std::array<std::array<int, 4>, HeroCount> equipment{{{{0,-1,-1,-1}},{{1,-1,-1,-1}},{{2,-1,-1,-1}}}};
    std::vector<bool> owned;
    std::array<bool, SkillCount> learned{};
    std::array<bool, MissionCount> completed{};
    std::vector<std::string> journal;
};

struct Unit {
    int id{}, source{}; // source is a hero index or EnemyDef index.
    bool party{};
    Pos cell;
    std::string name;
    int hp{}, maxHp{}, focus{}, maxFocus{}, attack{}, defense{}, range{}, move{}, speed{}, ap{};
    int bleed{}, poison{}, stun{}, guard{};
    std::array<int, SkillCount> cooldowns{};
    Perk perk{};
    bool alive() const { return hp > 0; }
};
struct Effect { Pos from, to; float life{.5f}; int kind{}, amount{}; };

const std::vector<EnemyDef>& enemies();
const std::vector<ItemDef>& items();
const std::vector<SkillDef>& skills();
const std::vector<Mission>& missions();
const std::array<std::string, HeroCount>& heroNames();
const std::array<std::string, 8>& chapterNames();
bool validateContent(std::string& reason);
} // namespace veil

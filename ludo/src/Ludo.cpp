#include "Ludo.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

// Keeping this directive inside a .cpp avoids changing the meaning of names in
// other files that include our header. It lets newcomers write string/vector
// here without having to repeat the std:: prefix on every line.
using namespace std;

namespace ludo {
namespace {
constexpr int Finish = 57;
constexpr int MaxEnergy = 6;
constexpr int MaxShields = 3;

int powerIndex(Power power) { return static_cast<int>(power); }
int classIndex(Class role) { return static_cast<int>(role); }

// A helper returns the energy price of each class's once-per-turn ability.
int abilityCost(Class role) {
    if (role == Class::Cleric) return 3;
    if (role == Class::Monk) return 1;
    return 2;
}

bool validClass(Class role) { return classIndex(role) >= 0 && classIndex(role) < 6; }
}

string modeName(Mode mode) {
    switch (mode) {
    case Mode::Basic: return "Basic Ludo";
    case Mode::Classes: return "Classes";
    case Mode::Anime: return "Anime";
    case Mode::Custom: return "Custom";
    }
    return "Basic Ludo";
}

string className(Class role, Mode mode) {
    const array<string, 6> ordinary{"Fighter", "Rogue", "Wizard", "Cleric", "Ranger", "Monk"};
    const array<string, 6> anime{"Iron Vanguard", "Storm Runner", "Star Weaver", "Dawn Keeper", "Spirit Archer", "Dragon Monk"};
    if (!validClass(role)) return "Fighter";
    return mode == Mode::Anime ? anime[classIndex(role)] : ordinary[classIndex(role)];
}

string classDescription(Class role, Mode mode) {
    // Anime uses original names for the same clearly documented mechanical kits.
    // Passing mode keeps this function convenient for both setup screens.
    (void)mode;
    switch (role) {
    case Class::Fighter: return "2 energy: give the selected cone 2 shields. Once per turn.";
    case Class::Rogue: return "2 energy: arm a +3 sprint, plus the cone's Speed. Once per turn.";
    case Class::Wizard: return "2 energy: change the selected unused die to 1. Once per turn.";
    case Class::Cleric: return "3 energy: give every deployed cone 1 shield. Once per turn.";
    case Class::Ranger: return "2 energy: arm a +1 sprint plus Speed, and add 1 shield. Once per turn.";
    case Class::Monk: return "1 energy: arm a +2 sprint, plus the cone's Speed. Once per turn.";
    }
    return "Choose a class to see its ability.";
}

string powerName(Power power) {
    switch (power) {
    case Power::Shield: return "Shield";
    case Power::Sprint: return "Sprint";
    case Power::Reroll: return "Reroll";
    }
    return "Power-up";
}

string coneName(int index) {
    const array<string, 4> names{"Scout", "Sentinel", "Fortune", "Balanced"};
    return index >= 0 && index < 4 ? names[index] : "Cone";
}

string rulesSummary(const Config& setup) {
    ostringstream text;
    text << "Roll " << (setup.diceCount == 2 ? "two D6; spend them separately in either order" : "one D6")
         << ". Every cone needs a raw 1 to leave base. Choose any of your cones for each unused die. "
         << "Travel all 52 outer cells, then your own five-cell home lane and the center. Land exactly on the center. "
         << (setup.firstConeWins ? "Your first cone home earns your place. " : "All four cones must reach home to earn your place. ")
         << "A 6 gives no extra roll. Unusable dice are forfeited when the turn ends. ";
    if (setup.captures) text << "Landing on an opponent sends it to base" << (setup.safeSquares ? ", except on marked safe cells. " : ". ");
    else text << "Captures are off; opposing cones may share a cell. ";
    text << "Friendly cones may share cells; there are no blockades. The last remaining player receives the final place.";
    return text.str();
}

Game::Game(Config setup) : config(std::move(setup)), random(config.seed) {
    // Presets are normalized in the rules layer too: Basic can never acquire a
    // hidden class/stat advantage, even if a caller accidentally left a toggle on.
    if (config.mode == Mode::Basic) {
        config.powerups = false;
        config.classAbilities = false;
        config.uniqueStats = false;
    } else if (config.mode == Mode::Classes) {
        config.powerups = true;
        config.classAbilities = true;
        config.uniqueStats = true;
    }
    // Anime and Custom intentionally honor their toggles, which allows the three
    // anime presets and user-created combinations to share this exact engine.
    config.diceCount = clamp(config.diceCount, 1, 2);
    if (config.players.empty()) config.players.push_back({"Player 1", false, Class::Fighter});
    if (config.players.size() > 4) config.players.resize(4);

    const array<Stats, 4> personalities{{{2, 0, 0}, {0, 1, 0}, {0, 0, 2}, {1, 0, 1}}};
    for (size_t index = 0; index < config.players.size(); ++index) {
        auto& selection = config.players[index];
        if (selection.name.empty()) selection.name = "Player " + to_string(index + 1);
        if (!validClass(selection.heroClass)) selection.heroClass = Class::Fighter;
        Player player;
        player.name = selection.name;
        player.computer = selection.computer;
        player.heroClass = selection.heroClass;
        if (config.powerups) player.cards = {1, 1, 1};
        for (int cone = 0; cone < 4; ++cone)
            player.cones[cone].stats = config.uniqueStats ? personalities[cone] : Stats{};
        players.push_back(player);
    }
    record("Game started. " + players[current].name + " rolls first.");
}

void Game::record(const string& message) {
    history.push_back(message);
    // Keeping a short history prevents a very long match from growing memory.
    while (history.size() > 40) history.pop_front();
}

bool Game::activeTurn() const {
    return !over && current >= 0 && current < static_cast<int>(players.size()) && players[current].rank == 0;
}

bool Game::validDie(int index) const {
    return index >= 0 && index < config.diceCount && rolled && !spent[index] && dice[index] >= 1 && dice[index] <= 6;
}

bool Game::activeCone(int index) const {
    return index >= 0 && index < 4 && players[current].cones[index].progress >= 0
        && players[current].cones[index].progress < Finish;
}

bool Game::canArmSprint(int index, int baseBoost) const {
    if (!activeCone(index)) return false;
    const auto& cone = players[current].cones[index];
    if (cone.sprint != 0) return false;
    const int boost = min(6, baseBoost + cone.stats.speed);
    // A boost cannot be armed if it would make every remaining die overshoot.
    // Otherwise a cone two steps from home could acquire a permanent +4 sprint
    // and become impossible to finish with any D6 result.
    for (int die = 0; die < config.diceCount; ++die)
        if (validDie(die) && cone.progress + dice[die] + boost <= Finish) return true;
    return false;
}

bool Game::roll() {
    if (!activeTurn() || rolled) return false;
    uniform_int_distribution<int> d6(1, 6);
    array<int, 2> values{d6(random), d6(random)};
    return rollWith(values);
}

bool Game::rollWith(array<int, 2> values) {
    if (!activeTurn() || rolled) return false;
    for (int index = 0; index < config.diceCount; ++index)
        if (values[index] < 1 || values[index] > 6) return false;
    rolled = true;
    players[current].turnsTaken++;
    dice = values;
    spent = {false, config.diceCount == 1};
    if (config.diceCount == 1) dice[1] = 0;
    record(players[current].name + " rolled " + to_string(dice[0])
           + (config.diceCount == 2 ? " and " + to_string(dice[1]) : "") + ".");
    return true;
}

int Game::moveDistance(int dieIndex, int coneIndex) const {
    if (!activeTurn() || !validDie(dieIndex) || coneIndex < 0 || coneIndex >= 4) return 0;
    const auto& cone = players[current].cones[coneIndex];
    if (cone.progress < 0) return dice[dieIndex];
    return dice[dieIndex] + cone.sprint;
}

bool Game::canMove(int dieIndex, int coneIndex) const {
    if (!activeTurn() || !validDie(dieIndex) || coneIndex < 0 || coneIndex >= 4) return false;
    const auto& cone = players[current].cones[coneIndex];
    if (cone.progress < 0) return dice[dieIndex] == 1;
    return cone.progress < Finish && cone.progress + moveDistance(dieIndex, coneIndex) <= Finish;
}

vector<int> Game::legalMoves(int dieIndex) const {
    vector<int> result;
    for (int cone = 0; cone < 4; ++cone) if (canMove(dieIndex, cone)) result.push_back(cone);
    return result;
}

int Game::absolutePosition(int playerIndex, int coneIndex) const {
    if (playerIndex < 0 || playerIndex >= static_cast<int>(players.size()) || coneIndex < 0 || coneIndex >= 4) return -1;
    const int progress = players[playerIndex].cones[coneIndex].progress;
    return progress >= 0 && progress < 52 ? (playerIndex * 13 + progress) % 52 : -1;
}

bool Game::safe(int absoluteIndex) const {
    if (!config.safeSquares || absoluteIndex < 0 || absoluteIndex >= 52) return false;
    const int quarter = absoluteIndex % 13;
    return quarter == 0 || quarter == 8;
}

bool Game::pickup(int absoluteIndex) {
    return absoluteIndex >= 0 && absoluteIndex < 52 && absoluteIndex % 8 == 4;
}

void Game::collect(Cone& cone, int absoluteIndex) {
    // The Mecha anime preset keeps classes and cone stats but switches cards
    // off. Its glowing pickup cells still supply energy, so Luck remains useful.
    if ((!config.powerups && !config.classAbilities) || !pickup(absoluteIndex)) return;
    auto& player = players[current];
    player.energy = min(MaxEnergy, player.energy + 1 + cone.stats.luck);
    if (!config.powerups) {
        record(player.name + " collected energy.");
        return;
    }
    uniform_int_distribution<int> reward(0, 2);
    const int card = reward(random);
    const bool added = player.cards[card] < 3;
    player.cards[card] = min(3, player.cards[card] + 1);
    record(player.name + " collected " + powerName(static_cast<Power>(card))
           + (added ? " and energy." : " (inventory full); gained energy."));
}

void Game::rankCurrent() {
    auto& player = players[current];
    const int finished = static_cast<int>(count_if(player.cones.begin(), player.cones.end(), [](const Cone& cone) { return cone.progress == Finish; }));
    if (finished < (config.firstConeWins ? 1 : 4)) return;
    player.rank = static_cast<int>(podium.size()) + 1;
    podium.push_back(current);
    record(player.name + " takes place " + to_string(player.rank) + "!");
    int remaining = 0;
    int last = -1;
    for (int index = 0; index < static_cast<int>(players.size()); ++index)
        if (players[index].rank == 0) { ++remaining; last = index; }
    // A solo run ends only when that player finishes. In multiplayer, there is
    // no suspense left once one player remains, so that player gets last place.
    if (players.size() > 1 && remaining == 1) {
        players[last].rank = static_cast<int>(podium.size()) + 1;
        podium.push_back(last);
        record(players[last].name + " receives the final place.");
        remaining = 0;
    }
    over = remaining == 0;
    if (over) record("Match complete.");
}

bool Game::move(int dieIndex, int coneIndex) {
    if (!canMove(dieIndex, coneIndex)) return false;
    auto& player = players[current];
    auto& cone = player.cones[coneIndex];
    spent[dieIndex] = true;
    const bool deployed = cone.progress == -1;
    // canMove already checked the unused die. Once we mark it spent above,
    // movement uses its stored face directly rather than querying legality again.
    if (deployed) {
        cone.progress = 0;
        cone.shields = min(MaxShields, cone.stats.guard);
        cone.sprint = 0;
        record(player.name + " deployed cone " + to_string(coneIndex + 1) + " with a 1.");
    } else {
        const int steps = dice[dieIndex] + cone.sprint;
        cone.progress += steps;
        cone.sprint = 0;
        record(player.name + " moved cone " + to_string(coneIndex + 1) + " by " + to_string(steps) + ".");
    }
    const int destination = absolutePosition(current, coneIndex);
    if (config.captures && destination >= 0 && !safe(destination)) {
        for (int other = 0; other < static_cast<int>(players.size()); ++other) {
            if (other == current || players[other].rank != 0) continue;
            for (int victim = 0; victim < 4; ++victim) {
                if (absolutePosition(other, victim) != destination) continue;
                auto& target = players[other].cones[victim];
                if (target.shields > 0) {
                    target.shields--;
                    record(players[other].name + " blocked a capture with a shield.");
                } else {
                    target.progress = -1;
                    target.shields = 0;
                    target.sprint = 0;
                    record(player.name + " captured " + players[other].name + "'s cone " + to_string(victim + 1) + ".");
                }
            }
        }
    }
    collect(cone, destination);
    if (cone.progress == Finish) {
        cone.shields = 0;
        record(player.name + " brought cone " + to_string(coneIndex + 1) + " home.");
        rankCurrent();
    }
    return true;
}

bool Game::canEndTurn() const {
    if (over || !rolled) return false;
    if (players[current].rank > 0) return true;
    for (int die = 0; die < config.diceCount; ++die)
        if (!legalMoves(die).empty()) return false;
    return true;
}

bool Game::endTurn() {
    if (!canEndTurn()) return false;
    do { current = (current + 1) % static_cast<int>(players.size()); }
    while (players[current].rank > 0);
    ++turn;
    rolled = false;
    dice = {0, 0};
    spent = {true, true};
    abilityUsed = false;
    rerollUsed = false;
    auto& player = players[current];
    if (player.turnsTaken > 0) player.energy = min(MaxEnergy, player.energy + 1);
    record(player.name + "'s turn.");
    return true;
}

bool Game::canUsePower(Power power, int coneIndex, int dieIndex) const {
    if (!activeTurn() || !rolled || !config.powerups) return false;
    const int card = powerIndex(power);
    if (card < 0 || card >= 3 || players[current].cards[card] <= 0) return false;
    if (power == Power::Reroll) return !rerollUsed && validDie(dieIndex);
    if (!activeCone(coneIndex)) return false;
    const auto& cone = players[current].cones[coneIndex];
    if (power == Power::Shield) return cone.shields < MaxShields;
    return power == Power::Sprint && canArmSprint(coneIndex, 2);
}

bool Game::usePower(Power power, int coneIndex, int dieIndex) {
    if (!canUsePower(power, coneIndex, dieIndex)) return false;
    auto& player = players[current];
    --player.cards[powerIndex(power)];
    if (power == Power::Reroll) {
        uniform_int_distribution<int> d6(1, 6);
        dice[dieIndex] = d6(random);
        rerollUsed = true;
        record(player.name + " rerolled die " + to_string(dieIndex + 1) + " to " + to_string(dice[dieIndex]) + ".");
    } else {
        auto& cone = player.cones[coneIndex];
        if (power == Power::Shield) cone.shields = min(MaxShields, cone.shields + 1);
        else cone.sprint = min(6, 2 + cone.stats.speed);
        record(player.name + " used " + powerName(power) + " on cone " + to_string(coneIndex + 1) + ".");
    }
    return true;
}

bool Game::canUseAbility(int coneIndex, int dieIndex) const {
    if (!activeTurn() || !rolled || !config.classAbilities || abilityUsed) return false;
    const auto& player = players[current];
    if (player.energy < abilityCost(player.heroClass)) return false;
    if (player.heroClass == Class::Wizard) return validDie(dieIndex) && dice[dieIndex] != 1;
    if (player.heroClass == Class::Cleric)
        return any_of(player.cones.begin(), player.cones.end(), [](const Cone& cone) {
            return cone.progress >= 0 && cone.progress < Finish && cone.shields < MaxShields;
        });
    if (!activeCone(coneIndex)) return false;
    const auto& cone = player.cones[coneIndex];
    if (player.heroClass == Class::Fighter) return cone.shields < MaxShields;
    const int baseBoost = player.heroClass == Class::Rogue ? 3 : player.heroClass == Class::Ranger ? 1 : 2;
    return canArmSprint(coneIndex, baseBoost);
}

bool Game::useAbility(int coneIndex, int dieIndex) {
    if (!canUseAbility(coneIndex, dieIndex)) return false;
    auto& player = players[current];
    player.energy -= abilityCost(player.heroClass);
    abilityUsed = true;
    if (player.heroClass == Class::Wizard) dice[dieIndex] = 1;
    else if (player.heroClass == Class::Cleric) {
        for (auto& cone : player.cones)
            if (cone.progress >= 0 && cone.progress < Finish) cone.shields = min(MaxShields, cone.shields + 1);
    } else {
        auto& cone = player.cones[coneIndex];
        switch (player.heroClass) {
        case Class::Fighter: cone.shields = min(MaxShields, cone.shields + 2); break;
        case Class::Rogue: cone.sprint = min(6, 3 + cone.stats.speed); break;
        case Class::Ranger:
            cone.sprint = min(6, 1 + cone.stats.speed);
            cone.shields = min(MaxShields, cone.shields + 1);
            break;
        case Class::Monk: cone.sprint = min(6, 2 + cone.stats.speed); break;
        default: break; // Wizard and Cleric were already handled above.
        }
    }
    record(player.name + " used " + className(player.heroClass, config.mode) + "'s ability.");
    return true;
}

} // namespace ludo

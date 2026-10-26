#pragma once

// This file describes the rules without knowing anything about windows, mice,
// colors, or raylib. That separation lets us test a whole game without a screen.
#include <array>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace ludo {

// An enum gives meaningful names to a small, fixed collection of choices.
enum class Mode { Basic, Classes, Anime, Custom };
enum class Class { Fighter, Rogue, Wizard, Cleric, Ranger, Monk };
enum class Power { Shield, Sprint, Reroll };

struct Stats {
    int speed = 0; // Added ONLY when the player deliberately activates a sprint.
    int guard = 0; // Shield charges received when this cone leaves its base.
    int luck = 0;  // Extra energy received when this cone collects a power-up.
};

struct Cone {
    // -1 means base; 0..51 means outer circuit; 52..56 means private home lane;
    // 57 means the cone has reached the center and can never move again.
    int progress = -1;
    int shields = 0;
    int sprint = 0; // An optional boost already armed for the NEXT ordinary move.
    Stats stats{};
};

struct PlayerSetup {
    std::string name = "Player";
    bool computer = false;
    Class heroClass = Class::Fighter;
};

struct Config {
    Mode mode = Mode::Basic;
    int diceCount = 1;
    bool firstConeWins = false; // Normal race: bring all four cones home.
    bool powerups = true;
    bool classAbilities = true;
    bool uniqueStats = true;
    bool captures = true;
    bool safeSquares = true;
    std::vector<PlayerSetup> players{{"Player 1", false, Class::Fighter},
                                     {"Computer", true, Class::Rogue}};
    unsigned seed = 2026;
};

struct Player {
    std::string name;
    bool computer = false;
    Class heroClass = Class::Fighter;
    std::array<Cone, 4> cones{};
    std::array<int, 3> cards{}; // Indexed by Power: shield, sprint, reroll.
    int energy = 3;
    int rank = 0; // Zero means still playing; 1 means first place, and so on.
    int turnsTaken = 0;
};

// These small helpers are also used by the interface and by the instructions.
std::string modeName(Mode mode);
std::string className(Class role, Mode mode = Mode::Classes);
std::string classDescription(Class role, Mode mode = Mode::Classes);
std::string powerName(Power power);
std::string coneName(int index);
std::string rulesSummary(const Config& config);

class Game {
public:
    explicit Game(Config setup = {});

    // Public state is deliberately easy to inspect while learning. In a larger
    // project it could be hidden behind getters. Mutating it directly bypasses
    // the rules, so the actual app always uses the methods below for actions.
    Config config;
    std::vector<Player> players;
    int current = 0;
    int turn = 1;
    std::array<int, 2> dice{0, 0};
    std::array<bool, 2> spent{true, true};
    bool rolled = false;
    bool over = false;
    std::vector<int> podium; // Player indices, ordered from first to last place.
    std::deque<std::string> history;
    bool abilityUsed = false;
    bool rerollUsed = false;

    bool roll();
    // The test suite uses fixed dice to reproduce edge cases. A real turn uses
    // roll(), which obtains both values from the game's seeded random engine.
    bool rollWith(std::array<int, 2> values);
    std::vector<int> legalMoves(int dieIndex) const;
    bool canMove(int dieIndex, int coneIndex) const;
    int moveDistance(int dieIndex, int coneIndex) const;
    bool move(int dieIndex, int coneIndex);
    bool canEndTurn() const;
    bool endTurn();
    bool canUsePower(Power power, int coneIndex, int dieIndex = 0) const;
    bool usePower(Power power, int coneIndex, int dieIndex = 0);
    bool canUseAbility(int coneIndex, int dieIndex = 0) const;
    bool useAbility(int coneIndex, int dieIndex = 0);
    int absolutePosition(int playerIndex, int coneIndex) const;
    bool safe(int absoluteIndex) const;
    static bool pickup(int absoluteIndex);

private:
    std::mt19937 random;
    void record(const std::string& message);
    void rankCurrent();
    bool activeTurn() const;
    bool validDie(int index) const;
    bool activeCone(int index) const;
    bool canArmSprint(int index, int baseBoost) const;
    void collect(Cone& cone, int absoluteIndex);
};

} // namespace ludo

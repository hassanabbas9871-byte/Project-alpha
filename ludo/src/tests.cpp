// This executable checks the real Ludo rules without opening a game window.
// A failed check prints its explanation and returns a nonzero exit status,
// which tells CTest (and the Windows build helper) that something needs fixing.
#include "Ludo.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace ludo;

namespace {
int passed = 0;
int failed = 0;

// Give every check a useful sentence instead of an unexplained assertion.
void check(bool condition, const string& description) {
    cout << (condition ? "PASS " : "FAIL ") << description << '\n';
    condition ? ++passed : ++failed;
}

// Most examples use two human seats so a computer controller cannot hide a
// rules error. The controller is a separate part of the graphical application.
Config settings(Mode mode = Mode::Basic, int seats = 2, int dice = 1,
                bool first = true, unsigned seed = 2026) {
    Config result;
    result.mode = mode;
    result.diceCount = dice;
    result.firstConeWins = first;
    result.seed = seed;
    result.players.clear();
    for (int index = 0; index < seats; ++index) {
        result.players.push_back({"Seat " + to_string(index + 1), false,
                                  static_cast<Class>(index % 6)});
    }
    return result;
}

// A compact snapshot lets an invalid-action test verify that *all* important
// visible state stayed the same, rather than only checking the return value.
string snapshot(const Game& game) {
    ostringstream out;
    out << game.current << ',' << game.turn << ',' << game.rolled << ',' << game.over
        << ',' << game.abilityUsed << ',' << game.rerollUsed;
    for (int die = 0; die < 2; ++die) out << ',' << game.dice[die] << ',' << game.spent[die];
    for (const auto& player : game.players) {
        out << '|' << player.name << ',' << player.energy << ',' << player.rank << ',' << player.turnsTaken;
        for (int cards : player.cards) out << ',' << cards;
        for (const auto& cone : player.cones) {
            out << ':' << cone.progress << ',' << cone.shields << ',' << cone.sprint;
        }
    }
    for (int place : game.podium) out << '/' << place;
    for (const auto& event : game.history) out << '\n' << event;
    return out.str();
}

void ordinaryRules() {
    Game game(settings());
    check(!game.rolled && game.current == 0 && game.turn == 1,
          "a new game starts with the first player waiting to roll");
    bool allBased = true;
    for (const auto& player : game.players)
        for (const auto& cone : player.cones) allBased &= cone.progress == -1;
    check(allBased, "all four cones for every player begin inside their base");

    const string before = snapshot(game);
    check(!game.move(0, 0) && !game.endTurn() && !game.usePower(Power::Shield, 0)
          && snapshot(game) == before,
          "moving, passing and spending powers before the roll change nothing");

    bool strictDeployment = true;
    for (int value = 2; value <= 6; ++value) {
        Game waiting(settings());
        strictDeployment &= waiting.rollWith({value, 1}) && waiting.legalMoves(0).empty()
            && !waiting.move(0, 0) && waiting.canEndTurn() && waiting.endTurn()
            && waiting.current == 1;
    }
    check(strictDeployment, "a base cone requires a literal 1; values 2 through 6 only pass the turn");

    check(game.rollWith({1, 6}) && game.legalMoves(0).size() == 4,
          "a 1 offers a choice of all four of the current player's base cones");
    check(game.move(0, 2) && game.players[0].cones[2].progress == 0
          && game.players[0].cones[0].progress == -1 && game.spent[0],
          "deployment spends one die on exactly the selected cone");
    const string deployed = snapshot(game);
    check(!game.move(0, 2) && !game.roll() && !game.rollWith({1, 1})
          && snapshot(game) == deployed,
          "a spent die cannot move twice and a player cannot reroll the turn for free");
    check(game.canEndTurn() && game.endTurn() && game.current == 1 && !game.rolled,
          "spending the only die lets the next player roll");

    Game choice(settings(Mode::Basic, 2, 2));
    check(choice.rollWith({6, 1}) && choice.legalMoves(0).empty()
          && choice.move(1, 1) && choice.canMove(0, 1)
          && choice.move(0, 1) && choice.players[0].cones[1].progress == 6,
          "two dice can be used in either order: deploy with 1, then move that cone with 6");
    check(choice.endTurn() && choice.current == 1,
          "a 6 does not grant an undocumented extra roll or turn");

    Game separate(settings(Mode::Basic, 2, 2));
    check(separate.rollWith({1, 1}) && separate.move(0, 3) && separate.move(1, 0)
          && separate.players[0].cones[3].progress == 0 && separate.players[0].cones[0].progress == 0,
          "the dice may instead deploy two different cones, with friendly stacking allowed");

    Game cannotSkip(settings());
    cannotSkip.rollWith({1, 1});
    check(!cannotSkip.canEndTurn() && !cannotSkip.endTurn(),
          "a player must spend a die when it still has a legal move");

    Game invalid(settings(Mode::Basic, 2, 2));
    const string untouched = snapshot(invalid);
    check(!invalid.rollWith({0, 6}) && !invalid.rollWith({1, 7})
          && !invalid.canMove(-1, 0) && !invalid.canMove(2, 0)
          && !invalid.canMove(0, -1) && !invalid.canMove(0, 4)
          && snapshot(invalid) == untouched,
          "invalid dice and out-of-range cone or die indices are rejected without mutation");
}

void boardAndFinishing() {
    Game rotation(settings(Mode::Basic, 4));
    for (auto& player : rotation.players) player.cones[0].progress = 0;
    check(rotation.absolutePosition(0, 0) == 0 && rotation.absolutePosition(1, 0) == 13
          && rotation.absolutePosition(2, 0) == 26 && rotation.absolutePosition(3, 0) == 39,
          "the four starting squares are one quarter of the 52-square circuit apart");
    bool safeSquares = true;
    const set<int> expectedSafe{0, 8, 13, 21, 26, 34, 39, 47};
    for (int square = 0; square < 52; ++square)
        safeSquares &= rotation.safe(square) == expectedSafe.contains(square);
    check(safeSquares, "all four starts and four starred squares protect visiting cones");

    Game home(settings());
    home.players[0].cones[0].progress = 51;
    home.rollWith({1, 1});
    check(home.move(0, 0) && home.players[0].cones[0].progress == 52,
          "the full outside circuit leads into the cone's private home lane");

    Game overshoot(settings());
    overshoot.players[0].cones[0].progress = 56;
    overshoot.rollWith({2, 1});
    check(!overshoot.canMove(0, 0) && !overshoot.move(0, 0)
          && overshoot.players[0].cones[0].progress == 56,
          "finishing needs the exact distance; an overshoot never moves a cone backwards");

    Game first(settings());
    first.players[0].cones[2].progress = 56;
    first.rollWith({1, 1});
    check(first.move(0, 2) && first.players[0].cones[2].progress == 57
          && first.over && first.podium == vector<int>({0, 1})
          && first.players[0].rank == 1 && first.players[1].rank == 2,
          "first-cone victory records the winner and assigns the final remaining player's place");
    const string ended = snapshot(first);
    check(!first.roll() && !first.move(0, 1) && !first.endTurn() && snapshot(first) == ended,
          "the completed match refuses further turns and moves");

    Game allFour(settings(Mode::Basic, 2, 1, false));
    allFour.players[0].cones[0].progress = 56;
    allFour.rollWith({1, 1});
    allFour.move(0, 0);
    check(!allFour.over && allFour.players[0].rank == 0,
          "the optional all-four goal keeps playing after just one cone finishes");
    allFour.players[0].cones[1].progress = 57;
    allFour.players[0].cones[2].progress = 57;
    allFour.players[0].cones[3].progress = 56;
    allFour.endTurn();
    allFour.rollWith({6, 6}); // The other player has no deployable cone.
    allFour.endTurn();
    allFour.rollWith({1, 1});
    check(allFour.move(0, 3) && allFour.over && allFour.players[0].rank == 1,
          "all-four victory occurs only when that player's fourth cone reaches the center");

    Game ranking(settings(Mode::Basic, 4));
    ranking.players[0].cones[0].progress = 56;
    ranking.rollWith({1, 1});
    ranking.move(0, 0);
    check(!ranking.over && ranking.players[0].rank == 1 && ranking.podium.size() == 1,
          "a four-player match continues after first place is decided");
    ranking.endTurn();
    ranking.players[1].cones[0].progress = 56;
    ranking.rollWith({1, 1});
    ranking.move(0, 0);
    ranking.endTurn();
    ranking.players[2].cones[0].progress = 56;
    ranking.rollWith({1, 1});
    ranking.move(0, 0);
    check(ranking.over && ranking.podium == vector<int>({0, 1, 2, 3}),
          "later finishers receive second and third place while the final seat receives fourth");

    Game solo(settings(Mode::Basic, 1));
    solo.players[0].cones[0].progress = 56;
    solo.rollWith({1, 1});
    check(!solo.over && solo.move(0, 0) && solo.over && solo.players[0].rank == 1,
          "a single-player race is playable and ends only when its goal is reached");
}

// Positioning a cone directly is appropriate for a focused edge-case test.
// Actual whole-match checks below start from base and use only public actions.
void captures() {
    Game capture(settings());
    capture.players[0].cones[0].progress = 4;
    capture.players[1].cones[0].progress = 45; // Absolute square 6.
    capture.rollWith({2, 1});
    check(capture.move(0, 0) && capture.players[1].cones[0].progress == -1,
          "landing on an unprotected opponent returns that opponent's cone to base");

    Game safe(settings());
    safe.players[0].cones[0].progress = 6;
    safe.players[1].cones[0].progress = 47; // Absolute square 8: a star.
    safe.rollWith({2, 1});
    check(safe.move(0, 0) && safe.players[1].cones[0].progress == 47,
          "opposing cones can share a starred safe square without a capture");

    Config noCapture = settings(Mode::Custom);
    noCapture.captures = false;
    Game peaceful(noCapture);
    peaceful.players[0].cones[0].progress = 4;
    peaceful.players[1].cones[0].progress = 45;
    peaceful.rollWith({2, 1});
    check(peaceful.move(0, 0) && peaceful.players[1].cones[0].progress == 45,
          "Custom mode can disable captures while preserving normal movement");

    Config noSafe = settings(Mode::Custom);
    noSafe.safeSquares = false;
    Game dangerous(noSafe);
    dangerous.players[0].cones[0].progress = 6;
    dangerous.players[1].cones[0].progress = 47;
    dangerous.rollWith({2, 1});
    check(dangerous.move(0, 0) && dangerous.players[1].cones[0].progress == -1,
          "Custom mode can remove safe-square protection");

    Game guard(settings(Mode::Classes));
    guard.players[0].cones[0].progress = 4;
    guard.players[1].cones[0].progress = 45;
    guard.players[1].cones[0].shields = 1;
    guard.rollWith({2, 1});
    check(guard.move(0, 0) && guard.players[1].cones[0].progress == 45
          && guard.players[1].cones[0].shields == 0,
          "a shield absorbs one capture attempt and is consumed");
}

void enhancements() {
    Game basic(settings(Mode::Basic));
    bool plain = !basic.config.powerups && !basic.config.classAbilities && !basic.config.uniqueStats;
    for (const auto& cone : basic.players[0].cones)
        plain &= cone.stats.speed == 0 && cone.stats.guard == 0 && cone.stats.luck == 0;
    for (int cards : basic.players[0].cards) plain &= cards == 0;
    basic.players[0].cones[0].progress = 4;
    basic.rollWith({2, 1});
    plain &= !basic.canUseAbility(0) && !basic.useAbility(0)
        && !basic.canUsePower(Power::Shield, 0) && !basic.usePower(Power::Shield, 0);
    check(plain, "Basic mode removes every cone stat, card and class ability even if supplied in its configuration");

    Config disabled = settings(Mode::Custom);
    disabled.powerups = disabled.classAbilities = disabled.uniqueStats = false;
    Game custom(disabled);
    bool noExtras = custom.players[0].cards == array<int, 3>{0, 0, 0};
    for (const auto& cone : custom.players[0].cones)
        noExtras &= cone.stats.speed == 0 && cone.stats.guard == 0 && cone.stats.luck == 0;
    custom.players[0].cones[0].progress = 4;
    custom.rollWith({2, 1});
    noExtras &= !custom.canUseAbility(0) && !custom.canUsePower(Power::Sprint, 0);
    check(noExtras, "Custom mode independently respects disabled powers, classes and unique stats");

    Game sentinel(settings(Mode::Classes));
    sentinel.rollWith({1, 1});
    check(sentinel.move(0, 1) && sentinel.players[0].cones[1].shields == 1,
          "the sentinel cone's guard stat supplies its shield when it actually deploys");

    Game sprint(settings(Mode::Classes));
    sprint.players[0].cones[0].progress = 4; // Scout has speed 2.
    sprint.rollWith({2, 1});
    const int speed = sprint.players[0].cones[0].stats.speed;
    check(sprint.canUsePower(Power::Sprint, 0) && sprint.usePower(Power::Sprint, 0)
          && sprint.moveDistance(0, 0) == 4 + speed
          && !sprint.canUsePower(Power::Sprint, 0) && sprint.move(0, 0)
          && sprint.players[0].cones[0].progress == 8 + speed
          && sprint.players[0].cones[0].sprint == 0,
          "an optional sprint spends its card, adds the selected cone's speed once and clears after movement");

    Game noDeploymentShortcut(settings(Mode::Classes));
    noDeploymentShortcut.rollWith({2, 1});
    check(!noDeploymentShortcut.canUsePower(Power::Sprint, 0)
          && !noDeploymentShortcut.usePower(Power::Sprint, 0)
          && !noDeploymentShortcut.canMove(0, 0),
          "speed and sprint never turn a non-1 roll into permission to leave base");

    Game shield(settings(Mode::Classes));
    shield.players[0].cones[0].progress = 5;
    shield.rollWith({2, 1});
    check(shield.usePower(Power::Shield, 0) && shield.players[0].cones[0].shields == 1
          && shield.players[0].cards[static_cast<int>(Power::Shield)] == 0
          && !shield.usePower(Power::Shield, 0),
          "a shield card protects the selected active cone and cannot be reused after spending");

    Game reroll(settings(Mode::Classes, 2, 2));
    reroll.rollWith({6, 5});
    check(reroll.usePower(Power::Reroll, 0, 1) && reroll.rerollUsed
          && reroll.dice[1] >= 1 && reroll.dice[1] <= 6 && reroll.dice[0] == 6
          && !reroll.canUsePower(Power::Reroll, 0, 0),
          "a reroll changes only the selected unspent die and is limited to one per turn");

    Game usedDie(settings(Mode::Classes, 2, 2));
    usedDie.rollWith({1, 6});
    usedDie.move(0, 0);
    check(!usedDie.canUsePower(Power::Reroll, 0, 0)
          && !usedDie.usePower(Power::Reroll, 0, 0),
          "cards cannot reroll a die after it has been spent on a move");

    bool classKits = true;
    for (Class role : {Class::Fighter, Class::Rogue, Class::Wizard,
                       Class::Cleric, Class::Ranger, Class::Monk}) {
        Config setup = settings(Mode::Classes, 2, 2);
        setup.players[0].heroClass = role;
        Game kit(setup);
        kit.players[0].cones[0].progress = 4;
        kit.players[0].cones[1].progress = 5;
        kit.rollWith({5, 3});
        const int energy = kit.players[0].energy;
        classKits &= kit.canUseAbility(0, 0) && kit.useAbility(0, 0)
            && kit.abilityUsed && kit.players[0].energy < energy
            && !kit.canUseAbility(0, 0) && !kit.useAbility(0, 0);
        const auto& cone = kit.players[0].cones[0];
        switch (role) {
            case Class::Fighter: classKits &= cone.shields == 2; break;
            case Class::Rogue: classKits &= cone.sprint == 3 + cone.stats.speed; break;
            case Class::Wizard: classKits &= kit.dice[0] == 1 && kit.canMove(0, 2); break;
            case Class::Cleric:
                classKits &= cone.shields == 1 && kit.players[0].cones[1].shields == 1;
                break;
            case Class::Ranger:
                classKits &= cone.shields == 1 && cone.sprint == 1 + cone.stats.speed;
                break;
            case Class::Monk: classKits &= cone.sprint == 2 + cone.stats.speed; break;
        }
    }
    check(classKits, "all six class kits produce their distinct advertised effect, spend energy and obey one-use-per-turn");

    Game poor(settings(Mode::Classes));
    poor.players[0].cones[0].progress = 4;
    poor.players[0].energy = 0;
    poor.rollWith({2, 1});
    check(!poor.canUseAbility(0) && !poor.useAbility(0),
          "a class ability cannot be activated without its required energy");

    Game fortune(settings(Mode::Classes));
    fortune.players[0].cones[2].progress = 2; // Fortune has luck 2.
    const int energy = fortune.players[0].energy;
    const int cards = fortune.players[0].cards[0] + fortune.players[0].cards[1] + fortune.players[0].cards[2];
    fortune.rollWith({2, 1});
    fortune.move(0, 2); // Square 4 is a power-up.
    check(fortune.players[0].energy == min(6, energy + 1 + fortune.players[0].cones[2].stats.luck)
          && fortune.players[0].cards[0] + fortune.players[0].cards[1] + fortune.players[0].cards[2] == cards + 1,
          "landing on a power-up grants a card and energy increased by that cone's luck stat");

    bool animeNames = true;
    for (Class role : {Class::Fighter, Class::Rogue, Class::Wizard,
                       Class::Cleric, Class::Ranger, Class::Monk}) {
        animeNames &= !classDescription(role, Mode::Anime).empty()
            && className(role, Mode::Anime) != className(role, Mode::Classes);
    }
    check(animeNames, "Anime mode provides six named original disciplines with descriptions");

    bool noTraps = true;
    for (Class role : {Class::Rogue, Class::Ranger, Class::Monk}) {
        Config setup = settings(Mode::Classes);
        setup.players[0].heroClass = role;
        Game nearFinish(setup);
        nearFinish.players[0].cones[0].progress = 56;
        nearFinish.rollWith({1, 1});
        noTraps &= !nearFinish.canUsePower(Power::Sprint, 0)
            && !nearFinish.usePower(Power::Sprint, 0)
            && !nearFinish.canUseAbility(0) && !nearFinish.useAbility(0)
            && nearFinish.canMove(0, 0);
    }
    check(noTraps, "sprint cards and all sprint classes cannot strand a cone beside the finish with an unusable permanent boost");

    Game carried(settings(Mode::Classes));
    carried.players[0].cones[0].progress = 50;
    carried.players[0].cones[1].progress = 5;
    carried.rollWith({1, 1});
    bool persists = carried.usePower(Power::Sprint, 0) && carried.move(0, 1)
        && carried.endTurn() && carried.rollWith({6, 6}) && carried.endTurn()
        && carried.players[0].cones[0].sprint == 4 && carried.rollWith({3, 1});
    check(persists && carried.move(0, 0) && carried.players[0].cones[0].progress == 57,
          "an unused sprint can carry to the next turn and still reach the exact finish legally");

    Config energyOnly = settings(Mode::Anime);
    energyOnly.powerups = false;
    Game mecha(energyOnly);
    mecha.players[0].cones[2].progress = 2;
    mecha.rollWith({2, 1});
    check(mecha.move(0, 2) && mecha.players[0].energy == 6
          && mecha.players[0].cards == array<int, 3>{0, 0, 0},
          "a card-free class preset still lets the Fortune cone collect its luck bonus as energy");
}

void configuration() {
    Game defaults;
    check(!defaults.config.firstConeWins,
          "the default game requires all four cones to finish, matching the selected victory rule");

    Config empty = settings();
    empty.players.clear();
    empty.diceCount = 0;
    Game low(empty);
    check(low.players.size() == 1 && !low.players[0].name.empty() && low.config.diceCount == 1,
          "an empty lobby and too few dice normalize to a valid one-player, one-die race");

    Config excess = settings(Mode::Classes, 6, 20);
    excess.players[0].name.clear();
    excess.players[0].heroClass = static_cast<Class>(999);
    Game high(excess);
    check(high.players.size() == 4 && high.config.diceCount == 2
          && !high.players[0].name.empty() && high.players[0].heroClass == Class::Fighter,
          "oversized lobbies, dice counts, blank names and invalid classes normalize safely");
}

void reproducibility() {
    Config setup = settings(Mode::Classes, 4, 2, false, 94031);
    Game first(setup);
    Game second(setup);
    bool same = snapshot(first) == snapshot(second);
    // A full replay must agree on both dice and random power-up rewards. We
    // intentionally choose identical legal actions rather than copying state.
    for (int turn = 0; turn < 1000 && !first.over; ++turn) {
        same &= first.roll() && second.roll() && snapshot(first) == snapshot(second);
        for (int action = 0; action < 2 && !first.over; ++action) {
            int selectedDie = -1;
            int selectedCone = -1;
            for (int die = 0; die < first.config.diceCount && selectedDie < 0; ++die) {
                const auto legal = first.legalMoves(die);
                same &= legal == second.legalMoves(die);
                if (!legal.empty()) {
                    selectedDie = die;
                    selectedCone = legal.front();
                }
            }
            if (selectedDie < 0) break;
            same &= first.move(selectedDie, selectedCone) && second.move(selectedDie, selectedCone)
                && snapshot(first) == snapshot(second);
        }
        if (!first.over)
            same &= first.endTurn() && second.endTurn() && snapshot(first) == snapshot(second);
    }
    check(same, "the same seed and legal choices replay identical dice, rewards, turns and ranking");
}

bool invariant(const Game& game) {
    if (game.current < 0 || game.current >= static_cast<int>(game.players.size())) return false;
    set<int> ranked;
    for (int index : game.podium) {
        if (index < 0 || index >= static_cast<int>(game.players.size()) || !ranked.insert(index).second)
            return false;
        if (game.players[index].rank != static_cast<int>(ranked.size())) return false;
    }
    for (const auto& player : game.players) {
        for (const auto& cone : player.cones)
            if (cone.progress < -1 || cone.progress > 57 || cone.shields < 0 || cone.sprint < 0) return false;
        for (int count : player.cards) if (count < 0 || count > 3) return false;
        if (player.energy < 0) return false;
    }
    return !game.over || game.podium.size() == game.players.size();
}

// Pick a legal move that advances a leading cone. This deliberately knows no
// hidden random numbers and never changes the engine's public state directly.
// Running every combination catches turn/ranking bugs missed by short examples.
bool playMatch(Config setup, int& totalTurns, bool useEnhancements = false) {
    Game game(setup);
    for (int turn = 0; turn < 40000 && !game.over; ++turn) {
        if (!invariant(game) || !game.roll()) return false;
        for (int action = 0; action < 2; ++action) {
            if (useEnhancements && !game.over && game.players[game.current].rank == 0) {
                // These choices use only advertised actions. Recompute legal
                // moves afterwards because magic can change a die's face.
                for (int cone = 0; cone < 4; ++cone) {
                    for (int die = 0; die < game.config.diceCount; ++die) {
                        if (game.canUseAbility(cone, die)) game.useAbility(cone, die);
                    }
                    if (game.canUsePower(Power::Shield, cone)) game.usePower(Power::Shield, cone);
                    if (game.canUsePower(Power::Sprint, cone)) game.usePower(Power::Sprint, cone);
                }
                bool anyMove = false;
                for (int die = 0; die < game.config.diceCount; ++die)
                    anyMove |= !game.legalMoves(die).empty();
                if (!anyMove) {
                    for (int die = 0; die < game.config.diceCount; ++die) {
                        if (game.canUsePower(Power::Reroll, 0, die)) {
                            game.usePower(Power::Reroll, 0, die);
                            break;
                        }
                    }
                }
            }
            int selectedDie = -1;
            int selectedCone = -1;
            int bestScore = -10000;
            for (int die = 0; die < game.config.diceCount; ++die) {
                for (int cone : game.legalMoves(die)) {
                    const int progress = game.players[game.current].cones[cone].progress;
                    const int score = progress < 0 ? -1 : progress + game.moveDistance(die, cone);
                    if (score > bestScore) {
                        bestScore = score;
                        selectedDie = die;
                        selectedCone = cone;
                    }
                }
            }
            if (selectedDie < 0) break;
            if (!game.move(selectedDie, selectedCone) || !invariant(game)) return false;
            if (game.over) break;
        }
        ++totalTurns;
        if (!game.over && (!game.canEndTurn() || !game.endTurn())) return false;
    }
    return game.over && invariant(game);
}

void seededMatches() {
    int games = 0;
    int turns = 0;
    bool valid = true;
    for (Mode mode : {Mode::Basic, Mode::Classes, Mode::Anime, Mode::Custom}) {
        for (int seats = 1; seats <= 4; ++seats) {
            for (int dice = 1; dice <= 2; ++dice) {
                for (bool first : {true, false}) {
                    const unsigned seed = 18000u + static_cast<unsigned>(games * 53);
                    if (!playMatch(settings(mode, seats, dice, first, seed), turns)) {
                        cerr << "Match failed: " << modeName(mode) << ", seats=" << seats
                             << ", dice=" << dice << ", first=" << first << ", seed=" << seed << '\n';
                        valid = false;
                    }
                    ++games;
                }
            }
        }
    }
    check(valid, to_string(games) + " seeded complete games terminate legally across every mode, seat count, dice count and goal");
    cout << "Whole-match simulation exercised " << turns << " real turns.\n";

    int poweredGames = 0;
    int poweredTurns = 0;
    bool poweredValid = true;
    for (Mode mode : {Mode::Classes, Mode::Anime, Mode::Custom}) {
        for (Class role : {Class::Fighter, Class::Rogue, Class::Wizard,
                           Class::Cleric, Class::Ranger, Class::Monk}) {
            for (int dice = 1; dice <= 2; ++dice) {
                Config setup = settings(mode, 4, dice, false,
                                        64000u + static_cast<unsigned>(poweredGames * 31));
                setup.players[0].heroClass = role;
                setup.players[1].heroClass = static_cast<Class>((static_cast<int>(role) + 1) % 6);
                setup.players[2].heroClass = static_cast<Class>((static_cast<int>(role) + 2) % 6);
                setup.players[3].heroClass = static_cast<Class>((static_cast<int>(role) + 3) % 6);
                poweredValid &= playMatch(setup, poweredTurns, true);
                ++poweredGames;
            }
        }
    }
    check(poweredValid, to_string(poweredGames) + " more all-four matches finish while using actual cards and every class ability");
    cout << "Enhanced simulation exercised " << poweredTurns << " real turns.\n";
}
} // namespace

int main() {
    try {
        ordinaryRules();
        boardAndFinishing();
        captures();
        enhancements();
        configuration();
        reproducibility();
        seededMatches();
    } catch (const exception& error) {
        check(false, string("unexpected exception: ") + error.what());
    }
    cout << passed << " checks passed; " << failed << " checks failed.\n";
    return failed == 0 ? 0 : 1;
}

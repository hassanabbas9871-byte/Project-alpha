#pragma once

#include "Types.hpp"

namespace veil {

// Battle owns one temporary tactical encounter. Persistent progress is copied
// in at the start; the campaign controller awards rewards after a victory.
class Battle {
public:
    std::array<Terrain, BoardWidth * BoardHeight> board{};
    std::vector<Unit> units;
    std::vector<Effect> effects;
    std::vector<std::string> log;
    int current{}, round{1}, turns{}, seals{}, escortX{}, wave{}, missionId{};
    Outcome outcome{Outcome::Ongoing};

    void start(const Mission& mission, int selectedWave, const Progress& progress);
    void update(float dt);
    const Unit* actor() const;
    Unit* actor();
    std::vector<Pos> reachable() const;
    bool move(Pos destination);
    bool attack(Pos destination);
    bool useSkill(int skillId, Pos destination);
    void endTurn();
    bool isPartyTurn() const;
    bool blocked(Pos position) const;
    bool lineOfSight(Pos from, Pos to) const;
    int remainingEnemies() const;
    std::string objectiveText() const;

private:
    Mission mission_;
    Progress progress_;
    float enemyDelay_{};
    int summonCount_{};
    std::vector<bool> enraged_;
    std::array<unsigned, HeroCount> heroPerks_{};
    void addLog(const std::string& message);
    int unitAt(Pos position, bool livingOnly = true) const;
    void beginTurn();
    void nextTurn();
    void finishAction();
    void checkOutcome();
    void enemyAction();
    int damage(int source, int target, int amount, int kind = 0);
    void resolveTile(Unit& unit);
    std::vector<Pos> reachableFor(int index, int distance) const;
    int weaponRange(WeaponClass weapon) const;
};

bool runBattleChecks();
} // namespace veil

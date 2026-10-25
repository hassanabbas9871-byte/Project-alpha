#include "Battle.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <string>

using namespace std;

namespace veil {
namespace {
bool inside(Pos p) { return p.x >= 0 && p.y >= 0 && p.x < BoardWidth && p.y < BoardHeight; }
int cellIndex(Pos p) { return p.y * BoardWidth + p.x; }
int distance(Pos a, Pos b) { return abs(a.x - b.x) + abs(a.y - b.y); }
constexpr array<Pos, 4> Directions{{{1,0},{0,1},{-1,0},{0,-1}}};
} // namespace

void Battle::addLog(const string& message) {
    log.push_back(message);
    if (log.size() > 10) log.erase(log.begin());
}

int Battle::weaponRange(WeaponClass weapon) const {
    switch (weapon) {
        case WeaponClass::Revolver: return 4;
        case WeaponClass::Rifle: return 6;
        case WeaponClass::Charm: return 4;
        case WeaponClass::Cane: return 2;
        default: return 1;
    }
}

void Battle::start(const Mission& mission, int selectedWave, const Progress& progress) {
    mission_ = mission;
    progress_ = progress;
    missionId = mission.id;
    wave = clamp(selectedWave, 0, 1);
    current = turns = seals = summonCount_ = 0;
    round = 1;
    escortX = 2;
    outcome = Outcome::Ongoing;
    enemyDelay_ = .25f;
    units.clear(); effects.clear(); log.clear();
    heroPerks_.fill(0);
    board.fill(Terrain::Floor);

    // A deterministic arrangement gives each case a reproducible board.
    // The central street and deployment lanes are always open, ensuring every
    // starting unit can reach the opposite side rather than spawning in a box.
    for (int y = 1; y < BoardHeight - 1; ++y) {
        for (int x = 3; x < 9; ++x) {
            if (y == 4) continue;
            const int pattern = (x * 17 + y * 31 + mission.id * 13 + wave * 7) % 23;
            if (pattern < 2) board[cellIndex({x,y})] = Terrain::Wall;
            else if (pattern < 6) board[cellIndex({x,y})] = Terrain::Cover;
            else if (pattern == 7 && mission.chapter > 0) board[cellIndex({x,y})] = Terrain::Hazard;
        }
    }
    if (mission.objective == Objective::Seal) {
        for (Pos p : array<Pos,3>{{{4,1},{7,6},{10,3}}}) board[cellIndex(p)] = Terrain::Sigil;
    }
    if (mission.objective == Objective::Escort) board[cellIndex({10,4})] = Terrain::Exit;

    const auto& catalog = items();
    for (int hero = 0; hero < HeroCount; ++hero) {
        Unit unit;
        unit.id = hero;
        unit.source = hero;
        unit.party = true;
        unit.cell = {1, 2 + hero * 2};
        unit.name = heroNames()[hero];
        unit.maxHp = 88 + progress.level * 12 + (hero == 1 ? 8 : 0);
        unit.maxFocus = 12 + progress.level / 2;
        unit.attack = 11 + progress.level * 3;
        unit.defense = 2 + progress.level / 3;
        unit.range = hero == 0 ? 4 : hero == 1 ? 2 : 4;
        unit.move = 4;
        unit.speed = 20 - hero;
        for (int itemId : progress.equipment[hero]) {
            if (itemId < 0 || static_cast<size_t>(itemId) >= catalog.size()) continue;
            const auto& item = catalog[static_cast<size_t>(itemId)];
            unit.attack += item.attack;
            unit.defense += item.defense;
            unit.maxHp += item.hp;
            unit.maxFocus += item.focus;
            unit.move += item.move;
            if (item.slot == Slot::Weapon) unit.range = weaponRange(item.weaponClass);
            if (item.perk != Perk::None) {
                // Equipment effects stack across slots. The public perk field
                // remains a convenient icon; the bit mask remembers all effects.
                heroPerks_[static_cast<size_t>(hero)] |= 1u << static_cast<unsigned>(item.perk);
                if (item.slot == Slot::Weapon || unit.perk == Perk::None) unit.perk = item.perk;
            }
        }
        unit.move = clamp(unit.move, 2, 7);
        unit.hp = unit.maxHp;
        unit.focus = unit.maxFocus;
        units.push_back(unit);
    }
    const array<Pos,10> deployment{{{9,1},{10,3},{9,6},{8,4},{10,7},{10,0},{8,2},{8,6},{11,4},{7,0}}};
    for (size_t i = 0; i < mission.enemies[wave].size() && i < deployment.size(); ++i) {
        const int enemyId = mission.enemies[wave][i];
        if (enemyId < 0 || static_cast<size_t>(enemyId) >= enemies().size()) continue;
        const auto& definition = enemies()[static_cast<size_t>(enemyId)];
        Unit unit;
        unit.id = static_cast<int>(units.size());
        unit.source = enemyId;
        unit.cell = deployment[i];
        // Clear only solid spawn obstructions. A hostile can stand on a
        // rupture; replacing that tile would make a three-seal case unwinnable.
        if (board[cellIndex(unit.cell)] == Terrain::Wall) board[cellIndex(unit.cell)] = Terrain::Floor;
        unit.name = definition.name;
        unit.maxHp = definition.hp + mission.level * (definition.boss ? 7 : 4);
        unit.hp = unit.maxHp;
        unit.attack = definition.attack + mission.level;
        unit.defense = definition.defense + mission.level / 4;
        unit.range = definition.range;
        unit.move = definition.move;
        unit.speed = definition.speed;
        units.push_back(unit);
    }
    enraged_.assign(units.size(), false);
    addLog("Encounter " + to_string(wave + 1) + ": " + objectiveText());
    addLog("Each turn: two actions. Move, attack, or use a skill.");
    beginTurn();
    checkOutcome();
}

const Unit* Battle::actor() const {
    return current >= 0 && static_cast<size_t>(current) < units.size() ? &units[static_cast<size_t>(current)] : nullptr;
}
Unit* Battle::actor() {
    return current >= 0 && static_cast<size_t>(current) < units.size() ? &units[static_cast<size_t>(current)] : nullptr;
}
bool Battle::isPartyTurn() const { return outcome == Outcome::Ongoing && actor() && actor()->party && actor()->alive(); }

int Battle::unitAt(Pos position, bool livingOnly) const {
    for (size_t i = 0; i < units.size(); ++i)
        if (units[i].cell == position && (!livingOnly || units[i].alive())) return static_cast<int>(i);
    return -1;
}
bool Battle::blocked(Pos position) const {
    return !inside(position) || board[static_cast<size_t>(cellIndex(position))] == Terrain::Wall || unitAt(position) >= 0;
}

vector<Pos> Battle::reachableFor(int index, int allowance) const {
    vector<Pos> result;
    if (index < 0 || static_cast<size_t>(index) >= units.size() || !units[static_cast<size_t>(index)].alive()) return result;
    array<int, BoardWidth * BoardHeight> distances;
    distances.fill(-1);
    const Pos start = units[static_cast<size_t>(index)].cell;
    queue<Pos> open;
    open.push(start);
    distances[static_cast<size_t>(cellIndex(start))] = 0;
    while (!open.empty()) {
        const Pos p = open.front(); open.pop();
        const int steps = distances[static_cast<size_t>(cellIndex(p))];
        if (steps > 0) result.push_back(p);
        if (steps >= allowance) continue;
        for (Pos direction : Directions) {
            const Pos next{p.x + direction.x, p.y + direction.y};
            if (blocked(next) || distances[static_cast<size_t>(cellIndex(next))] >= 0) continue;
            distances[static_cast<size_t>(cellIndex(next))] = steps + 1;
            open.push(next);
        }
    }
    return result;
}
vector<Pos> Battle::reachable() const {
    return isPartyTurn() && actor()->ap > 0 ? reachableFor(current, actor()->move) : vector<Pos>{};
}

bool Battle::lineOfSight(Pos from, Pos to) const {
    if (!inside(from) || !inside(to)) return false;
    // Bresenham's integer line walks through the squares crossed by a shot.
    // Other units do not block a shot, but solid walls do.
    const int dx = abs(to.x - from.x), dy = -abs(to.y - from.y);
    const int sx = from.x < to.x ? 1 : -1, sy = from.y < to.y ? 1 : -1;
    int error = dx + dy;
    Pos p = from;
    while (!(p == to)) {
        const int twice = error * 2;
        if (twice >= dy) { error += dy; p.x += sx; }
        if (twice <= dx) { error += dx; p.y += sy; }
        if (board[static_cast<size_t>(cellIndex(p))] == Terrain::Wall) return false;
    }
    return true;
}

void Battle::resolveTile(Unit& unit) {
    if (!inside(unit.cell)) return;
    auto& tile = board[static_cast<size_t>(cellIndex(unit.cell))];
    if (tile == Terrain::Hazard) {
        const int harm = 3 + mission_.level / 3;
        unit.hp = max(0, unit.hp - harm);
        addLog(unit.name + " takes " + to_string(harm) + " from the tainted ground.");
        effects.push_back({unit.cell, unit.cell, .5f, 3, harm});
    }
    if (unit.party && tile == Terrain::Sigil) {
        tile = Terrain::Floor;
        ++seals;
        unit.focus = min(unit.maxFocus, unit.focus + 4);
        addLog("A rupture is sealed. " + to_string(seals) + "/3 restored.");
        effects.push_back({unit.cell, unit.cell, .8f, 2, 0});
    }
}

bool Battle::move(Pos destination) {
    if (!isPartyTurn() || actor()->ap <= 0) return false;
    const auto possibilities = reachable();
    if (find(possibilities.begin(), possibilities.end(), destination) == possibilities.end()) return false;
    auto& unit = *actor();
    const Pos from = unit.cell;
    unit.cell = destination;
    --unit.ap;
    effects.push_back({from, destination, .25f, 4, 0});
    resolveTile(unit);
    finishAction();
    return true;
}

int Battle::damage(int source, int target, int amount, int kind) {
    if (target < 0 || static_cast<size_t>(target) >= units.size() || !units[static_cast<size_t>(target)].alive()) return 0;
    auto& defender = units[static_cast<size_t>(target)];
    int protection = defender.defense + (defender.guard > 0 ? 6 : 0);
    if (board[static_cast<size_t>(cellIndex(defender.cell))] == Terrain::Cover) protection += 3;
    const int actual = max(1, amount - protection);
    defender.hp = max(0, defender.hp - actual);
    const Pos origin = source >= 0 ? units[static_cast<size_t>(source)].cell : defender.cell;
    effects.push_back({origin, defender.cell, .55f, kind, actual});
    if (!defender.alive()) addLog(defender.name + " falls.");
    return actual;
}

bool Battle::attack(Pos destination) {
    if (!isPartyTurn() || actor()->ap <= 0) return false;
    const int target = unitAt(destination);
    if (target < 0 || units[static_cast<size_t>(target)].party || distance(actor()->cell, destination) > actor()->range ||
        !lineOfSight(actor()->cell, destination)) return false;
    const int source = current;
    const string name = actor()->name;
    const int dealt = damage(source, target, actor()->attack);
    auto& attacker = units[static_cast<size_t>(source)];
    auto& victim = units[static_cast<size_t>(target)];
    for (int effect = static_cast<int>(Perk::Bleed); effect <= static_cast<int>(Perk::Guard); ++effect) {
    if ((heroPerks_[static_cast<size_t>(attacker.source)] & (1u << static_cast<unsigned>(effect))) == 0) continue;
    switch (static_cast<Perk>(effect)) {
        case Perk::Bleed: victim.bleed = max(victim.bleed, 2); break;
        case Perk::Drain: attacker.hp = min(attacker.maxHp, attacker.hp + dealt / 4); break;
        case Perk::Stun: if ((turns + source) % 3 == 0) victim.stun = 1; break;
        case Perk::Focus: attacker.focus = min(attacker.maxFocus, attacker.focus + 2); break;
        case Perk::Guard: attacker.guard = 1; break;
        default: break;
    }
    }
    --attacker.ap;
    addLog(name + " deals " + to_string(dealt) + " to " + victim.name + ".");
    finishAction();
    return true;
}

bool Battle::useSkill(int skillId, Pos destination) {
    if (!isPartyTurn() || skillId < 0 || skillId >= SkillCount || static_cast<size_t>(skillId) >= skills().size()) return false;
    const auto& skill = skills()[static_cast<size_t>(skillId)];
    if (!progress_.learned[static_cast<size_t>(skillId)] || skill.hero != actor()->source || actor()->ap <= 0 ||
        actor()->focus < skill.focusCost || actor()->cooldowns[static_cast<size_t>(skillId)] > 0 || !inside(destination)) return false;
    const int source = current;
    const Pos origin = actor()->cell;
    if (distance(origin, destination) > skill.range || !lineOfSight(origin, destination)) return false;
    const int target = unitAt(destination);
    const bool friendly = skill.kind == SkillKind::Heal || skill.kind == SkillKind::Guard || skill.kind == SkillKind::Restore;
    if (skill.kind == SkillKind::Blink) {
        if (blocked(destination)) return false;
        actor()->cell = destination;
        effects.push_back({origin, destination, .7f, 2, 0});
        resolveTile(*actor());
    } else if (friendly) {
        if (target < 0 || !units[static_cast<size_t>(target)].party) return false;
        for (auto& ally : units) {
            if (!ally.alive() || !ally.party || distance(ally.cell, destination) > skill.radius) continue;
            if (skill.kind == SkillKind::Heal) {
                const int healing = max(8, (actor()->attack * skill.power) / 100);
                ally.hp = min(ally.maxHp, ally.hp + healing);
                ally.poison = ally.bleed = 0;
                effects.push_back({origin, ally.cell, .7f, 1, healing});
            } else if (skill.kind == SkillKind::Guard) {
                ally.guard = 3;
                effects.push_back({origin, ally.cell, .6f, 2, 0});
            } else {
                ally.focus = min(ally.maxFocus, ally.focus + max(3, skill.power / 20));
                ally.poison = ally.bleed = ally.stun = 0;
                effects.push_back({origin, ally.cell, .7f, 1, 0});
            }
        }
    } else {
        if (target < 0 && skill.radius == 0) return false;
        bool hit = false;
        // Iterate by index because damage changes HP, but never the list size.
        for (size_t i = 0; i < units.size(); ++i) {
            auto& victim = units[i];
            if (!victim.alive() || victim.party || distance(victim.cell, destination) > skill.radius) continue;
            hit = true;
            const int dealt = damage(source, static_cast<int>(i), max(1, actor()->attack * skill.power / 100), 2);
            if (skill.kind == SkillKind::Poison) victim.poison = 3;
            if (skill.kind == SkillKind::Stun) victim.stun = 1;
            if (skill.kind == SkillKind::Drain) actor()->hp = min(actor()->maxHp, actor()->hp + dealt / 2);
            if (skill.kind == SkillKind::Pull && victim.alive()) {
                Pos step = victim.cell;
                if (abs(victim.cell.x - origin.x) >= abs(victim.cell.y - origin.y)) step.x += origin.x > victim.cell.x ? 1 : -1;
                else step.y += origin.y > victim.cell.y ? 1 : -1;
                if (!blocked(step)) victim.cell = step;
            }
        }
        if (!hit) return false;
    }
    auto& user = units[static_cast<size_t>(source)];
    user.focus -= skill.focusCost;
    user.cooldowns[static_cast<size_t>(skillId)] = skill.cooldown + 1;
    --user.ap;
    addLog(user.name + " uses " + skill.name + ".");
    finishAction();
    return true;
}

int Battle::remainingEnemies() const {
    return static_cast<int>(count_if(units.begin(), units.end(), [](const Unit& unit) { return !unit.party && unit.alive(); }));
}

void Battle::checkOutcome() {
    if (outcome != Outcome::Ongoing) return;
    if (none_of(units.begin(), units.end(), [](const Unit& unit) { return unit.party && unit.alive(); })) {
        outcome = Outcome::Defeat;
        addLog("The bureau retreats. Retry this encounter from the case checkpoint.");
        return;
    }
    bool won = false;
    switch (mission_.objective) {
        case Objective::Hunt: won = remainingEnemies() == 0; break;
        case Objective::Survive: won = round >= 6 || remainingEnemies() == 0; break;
        case Objective::Seal: won = seals >= 3; break;
        case Objective::Escort: won = escortX >= 10; break;
    }
    if (won) { outcome = Outcome::Victory; addLog("Encounter resolved. The trail continues."); }
}

string Battle::objectiveText() const {
    switch (mission_.objective) {
        case Objective::Hunt: return "Defeat the hostile remnants (" + to_string(remainingEnemies()) + " remain).";
        case Objective::Survive: return "Survive five rounds or clear all foes (round " + to_string(round) + ").";
        case Objective::Seal: return "Step onto three ruptures to seal them (" + to_string(seals) + "/3).";
        case Objective::Escort: return "Stay near the evidence carriage on row 5 (" + to_string(escortX - 2) + "/8).";
    }
    return {};
}

void Battle::beginTurn() {
    auto* unit = actor();
    if (!unit || !unit->alive()) return;
    unit->ap = 2;
    unit->focus = min(unit->maxFocus, unit->focus + 2);
    if (unit->guard > 0) --unit->guard;
    for (auto& cooldown : unit->cooldowns) cooldown = max(0, cooldown - 1);
    if (unit->party && (heroPerks_[static_cast<size_t>(unit->source)] & (1u << static_cast<unsigned>(Perk::Regen))) != 0)
        unit->hp = min(unit->maxHp, unit->hp + 4);
    if (unit->bleed > 0) { --unit->bleed; unit->hp = max(0, unit->hp - 4); }
    if (unit->poison > 0) { --unit->poison; unit->hp = max(0, unit->hp - 5); }
    if (unit->stun > 0) { --unit->stun; unit->ap = 0; addLog(unit->name + " is stunned this turn."); }
    resolveTile(*unit);
    if (!unit->party && unit->alive()) {
        const auto& definition = enemies()[static_cast<size_t>(unit->source)];
        if (definition.boss && unit->hp <= unit->maxHp / 2 && !enraged_[static_cast<size_t>(current)]) {
            enraged_[static_cast<size_t>(current)] = true;
            unit->attack += 4 + mission_.level / 2;
            unit->move = min(6, unit->move + 1);
            unit->guard = 2;
            addLog(unit->name + " abandons its disguise: second phase!");
            effects.push_back({unit->cell, unit->cell, 1.f, 3, 0});
        }
    }
    if (!unit->alive()) unit->ap = 0;
    checkOutcome();
}

void Battle::nextTurn() {
    if (outcome != Outcome::Ongoing || units.empty()) return;
    // The bounded search skips defeated/stunned units without recursion. This
    // matters when several status effects remove turns at the same time.
    for (size_t attempt = 0; attempt <= units.size() * 2; ++attempt) {
        ++current;
        ++turns;
        if (static_cast<size_t>(current) >= units.size()) { current = 0; ++round; }
        if (!actor()->alive()) continue;
        beginTurn();
        if (outcome != Outcome::Ongoing || actor()->ap > 0) break;
    }
    enemyDelay_ = .25f;
}

void Battle::endTurn() {
    if (!isPartyTurn()) return;
    if (mission_.objective == Objective::Escort && actor()->alive() && distance(actor()->cell, {escortX,4}) <= 2) {
        escortX = min(10, escortX + 1);
        addLog("The evidence carriage advances to column " + to_string(escortX + 1) + ".");
    }
    actor()->ap = 0;
    checkOutcome();
    nextTurn();
}

void Battle::finishAction() {
    checkOutcome();
    if (outcome != Outcome::Ongoing) return;
    if (!actor() || !actor()->alive() || actor()->ap <= 0) {
        // Escort advances after a completed hero turn as well as a manual pass.
        if (actor() && actor()->party && actor()->alive() && mission_.objective == Objective::Escort &&
            distance(actor()->cell, {escortX,4}) <= 2) escortX = min(10, escortX + 1);
        checkOutcome();
        nextTurn();
    }
}

void Battle::enemyAction() {
    if (outcome != Outcome::Ongoing || !actor() || actor()->party) return;
    if (!actor()->alive() || actor()->ap <= 0) { nextTurn(); return; }
    const int source = current;
    const Ai ai = enemies()[static_cast<size_t>(actor()->source)].ai;
    auto chooseTarget = [&]() {
        int best = -1;
        float bestScore = numeric_limits<float>::max();
        for (size_t i = 0; i < units.size(); ++i) {
            if (!units[i].party || !units[i].alive()) continue;
            float score = static_cast<float>(distance(actor()->cell, units[i].cell));
            if (ai == Ai::Stalker || ai == Ai::Leech) score += units[i].hp * .055f;
            if (score < bestScore) { bestScore = score; best = static_cast<int>(i); }
        }
        return best;
    };

    if (ai == Ai::Medic && actor()->ap == 2) {
        int patient = -1;
        for (size_t i = 0; i < units.size(); ++i)
            if (!units[i].party && units[i].alive() && units[i].hp < units[i].maxHp * 2 / 3 &&
                distance(actor()->cell, units[i].cell) <= 5 && (patient < 0 || units[i].hp < units[static_cast<size_t>(patient)].hp)) patient = static_cast<int>(i);
        if (patient >= 0) {
            auto& ally = units[static_cast<size_t>(patient)];
            const int healing = 12 + mission_.level * 2;
            ally.hp = min(ally.maxHp, ally.hp + healing);
            effects.push_back({actor()->cell, ally.cell, .7f, 1, healing});
            addLog(actor()->name + " stitches a remnant back together.");
            --actor()->ap;
            return;
        }
    }
    if (ai == Ai::Summoner && actor()->ap == 2 && round % 2 == 1 && summonCount_ < 3 && remainingEnemies() < 10) {
        const auto available = reachableFor(current, 2);
        if (!available.empty()) {
            Unit summoned;
            summoned.id = static_cast<int>(units.size());
            summoned.source = 0;
            summoned.name = "Unwritten echo";
            summoned.cell = available.front();
            summoned.maxHp = summoned.hp = 18 + mission_.level * 2;
            summoned.attack = 5 + mission_.level;
            summoned.defense = 0;
            summoned.range = 1; summoned.move = 3; summoned.speed = 3;
            const string name = actor()->name;
            --actor()->ap; // Do this before push_back can invalidate references.
            units.push_back(summoned);
            enraged_.push_back(false);
            ++summonCount_;
            addLog(name + " summons an unwritten echo.");
            return;
        }
    }
    const int target = chooseTarget();
    if (target < 0) { checkOutcome(); return; }
    const Pos targetCell = units[static_cast<size_t>(target)].cell;
    const int range = max(actor()->range, ai == Ai::Artillery ? 6 : 1);
    if (distance(actor()->cell, targetCell) <= range && lineOfSight(actor()->cell, targetCell)) {
        const string name = actor()->name;
        const int power = actor()->attack + (ai == Ai::Brute ? 3 : 0);
        const int dealt = damage(source, target, power, ai == Ai::Artillery ? 3 : 0);
        if (ai == Ai::Leech) actor()->hp = min(actor()->maxHp, actor()->hp + dealt / 2);
        if (ai == Ai::Stalker) units[static_cast<size_t>(target)].bleed = 2;
        if (ai == Ai::Trickster) {
            auto& victim = units[static_cast<size_t>(target)];
            victim.focus = max(0, victim.focus - 3);
            if (round % 3 == 0) victim.poison = 2;
        }
        if (ai == Ai::Brute && round % 3 == 0) units[static_cast<size_t>(target)].stun = 1;
        if (ai == Ai::Artillery) {
            for (size_t i = 0; i < units.size(); ++i)
                if (static_cast<int>(i) != target && units[i].party && units[i].alive() && distance(units[i].cell, targetCell) <= 1)
                    damage(source, static_cast<int>(i), max(2, power / 2), 3);
        }
        if (ai == Ai::Guardian) actor()->guard = 2;
        addLog(name + " strikes for " + to_string(dealt) + ".");
        --actor()->ap;
        finishAction();
        return;
    }

    auto available = reachableFor(current, actor()->move);
    Pos chosen = actor()->cell;
    int score = distance(chosen, targetCell) * 10;
    const bool retreat = (ai == Ai::Ranged || ai == Ai::Artillery) && distance(chosen, targetCell) < 3;
    if (retreat) score = -distance(chosen, targetCell) * 10;
    for (Pos p : available) {
        int candidate = distance(p, targetCell) * (retreat ? -10 : 10);
        if (board[static_cast<size_t>(cellIndex(p))] == Terrain::Hazard) candidate += 18;
        if (board[static_cast<size_t>(cellIndex(p))] == Terrain::Cover) candidate -= ai == Ai::Guardian ? 6 : 2;
        if (candidate < score) { score = candidate; chosen = p; }
    }
    if (!(chosen == actor()->cell)) {
        const Pos from = actor()->cell;
        actor()->cell = chosen;
        effects.push_back({from, chosen, .25f, 4, 0});
        resolveTile(*actor());
    } else if (ai == Ai::Guardian) actor()->guard = 2;
    --actor()->ap;
    finishAction();
}

void Battle::update(float dt) {
    dt = clamp(dt, 0.f, 1.f);
    for (auto& effect : effects) effect.life -= dt;
    erase_if(effects, [](const Effect& effect) { return effect.life <= 0; });
    if (outcome != Outcome::Ongoing) return;
    checkOutcome();
    if (outcome != Outcome::Ongoing) return;
    if (actor() && (!actor()->alive() || actor()->ap <= 0)) nextTurn();
    if (!actor() || actor()->party) return;
    enemyDelay_ -= dt;
    if (enemyDelay_ <= 0) { enemyDelay_ = .25f; enemyAction(); }
}

bool runBattleChecks() {
    int passed = 0, failed = 0;
    auto check = [&](bool result, const string& name) {
        cout << (result ? "PASS " : "FAIL ") << name << '\n';
        result ? ++passed : ++failed;
    };
    if (missions().empty() || enemies().empty() || items().empty() || skills().empty()) {
        cout << "FAIL combat content is unavailable\n";
        return false;
    }
    Progress progress;
    progress.level = 3;
    progress.learned.fill(true);
    progress.owned.assign(items().size(), true);
    auto fresh = [&]() { Battle battle; battle.start(missions().front(), 0, progress); return battle; };
    Battle battle = fresh();
    check(battle.units.size() >= 4 && battle.isPartyTurn() && battle.actor()->ap == 2,
          "battle deploys a party and starts a two-action hero turn");
    const int originalAp = battle.actor()->ap;
    const Pos originalPos = battle.actor()->cell;
    check(!battle.move({-1,0}) && battle.actor()->ap == originalAp && battle.actor()->cell == originalPos,
          "invalid movement consumes no action");
    const auto reachable = battle.reachable();
    check(!reachable.empty() && battle.move(reachable.front()) && battle.actor()->ap == 1,
          "pathfinding movement consumes exactly one action");
    battle.board.fill(Terrain::Floor);
    battle.board[static_cast<size_t>(cellIndex({4,4}))] = Terrain::Wall;
    check(!battle.lineOfSight({2,4},{6,4}) && battle.lineOfSight({2,3},{6,3}),
          "solid walls block shots while clear streets do not");

    Battle attackTest = fresh();
    attackTest.board.fill(Terrain::Floor);
    attackTest.units[3].cell = {2,2};
    const int beforeHp = attackTest.units[3].hp;
    check(attackTest.attack({2,2}) && attackTest.units[3].hp < beforeHp && attackTest.units[0].ap == 1,
          "a legal attack damages the target and spends one action");
    check(!attackTest.attack(attackTest.units[1].cell), "basic attacks cannot harm companions");
    attackTest.units[0].ap = 1;
    attackTest.endTurn();
    check(attackTest.current == 1 && attackTest.actor()->ap == 2, "passing advances to the next living companion");

    Battle skillTest = fresh();
    skillTest.board.fill(Terrain::Floor);
    int healingSkill = -1;
    for (const auto& skill : skills()) if (skill.kind == SkillKind::Heal) { healingSkill = skill.id; break; }
    if (healingSkill >= 0) {
        const auto& skill = skills()[static_cast<size_t>(healingSkill)];
        skillTest.current = skill.hero;
        skillTest.actor()->ap = 2;
        skillTest.actor()->hp -= 35;
        const int injured = skillTest.actor()->hp;
        const int focus = skillTest.actor()->focus;
        const Pos cell = skillTest.actor()->cell;
        check(skillTest.useSkill(healingSkill, cell) && skillTest.units[static_cast<size_t>(skill.hero)].hp > injured &&
              skillTest.units[static_cast<size_t>(skill.hero)].focus < focus,
              "healing restores an injured ally and consumes focus");
        check(!skillTest.useSkill(healingSkill, cell), "a skill cannot bypass its cooldown");
    } else check(false, "campaign provides a usable healing skill");

    Battle status = fresh();
    status.units[1].poison = 2;
    const int unpoisonedHp = status.units[1].hp;
    status.endTurn();
    check(status.units[1].hp < unpoisonedHp && status.units[1].poison == 1,
          "poison ticks once when its victim begins a turn");

    Mission hunt = missions().front(); hunt.objective = Objective::Hunt;
    Battle victory; victory.start(hunt,0,progress);
    for (auto& unit : victory.units) if (!unit.party) unit.hp = 0;
    victory.endTurn();
    check(victory.outcome == Outcome::Victory, "defeating the last foe resolves a hunt");
    Battle defeat = fresh();
    for (auto& unit : defeat.units) if (unit.party) unit.hp = 0;
    defeat.update(1);
    check(defeat.outcome == Outcome::Defeat, "a fallen party triggers defeat instead of an infinite turn loop");

    Mission sealing = missions().front(); sealing.objective = Objective::Seal;
    Battle seal; seal.start(sealing,0,progress);
    check(count(seal.board.begin(), seal.board.end(), Terrain::Sigil) == 3,
          "hostile deployment preserves all three rupture objectives");
    seal.board.fill(Terrain::Floor);
    seal.board[static_cast<size_t>(cellIndex({2,2}))] = Terrain::Sigil;
    check(seal.move({2,2}) && seal.seals == 1 && seal.board[static_cast<size_t>(cellIndex({2,2}))] == Terrain::Floor,
          "stepping on a rupture seals it and removes its objective tile");
    Battle allSeals; allSeals.start(sealing,0,progress);
    allSeals.board.fill(Terrain::Floor);
    for (auto& unit : allSeals.units) if (!unit.party) unit.hp = 0;
    for (int hero = 0; hero < HeroCount; ++hero) {
        const Pos target{2, 2 + hero * 2};
        allSeals.board[static_cast<size_t>(cellIndex(target))] = Terrain::Sigil;
        allSeals.move(target);
        if (allSeals.outcome == Outcome::Ongoing) allSeals.endTurn();
    }
    check(allSeals.outcome == Outcome::Victory && allSeals.seals == 3,
          "sealing the final rupture resolves the encounter without a softlock");

    Mission escort = missions().front(); escort.objective = Objective::Escort;
    Battle carriage; carriage.start(escort,0,progress);
    carriage.units[0].cell = {2,3};
    carriage.endTurn();
    check(carriage.escortX == 3, "a nearby companion advances the evidence carriage after their turn");

    Battle automatic = fresh();
    for (int i = 0; i < 3; ++i) automatic.endTurn();
    const int previousTurns = automatic.turns;
    for (int i = 0; i < 200 && !automatic.isPartyTurn() && automatic.outcome == Outcome::Ongoing; ++i) automatic.update(.3f);
    check(automatic.outcome != Outcome::Ongoing || (automatic.isPartyTurn() && automatic.turns > previousTurns),
          "automatic enemy turns return control to the player");

    bool enemyDefinitionsWork = true;
    for (const auto& definition : enemies()) {
        Mission fixture = missions().front();
        fixture.enemies[0] = {definition.id, 0};
        fixture.objective = Objective::Hunt;
        Battle trial; trial.start(fixture,0,progress);
        trial.board.fill(Terrain::Floor);
        trial.units[0].cell = {3,2};
        trial.units[3].cell = {4,2};
        trial.units[4].cell = {5,2};
        trial.units[4].hp = max(1, trial.units[4].maxHp / 3);
        trial.units[3].hp = max(1, trial.units[3].maxHp / 2);
        for (int hero = 0; hero < HeroCount; ++hero) trial.endTurn();
        // Exercise several actions, including bosses crossing their second
        // phase threshold, medics healing, and summoners appending real units.
        for (int tick = 0; tick < 8 && !trial.isPartyTurn(); ++tick) trial.update(.3f);
        enemyDefinitionsWork &= trial.outcome != Outcome::Ongoing || trial.isPartyTurn();
        for (const auto& unit : trial.units)
            enemyDefinitionsWork &= unit.hp >= 0 && unit.hp <= unit.maxHp && inside(unit.cell);
    }
    check(enemyDefinitionsWork, "every enemy and boss definition executes valid automatic actions");

    bool connected = true;
    for (const auto& mission : missions()) {
        Battle map; map.start(mission,0,progress);
        // Test connectivity with living units removed as blockers, because
        // they are movable actors rather than permanent level geometry.
        for (auto& unit : map.units) if (!unit.party) unit.hp = 0;
        map.units[0].move = 100;
        const auto cells = map.reachable();
        connected &= any_of(cells.begin(), cells.end(), [](Pos p) { return p.x == 10; });
    }
    check(connected, "all thirty-two case layouts have a traversable route across the board");
    cout << passed << " battle checks passed, " << failed << " failed\n";
    return failed == 0;
}
} // namespace veil

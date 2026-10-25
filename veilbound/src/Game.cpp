#include "Game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

using namespace std;

namespace veil {
namespace {
constexpr int SaveVersion = 1;
constexpr int MaximumLevel = 20;

// This checksum detects truncated/accidentally edited saves. It is not a
// security system: a single-player game does not need to hide its save format.
uint64_t checksum(const string& bytes) {
    uint64_t result = 14695981039346656037ull;
    for (unsigned char byte : bytes) {
        result ^= byte;
        result *= 1099511628211ull;
    }
    return result;
}

bool valid(const Progress& p) {
    if (p.mission < 0 || p.mission > MissionCount || p.wave < 0 || p.wave > 1
        || p.level < 1 || p.level > MaximumLevel || p.xp < 0 || p.xp > 10000000
        || p.gold < 0 || p.gold > 10000000 || p.skillPoints < 0 || p.skillPoints > 10000
        || p.compassion < -MissionCount || p.compassion > MissionCount
        || p.clues < 0 || p.clues > MissionCount || p.deaths < 0 || p.deaths > 1000000
        || !isfinite(p.playSeconds) || p.playSeconds < 0 || p.playSeconds > 1e10
        || p.owned.size() != items().size() || p.journal.size() > 512) return false;
    if (p.campaignComplete != (p.mission == MissionCount)) return false;
    if (p.campaignComplete && p.wave != 0) return false;
    for (int mission = 0; mission < MissionCount; ++mission)
        if (p.completed[static_cast<size_t>(mission)] != (mission < p.mission)) return false;
    for (const auto& equipment : p.equipment) {
        for (size_t slot = 0; slot < equipment.size(); ++slot) {
            const int id = equipment[slot];
            if (id == -1 && slot != 0) continue;
            if (id < 0 || static_cast<size_t>(id) >= items().size()
                || !p.owned[static_cast<size_t>(id)]
                || static_cast<size_t>(items()[static_cast<size_t>(id)].slot) != slot) return false;
        }
    }
    return all_of(p.journal.begin(), p.journal.end(), [](const string& s) { return s.size() <= 4096; });
}

// The text payload is deliberately readable. quoted() escapes spaces, quote
// marks and newlines so a journal entry still comes back as one string.
string encode(const Progress& p, int storyMission, int storyLine, int choice,
              bool clueAnswered, int clueSelection, int lastMission) {
    ostringstream out;
    out << p.mission << ' ' << p.wave << ' ' << p.level << ' ' << p.xp << ' '
        << p.gold << ' ' << p.skillPoints << ' ' << p.compassion << ' '
        << p.clues << ' ' << p.deaths << ' ' << p.campaignComplete << ' '
        << setprecision(17) << p.playSeconds << '\n';
    out << p.owned.size() << '\n';
    for (bool owned : p.owned) out << (owned ? '1' : '0');
    out << '\n';
    for (bool learned : p.learned) out << (learned ? '1' : '0');
    out << '\n';
    for (bool complete : p.completed) out << (complete ? '1' : '0');
    out << '\n';
    for (const auto& equipment : p.equipment) {
        for (int id : equipment) out << id << ' ';
        out << '\n';
    }
    out << storyMission << ' ' << storyLine << ' ' << choice << ' ' << clueAnswered
        << ' ' << clueSelection << ' ' << lastMission << '\n';
    out << p.journal.size() << '\n';
    for (const auto& entry : p.journal) out << quoted(entry) << '\n';
    return out.str();
}

bool bitString(const string& s, size_t size) {
    return s.size() == size && all_of(s.begin(), s.end(), [](char c) { return c == '0' || c == '1'; });
}
} // namespace

Game::Game(filesystem::path file) : savePath(std::move(file)) {}

void Game::resetStory() {
    storyLine = 0;
    choice = clueSelection = -1;
    clueAnswered = false;
    storyMission_ = progress.mission;
}

void Game::newGame() {
    // Starting over is an explicit choice. Preserve the existing checkpoint,
    // even if it is corrupt, instead of overwriting the only copy silently.
    error_code error;
    if (filesystem::exists(savePath, error)) {
        int suffix = 1;
        filesystem::path archived;
        do { archived = savePath.string() + ".previous-" + to_string(suffix++); }
        while (filesystem::exists(archived, error) && suffix < 10000);
        filesystem::rename(savePath, archived, error);
        if (error) { message = "The previous save could not be preserved. New campaign cancelled."; return; }
    }
    progress = Progress{};
    progress.owned.assign(items().size(), false);
    for (int id = 0; id < HeroCount; ++id) progress.owned[static_cast<size_t>(id)] = true;
    for (int id : {0, 6, 12}) progress.learned[static_cast<size_t>(id)] = true;
    progress.journal.push_back("CASE ZERO: Mira Vale vanished after hearing a bell that no one else could hear.");
    lastMission = 0;
    resetStory();
    page = Page::Map;
    message = "A new casebook. Three investigators. One missing sister.";
    save();
}

bool Game::continueGame() {
    if (!load()) return false;
    page = progress.campaignComplete ? Page::Ending : Page::Map;
    message = "Checkpoint restored. Equipment and abilities are available between encounters.";
    return true;
}

void Game::beginMission() {
    if (progress.campaignComplete) { page = Page::Ending; return; }
    if (progress.mission < 0 || progress.mission >= static_cast<int>(missions().size())) return;
    if (progress.wave == 1) { enterBattle(); return; }
    if (storyMission_ != progress.mission) resetStory();
    page = Page::Story;
    message.clear();
}

string Game::storyText() const {
    if (progress.mission >= static_cast<int>(missions().size())) return "The ninth bell has fallen silent.";
    const auto& mission = missions()[static_cast<size_t>(progress.mission)];
    if (storyLine < static_cast<int>(mission.intro.size())) return mission.intro[static_cast<size_t>(max(0, storyLine))];
    if (awaitingChoice()) return "An investigator's methods leave a mark. How will you proceed?";
    if (awaitingClue()) return mission.clueQuestion;
    return mission.clueExplanation;
}

bool Game::awaitingChoice() const {
    return page == Page::Story && progress.mission < static_cast<int>(missions().size())
        && storyLine >= static_cast<int>(missions()[static_cast<size_t>(progress.mission)].intro.size()) && choice < 0;
}

bool Game::awaitingClue() const {
    return page == Page::Story && choice >= 0 && !clueAnswered
        && progress.mission < static_cast<int>(missions().size())
        && storyLine >= static_cast<int>(missions()[static_cast<size_t>(progress.mission)].intro.size());
}

void Game::advanceStory() {
    if (page != Page::Story) return;
    const auto& intro = missions()[static_cast<size_t>(progress.mission)].intro;
    if (storyLine < static_cast<int>(intro.size())) ++storyLine;
    else if (!awaitingChoice() && !awaitingClue()) enterBattle();
}

void Game::choose(int option) {
    if (!awaitingChoice() || option < 0 || option > 1) return;
    choice = option;
    progress.compassion += option == 0 ? 1 : -1;
    if (option == 1) progress.gold += 10; // Expediency pays, but changes the ending.
    progress.journal.push_back(missions()[static_cast<size_t>(progress.mission)].name + ": "
        + missions()[static_cast<size_t>(progress.mission)].choices[static_cast<size_t>(option)]);
    save();
}

void Game::answerClue(int option) {
    if (!awaitingClue() || option < 0 || option > 2) return;
    const auto& mission = missions()[static_cast<size_t>(progress.mission)];
    clueAnswered = true;
    clueSelection = option;
    if (option == mission.clueAnswer) {
        ++progress.clues;
        progress.gold += 15 + mission.chapter * 3;
        message = "Deduction confirmed. Evidence bonus awarded.";
    } else message = "The evidence tells a different story. The case can still proceed.";
    progress.journal.push_back(mission.name + " / EVIDENCE: " + mission.clueExplanation);
    save();
}

void Game::enterBattle() {
    if (progress.campaignComplete) { page = Page::Ending; return; }
    if (page == Page::Story && (awaitingChoice() || awaitingClue()
        || storyLine < static_cast<int>(missions()[static_cast<size_t>(progress.mission)].intro.size()))) return;
    battle.start(missions()[static_cast<size_t>(progress.mission)], progress.wave, progress);
    page = Page::Battle;
    message = "Each investigator has two action points. Move, attack, use abilities, then end the turn.";
}

int Game::nextLevelXp() const { return 50 + progress.level * 25; }

void Game::awardExperience(int amount) {
    progress.xp += max(0, amount);
    while (progress.level < MaximumLevel && progress.xp >= nextLevelXp()) {
        progress.xp -= nextLevelXp();
        ++progress.level;
        progress.skillPoints += 2;
    }
}

void Game::concludeBattle() {
    lastMission = progress.mission;
    if (battle.outcome == Outcome::Defeat) {
        ++progress.deaths;
        rewardText = "The bureau retrieves its wounded investigators. No equipment is lost.\n"
                     "Retry this encounter with a restored party, or adjust your equipment from the case map.";
        message = "The investigation is not over.";
        page = Page::Reward;
        save();
        return;
    }
    const auto& mission = missions()[static_cast<size_t>(progress.mission)];
    const bool finalWave = progress.wave == 1;
    const int earnedGold = mission.gold / 2 + (finalWave ? mission.gold % 2 : 0);
    const int earnedXp = mission.xp / 2 + (finalWave ? mission.xp % 2 : 0);
    const int oldLevel = progress.level;
    progress.gold += earnedGold;
    awardExperience(earnedXp);
    rewardText = to_string(earnedGold) + " crowns / " + to_string(earnedXp) + " experience recovered.\n";
    if (progress.level > oldLevel) rewardText += "The party reached level " + to_string(progress.level) + ". New skill points are available.\n";
    if (!finalWave) {
        progress.wave = 1;
        rewardText += "The first encounter is secured. The deeper confrontation awaits.\n"
                      "Your party regroups with restored health and focus before the next encounter.";
    } else {
        progress.completed[static_cast<size_t>(progress.mission)] = true;
        for (const auto& line : mission.outro) rewardText += "\n" + line;
        progress.journal.push_back("CLOSED: " + mission.name + ". " + mission.summary);
        // A player can close the game on a reward screen. Preserve the full
        // revelation in the casebook as well, so resuming never loses the story.
        for (const auto& line : mission.outro)
            progress.journal.push_back(mission.name + " / AFTERMATH: " + line);
        ++progress.mission;
        progress.wave = 0;
        progress.campaignComplete = progress.mission >= MissionCount;
        resetStory();
    }
    page = Page::Reward;
    message = finalWave ? "Case closed. The next piece of the conspiracy is visible." : "Encounter secured.";
    save();
}

void Game::update(float dt) {
    if (page != Page::Title && page != Page::Ending && isfinite(dt) && dt > 0)
        progress.playSeconds += min(dt, 1.f);
    if (page != Page::Battle) return;
    battle.update(dt);
    if (battle.outcome != Outcome::Ongoing) concludeBattle();
}

void Game::finishReward() {
    if (page != Page::Reward) return;
    if (battle.outcome == Outcome::Defeat) { retry(); return; }
    if (progress.campaignComplete) page = Page::Ending;
    else if (progress.wave == 1) enterBattle();
    else page = Page::Map;
}

void Game::retry() {
    if (progress.campaignComplete) { page = Page::Ending; return; }
    battle.start(missions()[static_cast<size_t>(progress.mission)], progress.wave, progress);
    page = Page::Battle;
    message = "The party regroups. The checkpoint and equipment are intact.";
}

bool Game::buy(int itemId) {
    if (page == Page::Battle || itemId < 0 || static_cast<size_t>(itemId) >= items().size()) return false;
    const auto& item = items()[static_cast<size_t>(itemId)];
    if (progress.owned.empty() || progress.owned[static_cast<size_t>(itemId)]) { message = "That item is already in the case locker."; return false; }
    if (item.unlockChapter > min(7, progress.mission / 4)) { message = "This requisition unlocks in a later district."; return false; }
    if (progress.gold < item.price) { message = "Not enough crowns for this requisition."; return false; }
    progress.gold -= item.price;
    progress.owned[static_cast<size_t>(itemId)] = true;
    message = "Requisitioned: " + item.name;
    save();
    return true;
}

bool Game::equip(int hero, int itemId) {
    if (page == Page::Battle || hero < 0 || hero >= HeroCount || itemId < 0
        || static_cast<size_t>(itemId) >= items().size() || progress.owned.empty()
        || !progress.owned[static_cast<size_t>(itemId)]) return false;
    const auto& item = items()[static_cast<size_t>(itemId)];
    progress.equipment[static_cast<size_t>(hero)][static_cast<size_t>(item.slot)] = itemId;
    message = heroNames()[static_cast<size_t>(hero)] + " equipped " + item.name + ".";
    save();
    return true;
}

bool Game::learn(int skillId) {
    if (page == Page::Battle || skillId < 0 || skillId >= SkillCount || progress.learned[static_cast<size_t>(skillId)]) return false;
    const auto& skill = skills()[static_cast<size_t>(skillId)];
    if (progress.level < skill.unlockLevel) { message = "This ability requires party level " + to_string(skill.unlockLevel) + "."; return false; }
    if (progress.skillPoints < 1) { message = "Gain a party level to receive more skill points."; return false; }
    --progress.skillPoints;
    progress.learned[static_cast<size_t>(skillId)] = true;
    message = "Learned: " + skill.name;
    save();
    return true;
}

bool Game::save() {
    if (!valid(progress)) { message = "The checkpoint could not be saved: its data is inconsistent."; return false; }
    try {
        if (!savePath.parent_path().empty()) filesystem::create_directories(savePath.parent_path());
        const string payload = encode(progress, storyMission_, storyLine, choice, clueAnswered, clueSelection, lastMission);
        const filesystem::path temporary = savePath.string() + ".tmp";
        const filesystem::path backup = savePath.string() + ".bak";
        {
            ofstream out(temporary, ios::binary | ios::trunc);
            out << "VEILBOUND_SAVE " << SaveVersion << '\n' << hex << checksum(payload) << '\n' << payload;
            out.flush();
            if (!out) throw runtime_error("Save write failed");
        }
        const bool existed = filesystem::exists(savePath);
        if (existed) {
            if (filesystem::exists(backup)) filesystem::remove(backup);
            filesystem::rename(savePath, backup);
        }
        try { filesystem::rename(temporary, savePath); }
        catch (...) {
            // Windows cannot rename over an existing file. Moving the old
            // file aside above permits replacement; this restores it on failure.
            if (existed && !filesystem::exists(savePath)) filesystem::rename(backup, savePath);
            throw;
        }
        return true;
    } catch (const exception&) {
        message = "Saving failed. Your previous checkpoint was preserved; check the save folder's permissions.";
        return false;
    }
}

bool Game::load() {
    try {
        if (!filesystem::exists(savePath)) { message = "No saved campaign was found."; return false; }
        if (filesystem::file_size(savePath) > 2 * 1024 * 1024) throw runtime_error("Oversized save");
        ifstream file(savePath, ios::binary);
        string header, digest;
        getline(file, header); getline(file, digest);
        if (header != "VEILBOUND_SAVE " + to_string(SaveVersion)) throw runtime_error("Unsupported save");
        const string payload{istreambuf_iterator<char>(file), istreambuf_iterator<char>()};
        istringstream hashInput(digest);
        uint64_t expected{};
        if (!(hashInput >> hex >> expected)) throw runtime_error("Bad checksum");
        hashInput >> ws;
        if (!hashInput.eof() || expected != checksum(payload)) throw runtime_error("Damaged save");
        istringstream input(payload);
        Progress candidate;
        int completeFlag{};
        if (!(input >> candidate.mission >> candidate.wave >> candidate.level >> candidate.xp
              >> candidate.gold >> candidate.skillPoints >> candidate.compassion >> candidate.clues
              >> candidate.deaths >> completeFlag >> candidate.playSeconds)
            || (completeFlag != 0 && completeFlag != 1)) throw runtime_error("Bad progress");
        candidate.campaignComplete = completeFlag == 1;
        size_t ownedCount{};
        string owned, learned, completed;
        if (!(input >> ownedCount >> owned >> learned >> completed) || ownedCount != items().size()
            || !bitString(owned, ownedCount) || !bitString(learned, SkillCount)
            || !bitString(completed, MissionCount)) throw runtime_error("Bad inventory");
        candidate.owned.resize(ownedCount);
        for (size_t i = 0; i < ownedCount; ++i) candidate.owned[i] = owned[i] == '1';
        for (size_t i = 0; i < SkillCount; ++i) candidate.learned[i] = learned[i] == '1';
        for (size_t i = 0; i < MissionCount; ++i) candidate.completed[i] = completed[i] == '1';
        for (auto& equipment : candidate.equipment)
            for (int& id : equipment) if (!(input >> id)) throw runtime_error("Bad equipment");
        int storyMission{}, line{}, selectedChoice{}, clueFlag{}, selectedClue{}, previousMission{};
        if (!(input >> storyMission >> line >> selectedChoice >> clueFlag >> selectedClue >> previousMission)
            || storyMission < -1 || storyMission > MissionCount || line < 0 || line > 1000
            || selectedChoice < -1 || selectedChoice > 1 || clueFlag < 0 || clueFlag > 1
            || selectedClue < -1 || selectedClue > 2 || previousMission < 0 || previousMission >= MissionCount)
            throw runtime_error("Bad story state");
        size_t entries{};
        if (!(input >> entries) || entries > 512) throw runtime_error("Bad journal");
        candidate.journal.clear();
        for (size_t i = 0; i < entries; ++i) {
            string entry;
            if (!(input >> quoted(entry)) || entry.size() > 4096) throw runtime_error("Bad journal entry");
            candidate.journal.push_back(std::move(entry));
        }
        input >> ws;
        if (!input.eof() || !valid(candidate)) throw runtime_error("Invalid checkpoint");
        // Commit only after every field passed validation. A failed load never
        // changes the active campaign and never writes over the damaged file.
        progress = std::move(candidate);
        storyMission_ = storyMission; storyLine = line; choice = selectedChoice;
        clueAnswered = clueFlag != 0; clueSelection = selectedClue; lastMission = previousMission;
        rewardText.clear();
        return true;
    } catch (const exception&) {
        message = "The save is damaged or uses an unsupported version. It was left untouched; a .bak checkpoint may also be available.";
        return false;
    }
}
} // namespace veil

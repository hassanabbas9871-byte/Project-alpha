// These checks run real campaign and combat operations. They do not open a
// window, synthesize input, or require any graphics/audio library.
#include "Game.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace std;

namespace veil {
namespace {
struct TemporaryFolder {
    filesystem::path path;
    TemporaryFolder() {
        const auto stamp = chrono::steady_clock::now().time_since_epoch().count();
        path = filesystem::temp_directory_path() / ("veilbound-tests-" + to_string(stamp));
        filesystem::create_directories(path);
    }
    ~TemporaryFolder() {
        error_code ignored;
        filesystem::remove_all(path, ignored); // Only this test's unique folder.
    }
};

string bytes(const filesystem::path& path) {
    ifstream file(path, ios::binary);
    return {istreambuf_iterator<char>(file), istreambuf_iterator<char>()};
}

void finishIntroduction(Game& game, int method = 0) {
    game.beginMission();
    if (game.page == Page::Story) {
        const auto& mission = missions()[static_cast<size_t>(game.progress.mission)];
        for (size_t line = 0; line <= mission.intro.size(); ++line) game.advanceStory();
        if (game.awaitingChoice()) game.choose(method);
        if (game.awaitingClue()) game.answerClue(mission.clueAnswer);
        game.enterBattle();
    }
}
} // namespace

bool runGameChecks() {
    int passed = 0, failed = 0;
    const auto check = [&](bool condition, const string& description) {
        cout << (condition ? "PASS " : "FAIL ") << description << '\n';
        condition ? ++passed : ++failed;
    };
    TemporaryFolder folder;
    Game game(folder.path / "checkpoint.sav");
    game.newGame();
    check(game.page == Page::Map && game.progress.owned.size() == items().size()
          && game.progress.learned[0] && game.progress.learned[6] && game.progress.learned[12]
          && filesystem::exists(game.savePath), "new campaign creates the starting loadout and checkpoint");

    game.progress.gold = 1234;
    game.progress.playSeconds = 123.75;
    game.progress.journal.push_back("A witness said \"listen\".\nThe bell answered.");
    check(game.save(), "a valid checkpoint is written successfully");
    Game restored(game.savePath);
    check(restored.continueGame() && restored.progress.gold == 1234
          && restored.progress.playSeconds == 123.75
          && restored.progress.journal.back() == game.progress.journal.back(),
          "save/load round-trips inventory, elapsed play time and escaped journal text");
    Game backup(game.savePath.string() + ".bak");
    check(backup.load() && backup.progress.gold == 90,
          "replacing a checkpoint retains the previous valid checkpoint as a backup");
    const string beforeInvalid = bytes(game.savePath);
    const int legitimateWeapon = game.progress.equipment[0][0];
    game.progress.equipment[0][0] = static_cast<int>(items().size()) + 99;
    check(!game.save() && bytes(game.savePath) == beforeInvalid,
          "invalid equipment references are rejected before changing the saved file");
    game.progress.equipment[0][0] = legitimateWeapon;

    game.beginMission();
    const auto& firstMission = missions().front();
    for (size_t i = 0; i <= firstMission.intro.size(); ++i) game.advanceStory();
    check(game.awaitingChoice(), "the introduction leads to its actual story choice");
    const int beforeChoice = game.progress.compassion;
    game.choose(0);
    game.choose(0);
    check(game.progress.compassion == beforeChoice + 1 && game.awaitingClue(),
          "story choices apply once and reveal the evidence question");
    const int beforeGold = game.progress.gold;
    game.answerClue(firstMission.clueAnswer);
    game.answerClue(firstMission.clueAnswer);
    check(game.progress.clues == 1 && game.progress.gold > beforeGold,
          "a correct deduction awards evidence once, without repeat-farming");
    Game storyResume(game.savePath);
    storyResume.continueGame();
    storyResume.beginMission();
    check(storyResume.clueAnswered && storyResume.choice == 0
          && !storyResume.awaitingChoice() && !storyResume.awaitingClue(),
          "resuming a story checkpoint preserves its answered choices and clue");

    game.page = Page::Inventory;
    game.progress.gold = 10000;
    const auto candidate = find_if(items().begin(), items().end(), [&](const ItemDef& item) {
        return item.unlockChapter == 0 && !game.progress.owned[static_cast<size_t>(item.id)];
    });
    bool purchased = false;
    if (candidate != items().end()) {
        const int oldGold = game.progress.gold;
        purchased = game.buy(candidate->id)
            && game.progress.gold == oldGold - candidate->price
            && !game.buy(candidate->id);
        check(game.equip(1, candidate->id)
              && game.progress.equipment[1][static_cast<size_t>(candidate->slot)] == candidate->id,
              "owned equipment moves into the correct hero slot");
    }
    check(purchased, "requisition spends the actual price and rejects a duplicate purchase");
    const auto locked = find_if(skills().begin(), skills().end(), [&](const SkillDef& skill) {
        return skill.unlockLevel > game.progress.level;
    });
    bool learning = false;
    if (locked != skills().end()) {
        const int points = game.progress.skillPoints;
        learning = !game.learn(locked->id) && game.progress.skillPoints == points;
        game.progress.level = 20;
        learning = learning && game.learn(locked->id) && !game.learn(locked->id)
            && game.progress.skillPoints == points - 1;
    }
    check(learning, "skills enforce level requirements and spend a point exactly once");
    game.page = Page::Battle;
    check(!game.equip(0, 1) && !game.buy(3) && !game.learn(1),
          "combat cannot mutate the persistent loadout or learn abilities mid-turn");

    // Damage an on-disk file, not the in-memory game. Rejection must leave both
    // the active state and damaged bytes intact for possible manual recovery.
    game.page = Page::Map;
    game.save();
    const string intact = bytes(game.savePath);
    string damaged = intact;
    damaged.back() = damaged.back() == 'X' ? 'Y' : 'X';
    { ofstream out(game.savePath, ios::binary | ios::trunc); out << damaged; }
    const int preservedGold = game.progress.gold;
    check(!game.load() && game.progress.gold == preservedGold && bytes(game.savePath) == damaged,
          "a checksum failure preserves the active campaign and the damaged save");
    { ofstream out(game.savePath, ios::binary | ios::trunc); out << "VEILBOUND_SAVE 999\n0\n"; }
    check(!game.load(), "unknown save versions are rejected rather than misread");
    game.newGame();
    check(bytes(game.savePath.string() + ".previous-1") == "VEILBOUND_SAVE 999\n0\n"
          && game.progress.mission == 0 && game.page == Page::Map,
          "an explicitly requested new campaign archives the previous save first");

    finishIntroduction(game);
    game.battle.outcome = Outcome::Defeat;
    const int defeatGold = game.progress.gold;
    game.update(.01f);
    game.update(.01f);
    check(game.page == Page::Reward && game.progress.deaths == 1 && game.progress.gold == defeatGold,
          "defeat is recorded once without losing equipment or granting rewards");
    game.finishReward();
    check(game.page == Page::Battle && game.battle.outcome == Outcome::Ongoing,
          "retry restarts the same encounter with a restored party");
    game.battle.outcome = Outcome::Victory;
    game.update(.01f);
    const int awarded = game.progress.gold;
    game.update(.01f);
    check(game.progress.wave == 1 && game.page == Page::Reward && game.progress.gold == awarded,
          "victory advances the checkpoint exactly once, even if update keeps running");
    Game midpoint(game.savePath);
    midpoint.continueGame();
    midpoint.beginMission();
    check(midpoint.page == Page::Battle && midpoint.progress.wave == 1
          && midpoint.battle.wave == 1 && midpoint.battle.outcome == Outcome::Ongoing,
          "an encounter checkpoint resumes the second encounter without replaying rewards");

    // Complete the entire real campaign using forced battle outcomes. Battle
    // mechanics have separate tests; here the purpose is to detect skipped
    // missions, duplicate rewards, a missing ending, or out-of-bounds story data.
    Game campaign(folder.path / "full-campaign.sav");
    campaign.newGame();
    int encounters = 0;
    bool allTransitions = true;
    for (int mission = 0; mission < MissionCount && allTransitions; ++mission) {
        allTransitions = campaign.progress.mission == mission;
        finishIntroduction(campaign, mission % 2);
        for (int wave = 0; wave < 2 && allTransitions; ++wave) {
            allTransitions = campaign.page == Page::Battle && campaign.progress.wave == wave;
            if (!allTransitions) break;
            campaign.battle.outcome = Outcome::Victory;
            campaign.update(.01f);
            ++encounters;
            allTransitions = campaign.page == Page::Reward;
            campaign.finishReward();
        }
    }
    check(allTransitions && encounters == 64 && campaign.progress.mission == MissionCount
          && campaign.progress.campaignComplete && campaign.page == Page::Ending
          && all_of(campaign.progress.completed.begin(), campaign.progress.completed.end(), [](bool value) { return value; }),
          "all 32 cases and 64 encounters progress to the ending without a skipped checkpoint");
    check(campaign.progress.level > 1 && campaign.progress.clues == MissionCount
          && campaign.progress.compassion == 0 && campaign.progress.journal.size() >= 3 * MissionCount,
          "full campaign carries experience, evidence, moral choices and journal discoveries");
    Game epilogue(campaign.savePath);
    check(epilogue.continueGame() && epilogue.page == Page::Ending
          && epilogue.progress.completed.back(), "a completed campaign reloads into its ending safely");
    check(any_of(epilogue.progress.journal.begin(), epilogue.progress.journal.end(), [](const string& entry) {
              return entry.find(missions().back().outro.back()) != string::npos;
          }), "the full final-case aftermath survives save/load in the journal");

    cout << passed << " campaign checks passed, " << failed << " failed\n";
    return failed == 0;
}
} // namespace veil

int main() {
    string reason;
    if (!veil::validateContent(reason)) {
        cerr << "FAIL content validation: " << reason << '\n';
        return 1;
    }
    cout << "PASS campaign, enemy, item and ability content validation\n";
    const bool battle = veil::runBattleChecks();
    const bool campaign = veil::runGameChecks();
    return battle && campaign ? 0 : 1;
}

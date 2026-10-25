#pragma once

// Game is the campaign coordinator. Battle owns one tactical encounter;
// Progress owns the information that should survive after closing the game.
#include "Battle.hpp"
#include <filesystem>

namespace veil {

class Game {
public:
    explicit Game(std::filesystem::path file);
    void newGame();
    bool continueGame();
    void beginMission();
    void advanceStory();
    void choose(int option);
    void answerClue(int option);
    void enterBattle();
    void update(float dt);
    void finishReward();
    void retry();
    bool buy(int itemId);
    bool equip(int hero, int itemId);
    bool learn(int skillId);
    bool save();
    bool load();
    int nextLevelXp() const;
    bool awaitingChoice() const;
    bool awaitingClue() const;
    std::string storyText() const;

    Progress progress;
    Battle battle;
    Page page{Page::Title};
    int storyLine{}, choice{-1}, clueSelection{-1}, lastMission{};
    bool clueAnswered{};
    std::string message, rewardText;
    std::filesystem::path savePath;

private:
    int storyMission_{-1};
    void resetStory();
    void awardExperience(int amount);
    void concludeBattle();
};

// Like the battle tests, these exercise real public operations without raylib.
bool runGameChecks();
} // namespace veil

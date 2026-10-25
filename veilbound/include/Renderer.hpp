#pragma once

// The renderer knows about graphics; the battle and campaign do not. This
// boundary is why their tests can run without a window or a graphics driver.
#include "Game.hpp"
#include <raylib.h>
#include <filesystem>

namespace veil {
inline constexpr int TileSize = 64;
inline constexpr Rectangle BoardRect{278, 210, 768, 512};

// Transient interface choices are not saved campaign progress. action -2
// means movement, -1 means basic attack, and other values are skill IDs.
struct UiState {
    int selectedHero{}, selectedItem{}, selectedSkill{}, action{-2}, inventoryPage{};
    float time{};
    Pos hovered{-1, -1};
    Vector2 mouse{-100, -100};
    bool interactive{true};
    bool paused{};
};

class Renderer {
public:
    explicit Renderer(const std::filesystem::path& executableDirectory);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void draw(Game& game, UiState& ui);

private:
    Font font_{};
    bool ownsFont_{}, confirmNew_{};
    int journalPage_{}, rewardPage_{};
    std::string rewardSeen_;
    void text(const std::string&, float x, float y, float size, Color color) const;
    float wrap(const std::string&, Rectangle area, float size, Color color) const;
    bool button(const std::string&, Rectangle, const UiState&, bool enabled = true, bool selected = false) const;
    void header(Game&, const UiState&) const;
    void title(Game&, UiState&);
    void map(Game&, UiState&);
    void story(Game&, UiState&);
    void battle(Game&, UiState&);
    void inventory(Game&, UiState&);
    void skillTree(Game&, UiState&);
    void journal(Game&, UiState&);
    void reward(Game&, UiState&);
    void ending(Game&, UiState&);
    void actorFigure(const Unit&, Vector2 center, float time) const;
};
} // namespace veil

#pragma once

// A header describes an interface: other files can use these types without
// needing to know all the drawing instructions hidden in Renderer.cpp.
#include "World.hpp"
#include <SFML/Graphics.hpp>
#include <filesystem>

namespace skyline {

// An enum gives readable names to the four possible screens.
enum class Screen { Title, Playing, Paused, Finished };

// This little bundle is the information the interface needs each frame.
struct UiState {
    Screen screen{Screen::Title};
    float visualTime{};  // Decorative animations continue while physics pauses.
    float fps{};
    bool showStats{};
    bool muted{};
    float bestTime{};    // Zero means no successful run in this session yet.
};

class Renderer {
public:
    // The executable directory helps us find the bundled, licensed font.
    explicit Renderer(const std::filesystem::path& executableDirectory);
    void draw(sf::RenderTarget& target, const World& world, const UiState& ui);

private:
    sf::Font font_; // Kept alive as long as any text uses it.

    // Helpers keep the main drawing routine readable rather than repeating
    // all of SFML's shape setup instructions for every rectangle and label.
    void text(sf::RenderTarget& target, const std::string& message,
              sf::Vector2f position, unsigned size, sf::Color color,
              float letterSpacing = 1.f) const;
    void background(sf::RenderTarget& target, float camera, float time) const;
    void course(sf::RenderTarget& target, const World& world, float time, bool showSigns) const;
    void runner(sf::RenderTarget& target, const Player& player, float time) const;
    void hud(sf::RenderTarget& target, const World& world, const UiState& ui) const;
    void title(sf::RenderTarget& target, const UiState& ui) const;
    void overlay(sf::RenderTarget& target, const World& world, const UiState& ui) const;
};

} // namespace skyline

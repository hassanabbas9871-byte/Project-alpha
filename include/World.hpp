#pragma once

// A header describes the things other source files are allowed to use.
// SFML supplies vectors (x/y pairs), rectangles, and colors here. No window is
// required by this file, so the physics can also run in a terminal-only test.
#include <SFML/Graphics.hpp>
#include <vector>

namespace skyline {

// Coordinates use a 1280 by 720 virtual canvas. The renderer can scale this
// canvas to a real window without changing any of the game rules.
inline constexpr float ViewWidth = 1280.f;
inline constexpr float ViewHeight = 720.f;

// The game loop translates keyboard buttons into this small packet of intent.
// "Pressed" means a new press; "Held" stays true until a button is released.
struct Input {
    float horizontal{}; // -1 = left, 0 = neutral, +1 = right.
    bool jumpPressed{};
    bool jumpHeld{};
    bool dashPressed{};
    bool restartPressed{};
};

// A platform is a solid rectangle. origin remembers a moving platform's
// starting position, so its motion never accumulates rounding errors.
struct Platform {
    sf::FloatRect bounds;
    bool moving{};
    sf::Vector2f origin{};
    float phase{}; // Offsets its sine-wave motion from the other platforms.
};

struct Hazard { sf::FloatRect bounds; }; // Touching a red hazard causes a respawn.
struct Orb { sf::Vector2f position; bool collected{}; }; // Also refills the dash.
struct Checkpoint { sf::Vector2f position; bool active{}; };

// Particles are tiny temporary visual objects. life counts down to zero;
// maxLife lets the renderer fade each particle smoothly as it ages.
struct Particle {
    sf::Vector2f position, velocity;
    float life{}, maxLife{}, size{};
    sf::Color color;
};

// position is the player's TOP-LEFT corner, not the middle of their body.
// Velocity is measured in pixels per second. Positive y points DOWN the screen.
struct Player {
    sf::Vector2f position, velocity;
    sf::Vector2f size{26.f, 42.f};
    bool grounded{}, onWall{}, dashReady{true};
    int wallDirection{}, facing{1}; // Wall side and looking direction: -1 or +1.
    float dashTime{}, squash{}; // Timers for dash movement and landing animation.
};

// World owns the level and the rules. The renderer reads these public objects,
// but input, collisions, and movement remain in this one easy-to-find class.
class World {
public:
    static constexpr float LevelWidth = 7600.f;

    World();
    void reset(); // Start a completely new run, including collectibles and time.
    void update(float dt, const Input& input); // Advance the simulation by dt seconds.
    void respawn(); // Return to the most recent checkpoint.
    sf::Vector2f spawn() const; // Top-left position to use for the next respawn.

    Player player;
    std::vector<Platform> platforms;
    std::vector<Hazard> hazards;
    std::vector<Orb> orbs;
    std::vector<Checkpoint> checkpoints;
    std::vector<Particle> particles;
    float cameraX{}, elapsed{}, shake{}, distance{};
    int deaths{}, collected{};
    bool finished{};

private:
    // Grace periods make inputs feel forgiving without changing the level.
    float coyoteTime_{}, jumpBuffer_{}, wallGrace_{}, wallJumpLock_{}, trailTime_{};
    int rememberedWall_{}, standingPlatform_{-1};
    bool wasJumpHeld_{};
    unsigned particleSeed_{1}; // A repeatable random-number seed for visual sparks.

    void burst(sf::Vector2f center, sf::Color color, int count, float speed);
    void updateParticles(float dt);
    void die();
};

// These checks exercise the same movement code as the real game, without
// opening a window. main() exposes them through the --self-test command.
bool runSelfTests();

} // namespace skyline

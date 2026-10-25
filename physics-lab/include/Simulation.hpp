#pragma once

// This header contains NO graphics-library types. The same physics can run in
// a window, in a test, or on a computer that has no monitor at all.
#include <cstddef>
#include <vector>

namespace flux {

// A vector is an arrow: x is the horizontal part and y is the vertical part.
// These operators let the equations below look like ordinary arithmetic.
struct Vec {
    float x{}, y{};
    constexpr Vec operator+(Vec b) const { return {x + b.x, y + b.y}; }
    constexpr Vec operator-(Vec b) const { return {x - b.x, y - b.y}; }
    constexpr Vec operator*(float k) const { return {x * k, y * k}; }
    constexpr Vec operator/(float k) const { return {x / k, y / k}; }
    constexpr Vec& operator+=(Vec b) { x += b.x; y += b.y; return *this; }
    constexpr Vec& operator-=(Vec b) { x -= b.x; y -= b.y; return *this; }
};
inline constexpr float WorldWidth = 1060.f, WorldHeight = 650.f;
enum class Scene { Cascade, Cloth, Orbits };
enum class Tool { Attract, Repel, Grab, Cut };

// Verlet integration remembers the last position instead of storing velocity.
// The difference between those positions is how far we moved during one step.
struct Particle {
    Vec position, previous;
    float radius{4.f}, inverseMass{1.f}; // Inverse mass is 1 / mass.
    int palette{};
    bool pinned{}; // A pinned particle is an immovable cloth anchor.
};
struct Link { std::size_t a{}, b{}; float rest{}; bool active{true}; };
struct Obstacle { Vec position; float radius{}; };

class Simulation {
public:
    Simulation();
    void reset(Scene selected);
    void update(float dt);
    void burst(Vec position); // Send a pressure-wave impulse through nearby dots.
    void spawn(Vec position, int count = 50);
    void releasePointer();
    int activeLinks() const;
    float meanSpeed() const;
    bool finite() const;

    std::vector<Particle> particles;
    std::vector<Link> links;
    std::vector<Obstacle> obstacles;
    Scene scene{Scene::Cascade};
    Tool tool{Tool::Attract};
    Vec pointer{};
    bool pointerDown{};
    float gravity{850.f}, timeScale{1.f}, elapsed{}, pointerRadius{100.f};
    bool collisions{true};
    int grabbed{-1};
    int clothColumns{35}; // Lets the renderer recognize neighboring cloth cells.

private:
    float lastStep_{1.f / 120.f}, emitTimer_{};
    unsigned random_{0xC0FFEEu};
    Vec lastPointer_{};
    bool wasPointerDown_{};
    std::vector<int> gridHeads_, gridNext_; // Reused spatial-hash storage.

    float random01();
    void step(float dt);
    void collideParticles();
    void constrain(Particle& particle);
    void cutStroke(Vec from, Vec to);
};

bool runPhysicsChecks();
} // namespace flux

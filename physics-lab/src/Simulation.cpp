#include "Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>

// This convenient shorthand is kept in the implementation file. A header
// should not unexpectedly add names to every other file that includes it.
using namespace std;

namespace flux {
namespace {
constexpr float NominalStep = 1.f / 120.f;
constexpr size_t ParticleLimit = 1600;
constexpr float OrbitStrength = 1100000.f;
constexpr Vec Sun{530.f, 320.f};
float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y; }
float length(Vec a) { return sqrt(dot(a, a)); }
Vec safeNormal(Vec a) { const float n = length(a); return n > .0001f ? a / n : Vec{1, 0}; }
Vec clampPoint(Vec p, float inset = 8.f) {
    return {clamp(p.x, inset, WorldWidth - inset), clamp(p.y, inset, WorldHeight - inset)};
}

// How close is a point to a finite line segment? Project onto the line, then
// clamp to the ends. This also handles a segment that has zero length.
float pointSegment(Vec p, Vec a, Vec b) {
    const Vec segment = b - a;
    const float t = clamp(dot(p - a, segment) / max(.000001f, dot(segment, segment)), 0.f, 1.f);
    return length(p - (a + segment * t));
}
float cross(Vec a, Vec b) { return a.x * b.y - a.y * b.x; }
bool segmentsCross(Vec a, Vec b, Vec c, Vec d) {
    const float determinant = cross(b - a, d - c);
    if (abs(determinant) < .00001f) return false;
    const float t = cross(c - a, d - c) / determinant;
    const float u = cross(c - a, b - a) / determinant;
    return t >= 0 && t <= 1 && u >= 0 && u <= 1;
}
} // namespace

Simulation::Simulation() { reset(Scene::Cascade); }
float Simulation::random01() {
    // A tiny repeatable generator makes the same preset reproducible in tests.
    random_ = random_ * 1664525u + 1013904223u;
    return static_cast<float>((random_ >> 8) & 0xFFFFFFu) / 16777215.f;
}

void Simulation::reset(Scene selected) {
    scene = selected;
    particles.clear(); links.clear(); obstacles.clear();
    elapsed = emitTimer_ = 0;
    lastStep_ = NominalStep;
    random_ = 0xC0FFEEu;
    grabbed = -1;
    pointerDown = wasPointerDown_ = false;
    gravity = 850;
    collisions = true;
    // Time scale is a user's playback preference, so changing presets keeps it.
    if (scene == Scene::Cascade) {
        obstacles = {{{305, 250}, 66}, {{755, 250}, 66}, {{530, 414}, 87},
                     {{175, 480}, 44}, {{885, 480}, 44}};
        // Two loose initial waterfalls give an interesting picture immediately.
        // Their x/y spacing exceeds the particle diameter: no explosive overlap.
        for (int side = 0; side < 2; ++side)
            for (int row = 0; row < 35; ++row)
                for (int column = 0; column < 10; ++column) {
                    const Vec p{135.f + side * 675.f + column * 10.f,
                                12.f + row * 10.f};
                    particles.push_back({p, p - Vec{side == 0 ? .25f : -.25f, .4f},
                                         4.2f, 1, side == 0 ? column % 3 : 3 + column % 3, false});
                }
    } else if (scene == Scene::Cloth) {
        constexpr int columns = 35, rows = 22;
        constexpr float spacing = 17.f;
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < columns; ++x) {
                const Vec p{241.f + x * spacing, 80.f + y * spacing};
                const bool fixed = y == 0 && (x % 5 == 0 || x == columns - 1);
                particles.push_back({p, p, 2.6f, fixed ? 0.f : 1.f, x * 6 / columns, fixed});
                const size_t i = particles.size() - 1;
                if (x > 0) links.push_back({i - 1, i, spacing});
                if (y > 0) links.push_back({i - columns, i, spacing});
                // Diagonal threads resist shear: without these, square cells
                // could fold into diamond shapes at no physical cost.
                if (x > 0 && y > 0) links.push_back({i - columns - 1, i, spacing * sqrt(2.f)});
            }
        obstacles = {{{530, 535}, 83}};
    } else {
        gravity = 0;
        // Circular orbit speed follows v²/r = GM/r², hence v = sqrt(GM/r).
        // A half-step acceleration offset gives Verlet a better initial orbit.
        for (int i = 0; i < 600; ++i) {
            const float radius = 78.f + random01() * 208.f;
            const float angle = random01() * 2.f * numbers::pi_v<float>;
            const Vec normal{cos(angle), sin(angle)};
            const Vec p = Sun + normal * radius;
            const Vec velocity{-normal.y * sqrt(OrbitStrength / radius), normal.x * sqrt(OrbitStrength / radius)};
            const Vec acceleration = normal * (-OrbitStrength / (radius * radius));
            particles.push_back({p, p - velocity * NominalStep + acceleration * (.5f * NominalStep * NominalStep),
                                 1.7f + random01() * 1.4f, 1, i % 6, false});
        }
    }
}

void Simulation::releasePointer() {
    if (grabbed >= 0 && static_cast<size_t>(grabbed) < particles.size()) {
        // A dragged point stops at release instead of acquiring an accidental
        // teleport-sized velocity. Ordinary free particles keep their momentum.
        particles[static_cast<size_t>(grabbed)].previous = particles[static_cast<size_t>(grabbed)].position;
    }
    grabbed = -1;
    pointerDown = false;
    wasPointerDown_ = false;
}

void Simulation::spawn(Vec position, int count) {
    if (scene == Scene::Cloth) return; // A cloth node also needs its thread links.
    count = clamp(count, 0, 120);
    for (int i = 0; i < count && particles.size() < ParticleLimit; ++i) {
        const float angle = random01() * 2.f * numbers::pi_v<float>;
        const Vec offset{cos(angle), sin(angle)};
        Vec p = clampPoint(position + offset * (10.f + random01() * 32.f));
        Vec velocity = offset * (20.f + random01() * 75.f);
        if (scene == Scene::Orbits) {
            const Vec n = safeNormal(p - Sun);
            velocity = Vec{-n.y, n.x} * sqrt(OrbitStrength / max(35.f, length(p - Sun)));
        }
        particles.push_back({p, p - velocity * lastStep_, scene == Scene::Orbits ? 2.4f : 4.2f,
                             1, static_cast<int>(random01() * 5.99f), false});
    }
}

void Simulation::burst(Vec position) {
    for (auto& particle : particles) {
        if (particle.pinned) continue;
        const Vec difference = particle.position - position;
        const float distance = length(difference);
        if (distance > 240.f) continue;
        // Impulse changes velocity immediately; moving previous backwards
        // increases the forward displacement at the next integration step.
        particle.previous -= safeNormal(difference) * ((1.f - distance / 240.f) * 650.f * lastStep_);
    }
}

void Simulation::cutStroke(Vec from, Vec to) {
    for (auto& link : links) {
        if (!link.active) continue;
        const Vec a = particles[link.a].position, b = particles[link.b].position;
        if (segmentsCross(from, to, a, b) || pointSegment(from, a, b) < 9.f
            || pointSegment(to, a, b) < 9.f) link.active = false;
    }
}

void Simulation::constrain(Particle& particle) {
    if (particle.pinned) return;
    // Reflect only the velocity component pointing into a collision surface.
    // Damping of the other component is surface friction, not an extra force.
    auto bounce = [&](Vec normal) {
        Vec velocity = particle.position - particle.previous;
        const float inward = dot(velocity, normal);
        if (inward < 0) velocity -= normal * (inward * 1.45f);
        velocity = velocity * .985f;
        particle.previous = particle.position - velocity;
    };
    const float r = particle.radius;
    const Vec before = particle.position;
    particle.position.x = clamp(particle.position.x, r, WorldWidth - r);
    particle.position.y = clamp(particle.position.y, r, WorldHeight - r);
    // Move previous by the same correction before reflecting, otherwise the
    // positional correction itself would inject additional kinetic energy.
    const Vec correction = particle.position - before;
    particle.previous += correction;
    if (before.x < r) bounce({1, 0});
    if (before.x > WorldWidth - r) bounce({-1, 0});
    if (before.y < r) bounce({0, 1});
    if (before.y > WorldHeight - r) bounce({0, -1});
    for (const auto& obstacle : obstacles) {
        const Vec difference = particle.position - obstacle.position;
        const float distance = length(difference), minimum = obstacle.radius + r;
        if (distance >= minimum) continue;
        const Vec normal = safeNormal(difference);
        const Vec adjustment = normal * (minimum - distance);
        particle.position += adjustment;
        particle.previous += adjustment;
        bounce(normal);
    }
}

void Simulation::collideParticles() {
    // A dense spatial hash divides the world into cells. A particle only needs
    // to inspect its own cell and eight neighbors, not all 1600 other particles.
    constexpr float cellSize = 12.f;
    constexpr int columns = 89, rows = 55;
    gridHeads_.assign(columns * rows, -1);
    gridNext_.resize(particles.size());
    for (size_t i = 0; i < particles.size(); ++i) {
        const int x = clamp(static_cast<int>(particles[i].position.x / cellSize), 0, columns - 1);
        const int y = clamp(static_cast<int>(particles[i].position.y / cellSize), 0, rows - 1);
        const auto cell = static_cast<size_t>(y * columns + x);
        gridNext_[i] = gridHeads_[cell];
        gridHeads_[cell] = static_cast<int>(i);
    }
    for (size_t i = 0; i < particles.size(); ++i) {
        auto& a = particles[i];
        const int x = clamp(static_cast<int>(a.position.x / cellSize), 0, columns - 1);
        const int y = clamp(static_cast<int>(a.position.y / cellSize), 0, rows - 1);
        for (int cy = max(0, y - 1); cy <= min(rows - 1, y + 1); ++cy)
            for (int cx = max(0, x - 1); cx <= min(columns - 1, x + 1); ++cx)
                for (int j = gridHeads_[static_cast<size_t>(cy * columns + cx)]; j != -1; j = gridNext_[static_cast<size_t>(j)]) {
                    if (static_cast<size_t>(j) <= i) continue;
                    auto& b = particles[static_cast<size_t>(j)];
                    const Vec difference = b.position - a.position;
                    const float minimum = a.radius + b.radius;
                    const float squared = dot(difference, difference);
                    if (squared >= minimum * minimum) continue;
                    const float distance = sqrt(squared);
                    const Vec normal = distance > .0001f ? difference / distance : Vec{1, 0};
                    const float weightA = (a.pinned || grabbed == static_cast<int>(i)) ? 0.f : a.inverseMass;
                    const float weightB = (b.pinned || grabbed == j) ? 0.f : b.inverseMass;
                    const float massSum = weightA + weightB;
                    if (massSum <= 0) continue;
                    const Vec correction = normal * ((minimum - distance) / massSum);
                    a.position -= correction * weightA;
                    b.position += correction * weightB;
                }
    }
}

void Simulation::update(float dt) {
    // Speed controls alter simulated time, not the stability of a numerical
    // step. Split a longer interval into several small, safe substeps.
    const float duration = clamp(dt, 0.f, 1.f / 30.f) * clamp(timeScale, 0.f, 3.f);
    if (duration <= 0) return;
    const int steps = max(1, static_cast<int>(ceil(duration / NominalStep)));
    const float stepTime = duration / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) step(stepTime);
}

void Simulation::step(float dt) {
    elapsed += dt;
    if (!pointerDown && wasPointerDown_) releasePointer();
    if (pointerDown && tool == Tool::Grab && grabbed < 0) {
        float nearest = 42.f;
        for (size_t i = 0; i < particles.size(); ++i) {
            const float distance = length(particles[i].position - pointer);
            if (!particles[i].pinned && distance < nearest) { nearest = distance; grabbed = static_cast<int>(i); }
        }
    }
    if (pointerDown && tool != Tool::Grab && grabbed >= 0) {
        grabbed = -1; // Switching tools must not leave a particle suspended.
    }
    if (pointerDown && scene == Scene::Cloth && tool == Tool::Cut)
        cutStroke(wasPointerDown_ ? lastPointer_ : pointer, pointer);

    if (scene == Scene::Cascade) {
        emitTimer_ += dt;
        if (emitTimer_ > .055f && particles.size() < ParticleLimit) {
            emitTimer_ = 0;
            spawn({165 + sin(elapsed * 1.2f) * 55, 18}, 1);
            spawn({895 + sin(elapsed * 1.1f) * 55, 18}, 1);
        }
    }
    const float ratio = dt / max(.00001f, lastStep_);
    for (size_t i = 0; i < particles.size(); ++i) {
        auto& particle = particles[i];
        if (particle.pinned) { particle.previous = particle.position; continue; }
        if (pointerDown && tool == Tool::Grab && grabbed == static_cast<int>(i)) {
            particle.position = clampPoint(pointer);
            particle.previous = particle.position;
            continue;
        }
        Vec acceleration{0, gravity};
        if (scene == Scene::Orbits) {
            const Vec radial = Sun - particle.position;
            // Softening prevents a singular infinity if someone drags a dot
            // straight into the sun; far away it approaches inverse-square gravity.
            const float squared = dot(radial, radial) + 12.f * 12.f;
            acceleration = radial * (OrbitStrength / (squared * sqrt(squared)));
        }
        if (pointerDown && (tool == Tool::Attract || tool == Tool::Repel)) {
            const Vec difference = pointer - particle.position;
            const float distance = length(difference);
            const float range = max(20.f, pointerRadius) * 2.5f;
            if (distance < range) {
                const float amount = (1.f - distance / range) * 2900.f;
                acceleration += safeNormal(difference) * (tool == Tool::Attract ? amount : -amount);
            }
        }
        // Verlet: next = current + previous displacement + acceleration * dt².
        Vec displacement = (particle.position - particle.previous) * ratio;
        const float damping = scene == Scene::Orbits ? 1.f : pow(.996f, dt / NominalStep);
        displacement = displacement * damping;
        // Strong mouse impulses are capped so a fast drag cannot tunnel through
        // every obstacle or produce infinities in a later collision calculation.
        const float speed = length(displacement) / dt;
        if (speed > 1800.f) displacement = displacement * (1800.f / speed);
        const Vec old = particle.position;
        particle.position += displacement + acceleration * (dt * dt);
        particle.previous = old;
    }
    lastStep_ = dt;

    const int iterations = scene == Scene::Cloth ? 7 : 3;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (const auto& link : links) {
            if (!link.active) continue;
            auto& a = particles[link.a]; auto& b = particles[link.b];
            const Vec difference = b.position - a.position;
            const float distance = length(difference);
            const float weightA = (a.pinned || grabbed == static_cast<int>(link.a)) ? 0.f : a.inverseMass;
            const float weightB = (b.pinned || grabbed == static_cast<int>(link.b)) ? 0.f : b.inverseMass;
            const float total = weightA + weightB;
            if (distance < .0001f || total <= 0) continue;
            const Vec correction = difference * ((distance - link.rest) / (distance * total));
            a.position += correction * weightA;
            b.position -= correction * weightB;
        }
        if (scene == Scene::Cascade && collisions) collideParticles();
        if (scene != Scene::Orbits)
            for (auto& particle : particles) constrain(particle);
    }
    // Orbits have a very generous safety boundary. Normally the circular
    // trajectories never touch it, but user impulses can deliberately eject dots.
    if (scene == Scene::Orbits) {
        for (auto& p : particles) {
            if (p.position.x < -400 || p.position.x > WorldWidth + 400
                || p.position.y < -400 || p.position.y > WorldHeight + 400) {
                p.position = clampPoint(p.position);
                p.previous = p.position;
            }
        }
    }
    lastPointer_ = pointer;
    wasPointerDown_ = pointerDown;
}

int Simulation::activeLinks() const {
    return static_cast<int>(count_if(links.begin(), links.end(), [](const Link& link) { return link.active; }));
}
float Simulation::meanSpeed() const {
    if (particles.empty()) return 0;
    float sum = 0;
    for (const auto& p : particles) sum += length(p.position - p.previous) / max(.00001f, lastStep_);
    return sum / static_cast<float>(particles.size());
}
bool Simulation::finite() const {
    return all_of(particles.begin(), particles.end(), [](const Particle& p) {
        return isfinite(p.position.x) && isfinite(p.position.y) && isfinite(p.previous.x) && isfinite(p.previous.y);
    });
}

bool runPhysicsChecks() {
    int passed = 0, failed = 0;
    auto check = [&](bool result, const string& description) {
        cout << (result ? "PASS " : "FAIL ") << description << '\n';
        result ? ++passed : ++failed;
    };
    auto advance = [](Simulation& sim, int count) { for (int i = 0; i < count; ++i) sim.update(NominalStep); };
    Simulation freeFall;
    freeFall.particles = {{{500, 50}, {500, 50}, 4, 1, 0, false}};
    freeFall.obstacles.clear();
    advance(freeFall, 20);
    check(freeFall.particles[0].position.y > 58, "gravity accelerates a free particle downward");

    Simulation floor;
    floor.particles = {{{500, 640}, {500, 634}, 4, 1, 0, false}};
    floor.obstacles.clear();
    floor.update(NominalStep);
    floor.update(NominalStep);
    check(floor.particles[0].position.y <= 646.01f && floor.finite(), "floor collisions contain and reflect a falling disk");

    Simulation obstacle;
    obstacle.particles = {{{530, 395}, {530, 390}, 4, 1, 0, false}};
    obstacle.obstacles = {{{530, 414}, 40}};
    obstacle.update(NominalStep);
    check(length(obstacle.particles[0].position - obstacle.obstacles[0].position) >= 43.99f,
          "circular obstacles push disks outside the collision surface");

    Simulation pair;
    pair.gravity = 0;
    pair.obstacles.clear();
    pair.particles = {{{300, 100}, {300, 100}, 4.2f, 1, 0, false},
                      {{303, 100}, {303, 100}, 4.2f, 1, 1, false}};
    pair.update(NominalStep);
    check(length(pair.particles[1].position - pair.particles[0].position) >= 8.39f,
          "spatial-hash collision separates overlapping disks");

    Simulation flow;
    advance(flow, 600);
    check(flow.finite() && flow.particles.size() <= ParticleLimit && flow.meanSpeed() < 2000,
          "dense cascade remains finite and within its particle budget");
    check(all_of(flow.particles.begin(), flow.particles.end(), [](const Particle& p) {
        return p.position.x >= p.radius - .1f && p.position.x <= WorldWidth - p.radius + .1f
            && p.position.y >= p.radius - .1f && p.position.y <= WorldHeight - p.radius + .1f;
    }), "cascade particles remain within all four container boundaries");

    Simulation cloth;
    cloth.reset(Scene::Cloth);
    const Vec anchor = cloth.particles.front().position;
    advance(cloth, 240);
    float error = 0;
    for (const auto& link : cloth.links)
        error += abs(length(cloth.particles[link.a].position - cloth.particles[link.b].position) - link.rest) / link.rest;
    error /= static_cast<float>(cloth.links.size());
    check(length(cloth.particles.front().position - anchor) < .0001f && error < .09f,
          "cloth anchors remain fixed and thread lengths resist stretching");
    const int beforeCut = cloth.activeLinks();
    cloth.tool = Tool::Cut; cloth.pointerDown = true; cloth.pointer = {400, 170};
    cloth.update(NominalStep); cloth.pointer = {650, 340}; cloth.update(NominalStep);
    check(cloth.activeLinks() < beforeCut - 10, "cutting a continuous mouse stroke breaks actual cloth threads");
    advance(cloth, 120);
    check(cloth.finite(), "torn cloth remains numerically stable");

    Simulation drag;
    drag.reset(Scene::Cloth);
    drag.tool = Tool::Grab; drag.pointerDown = true; drag.pointer = drag.particles[200].position;
    drag.update(NominalStep);
    const int selected = drag.grabbed;
    drag.pointer = {600, 250}; drag.update(NominalStep);
    check(selected >= 0 && length(drag.particles[static_cast<size_t>(selected)].position - drag.pointer) < .1f,
          "grab moves the selected node to the mouse position");
    drag.releasePointer();
    check(drag.grabbed == -1 && !drag.pointerDown, "releasing the pointer releases the grabbed node");

    Simulation orbit;
    orbit.reset(Scene::Orbits);
    const float initialRadius = length(orbit.particles[0].position - Sun);
    advance(orbit, 2400);
    const float finalRadius = length(orbit.particles[0].position - Sun);
    check(orbit.finite() && abs(finalRadius - initialRadius) < initialRadius * .12f,
          "unperturbed orbit stays bounded over twenty simulated seconds");

    Simulation paused;
    paused.timeScale = 0;
    const Vec beforePause = paused.particles[0].position;
    advance(paused, 60);
    check(length(paused.particles[0].position - beforePause) < .0001f && paused.elapsed == 0,
          "zero time scale pauses both particles and simulation clock");
    paused.spawn({400, 100}, 10000);
    check(paused.particles.size() <= ParticleLimit, "spawning honors the fixed particle budget");
    cout << passed << " passed, " << failed << " failed\n";
    return failed == 0;
}
} // namespace flux

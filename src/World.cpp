#include "World.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

// This is convenient in a small implementation file, as requested. We keep it
// OUT of the header so it cannot unexpectedly affect somebody else's code.
using namespace std;

namespace skyline {
namespace {

// A few named numbers are much easier to understand and tune than unexplained
// numbers scattered through the movement code. All times below use seconds.
constexpr float RunSpeed = 350.f;
constexpr float Acceleration = 2800.f;
constexpr float Friction = 2600.f;
constexpr float Gravity = 1750.f;
constexpr float JumpSpeed = 650.f;
constexpr float DashSpeed = 850.f;
constexpr float DashDuration = 0.16f;

// Move toward a target without overshooting it. This gives running momentum,
// but avoids a slippery character that takes a long time to stop.
float approach(float value, float target, float amount) {
    if (value < target) return min(value + amount, target);
    return max(value - amount, target);
}

sf::FloatRect body(const Player& player) {
    return {player.position.x, player.position.y, player.size.x, player.size.y};
}

sf::Vector2f center(const Player& player) {
    return player.position + player.size / 2.f;
}

} // An anonymous namespace keeps helpers private to this source file.

World::World() { reset(); }

void World::reset() {
    // {} creates a fresh object using the default values in the header.
    player = Player{};
    player.position = {110.f, 500.f};
    platforms.clear();
    hazards.clear();
    orbs.clear();
    checkpoints.clear();
    particles.clear();
    cameraX = elapsed = shake = distance = 0.f;
    deaths = collected = 0;
    finished = false;
    coyoteTime_ = jumpBuffer_ = wallGrace_ = wallJumpLock_ = trailTime_ = 0.f;
    rememberedWall_ = 0;
    standingPlatform_ = -1;
    wasJumpHeld_ = false;
    particleSeed_ = 1;

    // A lambda is a tiny local function. This one keeps the level recipe below
    // readable: x, top y, width, and an optional moving-platform flag.
    auto roof = [this](float x, float y, float width, bool moving = false,
                       float phase = 0.f) {
        platforms.push_back({{x, y, width, moving ? 22.f : 800.f - y},
                             moving, {x, y}, phase});
    };

    // ACT I: wide roofs teach running, short hops, and then longer dash jumps.
    roof(0, 610, 640);
    roof(730, 570, 280);
    roof(1110, 525, 260);
    roof(1470, 595, 560);

    // ACT II: staggered towers reward wall jumps. Their vertical sides are
    // solid, so a mistimed jump can still be rescued with a wall jump.
    roof(2110, 610, 240);
    roof(2430, 510, 180);
    roof(2680, 410, 170);
    roof(2930, 490, 170);
    roof(3180, 580, 500);

    // ACT III: floating maintenance lifts move vertically. A rider receives
    // the same displacement as the lift before their own movement is applied.
    roof(3770, 530, 180, true, 0.4f);
    roof(4040, 490, 190);
    roof(4320, 570, 400);
    roof(4810, 530, 180);
    roof(5090, 455, 200);
    roof(5380, 550, 380);
    roof(5860, 510, 190, true, 2.f);
    roof(6140, 455, 190);
    roof(6420, 545, 350);
    roof(6860, 595, 740);

    // Hazards sit on top of roofs. Their collision boxes are slightly inset
    // from the drawn spikes, which makes close successful jumps feel fair.
    hazards = {{{448, 588, 58, 22}}, {{1740, 573, 70, 22}},
               {{3430, 558, 72, 22}}, {{4520, 548, 66, 22}},
               {{5540, 528, 80, 22}}, {{6590, 523, 64, 22}},
               {{7080, 573, 76, 22}}};

    // Orbs encourage optional high lines; touching one restores your dash even
    // in midair, so an experienced player can chain a faster route.
    for (sf::Vector2f point : vector<sf::Vector2f>{
             {480, 485}, {865, 455}, {1225, 405}, {1780, 463},
             {2260, 490}, {2510, 395}, {2765, 295}, {3015, 375},
             {3465, 445}, {3860, 390}, {4130, 375}, {4550, 435},
             {4900, 410}, {5190, 340}, {5580, 415}, {5950, 360},
             {6235, 340}, {6620, 410}, {7120, 460}, {7320, 470}}) {
        orbs.push_back({point, false});
    }

    // A checkpoint's position is its base on the roof, not a respawn top-left.
    checkpoints = {{{1630, 595}, false}, {{3280, 580}, false},
                   {{4410, 570}, false}, {{5450, 550}, false},
                   {{6490, 545}, false}};
}

sf::Vector2f World::spawn() const {
    sf::Vector2f result{110.f, 500.f};
    // Checkpoints are sorted left to right. The last active one wins.
    for (const auto& checkpoint : checkpoints) {
        if (checkpoint.active) result = checkpoint.position - sf::Vector2f{13.f, 44.f};
    }
    return result;
}

void World::respawn() {
    player = Player{};
    player.position = spawn();
    standingPlatform_ = -1;
    coyoteTime_ = jumpBuffer_ = wallGrace_ = wallJumpLock_ = trailTime_ = 0.f;
    wasJumpHeld_ = false;
    // Collected orbs and activated checkpoints stay collected after a death.
    burst(center(player), sf::Color(91, 255, 213), 22, 170.f);
}

void World::die() {
    ++deaths; // ++ means "add one".
    burst(center(player), sf::Color(255, 101, 135), 24, 250.f);
    shake = 11.f;
    respawn();
    // Avoid making the camera pan back across the whole level after a fall.
    cameraX = clamp(player.position.x - 330.f, 0.f, LevelWidth - ViewWidth);
}

void World::burst(sf::Vector2f position, sf::Color color, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        // A small deterministic generator is enough for cosmetic randomness.
        // Unsigned overflow is well-defined in C++, unlike signed overflow.
        particleSeed_ = particleSeed_ * 1664525u + 1013904223u;
        const float random = static_cast<float>(particleSeed_ % 1000u) / 1000.f;
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
        const float lifetime = 0.25f + random * 0.35f;
        particles.push_back({position,
            {cos(angle) * speed * (0.4f + random), sin(angle) * speed * (0.4f + random)},
            lifetime, lifetime, 2.f + random * 3.f, color});
    }
    // A cap prevents unbounded memory growth if many effects happen together.
    if (particles.size() > 600) particles.erase(particles.begin(), particles.end() - 600);
}

void World::updateParticles(float dt) {
    for (auto& particle : particles) {
        particle.life -= dt;
        particle.position += particle.velocity * dt;
        particle.velocity.y += 220.f * dt;
    }
    // erase_if removes expired elements without invalidating a loop iterator.
    erase_if(particles, [](const Particle& particle) { return particle.life <= 0.f; });
}

void World::update(float dt, const Input& input) {
    // A fixed game-loop timestep is the normal caller. This guard also keeps
    // a paused debugger or accidental oversized call from tunneling through roofs.
    dt = clamp(dt, 0.f, 1.f / 30.f);
    updateParticles(dt);
    shake = max(0.f, shake - dt * 35.f);
    player.squash = max(0.f, player.squash - dt * 5.f);
    if (finished) return;
    elapsed += dt;

    if (input.restartPressed) {
        die();
        return;
    }

    // Update lifts before the player. A saved platform index tells us which
    // surface, if any, the player stood on at the end of the previous step.
    for (size_t i = 0; i < platforms.size(); ++i) {
        auto& platform = platforms[i];
        if (!platform.moving) continue;
        const float oldY = platform.bounds.top;
        platform.bounds.top = platform.origin.y + sin(elapsed * 1.6f + platform.phase) * 42.f;
        if (player.grounded && standingPlatform_ == static_cast<int>(i)) {
            player.position.y += platform.bounds.top - oldY;
        }
    }

    // Coyote time: you may still jump for 0.11 seconds after leaving a ledge.
    // Jump buffering: pressing just before landing queues that jump briefly.
    coyoteTime_ = player.grounded ? 0.11f : max(0.f, coyoteTime_ - dt);
    jumpBuffer_ = input.jumpPressed ? 0.12f : max(0.f, jumpBuffer_ - dt);
    wallGrace_ = player.onWall ? 0.10f : max(0.f, wallGrace_ - dt);
    if (player.onWall) rememberedWall_ = player.wallDirection;
    wallJumpLock_ = max(0.f, wallJumpLock_ - dt);
    if (input.horizontal != 0.f) player.facing = input.horizontal > 0.f ? 1 : -1;

    // A dash is a short burst of horizontal velocity with gravity suspended.
    if (input.dashPressed && player.dashReady) {
        player.dashReady = false;
        player.dashTime = DashDuration;
        player.velocity = {static_cast<float>(player.facing) * DashSpeed, 0.f};
        shake = max(shake, 3.5f);
        burst(center(player), sf::Color(91, 255, 213), 12, 110.f);
    }

    if (jumpBuffer_ > 0.f && player.dashTime <= 0.f) {
        if (coyoteTime_ > 0.f) {
            player.velocity.y = -JumpSpeed;
            player.grounded = false;
            coyoteTime_ = jumpBuffer_ = 0.f;
            player.squash = 0.35f;
            burst(player.position + sf::Vector2f{13, 42}, sf::Color(203, 229, 255), 9, 100.f);
        } else if (wallGrace_ > 0.f) {
            // Push away from the wall. A brief steering lock stops the held
            // direction from immediately cancelling this outward impulse.
            player.velocity = {-static_cast<float>(rememberedWall_) * 460.f, -625.f};
            wallJumpLock_ = 0.16f;
            player.dashReady = true;
            wallGrace_ = jumpBuffer_ = 0.f;
            burst(center(player), sf::Color(255, 206, 128), 12, 130.f);
        }
    }

    if (player.dashTime > 0.f) {
        player.dashTime = max(0.f, player.dashTime - dt);
        trailTime_ -= dt;
        if (trailTime_ <= 0.f) {
            trailTime_ = 0.018f;
            burst(center(player), sf::Color(91, 255, 213, 155), 3, 35.f);
        }
    } else {
        if (wallJumpLock_ <= 0.f) {
            const float target = clamp(input.horizontal, -1.f, 1.f) * RunSpeed;
            player.velocity.x = approach(player.velocity.x, target,
                (input.horizontal == 0.f ? Friction : Acceleration) * dt);
        }
        // Releasing jump early cuts upward momentum: a tap makes a short hop,
        // while holding the button produces the full-height jump.
        if (wasJumpHeld_ && !input.jumpHeld && player.velocity.y < -200.f) {
            player.velocity.y *= 0.48f;
        }
        player.velocity.y = min(player.velocity.y + Gravity * dt, 1000.f);
    }
    wasJumpHeld_ = input.jumpHeld;

    // Resolve each axis separately. First try horizontal movement, then push
    // the player out of any intersected rectangle on that same axis.
    player.onWall = false;
    player.wallDirection = 0;
    player.position.x += player.velocity.x * dt;
    for (const auto& platform : platforms) {
        if (!body(player).intersects(platform.bounds)) continue;
        // A lift may rise a fraction of a pixel into falling feet before this
        // horizontal pass. Leave that shallow contact for the vertical pass;
        // otherwise landing on an ascending lift could push us off its side.
        if (player.velocity.y >= 0.f && player.position.y + player.size.y
            <= platform.bounds.top + 2.f) continue;
        if (player.velocity.x > 0.f) {
            player.position.x = platform.bounds.left - player.size.x;
            player.wallDirection = 1;
        } else if (player.velocity.x < 0.f) {
            player.position.x = platform.bounds.left + platform.bounds.width;
            player.wallDirection = -1;
        }
        if (player.wallDirection != 0) {
            player.onWall = true;
            player.velocity.x = 0.f;
            player.dashTime = 0.f;
        }
    }
    player.position.x = clamp(player.position.x, 0.f, LevelWidth - player.size.x);

    if (player.onWall && player.velocity.y > 100.f) player.velocity.y = 100.f;

    const bool wasGrounded = player.grounded;
    const float fallSpeed = player.velocity.y;
    player.grounded = false;
    standingPlatform_ = -1;
    player.position.y += player.velocity.y * dt;
    for (size_t i = 0; i < platforms.size(); ++i) {
        const auto& platform = platforms[i];
        if (!body(player).intersects(platform.bounds)) continue;
        if (player.velocity.y >= 0.f) {
            player.position.y = platform.bounds.top - player.size.y;
            player.grounded = true;
            standingPlatform_ = static_cast<int>(i);
            player.velocity.y = 0.f;
            if (player.dashTime <= 0.f) player.dashReady = true;
        } else {
            player.position.y = platform.bounds.top + platform.bounds.height;
            player.velocity.y = 0.f;
        }
    }
    if (!wasGrounded && player.grounded && fallSpeed > 180.f) {
        player.squash = min(1.f, fallSpeed / 800.f);
        burst(player.position + sf::Vector2f{13, 42}, sf::Color(192, 205, 255), 12, 130.f);
    }

    // The player's collision body remains an honest rectangle. Only the drawn
    // character squashes and stretches; animation never changes collision size.
    for (const auto& hazard : hazards) {
        if (body(player).intersects(hazard.bounds)) {
            die();
            return;
        }
    }
    if (player.position.y > ViewHeight + 160.f) {
        die();
        return;
    }

    for (auto& orb : orbs) {
        if (orb.collected) continue;
        const sf::Vector2f difference = center(player) - orb.position;
        if (difference.x * difference.x + difference.y * difference.y < 32.f * 32.f) {
            orb.collected = true;
            ++collected;
            player.dashReady = true;
            burst(orb.position, sf::Color(255, 216, 133), 18, 150.f);
        }
    }
    for (auto& checkpoint : checkpoints) {
        if (!checkpoint.active && abs(center(player).x - checkpoint.position.x) < 36.f
            && abs(player.position.y + player.size.y - checkpoint.position.y) < 28.f) {
            checkpoint.active = true;
            burst(checkpoint.position - sf::Vector2f{0, 45}, sf::Color(91, 255, 213), 28, 180.f);
        }
    }

    // Track the furthest point for the progress meter, even after a respawn.
    distance = max(distance, player.position.x);
    if (player.position.x >= 7370.f && player.grounded) {
        finished = true;
        burst(center(player), sf::Color(255, 216, 133), 80, 350.f);
    }

    // Exponential smoothing is consistent at different update rates. A slight
    // look-ahead gives room to see the next jump before committing to it.
    const float targetCamera = clamp(player.position.x - 350.f
        + player.velocity.x * 0.16f, 0.f, LevelWidth - ViewWidth);
    cameraX += (targetCamera - cameraX) * (1.f - exp(-dt * 6.f));
}

bool runSelfTests() {
    // These are actual physics scenarios, not copies of the implementation.
    // Each one runs the same public update() used by the game loop.
    int passed = 0, failed = 0;
    auto check = [&](bool condition, const string& description) {
        cout << (condition ? "PASS " : "FAIL ") << description << '\n';
        condition ? ++passed : ++failed;
    };
    constexpr float dt = 1.f / 120.f;
    auto advance = [](World& world, int steps, Input input = {}) {
        for (int i = 0; i < steps; ++i) {
            world.update(1.f / 120.f, input);
            input.jumpPressed = input.dashPressed = input.restartPressed = false;
        }
    };
    auto flat = [&]() {
        World world;
        world.platforms = {{{0, 610, 7600, 200}, false, {0, 610}, 0}};
        world.hazards.clear();
        world.orbs.clear();
        world.checkpoints.clear();
        advance(world, 120);
        return world;
    };

    World landing = flat();
    check(landing.player.grounded && abs(landing.player.position.y - 568.f) < 0.01f,
          "falling player lands flush on the roof");

    World running = flat();
    advance(running, 90, {.horizontal = 1});
    check(running.player.velocity.x > 349.f && running.player.position.x > 300.f,
          "run accelerates to its intended maximum speed");
    advance(running, 30);
    check(abs(running.player.velocity.x) < 0.01f, "friction stops an idle player");

    World highJump = flat();
    highJump.update(dt, {.jumpPressed = true, .jumpHeld = true});
    advance(highJump, 42, {.jumpHeld = true});
    check(highJump.player.position.y < 455.f && !highJump.player.grounded,
          "holding jump produces the full-height arc");
    World shortJump = flat();
    shortJump.update(dt, {.jumpPressed = true, .jumpHeld = true});
    advance(shortJump, 20);
    check(shortJump.player.position.y > highJump.player.position.y + 35.f,
          "releasing jump early creates a shorter hop");

    World coyote = flat();
    coyote.platforms.clear();
    coyote.update(dt, {});
    coyote.update(dt, {.jumpPressed = true, .jumpHeld = true});
    check(coyote.player.velocity.y < -600.f, "coyote time permits a just-off-ledge jump");

    // Start a fresh airborne world, so there is no leftover coyote grace
    // period that could accidentally turn this into an ordinary jump test.
    World buffered;
    buffered.platforms = {{{0, 610, 7600, 200}, false, {0, 610}, 0}};
    buffered.hazards.clear();
    buffered.orbs.clear();
    buffered.checkpoints.clear();
    buffered.player.position.y = 560.f;
    buffered.player.velocity.y = 230.f;
    buffered.player.grounded = false;
    buffered.update(dt, {.jumpPressed = true, .jumpHeld = true});
    advance(buffered, 10, {.jumpHeld = true});
    check(buffered.player.velocity.y < 0.f && buffered.player.position.y < 550.f,
          "buffered jump triggers immediately after landing");

    World dash = flat();
    dash.player.position.y = 400.f;
    dash.player.grounded = false;
    const float dashStart = dash.player.position.x;
    dash.update(dt, {.dashPressed = true});
    advance(dash, 10);
    check(dash.player.position.x - dashStart > 70.f && !dash.player.dashReady,
          "air dash moves rapidly and consumes the charge");

    // Use a fresh airborne player so this tests wall grace, not floor grace.
    World wall;
    wall.platforms = {{{0, 610, 7600, 200}, false, {0, 610}, 0}};
    wall.hazards.clear();
    wall.orbs.clear();
    wall.checkpoints.clear();
    wall.platforms.push_back({{250, 300, 70, 310}, false, {250, 300}, 0});
    wall.player.position = {222, 390};
    wall.player.velocity = {300, 250};
    wall.player.grounded = false;
    wall.update(dt, {.horizontal = 1});
    check(wall.player.onWall && wall.player.velocity.y <= 100.f,
          "wall collision prevents penetration and limits sliding speed");
    wall.update(dt, {.horizontal = 1, .jumpPressed = true, .jumpHeld = true});
    check(wall.player.velocity.x < -400.f && wall.player.velocity.y < -580.f,
          "wall jump pushes upward and away from the wall");

    World lift = flat();
    lift.platforms = {{{0, 530, 500, 22}, true, {0, 530}, 0}};
    lift.player.position = {110, 400};
    lift.player.grounded = false;
    advance(lift, 240);
    check(lift.player.grounded && abs(lift.player.position.y + 42.f
          - lift.platforms.front().bounds.top) < 0.1f,
          "moving lift carries a standing player without sinking");

    World risingLift;
    risingLift.platforms = {{{0, 530, 500, 22}, true, {0, 530}, 3.14f}};
    risingLift.hazards.clear();
    risingLift.orbs.clear();
    risingLift.checkpoints.clear();
    risingLift.player.position = {110, 487.9f};
    risingLift.player.velocity = {150, 20};
    risingLift.update(dt, {.horizontal = 1});
    check(risingLift.player.grounded && risingLift.player.position.x > 110.f,
          "landing on an ascending lift does not cause a sideways collision");

    World pickup = flat();
    pickup.orbs = {{{123, 589}, false}};
    pickup.player.dashReady = false;
    pickup.update(dt, {});
    check(pickup.collected == 1 && pickup.orbs.front().collected && pickup.player.dashReady,
          "touching an orb collects it and refills the dash");

    World checkpoint = flat();
    checkpoint.checkpoints = {{{123, 610}, false}};
    checkpoint.update(dt, {});
    checkpoint.player.position = {500, 900};
    checkpoint.update(dt, {});
    check(checkpoint.checkpoints.front().active && checkpoint.deaths == 1
          && abs(checkpoint.player.position.x - 110.f) < 0.1f,
          "falling respawns at the activated checkpoint");

    World spikes = flat();
    spikes.hazards = {{{110, 590, 26, 20}}};
    spikes.update(dt, {});
    check(spikes.deaths == 1, "spike collision records a death and respawns");

    World finish = flat();
    finish.player.position.x = 7380;
    finish.update(dt, {});
    check(finish.finished, "reaching the grounded finish completes the run");
    const float endTime = finish.elapsed;
    advance(finish, 60);
    check(finish.elapsed == endTime, "finishing freezes the recorded run time");

    // Validate the actual level too: every adjacent roof must have a possible
    // jump. Try a small selection of takeoff points and dash timings rather
    // than assuming that a gap which looks small enough is really reachable.
    // Six starting times also cover lifts at different heights and directions.
    auto canCross = [&](size_t index, float startingTime) {
        for (float lead : {5.f, 20.f, 40.f, 60.f, 85.f}) {
            for (int dashDelay : {-1, 10, 20, 30}) {
                World trial;
                trial.elapsed = startingTime;
                for (auto& platform : trial.platforms) {
                    if (platform.moving) platform.bounds.top = platform.origin.y
                        + sin(startingTime * 1.6f + platform.phase) * 42.f;
                }
                const auto source = trial.platforms[index].bounds;
                // Begin just above the last 100 pixels of the source roof;
                // normal collision code must settle onto it before takeoff.
                trial.player.position = {source.left + source.width - 100.f,
                                         source.top - 45.f};
                trial.player.velocity.x = RunSpeed;
                bool jumped = false;
                int stepsAfterJump = 0;
                for (int step = 0; step < 220; ++step) {
                    Input control{.horizontal = 1.f, .jumpHeld = true};
                    if (!jumped && trial.player.position.x + trial.player.size.x
                        >= source.left + source.width - lead) {
                        control.jumpPressed = true;
                        jumped = true;
                    }
                    if (jumped) {
                        ++stepsAfterJump;
                        control.dashPressed = stepsAfterJump == dashDelay;
                    }
                    trial.update(dt, control);
                    if (trial.deaths > 0) break; // Failed attempt; try another timing.
                    const auto target = trial.platforms[index + 1].bounds;
                    if (trial.player.grounded
                        && trial.player.position.x + trial.player.size.x > target.left
                        && trial.player.position.x < target.left + target.width
                        && abs(trial.player.position.y + trial.player.size.y - target.top) < 0.1f)
                        return true;
                }
            }
        }
        return false;
    };
    bool allRoofsReachable = true;
    const World levelRecipe;
    for (size_t i = 0; i + 1 < levelRecipe.platforms.size(); ++i) {
        for (float phase : {0.f, 0.6f, 1.2f, 1.8f, 2.4f, 3.f}) {
            if (!canCross(i, phase)) {
                cout << "  Unreachable crossing: roof " << i << " at time " << phase << '\n';
                allRoofsReachable = false;
            }
        }
    }
    check(allRoofsReachable, "all 18 roof crossings are reachable at six different lift phases");

    cout << passed << " passed, " << failed << " failed\n";
    return failed == 0;
}

} // namespace skyline

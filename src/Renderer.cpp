#include "Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// The user asked for this beginner-friendly shorthand. It is fine in a .cpp
// file; headers avoid it so they do not change other files' name lookup.
using namespace std;

namespace skyline {
namespace {
// Our palette lives in one place, like labelled paint pots. An optional fourth
// number in sf::Color is opacity: 0 is invisible and 255 is completely opaque.
const sf::Color Ink{16, 21, 39};
const sf::Color Paper{246, 239, 221};
const sf::Color Muted{154, 172, 185};
const sf::Color Mint{125, 237, 206};
const sf::Color Coral{255, 135, 107};

// Drawing helpers receive the canvas by reference (&): they draw on the real
// canvas, rather than making a copy. Const references promise not to edit data.
void box(sf::RenderTarget& canvas, sf::FloatRect bounds, sf::Color color) {
    sf::RectangleShape shape({bounds.width, bounds.height});
    shape.setPosition(bounds.left, bounds.top);
    shape.setFillColor(color);
    canvas.draw(shape);
}

void circle(sf::RenderTarget& canvas, sf::Vector2f center, float radius, sf::Color color) {
    sf::CircleShape shape(radius, 48);
    shape.setOrigin(radius, radius);
    shape.setPosition(center);
    shape.setFillColor(color);
    canvas.draw(shape);
}

// A line is just a thin rectangle rotated to point at its second endpoint.
void line(sf::RenderTarget& canvas, sf::Vector2f a, sf::Vector2f b,
          sf::Color color, float thickness = 1.f) {
    const sf::Vector2f delta = b - a;
    sf::RectangleShape shape({sqrt(delta.x * delta.x + delta.y * delta.y), thickness});
    shape.setPosition(a);
    shape.setRotation(atan2(delta.y, delta.x) * 180.f / 3.14159265f);
    shape.setFillColor(color);
    canvas.draw(shape);
}

void diamond(sf::RenderTarget& canvas, sf::Vector2f center, float radius, sf::Color color) {
    sf::CircleShape shape(radius, 4);
    shape.setOrigin(radius, radius);
    shape.setPosition(center);
    shape.setFillColor(color);
    canvas.draw(shape);
}

// Format a time as minutes:seconds.hundredths without changing the real timer.
string clockLabel(float seconds) {
    const int hundredths = static_cast<int>(max(0.f, seconds) * 100.f);
    ostringstream label;
    label << setfill('0') << setw(2) << hundredths / 6000 << ':'
          << setw(2) << (hundredths / 100) % 60 << '.' << setw(2) << hundredths % 100;
    return label.str();
}

// Pills are small rectangular key labels used consistently throughout the UI.
void keycap(sf::RenderTarget& canvas, const sf::Font& font, const string& label,
            sf::Vector2f position, float width) {
    box(canvas, {position.x, position.y, width, 25}, {38, 49, 65});
    sf::Text text(label, font, 11);
    text.setFillColor(Paper);
    text.setPosition(position.x + 8, position.y + 4);
    canvas.draw(text);
}
} // An unnamed namespace keeps these helpers private to this file.

Renderer::Renderer(const filesystem::path& executableDirectory) {
    // A packaged game loads its own assets; the source path is only a fallback
    // when a developer launches it from an unusual working directory.
    const array paths{
        executableDirectory / "assets" / "DejaVuSans.ttf",
        filesystem::path("assets/DejaVuSans.ttf"),
        filesystem::path(AFTERLIGHT_ASSET_DIR) / "DejaVuSans.ttf"
    };
    for (const auto& path : paths) {
        if (filesystem::exists(path) && font_.loadFromFile(path.string())) return;
    }
    throw runtime_error("Cannot find assets/DejaVuSans.ttf. Keep the assets folder beside afterlight.");
}

void Renderer::text(sf::RenderTarget& target, const string& message,
                    sf::Vector2f position, unsigned size, sf::Color color,
                    float letterSpacing) const {
    sf::Text label(message, font_, size);
    label.setPosition(position);
    label.setFillColor(color);
    label.setLetterSpacing(letterSpacing);
    target.draw(label);
}

void Renderer::background(sf::RenderTarget& target, float camera, float time) const {
    // Each vertex has a color. The graphics card blends them into a smooth sky.
    sf::VertexArray sky(sf::Quads, 4);
    sky[0] = {{0, 0}, {20, 29, 54}};
    sky[1] = {{ViewWidth, 0}, {30, 32, 58}};
    sky[2] = {{ViewWidth, ViewHeight}, {185, 100, 102}};
    sky[3] = {{0, ViewHeight}, {80, 66, 91}};
    target.draw(sky);

    // Faint rings create a halo without requiring a shader or an image file.
    const sf::Vector2f sun{982 - camera * .025f, 292};
    for (int i = 6; i > 0; --i)
        circle(target, sun, 101.f + i * 12.f, {255, 155, 116, 3});
    circle(target, sun, 99, {250, 157, 123});
    // Thin horizontal cuts give the sun a distinctive printed-poster look.
    for (int i = 0; i < 7; ++i)
        box(target, {sun.x - 104, sun.y + 24 + i * 12.f, 208, 2.f + i}, {111, 77, 100});

    // Deterministic stars: formulas produce the same sky every frame, so there
    // is no distracting random flicker as the camera moves.
    for (int i = 0; i < 48; ++i) {
        float x = fmod(i * 193.7f + 29.f, 1280.f);
        const float y = fmod(i * 57.3f + 15.f, 245.f);
        const auto alpha = static_cast<sf::Uint8>(55 + 40 * sin(time * .7f + i));
        circle(target, {x, y}, i % 5 == 0 ? 1.5f : .8f, {246, 239, 221, alpha});
    }
    // City layers move at different fractions of camera speed. This is called
    // parallax: nearby buildings appear to move faster than distant ones.
    for (int layer = 0; layer < 3; ++layer) {
        const float speed = .08f + layer * .13f;
        const float spacing = layer == 0 ? 83.f : 116.f;
        const float baseline = 560.f + layer * 56;
        const sf::Color color = layer == 0 ? sf::Color(68, 62, 87)
                             : layer == 1 ? sf::Color(42, 44, 66) : sf::Color(25, 33, 51);
        // Negative indices also draw buildings to the left of the screen.
        const int first = static_cast<int>(camera * speed / spacing) - 2;
        for (int i = first; i < first + 21; ++i) {
            const int seed = abs(i * 37 + layer * 71);
            const float height = 72.f + (seed * 17 % 156);
            const float x = i * spacing - camera * speed;
            const float width = spacing - 12.f - seed % 19;
            box(target, {x, baseline - height, width, height + 200}, color);
            if (seed % 3 == 0) {
                box(target, {x + width * .3f, baseline - height - 17, width * .4f, 17}, color);
                line(target, {x + width * .5f, baseline - height - 17},
                     {x + width * .5f, baseline - height - 44}, color, 2);
            }
            if (layer > 0) {
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 4; ++col)
                        if ((row * 7 + col * 11 + seed) % 4 == 0)
                            box(target, {x + 10 + col * 15.f, baseline - height + 20 + row * 23.f,
                                         4, 7}, {227, 164, 131, static_cast<sf::Uint8>(layer == 1 ? 45 : 60)});
            }
        }
    }
    // A distant crane gives the skyline a recognizable landmark.
    const float crane = 610.f - camera * .12f;
    line(target, {crane, 465}, {crane, 277}, {61, 58, 82}, 4);
    line(target, {crane - 80, 296}, {crane + 155, 296}, {61, 58, 82}, 4);
    line(target, {crane - 80, 296}, {crane, 262}, {61, 58, 82}, 2);
    line(target, {crane, 262}, {crane + 155, 296}, {61, 58, 82}, 2);
    line(target, {crane + 132, 298}, {crane + 132, 369}, {61, 58, 82}, 1);

    // A few little birds follow looping paths high above the rooftops.
    for (int i = 0; i < 4; ++i) {
        const float x = fmod(time * 9 + i * 71 + 530, 1500.f) - 100;
        const float y = 190 + sin(time * .5f + i) * 12 + i * 12;
        line(target, {x - 5, y - 2}, {x, y}, {31, 36, 56});
        line(target, {x, y}, {x + 5, y - 2}, {31, 36, 56});
    }
}

void Renderer::course(sf::RenderTarget& target, const World& world, float time, bool showSigns) const {
    // The view has already moved to world space, so these are level positions,
    // not screen positions. Off-screen objects are skipped to save drawing work.
    for (size_t index = 0; index < world.platforms.size(); ++index) {
        const auto& platform = world.platforms[index];
        const auto& b = platform.bounds;
        if (b.left + b.width < world.cameraX - 80 || b.left > world.cameraX + ViewWidth + 80) continue;
        box(target, b, {20, 30, 45});
        box(target, {b.left, b.top + 9, b.width, min(11.f, b.height - 9)}, {35, 48, 61});
        box(target, {b.left, b.top, b.width, 4}, platform.moving ? Mint : sf::Color(215, 166, 139));
        box(target, {b.left + 6, b.top + 4, max(0.f, b.width - 12), 2}, {87, 78, 80});
        // Small seams, lit windows and vents make the collision rectangles
        // read as rooftops, rather than anonymous floating gray boxes.
        for (float x = b.left + 24; x < b.left + b.width - 12; x += 58) {
            line(target, {x, b.top + 25}, {x, b.top + b.height}, {30, 41, 55});
            if (b.height > 55) {
                box(target, {x + 12, b.top + 38, 13, 22}, {54, 67, 75});
                box(target, {x + 12, b.top + 38, 13, 2}, {104, 123, 121});
            }
        }
        if (b.width > 200 && !platform.moving) {
            const float vx = b.left + b.width - 70;
            box(target, {vx, b.top - 24, 36, 24}, {41, 53, 64});
            box(target, {vx - 3, b.top - 27, 42, 4}, {64, 76, 83});
            for (int i = 0; i < 4; ++i)
                line(target, {vx + 7.f, b.top - 19 + i * 4.f}, {vx + 29, b.top - 19 + i * 4.f}, {24, 36, 49});
        }
        if (platform.moving) {
            // The dashed guide hints at motion without being a solid obstacle.
            for (int i = -3; i <= 3; ++i)
                box(target, {platform.origin.x + b.width / 2, platform.origin.y + i * 20.f, 1, 6}, {125, 237, 206, 35});
            diamond(target, {b.left + b.width / 2, b.top + 16}, 4, Mint);
        }
    }

    // Every red triangle corresponds to a real hazardous collision rectangle.
    for (const auto& hazard : world.hazards) {
        const auto& b = hazard.bounds;
        for (float x = b.left; x < b.left + b.width; x += 14) {
            sf::ConvexShape spike(3);
            spike.setPoint(0, {x, b.top + b.height});
            spike.setPoint(1, {x + 7, b.top});
            spike.setPoint(2, {min(x + 14, b.left + b.width), b.top + b.height});
            spike.setFillColor(Coral);
            target.draw(spike);
        }
        box(target, {b.left, b.top + b.height, b.width, 3}, {151, 67, 71});
    }
    for (const auto& orb : world.orbs) {
        if (orb.collected) continue;
        const sf::Vector2f p = orb.position + sf::Vector2f(0, sin(time * 3 + orb.position.x) * 4);
        circle(target, p, 18, {125, 237, 206, 10});
        circle(target, p, 12, {125, 237, 206, 18});
        diamond(target, p, 7, Mint);
        diamond(target, p, 3, Paper);
    }
    for (size_t i = 0; i < world.checkpoints.size(); ++i) {
        const auto& checkpoint = world.checkpoints[i];
        // World stores the base of the flag. Our little flag drawing uses a
        // point 38 pixels above that base as its local reference position.
        const auto p = checkpoint.position - sf::Vector2f{0, 38};
        line(target, {p.x, p.y + 38}, {p.x, p.y - 35}, {101, 136, 143}, 2);
        const sf::Color color = checkpoint.active ? Mint : sf::Color(111, 125, 142);
        // A fluttering flag also makes respawn locations visible at a glance.
        sf::ConvexShape flag(4);
        flag.setPoint(0, {p.x + 2, p.y - 35});
        flag.setPoint(1, {p.x + 33, p.y - 31 + sin(time * 3) * 3});
        flag.setPoint(2, {p.x + 29, p.y - 14 + sin(time * 3) * 3});
        flag.setPoint(3, {p.x + 2, p.y - 18});
        flag.setFillColor(color);
        target.draw(flag);
        text(target, "0" + to_string(i + 1), {p.x + 7, p.y - 61}, 11, color);
        if (checkpoint.active) circle(target, {p.x, p.y + 37}, 9, {125, 237, 206, 30});
    }

    // Start-area environmental text teaches controls where they first matter.
    if (showSigns) {
    text(target, "01 / THE DEPARTURE", {90, 429}, 12, Mint, 1.5f);
    text(target, "Every rooftop is a way forward.", {90, 453}, 15, Paper);
    line(target, {115, 493}, {210, 493}, {125, 237, 206, 100});
    line(target, {210, 493}, {202, 489}, Mint);
    line(target, {210, 493}, {202, 497}, Mint);
    text(target, "HOLD JUMP TO GO HIGHER", {580, 368}, 12, Paper);
    text(target, "02 / FIND YOUR FLOW", {2170, 280}, 16, Mint, 1.3f);
    text(target, "Touch a wall. Jump away. Keep climbing.", {2170, 308}, 13, Paper);
    text(target, "03 / BETWEEN THE LIGHTS", {3670, 227}, 16, Mint, 1.3f);
    text(target, "SHIFT / DASH ACROSS", {3940, 260}, 12, Paper);
    text(target, "04 / ONE LAST LEAP", {5750, 270}, 16, Mint, 1.3f);
    }

    // The finish is a luminous arch you actually have to reach.
    box(target, {7340, 330, 9, 280}, Mint);
    box(target, {7500, 330, 9, 280}, Mint);
    box(target, {7340, 330, 169, 7}, Mint);
    box(target, {7349, 337, 151, 273}, {125, 237, 206, 13});
    text(target, "DAYBREAK", {7360, 348}, 16, Paper);
    for (int i = 0; i < 12; ++i) {
        const float y = 365 + fmod(i * 21.f + time * 25, 230.f);
        circle(target, {7358.f + (i * 37 % 135), y}, 2, {125, 237, 206, 110});
    }
    for (const auto& particle : world.particles) {
        auto color = particle.color;
        color.a = static_cast<sf::Uint8>(255 * clamp(particle.life / max(.001f, particle.maxLife), 0.f, 1.f));
        box(target, {particle.position.x, particle.position.y, particle.size, particle.size}, color);
    }
}

void Renderer::runner(sf::RenderTarget& target, const Player& player, float time) const {
    // Draw the character in a tiny local coordinate system, then move that
    // drawing onto the player's world position. Scaling around the feet adds
    // a springy landing animation without altering the collision rectangle.
    const sf::Vector2f p{0, 0};
    sf::Transform pose;
    pose.translate(player.position.x + 13, player.position.y + 42);
    pose.scale(1.f + player.squash * .18f, 1.f - player.squash * .16f);
    pose.translate(-13, -42);
    const auto box = [&pose](sf::RenderTarget& canvas, sf::FloatRect bounds, sf::Color color) {
        sf::RectangleShape shape({bounds.width, bounds.height});
        shape.setPosition(bounds.left, bounds.top);
        shape.setFillColor(color);
        canvas.draw(shape, pose);
    };
    const auto circle = [&pose](sf::RenderTarget& canvas, sf::Vector2f center, float radius, sf::Color color) {
        sf::CircleShape shape(radius, 16);
        shape.setOrigin(radius, radius);
        shape.setPosition(center);
        shape.setFillColor(color);
        canvas.draw(shape, pose);
    };
    const float direction = static_cast<float>(player.facing);
    const bool running = player.grounded && abs(player.velocity.x) > 40;
    const float stride = running ? sin(time * 21) * 5 : 0;
    // Ghost silhouettes convey a dash even when sound is disabled.
    if (player.dashTime > 0) {
        for (int i = 4; i > 0; --i)
            box(target, {p.x - direction * i * 12, p.y + 8, 25, 30},
                {125, 237, 206, static_cast<sf::Uint8>(65 - i * 12)});
    }
    // The runner is built from simple shapes: a dark suit, bright jacket, visor,
    // and flowing scarf. Layer order matters: the scarf goes behind the body.
    const float scarfWave = sin(time * 12) * 4;
    sf::ConvexShape scarf(4);
    scarf.setPoint(0, {p.x + 13, p.y + 13});
    scarf.setPoint(1, {p.x + 13 - direction * 38, p.y + 8 + scarfWave});
    scarf.setPoint(2, {p.x + 13 - direction * 26, p.y + 17 + scarfWave});
    scarf.setPoint(3, {p.x + 13, p.y + 19});
    scarf.setFillColor(Coral);
    target.draw(scarf, pose);
    const float lean = player.velocity.x * .008f;
    box(target, {p.x + 4 - stride, p.y + 31, 8, 12}, Ink);
    box(target, {p.x + 16 + stride, p.y + 31, 8, 12}, Ink);
    box(target, {p.x + 2 - stride, p.y + 40, 11, 3}, Paper);
    box(target, {p.x + 15 + stride, p.y + 40, 11, 3}, Paper);
    box(target, {p.x + 3 + lean, p.y + 14, 22, 21}, Ink);
    box(target, {p.x + 4 + lean, p.y + 14, 20, 14}, Mint);
    box(target, {p.x + 9 + lean, p.y + 26, 12, 8}, {55, 101, 103});
    box(target, {p.x + 3 + lean, p.y + 17, 5, 14}, {91, 185, 172});
    box(target, {p.x + 5 + lean, p.y, 19, 15}, Ink);
    box(target, {p.x + 6 + lean, p.y + 2, 17, 4}, {51, 68, 79});
    box(target, {p.x + (direction > 0 ? 14 : 5) + lean, p.y + 6, 11, 5}, Paper);
    box(target, {p.x + 1 + lean, p.y + 13, 26, 4}, Coral);
    if (player.dashReady) circle(target, {p.x + 13 + lean, p.y + 22}, 2, Paper);
}

void Renderer::hud(sf::RenderTarget& target, const World& world, const UiState& ui) const {
    box(target, {0, 0, ViewWidth, 88}, {16, 21, 39, 230});
    diamond(target, {44, 43}, 10, Coral);
    text(target, "AFTERLIGHT", {65, 25}, 23, Paper, 1.5f);
    text(target, "ROOFTOP RUNNER", {67, 56}, 8, Muted, 2.f);
    line(target, {307, 26}, {307, 63}, {76, 88, 103});
    const int district = world.player.position.x < 2100 ? 1 : world.player.position.x < 3650 ? 2 : world.player.position.x < 5700 ? 3 : 4;
    text(target, "0" + to_string(district) + " / " + array<string, 4>{"THE DEPARTURE", "FIND YOUR FLOW", "BETWEEN THE LIGHTS", "ONE LAST LEAP"}[district - 1],
         {334, 34}, 12, Muted, 1.2f);
    diamond(target, {761, 42}, 6, Mint);
    text(target, to_string(world.collected) + " / " + to_string(world.orbs.size()), {778, 31}, 16, Paper);
    text(target, "TIME", {910, 22}, 9, Muted, 1.5f);
    text(target, clockLabel(world.elapsed), {909, 38}, 20, Paper);
    keycap(target, font_, "ESC", {1164, 31}, 43);
    // A thin progress strip tracks the furthest point, even after a fall.
    box(target, {0, 86, ViewWidth, 2}, {42, 55, 68});
    box(target, {0, 86, ViewWidth * clamp(world.distance / 7350.f, 0.f, 1.f), 2}, Mint);

    box(target, {0, 658, ViewWidth, 62}, {16, 21, 39, 240});
    keycap(target, font_, "A  D", {32, 677}, 51);
    text(target, "MOVE", {94, 681}, 10, Muted);
    keycap(target, font_, "SPACE", {163, 677}, 65);
    text(target, "JUMP / WALL JUMP", {239, 681}, 10, Muted);
    keycap(target, font_, "SHIFT", {414, 677}, 61);
    text(target, "DASH", {487, 681}, 10, Muted);
    keycap(target, font_, "R", {562, 677}, 27);
    text(target, "RETRY", {600, 681}, 10, Muted);
    circle(target, {816, 690}, 4, world.player.dashReady ? Mint : sf::Color(63, 78, 89));
    text(target, world.player.dashReady ? "DASH READY" : "LAND TO RECHARGE", {829, 681}, 10,
         world.player.dashReady ? Mint : Muted);
    text(target, to_string(world.deaths) + " FALLS", {1080, 681}, 10, Muted);
    text(target, ui.muted ? "M / MUTED" : "M / SOUND", {1171, 681}, 9, Muted);
    if (ui.showStats) {
        box(target, {20, 105, 242, 80}, {16, 21, 39, 225});
        text(target, "FPS " + to_string(static_cast<int>(ui.fps)) + "  |  PHYSICS 120 HZ", {32, 115}, 11, Mint);
        text(target, "X " + to_string(static_cast<int>(world.player.position.x)) + "  Y " + to_string(static_cast<int>(world.player.position.y)), {32, 139}, 11, Paper);
        text(target, "VX " + to_string(static_cast<int>(world.player.velocity.x)) + "  VY " + to_string(static_cast<int>(world.player.velocity.y)), {32, 160}, 11, Paper);
    }
}

void Renderer::title(sf::RenderTarget& target, const UiState& ui) const {
    // This gradient panel leaves the sunset visible, while keeping text legible.
    sf::VertexArray shade(sf::Quads, 4);
    shade[0] = {{0, 0}, {16, 21, 39, 248}};
    shade[1] = {{890, 0}, {16, 21, 39, 0}};
    shade[2] = {{890, ViewHeight}, {16, 21, 39, 0}};
    shade[3] = {{0, ViewHeight}, {16, 21, 39, 248}};
    target.draw(shade);
    diamond(target, {59, 50}, 9, Coral);
    text(target, "AFTERLIGHT", {80, 33}, 21, Paper, 1.7f);
    text(target, "A LITTLE COURAGE. A LOT OF SKY.", {53, 143}, 11, Mint, 1.8f);
    text(target, "CHASE THE", {48, 176}, 65, Paper);
    text(target, "LAST LIGHT.", {48, 247}, 65, Paper);
    box(target, {54, 344, 51, 3}, Coral);
    text(target, "The city is falling asleep. You're just getting started.", {54, 376}, 15, Paper);
    text(target, "Run the rooftops. Ride the walls. Leave gravity behind.", {54, 405}, 14, Muted);
    const float pulse = 1.f + sin(ui.visualTime * 2.f) * .025f;
    box(target, {54, 470, 242 * pulse, 57}, Mint);
    text(target, "LET'S MAKE THE LEAP", {74, 489}, 13, Ink);
    text(target, "ENTER  /  CLICK TO BEGIN", {54, 543}, 10, Muted, 1.2f);
    text(target, "01", {54, 620}, 12, Coral);
    text(target, "MOVE", {82, 622}, 10, Paper);
    text(target, "02", {190, 620}, 12, Coral);
    text(target, "JUMP", {220, 622}, 10, Paper);
    text(target, "03", {326, 620}, 12, Coral);
    text(target, "FLOW", {356, 622}, 10, Paper);
    text(target, "C++23 / SFML", {54, 679}, 9, Muted, 1.5f);
    text(target, "HEADPHONES ON. CHIN UP.", {995, 679}, 9, Paper, 1.2f);
    text(target, "A ROOFTOP PARKOUR GAME", {1000, 36}, 9, Paper, 1.4f);
    // A large annotation frames the runner on the right like a magazine spread.
    line(target, {833, 535}, {871, 486}, {125, 237, 206, 130});
    line(target, {871, 486}, {1027, 486}, {125, 237, 206, 130});
    text(target, "YOUR NEXT CHAPTER", {880, 463}, 10, Mint, 1.4f);
}

void Renderer::overlay(sf::RenderTarget& target, const World& world, const UiState& ui) const {
    const bool finished = ui.screen == Screen::Finished;
    box(target, {0, 0, ViewWidth, ViewHeight}, {10, 16, 30, 195});
    box(target, {370, 160, 540, 400}, {22, 32, 49});
    box(target, {370, 160, 540, 3}, finished ? Mint : Coral);
    text(target, finished ? "THE CITY IS YOURS." : "TAKE A BREATHER.", {414, 195}, 29, Paper);
    text(target, finished ? "You found your way to the last light." : "Even the skyline can wait a minute.", {414, 245}, 14, Muted);
    if (finished) {
        text(target, clockLabel(world.elapsed), {414, 292}, 46, Mint);
        text(target, to_string(world.collected) + " SPARKS  /  " + to_string(world.deaths) + " FALLS", {417, 363}, 13, Paper);
        if (ui.bestTime > 0) text(target, "SESSION BEST  " + clockLabel(ui.bestTime), {417, 399}, 11, Muted);
    } else {
        text(target, "A / D        Move left / right", {417, 294}, 14, Paper);
        text(target, "SPACE     Jump / wall jump (hold for height)", {417, 326}, 14, Paper);
        text(target, "SHIFT      Dash   /   R  Retry   /   M  Sound", {417, 358}, 14, Paper);
        text(target, "Checkpoints save your place. Sparks are optional.", {417, 398}, 12, Muted);
    }
    box(target, {414, 461, 452, 53}, Mint);
    text(target, finished ? "ENTER  /  RUN IT BACK" : "ESC  /  BACK TO THE ROOFTOPS", {438, 478}, 14, Ink);
    if (!finished) text(target, "ENTER  /  RESTART THE WHOLE RUN", {416, 530}, 10, Muted);
}

void Renderer::draw(sf::RenderTarget& target, const World& world, const UiState& ui) {
    // Render at a consistent logical resolution; the window's view handles
    // letterboxing if the player resizes to a different aspect ratio.
    const sf::View screenView = target.getView();
    background(target, world.cameraX, ui.visualTime);
    sf::View worldView = screenView;
    float camera = world.cameraX;
    if (ui.screen == Screen::Title) camera = 0;
    const float shake = ui.screen == Screen::Playing ? world.shake : 0;
    worldView.setCenter(camera + ViewWidth / 2 + sin(ui.visualTime * 73) * shake,
                        ViewHeight / 2 + cos(ui.visualTime * 91) * shake * .5f);
    target.setView(worldView);
    course(target, world, ui.visualTime, ui.screen != Screen::Title);
    if (ui.screen == Screen::Title) {
        Player hero = world.player;
        hero.position = {795, 528}; // Feet meet the second roof at y = 570.
        hero.facing = 1;
        runner(target, hero, ui.visualTime);
    } else runner(target, world.player, ui.visualTime);
    target.setView(screenView);
    if (ui.screen == Screen::Title) title(target, ui);
    else {
        hud(target, world, ui);
        if (ui.screen == Screen::Paused || ui.screen == Screen::Finished) overlay(target, world, ui);
    }
}
} // namespace skyline

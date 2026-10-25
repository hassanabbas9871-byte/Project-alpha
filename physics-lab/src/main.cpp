// FLUX is a little laboratory you can touch. This file handles the window,
// drawing and controls; Simulation.cpp contains the actual physics equations.
#include "Simulation.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace std;
using namespace flux;

namespace {
// A fixed logical canvas makes the same layout work on different monitors.
// Drawing is scaled and letterboxed, while physics always uses world pixels.
constexpr int CanvasWidth = 1440, CanvasHeight = 900;
constexpr Rectangle Stage{316, 145, WorldWidth, WorldHeight};
constexpr float FixedStep = 1.f / 120.f;
const Color Background{12, 16, 25, 255};
const Color Panel{20, 27, 39, 255};
const Color Line{44, 56, 70, 255};
const Color Text{232, 240, 240, 255};
const Color Muted{132, 152, 169, 255};
const Color Mint{123, 244, 197, 255};
const array<Color, 6> Spectrum{{Mint, {109, 189, 255, 255}, {172, 148, 255, 255},
                              {255, 135, 163, 255}, {255, 188, 117, 255}, {224, 244, 156, 255}}};
const array<string, 3> Names{"CHROMATIC CASCADE", "THE FABRIC OF THINGS", "ORBITAL ECHOES"};
const array<string, 3> Subtitles{"A thousand little collisions. One beautiful mess.",
    "Pull a thread. Watch the whole world respond.", "Throw a little chaos into the cosmic clockwork."};

// Convert between our physics vector and raylib's drawing vector explicitly.
// Keeping the two types separate means physics tests need no graphics library.
Vector2 screen(Vec v) { return {Stage.x + v.x, Stage.y + v.y}; }
Color alpha(Color color, float amount) { return Fade(color, clamp(amount, 0.f, 1.f)); }

string decimal(float value, int places = 1) {
    ostringstream result;
    result << fixed << setprecision(places) << value;
    return result.str();
}

struct Options {
    bool selfTest{}, smokeTest{};
    int scene{};
    filesystem::path screenshot;
};

Options parse(int count, char** arguments) {
    Options options;
    for (int i = 1; i < count; ++i) {
        const string arg = arguments[i];
        if (arg == "--self-test") options.selfTest = true;
        else if (arg == "--smoke-test") options.smokeTest = true;
        else if (arg == "--screenshot" && i + 1 < count) options.screenshot = arguments[++i];
        else if (arg == "--scene" && i + 1 < count) {
            options.scene = stoi(arguments[++i]) - 1;
            if (options.scene < 0 || options.scene > 2) throw runtime_error("--scene must be 1, 2, or 3.");
        } else throw runtime_error("Options: --self-test, --smoke-test, --screenshot PATH, --scene 1|2|3");
    }
    return options;
}

class Playground {
public:
    explicit Playground(const filesystem::path& executableDirectory) {
        // The only external art asset is the openly licensed font already
        // bundled with Afterlight. All simulation artwork is generated live.
        const array paths{executableDirectory / "assets" / "DejaVuSans.ttf",
                          filesystem::path(FLUX_FONT_PATH)};
        for (const auto& path : paths) {
            if (filesystem::exists(path)) {
                font_ = LoadFontEx(path.string().c_str(), 72, nullptr, 0);
                if (font_.texture.id != 0) { ownsFont_ = true; break; }
            }
        }
        if (!ownsFont_) font_ = GetFontDefault(); // The app still runs if a font was moved.
        SetTextureFilter(font_.texture, TEXTURE_FILTER_BILINEAR);
        history_ = LoadRenderTexture(static_cast<int>(WorldWidth), static_cast<int>(WorldHeight));
        if (history_.id == 0) throw runtime_error("Could not allocate the orbital trail canvas.");
        clearHistory();
    }

    ~Playground() {
        // GPU objects must disappear BEFORE CloseWindow destroys their context.
        UnloadRenderTexture(history_);
        if (ownsFont_) UnloadFont(font_);
    }

    Simulation sim;
    bool paused{}, showHelp{};
    float visualTime{}, frameRate{};

    void select(int index) {
        sim.reset(static_cast<Scene>(index));
        paused = false;
        clearHistory();
    }

    // Controls use logical coordinates, so clicking still works after resizing.
    void input(Vector2 mouse) {
        mouse_ = mouse;
        if (showHelp) {
            sim.pointerDown = false;
            sim.releasePointer();
            if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_ESCAPE) ||
                (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, {1140, 35, 110, 38}))) showHelp = false;
            return; // Modal help must not pass clicks through to the playground.
        }
        const bool left = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const bool overStage = CheckCollisionPointRec(mouse, Stage);
        sim.pointer = {mouse.x - Stage.x, mouse.y - Stage.y};
        sim.pointerDown = overStage && (left || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) && !showHelp;
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) rememberedTool_ = sim.tool;
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) sim.tool = Tool::Repel;
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) sim.tool = rememberedTool_;
        if (!sim.pointerDown) sim.releasePointer();

        for (int i = 0; i < 3; ++i) {
            if (IsKeyPressed(KEY_ONE + i) || (click && CheckCollisionPointRec(mouse, {32, 166.f + i * 90, 250, 78}))) select(i);
        }
        for (int i = 0; i < 4; ++i) {
            const Rectangle button{32.f + (i % 2) * 129, 492.f + (i / 2) * 46, 121, 37};
            if (click && CheckCollisionPointRec(mouse, button)) sim.tool = static_cast<Tool>(i);
        }
        if (IsKeyPressed(KEY_TAB)) sim.tool = static_cast<Tool>((static_cast<int>(sim.tool) + 1) % 4);
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE) ||
            (click && CheckCollisionPointRec(mouse, {316, 824, 118, 42}))) paused = !paused;
        if (IsKeyPressed(KEY_R) || (click && CheckCollisionPointRec(mouse, {1266, 35, 110, 38})))
            select(static_cast<int>(sim.scene));
        if (IsKeyPressed(KEY_H) || (click && CheckCollisionPointRec(mouse, {1140, 35, 110, 38}))) showHelp = !showHelp;
        if (IsKeyPressed(KEY_G) && sim.scene != Scene::Orbits) sim.gravity = sim.gravity > 0 ? 0.f : 850.f;
        if (overStage && IsKeyPressed(KEY_B)) sim.burst(sim.pointer);
        if (overStage && IsKeyPressed(KEY_N)) sim.spawn(sim.pointer, 100);
        if (left) {
            // A wider hit rectangle makes the sliders easier to grab than
            // their thin visible tracks. Values are clamped at both ends.
            if (CheckCollisionPointRec(mouse, {32, 635, 250, 29}))
                sim.timeScale = .2f + clamp((mouse.x - 32) / 250.f, 0.f, 1.f) * 1.8f;
            if (sim.scene != Scene::Orbits && CheckCollisionPointRec(mouse, {32, 707, 250, 29}))
                sim.gravity = clamp((mouse.x - 32) / 250.f, 0.f, 1.f) * 1700.f;
        }
        const float wheel = GetMouseWheelMove();
        if (overStage) sim.pointerRadius = clamp(sim.pointerRadius + wheel * 10, 25.f, 200.f);
    }

    void updateTrails() {
        if (sim.scene != Scene::Orbits || paused || showHelp) return;
        // Render texture modes cannot nest. Update the long-exposure image
        // BEFORE drawing into the main UI canvas, then use it like a photograph.
        BeginTextureMode(history_);
        DrawRectangle(0, 0, static_cast<int>(WorldWidth), static_cast<int>(WorldHeight), {0, 0, 0, 1});
        for (const auto& p : sim.particles) {
            const Color color = Spectrum[static_cast<size_t>(p.palette) % Spectrum.size()];
            DrawLineEx({p.previous.x, p.previous.y}, {p.position.x, p.position.y}, 1.3f, alpha(color, .55f));
        }
        EndTextureMode();
    }

    void draw(float realDt) {
        visualTime += realDt;
        frameRate = static_cast<float>(GetFPS()); // Measured frames, not simulation speed.
        ClearBackground(Background);
        header();
        sidebar();
        stage();
        footer();
        if (showHelp) help();
    }

private:
    Font font_{};
    bool ownsFont_{};
    RenderTexture2D history_{};
    Vector2 mouse_{-100, -100};
    Tool rememberedTool_{Tool::Attract};

    void label(const string& value, float x, float y, float size, Color color, float spacing = .4f) const {
        DrawTextEx(font_, value.c_str(), {x, y}, max(size, 10.5f), spacing, color);
    }
    void rect(Rectangle r, Color fill, float rounding = .12f) const {
        DrawRectangleRounded(r, rounding, 8, fill);
    }
    void badge(const string& value, Rectangle r, Color color) const {
        rect(r, alpha(color, .09f));
        DrawRectangleRoundedLinesEx(r, .2f, 8, 1, alpha(color, .25f));
        label(value, r.x + 11, r.y + 8, 11, color);
    }
    void clearHistory() {
        BeginTextureMode(history_);
        ClearBackground({0, 0, 0, 255});
        EndTextureMode();
    }

    void header() const {
        // The logo is made from three little moving-particle rings.
        DrawRing({49, 52}, 10, 15, 25, 295, 24, Mint);
        DrawCircle(49, 52, 3, Mint);
        label("FLUX", 78, 27, 40, Text, 2.f);
        label("A PLAYGROUND FOR THE LAWS OF NATURE", 213, 40, 11, Muted, 1.1f);
        badge("LIVE PHYSICS", {887, 37, 126, 30}, Mint);
        label("120 HZ", 1032, 46, 12, Muted);
        rect({1140, 35, 110, 38}, Panel);
        label("H / GUIDE", 1158, 48, 11, Muted);
        rect({1266, 35, 110, 38}, {35, 46, 58, 255});
        label("R / RESET", 1284, 48, 11, Text);
        DrawLine(32, 94, 1376, 94, Line);
    }

    void sidebar() const {
        label("PICK A LITTLE WONDER", 32, 123, 11, Muted, 1.2f);
        const array<string, 3> titles{"Chromatic cascade", "The fabric of things", "Orbital echoes"};
        const array<string, 3> details{"COLLISION / FLOW", "CONSTRAINTS / TENSION", "GRAVITY / MOMENTUM"};
        for (int i = 0; i < 3; ++i) {
            const Rectangle card{32, 166.f + i * 90, 250, 78};
            const bool selected = static_cast<int>(sim.scene) == i;
            rect(card, selected ? Color{27, 45, 49, 255} : Panel);
            if (selected) DrawRectangleRoundedLinesEx(card, .12f, 8, 1, alpha(Mint, .55f));
            label("0" + to_string(i + 1), 48, card.y + 17, 11, selected ? Mint : Muted);
            label(titles[i], 80, card.y + 16, 16, Text);
            label(details[i], 80, card.y + 46, 8.5f, selected ? Mint : Muted, .9f);
        }
        DrawLine(32, 452, 282, 452, Line);
        label("YOUR HANDS, BUT MORE POWERFUL", 32, 471, 9, Muted, .7f);
        const array<string, 4> tools{"Attract", "Repel", "Grab", "Cut"};
        for (int i = 0; i < 4; ++i) {
            const Rectangle button{32.f + (i % 2) * 129, 492.f + (i / 2) * 46, 121, 37};
            const bool selected = static_cast<int>(sim.tool) == i;
            rect(button, selected ? Mint : Panel);
            label(tools[i], button.x + 18, button.y + 12, 13, selected ? Background : Muted);
        }
        label("LEFT MOUSE TO INTERACT", 33, 587, 9, Muted, 1.f);
        label("Time moves at your pace", 32, 616, 13, Text);
        label(decimal(sim.timeScale) + "x", 251, 617, 12, Mint);
        slider(32, 650, 250, (sim.timeScale - .2f) / 1.8f, Mint);
        label(sim.scene == Scene::Orbits ? "Central gravity" : "Gravity", 32, 688, 13, Text);
        label(sim.scene == Scene::Orbits ? "FIXED" : decimal(sim.gravity, 0), 239, 689, 11, Spectrum[4]);
        slider(32, 722, 250, sim.scene == Scene::Orbits ? .5f : sim.gravity / 1700.f, sim.scene == Scene::Orbits ? Muted : Spectrum[4]);
        label(sim.scene == Scene::Orbits ? "Inverse-square solar field" : "G / Toggle weightlessness", 32, 749, 10, Muted);
        DrawLine(32, 790, 282, 790, Line);
        label("MAKE A BEAUTIFUL MESS.", 32, 819, 11, Text, .8f);
        label("C++23  /  RAYLIB 6.2", 32, 848, 9, Muted, 1.2f);
    }

    void slider(float x, float y, float width, float fraction, Color color) const {
        DrawLineEx({x, y}, {x + width, y}, 3, Line);
        DrawLineEx({x, y}, {x + width * fraction, y}, 3, color);
        DrawCircleV({x + width * fraction, y}, 5, color);
        DrawCircleV({x + width * fraction, y}, 9, alpha(color, .10f));
    }

    void stage() {
        const int sceneIndex = static_cast<int>(sim.scene);
        label(Names[sceneIndex], Stage.x, 111, 17, Text, .8f);
        const string counts = to_string(sim.particles.size()) + " PARTICLES  /  " +
            (sim.scene == Scene::Cloth ? to_string(sim.activeLinks()) + " LINKS" : "VERLET DYNAMICS");
        label(counts, 1055, 115, 9, Muted, .5f);
        rect({Stage.x - 1, Stage.y - 1, Stage.width + 2, Stage.height + 2}, Line, .025f);
        DrawRectangleRec(Stage, {15, 22, 32, 255});
        BeginScissorMode(static_cast<int>(Stage.x), static_cast<int>(Stage.y),
                         static_cast<int>(Stage.width), static_cast<int>(Stage.height));
        // A faint coordinate grid offers scale without competing with motion.
        for (int x = 0; x <= static_cast<int>(WorldWidth); x += 40)
            DrawLineV(screen({static_cast<float>(x), 0}), screen({static_cast<float>(x), WorldHeight}), {26, 36, 47, 255});
        for (int y = 0; y <= static_cast<int>(WorldHeight); y += 40)
            DrawLineV(screen({0, static_cast<float>(y)}), screen({WorldWidth, static_cast<float>(y)}), {26, 36, 47, 255});
        if (sim.scene == Scene::Orbits) orbits();
        for (const auto& obstacle : sim.obstacles) {
            const auto center = screen(obstacle.position);
            DrawCircleGradient(center, obstacle.radius + 22,
                               alpha(Spectrum[1], .12f), alpha(Spectrum[1], 0));
            DrawCircleV(center, obstacle.radius, {26, 39, 53, 255});
            DrawCircleLinesV(center, obstacle.radius, {76, 105, 126, 255});
            DrawRing(center, obstacle.radius - 6, obstacle.radius - 5, 220, 320, 32, alpha(Mint, .6f));
            DrawLineV({center.x - 5, center.y}, {center.x + 5, center.y}, Muted);
            DrawLineV({center.x, center.y - 5}, {center.x, center.y + 5}, Muted);
        }
        if (sim.scene == Scene::Cloth) cloth();
        // Soft halos use additive blending: overlapping light gets brighter.
        BeginBlendMode(BLEND_ADDITIVE);
        for (size_t i = 0; i < sim.particles.size(); i += sim.scene == Scene::Cloth ? 4 : 2) {
            const auto& p = sim.particles[i];
            const auto center = screen(p.position);
            const Color color = Spectrum[static_cast<size_t>(p.palette) % Spectrum.size()];
            DrawCircleGradient(center,
                               p.radius * 3.2f, alpha(color, .12f), alpha(color, 0));
        }
        EndBlendMode();
        for (const auto& p : sim.particles) {
            const Color color = Spectrum[static_cast<size_t>(p.palette) % Spectrum.size()];
            const auto center = screen(p.position);
            if (sim.scene != Scene::Cloth) {
                DrawLineEx(screen(p.previous), center, max(1.f, p.radius), alpha(color, .4f));
                DrawCircleV(center, p.radius, color);
                if (p.radius >= 4) DrawCircleV({center.x - 1, center.y - 1}, p.radius * .28f, alpha(WHITE, .7f));
            } else {
                DrawCircleV(center, p.pinned ? 4.5f : 1.8f, p.pinned ? Text : alpha(color, .6f));
                if (p.pinned) DrawCircleLinesV(center, 8, alpha(Mint, .5f));
            }
        }
        if (CheckCollisionPointRec(mouse_, Stage) && !showHelp) {
            const Color color = sim.tool == Tool::Cut ? Spectrum[3] : Mint;
            DrawCircleLinesV(mouse_, sim.tool == Tool::Grab ? 16.f : sim.pointerRadius, alpha(color, .35f));
            DrawCircleV(mouse_, 3, color);
            DrawLineV({mouse_.x - 9, mouse_.y}, {mouse_.x + 9, mouse_.y}, alpha(color, .7f));
            DrawLineV({mouse_.x, mouse_.y - 9}, {mouse_.x, mouse_.y + 9}, alpha(color, .7f));
        }
        label(sim.scene == Scene::Cloth ? "PINNED TO THE MOMENT. FREE TO FALL APART." :
              sim.scene == Scene::Orbits ? "NOTHING STAYS STILL. EVERYTHING IS CONNECTED." : "SMALL THINGS. SURPRISING CONSEQUENCES.",
              Stage.x + 22, Stage.y + 22, 10, alpha(Muted, .7f), .9f);
        if (paused) badge("TIME IS YOURS / PAUSED", {Stage.x + 22, Stage.y + 49, 196, 28}, Spectrum[4]);
        EndScissorMode();
    }

    void cloth() const {
        // A surface is formed from adjacent points only when all its boundary
        // links remain intact. Cutting therefore opens a real, visible hole.
        unordered_set<uint64_t> edges;
        const auto edgeId = [](size_t a, size_t b) {
            return (static_cast<uint64_t>(min(a, b)) << 32) | max(a, b);
        };
        for (const auto& link : sim.links) if (link.active) edges.insert(edgeId(link.a, link.b));
        const size_t columns = static_cast<size_t>(sim.clothColumns);
        if (columns > 1) for (size_t a = 0; a + columns + 1 < sim.particles.size(); ++a) {
            if (a % columns == columns - 1) continue;
            const size_t b = a + 1, c = a + columns, d = c + 1;
            if (edges.contains(edgeId(a, b)) && edges.contains(edgeId(a, c)) &&
                edges.contains(edgeId(b, d)) && edges.contains(edgeId(c, d))) {
                const Color color = alpha(Spectrum[static_cast<size_t>(sim.particles[a].palette) % Spectrum.size()], .10f);
                DrawTriangle(screen(sim.particles[a].position), screen(sim.particles[c].position), screen(sim.particles[b].position), color);
                DrawTriangle(screen(sim.particles[b].position), screen(sim.particles[c].position), screen(sim.particles[d].position), color);
            }
        }
        for (const auto& link : sim.links) if (link.active) {
            const auto& a = sim.particles[link.a];
            const auto& b = sim.particles[link.b];
            const Vec delta = a.position - b.position;
            const float strain = abs(sqrt(delta.x * delta.x + delta.y * delta.y) / link.rest - 1);
            const Color color = ColorLerp(Spectrum[static_cast<size_t>(a.palette) % Spectrum.size()], Spectrum[3], clamp(strain * 8, 0.f, 1.f));
            DrawLineEx(screen(a.position), screen(b.position), .9f, alpha(color, .5f));
        }
    }

    void orbits() const {
        const auto center = screen({530, 320});
        for (float radius : {90.f, 150.f, 210.f, 280.f})
            DrawCircleLinesV(center, radius, {37, 45, 58, 255});
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(history_.texture, {0, 0, WorldWidth, -WorldHeight}, Stage, {0, 0}, 0, WHITE);
        DrawCircleGradient(center, 105,
                           alpha(Spectrum[4], .4f), alpha(Spectrum[4], 0));
        EndBlendMode();
        DrawCircleV(center, 22, Spectrum[4]);
        DrawCircleV(center, 16, {255, 227, 179, 255});
        DrawCircleLinesV(center, 29 + sin(visualTime * 2) * 2, alpha(Spectrum[4], .5f));
        label("THE ATTRACTOR", center.x - 48, center.y + 45, 8, Spectrum[4], 1.f);
    }

    void footer() const {
        rect({316, 824, 118, 42}, paused ? Spectrum[4] : Mint);
        label(paused ? "PLAY / SPACE" : "PAUSE / SPACE", 329, 839, 10, Background);
        label(Subtitles[static_cast<int>(sim.scene)], 456, 829, 14, Text);
        label("B / BURST     N / SPRINKLE     TAB / TOOL     SCROLL / RADIUS     F11 / FULLSCREEN", 456, 853, 8.5f, Muted, .3f);
        label(frameRate > 0 ? to_string(static_cast<int>(frameRate)) + " FPS" : "LIVE", 1293, 834, 10, Mint);
        DrawCircle(1355, 856, 3, paused ? Spectrum[4] : Mint);
        label("FLUX / 01", 32, 882, 8, Muted, 1.f);
        label("DRAG THE WORLD. SEE WHAT HAPPENS.", 1110, 882, 8, Muted, .7f);
    }

    void help() const {
        DrawRectangle(0, 0, CanvasWidth, CanvasHeight, {5, 9, 16, 205});
        rect({405, 185, 650, 510}, Panel, .04f);
        label("GO ON. TOUCH EVERYTHING.", 448, 224, 27, Text);
        label("A little guide to a very hands-on universe.", 448, 271, 15, Muted);
        const array<string, 8> lines{
            "1 / 2 / 3       Switch experiments",
            "LEFT MOUSE   Use your selected tool inside the canvas",
            "RIGHT MOUSE  Push particles away",
            "GRAB             Drag a cloth node or a particle",
            "CUT                Slice cloth links; reset to weave it again",
            "B / N              Burst impulse / add particles at the cursor",
            "SPACE / .       Pause time / advance one step while paused",
            "G / R / H       Gravity on-off / reset / close this guide"};
        for (size_t i = 0; i < lines.size(); ++i) label(lines[i], 448, 324 + i * 36.f, 14, i % 2 == 0 ? Text : Muted);
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse(argc, argv);
        if (options.selfTest) return runPhysicsChecks() ? 0 : 1;
        const bool automated = options.smokeTest || !options.screenshot.empty();
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
        InitWindow(CanvasWidth, CanvasHeight, "FLUX / A physics playground");
        if (!IsWindowReady()) throw runtime_error("Cannot create a graphics window.");
        SetWindowMinSize(960, 600);
        SetExitKey(KEY_NULL); // Escape pauses time instead of discarding the playground.
        SetTargetFPS(automated ? 0 : 120);
        int result = 0;
        {
            Playground app(filesystem::absolute(argv[0]).parent_path());
            app.select(options.scene);
            RenderTexture2D canvas = LoadRenderTexture(CanvasWidth, CanvasHeight);
            if (canvas.id == 0) throw runtime_error("Cannot allocate the UI canvas.");
            SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
            float accumulator = 0;
            int frames = 0;
            if (!options.screenshot.empty()) for (int i = 0; i < 200; ++i) {
                if (app.sim.scene == Scene::Cloth && i >= 60 && i < 170) {
                    // Photograph a real cloth interaction, rather than only a
                    // completely still sheet. This uses the normal mouse tool.
                    app.sim.tool = Tool::Grab;
                    app.sim.pointerDown = true;
                    const float t = static_cast<float>(i - 60) / 110.f;
                    app.sim.pointer = {530 + t * 160, 300 - sin(t * 3.14f) * 110};
                } else app.sim.releasePointer();
                app.sim.update(FixedStep);
                app.updateTrails();
            }

            while (!WindowShouldClose()) {
                const float dt = automated ? FixedStep : min(GetFrameTime(), .05f);
                if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
                const float scale = min(static_cast<float>(GetScreenWidth()) / CanvasWidth,
                                        static_cast<float>(GetScreenHeight()) / CanvasHeight);
                const Vector2 offset{(GetScreenWidth() - CanvasWidth * scale) / 2,
                                     (GetScreenHeight() - CanvasHeight * scale) / 2};
                const Vector2 raw = GetMousePosition();
                if (!automated) app.input({(raw.x - offset.x) / scale, (raw.y - offset.y) / scale});
                if (options.smokeTest) {
                    if (frames == 200) app.select(1);
                    if (frames == 400) app.select(2);
                    // Exercise real forces and slicing as well as static drawing.
                    app.sim.pointer = {500 + sin(frames * .03f) * 180, 300};
                    app.sim.pointerDown = frames % 100 > 30 && frames % 100 < 70;
                    app.sim.tool = app.sim.scene == Scene::Cloth ? Tool::Cut : Tool::Repel;
                }
                if (!app.paused && !app.showHelp) {
                    accumulator += dt;
                    while (accumulator >= FixedStep) { app.sim.update(FixedStep); accumulator -= FixedStep; }
                } else {
                    accumulator = 0;
                    if (IsKeyPressed(KEY_PERIOD)) app.sim.update(FixedStep);
                }
                if (options.smokeTest && !app.sim.finite())
                    throw runtime_error("Non-finite particle in the rendering smoke test.");
                app.updateTrails();
                BeginTextureMode(canvas);
                app.draw(dt);
                EndTextureMode();
                BeginDrawing();
                ClearBackground({5, 8, 13, 255});
                DrawTexturePro(canvas.texture, {0, 0, static_cast<float>(CanvasWidth), -static_cast<float>(CanvasHeight)},
                               {offset.x, offset.y, CanvasWidth * scale, CanvasHeight * scale}, {0, 0}, 0, WHITE);
                EndDrawing();
                ++frames;
                if (!options.screenshot.empty()) {
                    Image image = LoadImageFromScreen();
                    const bool saved = ExportImage(image, options.screenshot.string().c_str());
                    UnloadImage(image);
                    if (!saved) throw runtime_error("Could not write screenshot.");
                    cout << "Saved " << options.screenshot << '\n';
                    break;
                }
                if (options.smokeTest && frames >= 600) {
                    if (!app.sim.finite()) throw runtime_error("Non-finite particle in rendering smoke test.");
                    cout << "PASS: 600 rendered frames across all three experiments.\n";
                    break;
                }
            }
            UnloadRenderTexture(canvas);
        }
        CloseWindow();
        return result;
    } catch (const exception& error) {
        cerr << "FLUX: " << error.what() << '\n';
        if (IsWindowReady()) CloseWindow();
        return 1;
    }
}

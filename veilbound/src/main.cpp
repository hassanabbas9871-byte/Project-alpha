// VEILBOUND joins a persistent campaign to a deterministic tactical battle.
// raylib handles the window; Game/Content/Battle remain ordinary C++ so they
// can be tested without graphics hardware or a Windows desktop.
#include "Renderer.hpp"
#include "Audio.hpp"
#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace veil;

namespace {
// The desktop window opens at the requested 1600 by 1200 resolution.
// Geometry uses a logical 4:3 layout. Text is rasterized at the actual screen
// pixel size, so it is never blurred by enlarging a small off-screen image.
constexpr int WindowWidth = 1600, WindowHeight = 1200;
constexpr int CanvasWidth = UiWidth, CanvasHeight = UiHeight;
struct Options {
    bool smoke{}, mute{};
    string screen{"battle"};
    filesystem::path screenshot, saveDirectory;
};

Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i];
        if (arg == "--smoke-test") options.smoke = true;
        else if (arg == "--mute") options.mute = true;
        else if (arg == "--screenshot" && i + 1 < argc) options.screenshot = argv[++i];
        else if (arg == "--screen" && i + 1 < argc) options.screen = argv[++i];
        else if (arg == "--save-dir" && i + 1 < argc) options.saveDirectory = argv[++i];
        else throw runtime_error("Options: --mute, --smoke-test, --screenshot PATH, --screen title|map|battle|story|inventory|skills|journal, --save-dir PATH");
    }
    const string valid = "|title|map|battle|story|inventory|skills|journal|";
    if (valid.find("|" + options.screen + "|") == string::npos) throw runtime_error("Unknown screenshot screen.");
    return options;
}

filesystem::path playerDataDirectory() {
    // These are standard non-secret location variables. Saves belong to the
    // player, not the build folder that may be replaced by the next download.
#ifdef _WIN32
    if (const char* appData = getenv("LOCALAPPDATA")) return filesystem::path(appData) / "Veilbound";
#else
    if (const char* data = getenv("XDG_DATA_HOME")) return filesystem::path(data) / "Veilbound";
    if (const char* home = getenv("HOME")) return filesystem::path(home) / ".local/share/Veilbound";
#endif
    return filesystem::temp_directory_path() / "Veilbound";
}

// Automated captures receive their own save directory. They must never replace
// a human player's two-hour campaign, even if started beside the normal EXE.
struct TemporarySaves {
    filesystem::path directory;
    ~TemporarySaves() {
        if (!directory.empty()) {
            error_code ignored;
            filesystem::remove_all(directory, ignored);
        }
    }
};

void setCapturePage(Game& game, const string& name) {
    if (name == "title") { game.page = Page::Title; return; }
    if (name == "map") game.page = Page::Map;
    if (name == "story") game.beginMission();
    if (name == "inventory") game.page = Page::Inventory;
    if (name == "skills") game.page = Page::Skills;
    if (name == "journal") game.page = Page::Journal;
    if (name == "battle") game.enterBattle();
}

void battleControls(Game& game, UiState& ui) {
    if (ui.paused || !game.battle.isPartyTurn()) return;
    if (IsKeyPressed(KEY_M)) ui.action = -2;
    if (IsKeyPressed(KEY_A)) ui.action = -1;
    if (IsKeyPressed(KEY_E)) { game.battle.endTurn(); return; }
    const Unit* unit = game.battle.actor();
    if (!unit || !unit->party) return;
    for (int i = 0; i < 6; ++i) {
        if (IsKeyPressed(KEY_ONE + i)) ui.action = unit->source * 6 + i;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ui.hovered.x >= 0) {
        bool accepted = false;
        if (ui.action == -2) accepted = game.battle.move(ui.hovered);
        else if (ui.action == -1) accepted = game.battle.attack(ui.hovered);
        else accepted = game.battle.useSkill(ui.action, ui.hovered);
        if (!accepted) game.message = "That action is unavailable. Check range, line of sight, focus and action points.";
        else game.message.clear();
    }
}

// The smoke test uses the public combat actions. Its purpose is to exercise
// drawing and input state; the headless suite checks the detailed game rules.
void autoplayOneAction(Game& game) {
    if (game.page != Page::Battle || !game.battle.isPartyTurn()) return;
    auto* actor = game.battle.actor();
    if (!actor) return;
    for (const auto& unit : game.battle.units) {
        if (!unit.party && unit.alive() && game.battle.attack(unit.cell)) return;
    }
    auto destinations = game.battle.reachable();
    if (!destinations.empty()) {
        Pos target{10, 4};
        for (const auto& unit : game.battle.units) if (!unit.party && unit.alive()) { target = unit.cell; break; }
        sort(destinations.begin(), destinations.end(), [target](Pos a, Pos b) {
            return abs(a.x - target.x) + abs(a.y - target.y) < abs(b.x - target.x) + abs(b.y - target.y);
        });
        if (game.battle.move(destinations.front())) return;
    }
    game.battle.endTurn();
}
} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse(argc, argv);
        const bool automated = options.smoke || !options.screenshot.empty();
        TemporarySaves temporary;
        filesystem::path dataDirectory = options.saveDirectory;
        if (automated && dataDirectory.empty()) {
            const auto stamp = chrono::steady_clock::now().time_since_epoch().count();
            temporary.directory = filesystem::temp_directory_path() / ("veilbound-capture-" + to_string(stamp));
            dataDirectory = temporary.directory;
        }
        if (dataDirectory.empty()) dataDirectory = playerDataDirectory();
        filesystem::create_directories(dataDirectory);

        string reason;
        if (!validateContent(reason)) throw runtime_error("Campaign content is invalid: " + reason);
        Game game(dataDirectory / "campaign.sav");
        if (automated) { game.newGame(); setCapturePage(game, options.screen); }

        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
        InitWindow(WindowWidth, WindowHeight, "VEILBOUND / The Ninth Bell");
        if (!IsWindowReady()) throw runtime_error("Could not create the game window.");
        SetWindowMinSize(1024, 768);
        SetExitKey(KEY_NULL);
        SetTargetFPS(automated ? 0 : 60);
        {
            Renderer renderer(filesystem::absolute(argv[0]).parent_path());
            Audio audio(!automated && !options.mute);
            UiState ui;
            ui.interactive = !automated;
            int frames = 0;
            int displayedTurn = -1;

            while (!WindowShouldClose()) {
                const float dt = automated ? 1.f / 30.f : min(GetFrameTime(), .1f);
                ui.time += dt;
                if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
                const float scale = min(static_cast<float>(GetScreenWidth()) / CanvasWidth,
                                        static_cast<float>(GetScreenHeight()) / CanvasHeight);
                const Vector2 offset{(GetScreenWidth() - CanvasWidth * scale) / 2,
                                     (GetScreenHeight() - CanvasHeight * scale) / 2};
                const Vector2 raw = GetMousePosition();
                ui.mouse = {(raw.x - offset.x) / scale, (raw.y - offset.y) / scale};
                ui.hovered = {-1, -1};
                if (CheckCollisionPointRec(ui.mouse, BoardRect))
                    ui.hovered = {static_cast<int>((ui.mouse.x - BoardRect.x) / TileSize),
                                  static_cast<int>((ui.mouse.y - BoardRect.y) / TileSize)};
                const bool wasBattle = game.page == Page::Battle;
                const auto partyHealth = [&game](bool party) {
                    int total = 0;
                    for (const auto& unit : game.battle.units) if (unit.party == party) total += unit.hp;
                    return total;
                };
                const int oldPartyHealth = partyHealth(true), oldEnemyHealth = partyHealth(false);
                if (!automated) {
                    if (IsKeyPressed(KEY_F10)) {
                        audio.setMuted(!audio.muted());
                        game.message = audio.muted() ? "Sound muted. F10 toggles audio." : "Sound enabled. F10 toggles audio.";
                    }
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) audio.click();
                    if (IsKeyPressed(KEY_ESCAPE)) {
                        if (game.page == Page::Battle) ui.paused = !ui.paused;
                        else if (game.page != Page::Title) game.page = Page::Map;
                    }
                    if (IsKeyPressed(KEY_F5) && game.page != Page::Battle && game.page != Page::Title) game.save();
                    if (game.page == Page::Battle) {
                        // A new investigator starts in movement mode. Carrying
                        // the previous hero's spell selection into their turn
                        // would leave the mouse pointing at an unusable skill.
                        if (displayedTurn != game.battle.turns) {
                            displayedTurn = game.battle.turns;
                            ui.action = -2;
                            if (const auto* unit = game.battle.actor(); unit && unit->party) ui.selectedHero = unit->source;
                        }
                        battleControls(game, ui);
                    } else displayedTurn = -1;
                }
                if (options.smoke) {
                    if (frames == 0) game.page = Page::Map;
                    if (frames == 45) game.beginMission();
                    if (frames == 90) game.page = Page::Inventory;
                    if (frames == 135) game.page = Page::Skills;
                    if (frames == 180) game.page = Page::Journal;
                    if (frames == 225) game.enterBattle();
                    if (frames > 225 && frames % 12 == 0) autoplayOneAction(game);
                }
                if (!ui.paused) game.update(dt);
                if (wasBattle) {
                    if (partyHealth(false) < oldEnemyHealth) audio.attack();
                    if (partyHealth(true) < oldPartyHealth) audio.hurt();
                    if (game.page == Page::Reward) {
                        if (game.battle.outcome == Outcome::Victory) audio.victory();
                        else audio.defeat();
                    }
                }
                audio.update(dt, game.page == Page::Battle && !ui.paused);
                // Draw geometry directly, then draw crisp text in screen space.
                // The renderer uses the same viewport transform as mouse input.
                BeginDrawing();
                ClearBackground({6, 8, 14, 255});
                const float dpi = static_cast<float>(GetRenderWidth()) / max(1, GetScreenWidth());
                renderer.draw(game, ui, scale, offset, dpi);
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
                if (options.smoke && frames >= 360) {
                    cout << "PASS: 360 rendered frames across campaign, story, equipment, skills, journal and battle.\n";
                    break;
                }
            }
            // Battles resume from their last encounter checkpoint, not a partial
            // turn. Game saves that checkpoint automatically on every victory.
            if (!automated && game.page != Page::Title) game.save();
        }
        CloseWindow();
        return 0;
    } catch (const exception& error) {
        cerr << "VEILBOUND: " << error.what() << '\n';
        if (IsWindowReady()) CloseWindow();
        return 1;
    }
}

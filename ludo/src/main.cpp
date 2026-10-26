// LUDO / ARCANA is a local board game. This file owns the window and buttons;
// Ludo.cpp owns the rules, so clicking a button never bypasses a rule check.
// A beginner can read README.md first, then follow main() near the bottom.
#include "Ludo.hpp"
#include "Typography.hpp"
#include "Display.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using namespace ludo;

namespace {
// All positions below use this 1600 x 900 drawing space. A camera fits it to
// a resized window; the mouse uses the inverse of that same transformation.
// A landscape layout uses a 1080p laptop much better than a tall 4:3 layout.
constexpr float Width = 1600, Height = 900, Cell = 42;
constexpr Vector2 BoardOrigin{30, 155};
constexpr Color Background{242, 241, 234, 255}, Ink{30, 41, 52, 255};
constexpr Color Quiet{78, 92, 105, 255}, Line{192, 199, 198, 255};
constexpr Color Paper{255, 255, 250, 255}, Accent{41, 76, 89, 255};
constexpr array<Color, 4> Colors{{{207, 55, 68, 255}, {30, 116, 196, 255},
                                  {211, 145, 13, 255}, {38, 139, 97, 255}}};
constexpr array<const char*, 4> ColorNames{"Red", "Blue", "Gold", "Green"};
constexpr array<const char*, 3> AnimeNames{"Aura League", "Mecha Circuit", "Spirit Festival"};

struct CellPosition { int x, y; };
// Read clockwise from Red's entrance. Adding 13 rotates a player one quarter
// of the board. This is the familiar 52-square cross-shaped Ludo track.
constexpr array<CellPosition, 52> Track{{
    {0,6},{1,6},{2,6},{3,6},{4,6},{5,6},{6,5},{6,4},{6,3},{6,2},{6,1},{6,0},{7,0},
    {8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{9,6},{10,6},{11,6},{12,6},{13,6},{14,6},{14,7},
    {14,8},{13,8},{12,8},{11,8},{10,8},{9,8},{8,9},{8,10},{8,11},{8,12},{8,13},{8,14},{7,14},
    {6,14},{6,13},{6,12},{6,11},{6,10},{6,9},{5,8},{4,8},{3,8},{2,8},{1,8},{0,8},{0,7}
}};

Vector2 cellCenter(CellPosition p) {
    return {BoardOrigin.x + (p.x + .5f) * Cell, BoardOrigin.y + (p.y + .5f) * Cell};
}

CellPosition homeCell(int player, int step) {
    if (player == 0) return {1 + step, 7};
    if (player == 1) return {7, 1 + step};
    if (player == 2) return {13 - step, 7};
    return {7, 13 - step};
}

Vector2 basePosition(int player, int cone) {
    const int left = player == 1 || player == 2 ? 9 : 0;
    const int top = player >= 2 ? 9 : 0;
    return cellCenter({left + 2 + 2 * (cone % 2), top + 2 + 2 * (cone / 2)});
}

Vector2 conePosition(const Game& game, int player, int cone) {
    const int progress = game.players[player].cones[cone].progress;
    if (progress < 0) return basePosition(player, cone);
    if (progress < 52) return cellCenter(Track[(player * 13 + progress) % 52]);
    if (progress < 57) return cellCenter(homeCell(player, progress - 52));
    // Finished cones sit in their color's triangle rather than covering a path.
    const Vector2 middle = cellCenter({7, 7});
    constexpr array<Vector2, 4> directions{{{-31,0},{0,-31},{31,0},{0,31}}};
    return {middle.x + directions[player].x + (cone % 2 ? 8.f : -8.f),
            middle.y + directions[player].y + (cone / 2 ? 8.f : -8.f)};
}

// Stacks are spread into four small, individually clickable cones. Their
// location changes only on the screen: all still occupy the same rules square.
Vector2 displayedPosition(const Game& game, int player, int cone) {
    Vector2 position = conePosition(game, player, cone);
    vector<pair<int,int>> stack;
    const int progress = game.players[player].cones[cone].progress;
    if (progress < 0 || progress >= 57) return position;
    for (int p = 0; p < static_cast<int>(game.players.size()); ++p)
        for (int c = 0; c < 4; ++c) {
            const Vector2 other = conePosition(game, p, c);
            if (game.players[p].rank > 0 && game.players[p].cones[c].progress != 57) continue;
            if (game.players[p].cones[c].progress >= 0 &&
                abs(other.x - position.x) < .1f && abs(other.y - position.y) < .1f)
                stack.emplace_back(p, c);
        }
    if (stack.size() > 1) {
        const auto found = find(stack.begin(), stack.end(), pair{player, cone});
        const int index = static_cast<int>(found - stack.begin());
        // Up to 16 cones may share a protected square. Four columns keep even
        // that unusual state selectable through the cone cards in the sidebar.
        const int columns = stack.size() <= 4 ? 2 : 4;
        const float gap = columns == 2 ? 16.f : 9.f;
        position.x += (index % columns - (columns - 1) * .5f) * gap;
        const int rows = (static_cast<int>(stack.size()) + columns - 1) / columns;
        position.y += (index / columns - (rows - 1) * .5f) * gap;
    }
    return position;
}

// A small command line interface makes screenshots and automated smoke tests
// possible without adding a developer-only button to the actual game.
struct Options { bool smoke{}, demo{}; string screen{"match"}, mode{"basic"}; filesystem::path screenshot; };
Options arguments(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; ++i) {
        const string value = argv[i];
        if (value == "--smoke-test") result.smoke = true;
        else if (value == "--demo") result.demo = true;
        else if (value == "--screenshot" && i + 1 < argc) result.screenshot = argv[++i];
        else if (value == "--screen" && i + 1 < argc) result.screen = argv[++i];
        else if (value == "--mode" && i + 1 < argc) result.mode = argv[++i];
        else throw runtime_error("Options: --smoke-test, --demo, --screenshot PATH, --screen lobby|match, --mode basic|classes|anime|custom");
    }
    if (result.screen != "lobby" && result.screen != "match") throw runtime_error("Unknown screen.");
    if (result.mode != "basic" && result.mode != "classes" && result.mode != "anime" && result.mode != "custom") throw runtime_error("Unknown mode.");
    return result;
}

filesystem::path findFont(const filesystem::path& executable) {
    // Explorer does not promise a working directory. Look beside the EXE first.
    const auto bundled = filesystem::absolute(executable).parent_path() / "assets/DejaVuSans.ttf";
    if (filesystem::is_regular_file(bundled)) return bundled;
    return LUDO_FONT_PATH;
}

class Application {
public:
    explicit Application(const filesystem::path& executable) : type(findFont(executable)) {
        selectedClasses = {Class::Fighter, Class::Wizard, Class::Rogue, Class::Cleric};
    }

    void preset(const string& name) {
        if (name == "classes") setMode(Mode::Classes);
        else if (name == "anime") setMode(Mode::Anime);
        else if (name == "custom") setMode(Mode::Custom);
        if (name != "basic") diceCount = 2;
    }

    void start(bool demonstration = false) {
        Config configuration;
        configuration.mode = mode;
        configuration.diceCount = diceCount;
        configuration.firstConeWins = firstCone;
        configuration.powerups = mode != Mode::Basic && powers;
        configuration.classAbilities = mode != Mode::Basic && classes;
        configuration.uniqueStats = mode != Mode::Basic && traits;
        configuration.captures = captures;
        configuration.safeSquares = safeSquares;
        configuration.seed = static_cast<unsigned>(chrono::steady_clock::now().time_since_epoch().count());
        configuration.players.clear(); // Replace Config's sample seats with this table.
        for (int p = 0; p < humans + computers; ++p)
            configuration.players.push_back({string(ColorNames[p]) + (p >= humans ? " CPU" : " player"),
                                             p >= humans, selectedClasses[p]});
        game = make_unique<Game>(configuration);
        demonstrationBoard = demonstration;
        inMatch = true;
        paused = false;
        selectedCone = 0;
        selectedDie = 0;
        observedTurn = -1;
        aiTimer = 0;
        if (demonstration) {
            // This is a labelled preview only. Normal matches always start with
            // every cone in base and use the engine's random dice.
            for (int p = 0; p < static_cast<int>(game->players.size()); ++p) {
                game->players[p].cones[0].progress = 4 + p * 7;
                game->players[p].cones[1].progress = 17 + p * 3;
                game->players[p].cones[2].progress = 51 + p;
            }
            game->rollWith({1, 5});
        }
    }

    void update(float dt, bool automated) {
        time += dt;
        animation = max(0.f, animation - dt);
        if (!inMatch || !game || paused || rulesPage >= 0 || game->over) return;
        if (game->turn != observedTurn) {
            observedTurn = game->turn;
            selectedDie = 0;
            selectedCone = 0;
            aiTimer = 0;
        }
        // CPUs use precisely the same public actions as human button clicks.
        // A delay lets a person follow each die and move on the board.
        if (game->players[game->current].computer || automated) {
            aiTimer += dt;
            if (aiTimer >= (automated ? .02f : .72f)) {
                aiTimer = 0;
                computerAction();
            }
        }
    }

    void draw(float scale, Vector2 offset, float dpi, bool allowInput) {
        mouse = {(GetMouseX() - offset.x) / scale, (GetMouseY() - offset.y) / scale};
        input = allowInput && !paused && rulesPage < 0;
        type.begin(scale, offset, dpi);
        geometryScale = max(.01f, scale*dpi);
        // raylib's BeginMode2D replaces its normal DPI matrix. Include the
        // framebuffer ratio explicitly so geometry and the native text pass
        // still agree on a Retina display or any non-1 framebuffer scale.
        BeginMode2D(Camera2D{{offset.x*dpi,offset.y*dpi}, {0,0}, 0, scale*dpi});
        ClearBackground(Background);
        text("LUDO", 30, 14, 42, Ink);
        text("/  ARCANA", 172, 24, 26, Quiet);
        text("A familiar board. A few extraordinary possibilities.", 32, 70, 22, Quiet);
        if (button({1310,24,260,50}, "How to play", true)) rulesPage = 0;
        if (inMatch) match(); else lobby();
        if (allowInput && IsKeyPressed(KEY_ESCAPE)) {
            if (rulesPage >= 0) rulesPage = -1;
            else if (inMatch) paused = !paused;
        }
        if (paused && inMatch) pauseMenu();
        if (rulesPage >= 0) rulebook();
        EndMode2D();
        // Glyphs are drawn at native pixel size after the geometry camera.
        // There is no low-resolution texture to blur when the window grows.
        type.flush();
    }

    bool inMatch{};
    unique_ptr<Game> game;

private:
    Typography type;
    Mode mode{Mode::Basic};
    int humans{1}, computers{3}, diceCount{1}, animePreset{}, selectedCone{}, selectedDie{};
    int observedTurn{-1}, rulesPage{-1};
    bool firstCone{false}, powers{true}, classes{true}, traits{true}, captures{true}, safeSquares{true};
    bool input{}, paused{}, quitConfirm{}, demonstrationBoard{};
    array<Class,4> selectedClasses{};
    Vector2 mouse{};
    float time{}, aiTimer{}, animation{}, geometryScale{1};

    float stroke(float logicalWidth = 1) const {
        // Thin filled outlines can disappear when their width shrinks below a
        // physical pixel. Keep them visible at fractional laptop-window scales.
        return max(logicalWidth, 1.25f/geometryScale);
    }

    void text(const string& value, float x, float y, float size = 22, Color color = Ink) {
        type.add(value, x, y, size, color);
    }
    void wrapped(const string& value, Rectangle box, float size = 22, Color color = Quiet) {
        float y = box.y;
        for (const auto& line : type.lines(value, box.width, size)) {
            text(line, box.x, y, size, color);
            y += type.lineHeight(size);
        }
    }
    void panel(Rectangle area, Color color = Paper) {
        DrawRectangleRounded(area, .025f, 6, color);
        DrawRectangleRoundedLinesEx(area, .025f, 6, stroke(), Line);
    }
    bool button(Rectangle area, const string& label, bool enabled = true, bool active = false) {
        const bool hover = input && enabled && CheckCollisionPointRec(mouse, area);
        const Color fill = active ? Accent : hover ? Color{221,231,230,255} : Paper;
        DrawRectangleRounded(area, .12f, 5, fill);
        DrawRectangleRoundedLinesEx(area, .12f, 5, stroke(active ? 2.f : 1.f), enabled ? Accent : Line);
        const float size = 21;
        text(label, area.x + (area.width - type.measure(label, size)) / 2,
             area.y + (area.height - type.fontSize(size)) / 2 - 2, size,
             active ? WHITE : enabled ? Ink : Quiet);
        return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }
    void counter(const string& label, int& value, int minimum, int maximum, float y) {
        text(label, 724, y + 9, 24);
        if (button({1250,y,64,44}, "-", value > minimum)) --value;
        text(to_string(value), 1380, y + 7, 26);
        if (button({1482,y,64,44}, "+", value < maximum)) ++value;
    }

    void setMode(Mode next) {
        mode = next;
        powers = classes = traits = next != Mode::Basic;
        captures = safeSquares = true;
        animePreset = 0;
    }

    void lobby() {
        constexpr array<Mode,4> modes{Mode::Basic,Mode::Classes,Mode::Anime,Mode::Custom};
        for (int i = 0; i < 4; ++i)
            if (button({30.f + i * 389, 105, 374, 40}, modeName(modes[i]), true, mode == modes[i])) setMode(modes[i]);
        board(nullptr);
        panel({700,155,870,632});
        text("YOUR TABLE", 724, 175, 29);
        counter("Local humans", humans, 1, 4, 220);
        computers = min(computers, 4 - humans);
        counter("Computer rivals", computers, 0, 4 - humans, 275);
        text(humans + computers == 1 ? "Solo: race against your own turn count." : "Humans share this screen; CPUs play automatically.", 724, 328, 22, Quiet);
        text("Dice per turn", 724, 374, 24);
        if (button({1120,363,200,45}, "One D6", true, diceCount == 1)) diceCount = 1;
        if (button({1340,363,206,45}, "Two D6", true, diceCount == 2)) diceCount = 2;
        text("Victory", 724, 430, 24);
        if (button({1120,419,200,45}, "First cone", true, firstCone)) firstCone = true;
        if (button({1340,419,206,45}, "All four cones", true, !firstCone)) firstCone = false;

        if (mode == Mode::Basic) {
            text("THE ORIGINAL RECIPE", 724, 492, 25);
            wrapped("Roll a 1 to bring each cone out. Pick a die, then pick any legal cone. Race around the board and into your home lane.", {724,538,800,116}, 28, Ink);
            wrapped("No cards, class abilities, or cone bonuses. Captures and marked safe squares are on.", {724,680,800,82}, 25);
        } else {
            text(mode == Mode::Anime ? "CHOOSE YOUR DISCIPLINES" : "CHOOSE YOUR CLASSES", 724, 487, 24);
            for (int p = 0; p < humans + computers; ++p) {
                const float y = 530.f + p * 51;
                DrawCircle(740, static_cast<int>(y + 21), 9, Colors[p]);
                text(string(ColorNames[p]) + (p >= humans ? " CPU" : " local"), 763, y + 8, 24);
                if (button({1116,y,430,43}, className(selectedClasses[p], mode)))
                    selectedClasses[p] = static_cast<Class>((static_cast<int>(selectedClasses[p]) + 1) % 6);
            }
            if (mode == Mode::Anime) {
                if (button({724,741,390,36}, AnimeNames[animePreset])) {
                    animePreset = (animePreset + 1) % 3;
                    powers = animePreset != 1;
                    traits = animePreset != 2;
                    captures = animePreset != 2;
                }
                const array<string,3> descriptions{
                    "Cards, traits and captures.",
                    "Traits and classes. No cards.",
                    "Peaceful race. Equal traits."};
                text(descriptions[animePreset], 1130, 749, 21);
            } else if (mode == Mode::Custom) {
                // Each switch changes a real rule; the rules engine receives
                // exactly these flags when Start Match is pressed.
                const array<string,5> labels{"Cards", "Classes", "Traits", "Captures", "Safety"};
                array<bool*,5> switches{&powers,&classes,&traits,&captures,&safeSquares};
                for (int i = 0; i < 5; ++i) {
                    const float x = 724.f + i * 165;
                    if (button({x,741,155,39}, labels[i], true, *switches[i])) *switches[i] = !*switches[i];
                }
            } else {
                wrapped("Click a class to cycle it. Select a cone to use its ability during play.", {724,741,800,47}, 22);
            }
        }
        wrapped(mode == Mode::Basic ? "BASIC / Just dice, cones, and the race home." :
                mode == Mode::Anime ? "ANIME / Three original tournament presets. No prior anime knowledge needed." :
                mode == Mode::Custom ? "CUSTOM / Build a ruleset for your own table." :
                "CLASSES / Six tabletop-inspired kits, four cone traits, and collectible cards.",
                {30,810,630,80}, 24, Ink);
        if (button({700,810,870,60}, humans + computers == 1 ? "Start solo time trial" : "Start match", true, true)) start();
    }

    void board(const Game* state) {
        panel({BoardOrigin.x - 2, BoardOrigin.y - 2, 15*Cell+4, 15*Cell+4});
        // The four large home bases match the supplied reference's layout.
        for (int p = 0; p < 4; ++p) {
            const int x = p == 1 || p == 2 ? 9 : 0, y = p >= 2 ? 9 : 0;
            const Rectangle outer{BoardOrigin.x + x * Cell, BoardOrigin.y + y * Cell, 6*Cell, 6*Cell};
            DrawRectangleRec(outer, Colors[p]);
            DrawRectangleRounded({outer.x + 20, outer.y + 20, outer.width - 40, outer.height - 40}, .13f, 8, Paper);
            const bool active = state && p == state->current && !state->over && state->players[p].rank == 0;
            if (active) DrawRectangleLinesEx({outer.x + 8, outer.y + 8, outer.width - 16, outer.height - 16}, 4, WHITE);
            text(string(ColorNames[p]) + (state && p < static_cast<int>(state->players.size()) && state->players[p].computer ? " / CPU" : ""),
                 outer.x + 28, outer.y + 26, 22, Colors[p]);
            for (int c = 0; c < 4; ++c) {
                const Vector2 position = basePosition(p, c);
                DrawCircleV(position, 22, ColorAlpha(Colors[p], .13f));
                DrawCircleLinesV(position, 22, ColorAlpha(Colors[p], .6f));
                if (!state) drawCone(position, p, c, false, false, 0, 0);
            }
            if (state && p < static_cast<int>(state->players.size())) {
                string bottom = state->players[p].rank > 0 ? "Place " + to_string(state->players[p].rank) :
                    state->config.classAbilities ? className(state->players[p].heroClass, state->config.mode) : "Roll 1 to deploy";
                text(bottom, outer.x + 26, outer.y + 211, 20, Colors[p]);
            } else if (state) text("Empty seat", outer.x + 48, outer.y + 211, 20, Quiet);
        }
        for (int i = 0; i < 52; ++i) {
            const Vector2 center = cellCenter(Track[i]);
            Color fill = Paper;
            if (i % 13 == 0) fill = ColorAlpha(Colors[i / 13], .2f);
            DrawRectangleRec({center.x-Cell/2,center.y-Cell/2,Cell,Cell}, fill);
            DrawRectangleLinesEx({center.x-Cell/2,center.y-Cell/2,Cell,Cell}, stroke(), Line);
            const bool safe = state ? state->config.safeSquares && state->safe(i) :
                i % 13 == 0 || i % 13 == 8;
            if (safe) {
                // A hollow diamond marks protected cells; it is a shape, not
                // a font glyph that might be missing from a Windows font.
                DrawPolyLines(center, 4, 10, 0, Quiet);
            } else if (state && (state->config.powerups || state->config.classAbilities) && Game::pickup(i)) {
                DrawCircleV(center, 9, Color{128,78,170,255});
                // Draw the sign immediately with the board. Queuing text here
                // would put a '+' over a cone that later lands on this cell.
                DrawLineEx({center.x-5,center.y},{center.x+5,center.y},2,WHITE);
                DrawLineEx({center.x,center.y-5},{center.x,center.y+5},2,WHITE);
            }
            if (i % 13 == 0) {
                // A small corner dot identifies the corresponding entry color.
                DrawCircle(static_cast<int>(center.x-13), static_cast<int>(center.y-13), 3, Colors[i/13]);
            }
        }
        for (int p = 0; p < 4; ++p) for (int h = 0; h < 5; ++h) {
            const Vector2 center = cellCenter(homeCell(p,h));
            DrawRectangleRec({center.x-Cell/2,center.y-Cell/2,Cell,Cell}, ColorAlpha(Colors[p], .3f));
            DrawRectangleLinesEx({center.x-Cell/2,center.y-Cell/2,Cell,Cell}, stroke(), Colors[p]);
        }
        const Vector2 middle = cellCenter({7,7});
        const Vector2 tl{middle.x-1.5f*Cell,middle.y-1.5f*Cell}, tr{middle.x+1.5f*Cell,middle.y-1.5f*Cell};
        const Vector2 bl{middle.x-1.5f*Cell,middle.y+1.5f*Cell}, br{middle.x+1.5f*Cell,middle.y+1.5f*Cell};
        DrawTriangle(tl,bl,middle,Colors[0]);
        DrawTriangle(tr,tl,middle,Colors[1]);
        DrawTriangle(br,tr,middle,Colors[2]);
        DrawTriangle(bl,br,middle,Colors[3]);
        if (!state) return;

        // Landing previews use the engine's actual move distance, including
        // voluntary Sprint and class bonuses. They never guess from a die face.
        const bool humanTurn = !state->players[state->current].computer && !state->over &&
                               state->players[state->current].rank == 0 && animation <= 0;
        // Choose only the closest clicked cone once. Several circles in a stack
        // can overlap; processing each overlap would accidentally spend 2 dice.
        optional<int> clickedCone;
        float closest = 21.f;
        if (input && humanTurn && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            for (int c = 0; c < 4; ++c) {
                const Vector2 position = displayedPosition(*state, state->current, c);
                const float distance = hypotf(mouse.x-position.x, mouse.y-position.y);
                if (distance < closest) { closest = distance; clickedCone = c; }
            }
        for (int p = 0; p < static_cast<int>(state->players.size()); ++p)
            for (int c = 0; c < 4; ++c) {
                // A ranked player has left the race. Keep their finished cone
                // in the center, but retire their other pieces from the track.
                if (state->players[p].rank > 0 && state->players[p].cones[c].progress != 57) continue;
                const Vector2 position = displayedPosition(*state, p, c);
                const bool legal = humanTurn && p == state->current && state->canMove(selectedDie, c);
                const bool selected = p == state->current && c == selectedCone;
                const auto& cone = state->players[p].cones[c];
                drawCone(position, p, c, legal, selected, cone.shields, cone.sprint);
            }
        if (humanTurn && state->canMove(selectedDie, selectedCone)) {
            const int progress = state->players[state->current].cones[selectedCone].progress;
            const int target = progress < 0 ? 0 : progress + state->moveDistance(selectedDie, selectedCone);
            Vector2 landing = target < 52 ? cellCenter(Track[(state->current*13+target)%52]) :
                              target < 57 ? cellCenter(homeCell(state->current,target-52)) : middle;
            DrawCircleLinesV(landing, 20 + 2 * sinf(time*4), Ink);
        }
        if (clickedCone) {
            selectedCone = *clickedCone;
            if (game->canMove(selectedDie, selectedCone)) {
                game->move(selectedDie, selectedCone);
                selectUsableDie();
            }
        }
    }

    void drawCone(Vector2 position, int player, int cone, bool legal, bool selected, int shields, int sprint) {
        if (legal) DrawCircleV(position, 21 + 2*sinf(time*4), Color{255,224,123,255});
        if (selected) DrawCircleLinesV(position, 23, Ink);
        DrawCircleV({position.x+1,position.y+3}, 17, ColorAlpha(Ink,.2f));
        DrawCircleV(position, 17, Colors[player]);
        DrawCircleLinesV(position, 17, WHITE);
        const string number = to_string(cone+1);
        text(number, position.x - type.measure(number,23)/2, position.y - 14, 23, WHITE);
        if (shields > 0) DrawCircleLinesV(position, 20, Color{85,206,234,255});
        if (sprint > 0) DrawCircle(static_cast<int>(position.x+13), static_cast<int>(position.y-13), 5, Color{254,215,87,255});
    }

    void die(Rectangle area, int index, bool humanTurn) {
        const bool chosen = index == selectedDie;
        const bool spent = game->spent[index];
        const bool enabled = game->rolled && !spent && humanTurn;
        const bool hover = input && enabled && CheckCollisionPointRec(mouse,area);
        DrawRectangleRounded(area,.16f,8, spent ? Color{225,228,225,255} : chosen ? Color{225,237,234,255} : WHITE);
        DrawRectangleRoundedLinesEx(area,.16f,8,stroke(chosen ? 3.f : 1.f),chosen ? Accent : Line);
        if (game->rolled && !spent) {
            const int value = animation > 0 ? 1 + static_cast<int>(time*24 + index*3)%6 : game->dice[index];
            const Vector2 center{area.x+area.width/2,area.y+area.height/2};
            const float spacing=20;
            auto pip = [&](int x,int y) { DrawCircleV({center.x+x*spacing,center.y+y*spacing},6,spent ? Quiet : Ink); };
            if (value%2) pip(0,0);
            if (value>=2) { pip(-1,-1); pip(1,1); }
            if (value>=4) { pip(1,-1); pip(-1,1); }
            if (value==6) { pip(-1,0); pip(1,0); }
        } else if (game->rolled) {
            // A compact die cannot fit a bottom caption over its lower pips.
            // Once spent, replace the face with a clear centered label.
            text("USED",area.x+(area.width-type.measure("USED",21))/2,
                 area.y+(area.height-type.fontSize(21))/2-2,21,Quiet);
        } else text("?", area.x+area.width/2-9, area.y+area.height/2-19,34,Quiet);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectedDie = index;
    }

    void selectUsableDie() {
        for (int d=0; d<game->config.diceCount; ++d)
            if (!game->spent[d] && !game->legalMoves(d).empty()) { selectedDie=d; return; }
    }

    void match() {
        if (!game) return;
        board(game.get());
        auto& player = game->players[game->current];
        const bool humanTurn = !player.computer && !game->over && animation <= 0;
        text(modeName(game->config.mode) + "  /  " + to_string(game->config.diceCount) + " D6  /  " +
             (game->config.firstConeWins ? "First cone home" : "All four home"), 30, 110, 24);
        if (demonstrationBoard) text("DEMO BOARD", 1090, 112, 21, Quiet);
        if (button({1330,102,240,42},"Pause / menu")) paused=true;
        panel({700,155,870,632});
        DrawCircle(737,193,10,Colors[game->current]);
        text(game->over ? "RACE COMPLETE" : player.name + "'s turn", 760, 174, 29);
        text("Turn " + to_string(game->turn) + (player.computer ? "  /  computer thinking" : "  /  your move"), 724, 216, 22, Quiet);
        die({724,252,84,84},0,humanTurn);
        if (game->config.diceCount==2) die({828,252,84,84},1,humanTurn);
        const string explanation = animation > 0 ? "Rolling..." : !game->rolled ? "Roll to begin this turn." : game->canEndTurn() ?
            "No legal dice remain. End turn." : "Pick a die, then click a cone.";
        wrapped(explanation,{940,266,580,70},26,Ink);
        if (button({724,352,500,50}, game->rolled ? "Dice rolled" : "Roll dice", humanTurn && !game->rolled, !game->rolled)) {
            game->roll(); animation=.32f; selectUsableDie();
        }
        if (button({1240,352,306,50},"End turn", humanTurn && game->canEndTurn())) { game->endTurn(); return; }
        if (input && humanTurn && IsKeyPressed(KEY_SPACE)) {
            if (!game->rolled) { game->roll(); animation=.32f; selectUsableDie(); }
            else if (game->canEndTurn()) { game->endTurn(); return; }
        }
        text("YOUR CONES",724,416,23);
        for (int c=0;c<4;++c) {
            const float y=450.f+c*58;
            const auto& cone=player.cones[c];
            const bool legal=game->canMove(selectedDie,c);
            const bool selected=c==selectedCone;
            const Rectangle card{724,y,822,52};
            DrawRectangleRounded(card,.1f,5,selected ? Color{224,235,232,255} : Color{244,246,241,255});
            if(selected) DrawRectangleRoundedLinesEx(card,.1f,5,2,Accent);
            DrawCircle(747,static_cast<int>(y+25),14,Colors[game->current]);
            text(to_string(c+1),740,y+11,20,WHITE);
            text(game->config.uniqueStats ? coneName(c) : "Cone " + to_string(c+1),774,y+13,24);
            const string location=cone.progress<0 ? "Base (roll 1)" : cone.progress==57 ? "Finished" :
                cone.progress>=52 ? "Home lane " + to_string(cone.progress-51) + "/6" : "Circuit " + to_string(cone.progress) + "/52";
            text(location,1050,y+13,23,Quiet);
            if (button({1378,y+5,156,42},legal ? "Move " + to_string(game->moveDistance(selectedDie,c)) : selected ? "Selected" : "Select",humanTurn,selected)) {
                selectedCone=c;
                if(legal) { game->move(selectedDie,c); selectUsableDie(); }
            }
            if(input && humanTurn && CheckCollisionPointRec(mouse,{724,y,635,52}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectedCone=c;
        }
        const auto& selected=player.cones[selectedCone];
        string stats="Cone " + to_string(selectedCone+1);
        if(game->config.uniqueStats) stats += "  /  Speed " + to_string(selected.stats.speed) + "  Guard " + to_string(selected.stats.guard) + "  Luck " + to_string(selected.stats.luck);
        stats += "  /  Shield " + to_string(selected.shields) + "  Boost +" + to_string(selected.sprint);
        if (!game->config.uniqueStats && !game->config.powerups && !game->config.classAbilities)
            stats = "Identical cones / No class or movement bonuses.";
        text(stats,724,690,23,Ink);
        if(game->config.classAbilities) {
            const string label=className(player.heroClass,game->config.mode) + " / E: " + to_string(player.energy);
            if(button({724,730,340,45},label,humanTurn && game->canUseAbility(selectedCone,selectedDie))) game->useAbility(selectedCone,selectedDie);
        } else text("Select a cone first.",724,743,22,Quiet);
        if(game->config.powerups) {
            for(int i=0;i<3;++i) {
                const Power power=static_cast<Power>(i);
                if(button({1076.f+i*160,730,150,45},powerName(power)+" x"+to_string(player.cards[i]),
                    humanTurn && game->canUsePower(power,selectedCone,selectedDie))) {
                    game->usePower(power,selectedCone,selectedDie); selectUsableDie();
                }
            }
        } else text(!game->config.captures ? "No captures / Gold ring = legal move." :
                    game->config.safeSquares ? "Diamonds: safe. Gold ring: legal." :
                    "Safety off. Gold ring: legal.",1076,743,21,Quiet);
        // The history keeps the last move visible even after a CPU passes.
        text("TABLE LOG",30,806,21,Quiet);
        for(int i=0;i<2 && i<static_cast<int>(game->history.size());++i)
            wrapped(game->history[game->history.size()-1-i],{30,837.f+i*30,1520,30},22,Ink);
        if (game->over) results();
    }

    void computerAction() {
        if (!game || game->over) return;
        if(!game->rolled) { game->roll(); animation=.25f; return; }
        auto& player=game->players[game->current];
        // Prefer a finish, then a capture, then forward progress. Bringing a
        // second cone into play is useful insurance against being captured.
        int bestDie=-1,bestCone=-1,bestScore=-100000;
        for(int d=0;d<game->config.diceCount;++d) for(int c:game->legalMoves(d)) {
            const int progress=player.cones[c].progress;
            const int target=progress<0 ? 0 : progress+game->moveDistance(d,c);
            int score=target + (progress<0 ? 36 : 0) + (target==57 ? 10000 : 0);
            if(target<52) for(int p=0;p<static_cast<int>(game->players.size());++p)
                if(p!=game->current) for(int other=0;other<4;++other)
                    if(game->players[p].cones[other].progress>=0 && game->players[p].cones[other].progress<52 &&
                       game->absolutePosition(p,other)==(game->current*13+target)%52 && !game->safe((game->current*13+target)%52)) score+=70;
            if(score>bestScore) { bestScore=score; bestDie=d; bestCone=c; }
        }
        if(bestDie>=0) {
            // Defensive cards do not change the chosen move's legality.
            if(game->canUsePower(Power::Shield,bestCone,bestDie) && player.cones[bestCone].progress>=0)
                game->usePower(Power::Shield,bestCone,bestDie);
            if(game->canUseAbility(bestCone,bestDie)) game->useAbility(bestCone,bestDie);
            if(game->canMove(bestDie,bestCone)) game->move(bestDie,bestCone);
            else if(game->canEndTurn()) game->endTurn();
        } else {
            // A Wizard can rescue a turn without a deployment 1. Search both
            // unused dice: die zero may already have been spent this turn.
            for(int d=0;d<game->config.diceCount;++d) {
                if(player.heroClass==Class::Wizard && game->canUseAbility(0,d)) { game->useAbility(0,d); return; }
                if(game->canUsePower(Power::Reroll,0,d)) { game->usePower(Power::Reroll,0,d); return; }
            }
            if(game->canEndTurn()) game->endTurn();
        }
    }

    void overlay(Rectangle area) {
        DrawRectangle(0,0,static_cast<int>(Width),static_cast<int>(Height),Color{24,33,43,220});
        // Otherwise previously queued labels would draw over this modal.
        type.clear();
        panel(area);
    }
    void pauseMenu() {
        overlay({430,210,740,500});
        const bool old=input; input=rulesPage<0;
        text(quitConfirm ? "Leave this match?" : "Take a breath",480,250,38);
        wrapped(quitConfirm ? "Returning to the table ends this match. You can change every rule before starting again." :
                "Pass the laptop to the next player, review the rules, or return to the table.", {480,310,630,110},25);
        if(button({480,440,640,55},quitConfirm ? "Keep playing" : "Resume match",true,true)) { paused=false; quitConfirm=false; }
        if(button({480,515,640,55},quitConfirm ? "End match and return to table" : "Return to table")) {
            if(quitConfirm) { inMatch=false; paused=false; quitConfirm=false; }
            else quitConfirm=true;
        }
        if(button({480,590,640,55},"How to play")) rulesPage=0;
        input=old;
    }
    void results() {
        overlay({390,165,820,600});
        const bool old=input; input=rulesPage<0;
        text("THE FINISHING TABLE",442,205,37);
        text("Turns played: " + to_string(game->turn),442,265,24,Quiet);
        for(int i=0;i<static_cast<int>(game->podium.size());++i) {
            const int p=game->podium[i];
            DrawCircle(468,339+i*70,13,Colors[p]);
            text(to_string(i+1)+".  "+game->players[p].name,500,320.f+i*70,29);
        }
        if(button({442,655,716,60},"Back to the table",true,true)) inMatch=false;
        input=old;
    }
    void rulebook() {
        overlay({180,80,1240,750});
        const bool old=input; input=true;
        text("THE POCKET RULEBOOK",224,106,34);
        text(to_string(rulesPage+1)+" / 3",1260,110,24,Quiet);
        if(rulesPage==0) {
            text("The race",224,174,29);
            wrapped("Choose 1-4 local humans, then optionally fill spare seats with computer rivals. One seat alone is a turn-count time trial. Red starts; play continues clockwise.", {224,220,1140,100},24);
            wrapped("Roll one or two six-sided dice. Each die is a separate move: choose a die, then choose any of your four cones. A raw 1 is required EVERY time you bring a cone out of base. Deployment uses that die and places the cone on its colored entrance.", {224,330,1140,130},24);
            wrapped("Travel clockwise through all 52 outer squares, then along your own five colored home squares into the center. You need the exact roll to finish. A 6 gives no extra turn. When no legal die remains, press End turn or Space.", {224,475,1140,130},24);
            wrapped("Bring all four cones home to earn your place. Choose First cone in the lobby for a shorter race. Ranked players leave the race; the last remaining player takes the final place. Matches continue after first place.", {224,620,1140,110},24);
        } else if(rulesPage==1) {
            text("Cards, cones & contact",224,174,29);
            wrapped("Land on an unprotected enemy square to send its cones back to base. Diamond cells are safe when enabled. Stacks never form a blockade. Home lanes belong only to their matching color.", {224,220,1140,100},24);
            wrapped("Purple cells grant a card: Shield blocks one capture, Sprint boosts one move, Reroll replaces an unused die once per turn. Cards never deploy a cone without a 1. Mecha cells restore energy without cards.", {224,330,1140,130},24);
            wrapped("Scout, Sentinel, Fortune and Balanced are the four cone traits. Speed improves Sprint, Guard supplies shields on deployment, and Luck adds pickup energy. Read the selected cone's stats, shield count and boost in the sidebar.", {224,475,1140,130},24);
            wrapped("Roll before using cards or abilities. Most abilities cost 2 energy; Cleric costs 3, Monk costs 1. Use at most one per turn. Energy returns on later turns (maximum 6). Basic disables these extras.", {224,620,1140,110},24);
        } else {
            text("Six ways to bend the race",224,174,29);
            for(int i=0;i<6;++i) {
                const float x=224.f+(i%2)*590, y=220.f+(i/2)*166;
                text(className(static_cast<Class>(i),mode==Mode::Anime ? Mode::Anime : Mode::Classes),x,y,26);
                wrapped(classDescription(static_cast<Class>(i),mode==Mode::Anime ? Mode::Anime : Mode::Classes),{x,y+45,540,110},23);
            }
        }
        if(button({224,765,200,45},"Previous",rulesPage>0)) --rulesPage;
        if(button({440,765,200,45},"Next",rulesPage<2)) ++rulesPage;
        if(button({1120,765,250,45},"Back to game",true,true)) rulesPage=-1;
        input=old;
    }
};
} // namespace

int main(int argc,char** argv) {
    try {
        const Options options=arguments(argc,argv);
        const bool capture=!options.screenshot.empty();
        SetTraceLogLevel(LOG_WARNING);
        // GLFW already makes Windows DPI-aware. Its extra HIGHDPI flag in
        // raylib 6.2 enlarges this window again at 125/150% desktop scaling.
        // Native pixels keep shapes, TrueType text and input in the same units.
        SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT|FLAG_WINDOW_HIDDEN);
        initializeDisplay(1920,1080);
        SetExitKey(KEY_NULL); // Escape opens our menu instead of closing instantly.
        SetTargetFPS(options.smoke || capture ? 0 : 60);
        {
            Application application(argv[0]);
            application.preset(options.mode);
            if(options.demo || options.smoke || (capture && options.screen=="match")) application.start(capture || options.demo);
            int frames=0;
            while(!WindowShouldClose()) {
                if(IsKeyPressed(KEY_F11)) toggleDisplayFullscreen();
                if(IsKeyPressed(KEY_F9)) fitWindowToDesktop();
                const float scale=min(GetScreenWidth()/Width,GetScreenHeight()/Height);
                const Vector2 offset{(GetScreenWidth()-Width*scale)/2,(GetScreenHeight()-Height*scale)/2};
                const float dpi=static_cast<float>(GetRenderWidth())/max(1,GetScreenWidth());
                application.update(options.smoke ? .1f : min(GetFrameTime(),.1f),options.smoke);
                BeginDrawing();
                application.draw(scale,offset,dpi,!options.smoke && !capture);
                EndDrawing();
                ++frames;
                if(capture && frames==3) {
                    if(!options.screenshot.parent_path().empty()) filesystem::create_directories(options.screenshot.parent_path());
                    TakeScreenshot(options.screenshot.string().c_str());
                }
                if((capture && frames>=4) || (options.smoke && frames>=600)) break;
            }
            if(options.smoke) cout<<"Ludo graphical smoke passed (600 frames; "<<application.game->turn<<" turns).\n";
        } // All font textures are destroyed while the graphics context exists.
        CloseWindow();
        return 0;
    } catch(const exception& error) {
        cerr<<"Ludo: "<<error.what()<<'\n';
        if(IsWindowReady()) CloseWindow();
        return 1;
    }
}

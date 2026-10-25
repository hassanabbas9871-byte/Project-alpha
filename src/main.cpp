// main() is the program's entry point. This file joins the four pieces of the
// game: keyboard/window input, the simulated world, drawing, and sound.
#include "Audio.hpp"
#include "Renderer.hpp"
#include "World.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// These shorthand names are confined to this implementation file. Keeping
// namespace declarations out of headers avoids surprising other programmers.
using namespace std;
using namespace skyline;

namespace {
// Command-line switches let us check the real executable automatically, while
// ordinary players can simply double-click it without supplying any switches.
struct Options {
    bool selfTest{}, smokeTest{}, mute{}, titleScreen{};
    filesystem::path screenshot;
};

Options readOptions(int argc, char* argv[]) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const string argument = argv[i];
        if (argument == "--self-test") options.selfTest = true;
        else if (argument == "--smoke-test") options.smokeTest = true;
        else if (argument == "--mute") options.mute = true;
        else if (argument == "--title-screen") options.titleScreen = true;
        else if (argument == "--screenshot" && i + 1 < argc) options.screenshot = argv[++i];
        else if (argument == "--help") {
            cout << "Afterlight - a C++23 / SFML rooftop parkour game\n"
                 << "  --self-test          Test physics without a display\n"
                 << "  --smoke-test         Run 900 scripted frames, silently\n"
                 << "  --screenshot PATH    Save a gameplay image and exit\n"
                 << "  --title-screen       Capture the title instead of gameplay\n"
                 << "  --mute               Start without opening an audio device\n";
            // This exception is handled as a normal, successful help request.
            throw string("help");
        } else throw runtime_error("Unknown or incomplete option: " + argument + ". Try --help.");
    }
    return options;
}

// Letterboxing preserves the artwork's proportions when a window is resized.
// A viewport uses fractions (0..1) of the physical window, not pixel positions.
void resizeCanvas(sf::RenderWindow& window, unsigned width, unsigned height) {
    if (width == 0 || height == 0) return; // Minimized windows can have no area.
    const float ratio = static_cast<float>(width) / static_cast<float>(height);
    constexpr float desired = ViewWidth / ViewHeight;
    sf::View view(sf::FloatRect(0, 0, ViewWidth, ViewHeight));
    if (ratio > desired) {
        const float fraction = desired / ratio;
        view.setViewport({(1 - fraction) / 2, 0, fraction, 1});
    } else {
        const float fraction = ratio / desired;
        view.setViewport({0, (1 - fraction) / 2, 1, fraction});
    }
    window.setView(view);
}

// Read held buttons each simulation tick. A press event is handled separately
// so a quick tap is remembered even if rendering happens between physics ticks.
Input heldInput() {
    Input input;
    const bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::A)
                   || sf::Keyboard::isKeyPressed(sf::Keyboard::Left);
    const bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::D)
                    || sf::Keyboard::isKeyPressed(sf::Keyboard::Right);
    input.horizontal = static_cast<float>(right) - static_cast<float>(left);
    input.jumpHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Space)
                  || sf::Keyboard::isKeyPressed(sf::Keyboard::W)
                  || sf::Keyboard::isKeyPressed(sf::Keyboard::Up);
    return input;
}

// React to state changes after an update: simulation stays independent of sound
// hardware, which is why the same World can also run headless physics tests.
void simulate(World& world, Audio& audio, const Input& input, float dt) {
    const auto previous = world.player;
    const int oldDeaths = world.deaths;
    const int oldCollected = world.collected;
    const auto activeFlags = [&world] {
        return count_if(world.checkpoints.begin(), world.checkpoints.end(),
                        [](const Checkpoint& flag) { return flag.active; });
    };
    const auto oldFlags = activeFlags();
    const bool wasFinished = world.finished;
    world.update(dt, input);
    if (world.deaths > oldDeaths) audio.die();
    else if (world.player.velocity.y < -400 && previous.velocity.y > -400) audio.jump();
    if (world.player.dashTime > previous.dashTime) audio.dash();
    if (world.collected > oldCollected) audio.collect();
    if (activeFlags() > oldFlags) audio.checkpoint();
    if (!wasFinished && world.finished) audio.finish();
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = readOptions(argc, argv);
        // Return before constructing graphics or audio: this test works even
        // on a machine with no monitor, OpenGL context, or speakers.
        if (options.selfTest) return runSelfTests() ? 0 : 1;

        const bool automated = options.smokeTest || !options.screenshot.empty();
        sf::ContextSettings settings;
        settings.antialiasingLevel = 4; // Smooth diagonal edges on supported GPUs.
        sf::RenderWindow window(sf::VideoMode(1280, 720), "AFTERLIGHT / Rooftop Runner",
                                sf::Style::Default, settings);
        window.setKeyRepeatEnabled(false); // One jump per press, not per key-repeat.
        window.setVerticalSyncEnabled(!automated);
        if (!automated) window.setFramerateLimit(144); // Avoid an uncapped busy loop.
        resizeCanvas(window, window.getSize().x, window.getSize().y);

        const filesystem::path executable = filesystem::absolute(argv[0]);
        Renderer renderer(executable.parent_path());
        Audio audio(!options.mute && !automated);
        World world;
        UiState ui;
        ui.muted = audio.muted();
        if (automated && !options.titleScreen) ui.screen = Screen::Playing;

        // A fixed step makes collision and jump timing independent of FPS.
        // The accumulator holds real time that has not been simulated yet.
        constexpr float FixedStep = 1.f / 120.f;
        if (!options.screenshot.empty()) {
            // Let the spawn animation land before taking a still photograph.
            // This is the same physics used in play, with no synthetic teleport.
            for (int step = 0; step < 120; ++step) world.update(FixedStep, {});
            world.elapsed = 0;
        }
        float accumulator = 0;
        sf::Clock frameClock;
        bool pendingJump = false, pendingDash = false, pendingRetry = false;
        int frames = 0;
        float furthest = 0;

        while (window.isOpen()) {
            // Limit long stalls (dragging a window, a breakpoint) so the game
            // does not try to catch up several seconds of physics at once.
            const float frameTime = automated ? FixedStep : min(frameClock.restart().asSeconds(), .1f);
            ui.visualTime += frameTime;
            ui.fps += ((1.f / max(frameTime, .00001f)) - ui.fps) * .08f;

            sf::Event event{};
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) window.close();
                if (event.type == sf::Event::Resized) resizeCanvas(window, event.size.width, event.size.height);
                if (event.type == sf::Event::LostFocus && ui.screen == Screen::Playing && !automated)
                    ui.screen = Screen::Paused;

                // Clicking the visible start/resume button is also supported.
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    const auto click = window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                    if (ui.screen == Screen::Title && sf::FloatRect(54, 470, 250, 57).contains(click)) {
                        world.reset();
                        ui.screen = Screen::Playing;
                    } else if (sf::FloatRect(414, 461, 452, 53).contains(click)) {
                        if (ui.screen == Screen::Paused) ui.screen = Screen::Playing;
                        else if (ui.screen == Screen::Finished) { world.reset(); ui.screen = Screen::Playing; }
                    }
                }
                if (event.type != sf::Event::KeyPressed) continue;
                const auto key = event.key.code;
                if (key == sf::Keyboard::M) { audio.setMuted(!audio.muted()); ui.muted = audio.muted(); }
                if (key == sf::Keyboard::F3) ui.showStats = !ui.showStats;
                if (key == sf::Keyboard::Escape) {
                    if (ui.screen == Screen::Playing) ui.screen = Screen::Paused;
                    else if (ui.screen == Screen::Paused) ui.screen = Screen::Playing;
                    else if (ui.screen == Screen::Title) window.close();
                }
                if (key == sf::Keyboard::Enter && ui.screen != Screen::Playing) {
                    world.reset();
                    ui.screen = Screen::Playing;
                }
                if (ui.screen == Screen::Playing) {
                    if (key == sf::Keyboard::Space || key == sf::Keyboard::W || key == sf::Keyboard::Up) pendingJump = true;
                    if (key == sf::Keyboard::LShift || key == sf::Keyboard::RShift || key == sf::Keyboard::X) pendingDash = true;
                    if (key == sf::Keyboard::R) pendingRetry = true;
                }
            }
            if (!window.isOpen()) break;

            if (ui.screen == Screen::Playing) {
                accumulator += frameTime;
                while (accumulator >= FixedStep) {
                    Input input = automated ? Input{} : heldInput();
                    if (automated) {
                        // Real input goes through real collision code. This is
                        // a rendering smoke test, not a guaranteed perfect run.
                        input.horizontal = 1;
                        input.jumpPressed = frames % 100 == 30;
                        input.jumpHeld = frames % 100 >= 30 && frames % 100 < 78;
                        input.dashPressed = frames % 150 == 72;
                    } else {
                        input.jumpPressed = pendingJump;
                        input.dashPressed = pendingDash;
                        input.restartPressed = pendingRetry;
                    }
                    simulate(world, audio, input, FixedStep);
                    pendingJump = pendingDash = pendingRetry = false;
                    accumulator -= FixedStep;
                    furthest = max(furthest, world.player.position.x);
                    if (world.finished) {
                        ui.screen = Screen::Finished;
                        if (ui.bestTime == 0 || world.elapsed < ui.bestTime) ui.bestTime = world.elapsed;
                        accumulator = 0;
                        break;
                    }
                }
            } else {
                accumulator = 0;
                pendingJump = pendingDash = pendingRetry = false;
                // The finish freezes the runner and timer, but World still
                // knows how to fade and move the celebratory spark particles.
                if (ui.screen == Screen::Finished) world.update(frameTime, {});
            }
            audio.update(frameTime, ui.screen == Screen::Playing);
            window.clear(sf::Color(10, 15, 26));
            renderer.draw(window, world, ui);
            window.display();
            ++frames;

            if (!options.screenshot.empty()) {
                // Reading the actual window pixels tests font loading, drawing,
                // OpenGL, and the image encoder rather than just an open port.
                sf::Texture capture;
                if (!capture.create(window.getSize().x, window.getSize().y))
                    throw runtime_error("Could not allocate a screenshot texture.");
                capture.update(window);
                if (!capture.copyToImage().saveToFile(options.screenshot.string()))
                    throw runtime_error("Could not write screenshot: " + options.screenshot.string());
                cout << "Saved " << options.screenshot << " (1280 x 720).\n";
                window.close();
            } else if (options.smokeTest && frames >= 900) {
                if (furthest < 450 || !isfinite(world.player.position.x) || !isfinite(world.player.position.y))
                    throw runtime_error("Smoke test failed: scripted movement did not advance safely.");
                cout << "Rendering smoke test passed: " << frames << " frames; reached x=" << furthest
                     << "; " << world.deaths << " respawns.\n";
                window.close();
            }
        }
        return 0;
    } catch (const string&) {
        return 0; // --help is a successful request, not a game failure.
    } catch (const exception& error) {
        cerr << "Afterlight: " << error.what() << '\n';
        return 1; // A nonzero result lets CTest or a shell detect a real failure.
    }
}

// Windows supplies an exact usable desktop rectangle that excludes the taskbar.
// Its C API also defines names used by raylib, so hide those conflicting names
// while reading windows.h and remove text/image macro aliases afterward.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define CloseWindow WindowsCloseWindow
#define ShowCursor WindowsShowCursor
#define Rectangle WindowsRectangle
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound
#endif

#include "Display.hpp"
#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace std;

namespace ludo {
namespace {
// Leave a little breathing room outside the decorated window. These are actual
// desktop pixels, rather than the board game's virtual drawing coordinates.
constexpr int Margin = 12;

struct Desktop {
    int x{}, y{}, width{}, height{};
    int leftFrame{8}, topFrame{32}, rightFrame{8}, bottomFrame{8};
};

struct WindowedState {
    int width{}, height{};
    Vector2 position{};
    bool recorded{};
} normalWindow;

Desktop desktop() {
    const int monitor = max(0, GetCurrentMonitor());
    const Vector2 position = GetMonitorPosition(monitor);
    // raylib's portable API reports the whole monitor, not its work area.
    // Reserve a small dock/taskbar region on platforms without the Win32 API.
    Desktop result;
    result.x = static_cast<int>(position.x);
    result.y = static_cast<int>(position.y);
    result.width = max(1, GetMonitorWidth(monitor));
    result.height = max(1, GetMonitorHeight(monitor) - 50);

#if defined(_WIN32)
    const HWND window = static_cast<HWND>(GetWindowHandle());
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
        const RECT area = monitorInfo.rcWork;
        result.x = area.left;
        result.y = area.top;
        result.width = max(1L, area.right - area.left);
        result.height = max(1L, area.bottom - area.top);
    }

    RECT outer{}, client{};
    POINT clientOrigin{};
    if (GetWindowRect(window, &outer) && GetClientRect(window, &client) &&
        ClientToScreen(window, &clientOrigin)) {
        // GLFW's SetWindowPosition positions the CLIENT area's top left. Its
        // title bar and border extend outside that position, so measure all
        // four sides instead of placing the title bar above the visible screen.
        result.leftFrame = max(0L, clientOrigin.x - outer.left);
        result.topFrame = max(0L, clientOrigin.y - outer.top);
        result.rightFrame = max(0L, outer.right - clientOrigin.x - client.right);
        result.bottomFrame = max(0L, outer.bottom - clientOrigin.y - client.bottom);
    }
#endif
    return result;
}

void fit(int requestedWidth, int requestedHeight, bool center) {
    const Desktop area = desktop();
    const int availableWidth = max(1, area.width - area.leftFrame - area.rightFrame - Margin * 2);
    const int availableHeight = max(1, area.height - area.topFrame - area.bottomFrame - Margin * 2);

    // A large fixed minimum was the reason small laptops could not show the
    // complete game. Never set a minimum larger than the available desktop.
    const int minimumWidth = min(960, availableWidth);
    const int minimumHeight = min(540, availableHeight);
    SetWindowMinSize(minimumWidth, minimumHeight);

    requestedWidth = max(1, requestedWidth);
    requestedHeight = max(1, requestedHeight);
    const double scale = min({1.0, static_cast<double>(availableWidth) / requestedWidth,
                             static_cast<double>(availableHeight) / requestedHeight});
    const int width = clamp(static_cast<int>(floor(requestedWidth * scale)), minimumWidth, availableWidth);
    const int height = clamp(static_cast<int>(floor(requestedHeight * scale)), minimumHeight, availableHeight);
    SetWindowSize(width, height);

    const int minimumX = area.x + Margin + area.leftFrame;
    const int minimumY = area.y + Margin + area.topFrame;
    const int maximumX = max(minimumX, area.x + area.width - Margin - area.rightFrame - width);
    const int maximumY = max(minimumY, area.y + area.height - Margin - area.bottomFrame - height);
    const Vector2 previous = GetWindowPosition();
    const int x = center ? minimumX + (maximumX - minimumX) / 2 :
                          clamp(static_cast<int>(previous.x), minimumX, maximumX);
    const int y = center ? minimumY + (maximumY - minimumY) / 2 :
                          clamp(static_cast<int>(previous.y), minimumY, maximumY);
    SetWindowPosition(x, y);
}
} // namespace

void initializeDisplay(int preferredWidth, int preferredHeight) {
    // The small hidden window lets Windows report the real monitor and border
    // sizes before any oversized window can appear to the player.
    InitWindow(640, 360, "LUDO / ARCANA");
    if (!IsWindowReady()) throw runtime_error("The game could not open a window.");
    fit(preferredWidth, preferredHeight, true);
    ClearWindowState(FLAG_WINDOW_HIDDEN);
}

void fitWindowToDesktop() {
    if (!IsWindowReady()) return;
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) toggleDisplayFullscreen();
    if (IsWindowMaximized()) RestoreWindow();
    fit(GetScreenWidth(), GetScreenHeight(), true);
}

void toggleDisplayFullscreen() {
    if (!IsWindowReady()) return;
    const bool leaving = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (!leaving) {
        normalWindow.width = GetScreenWidth();
        normalWindow.height = GetScreenHeight();
        normalWindow.position = GetWindowPosition();
        normalWindow.recorded = true;
    }

    ToggleBorderlessWindowed();
    if (leaving && normalWindow.recorded) {
        SetWindowPosition(static_cast<int>(normalWindow.position.x), static_cast<int>(normalWindow.position.y));
        fit(normalWindow.width, normalWindow.height, false);
    }
}

} // namespace ludo

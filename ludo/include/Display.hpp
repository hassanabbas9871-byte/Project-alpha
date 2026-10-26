#pragma once

#include <raylib.h>

// Window management is separate from the board layout. A large board design
// should be allowed to fit a smaller laptop instead of forcing a large window.
namespace ludo {

// Width, height and mouse position all describe the actual drawable pixels.
// On Windows these come directly from the same native client rectangle.
struct Surface {
    int width{}, height{};
    Vector2 mouse{};
};
Surface displaySurface();

// Replace inherited framework projection/DPI matrices with one pixel viewport.
// Call after BeginDrawing(), before drawing either shapes or text.
void beginNativeFrame(const Surface& surface);

// The framework screenshot helper may use cached window sizes; capture the
// actual viewport instead, which also supports display-regression checks.
bool captureNativeFrame(const Surface& surface, const char* filename);

// Call SetConfigFlags(...) first, including FLAG_WINDOW_HIDDEN. This creates
// the window out of sight, fits it to the usable desktop, then makes it visible.
void initializeDisplay(int preferredWidth = 1920, int preferredHeight = 1080);

// Recover a window that was moved off screen or enlarged beyond this monitor.
// The taskbar and title bar are included in the fitting calculation on Windows.
void fitWindowToDesktop();

// Remember the normal window's size and position, enter borderless fullscreen,
// then safely restore the normal window when this is called a second time.
void toggleDisplayFullscreen();

} // namespace ludo

#pragma once

// Window management is separate from the board layout. A large board design
// should be allowed to fit a smaller laptop instead of forcing a large window.
namespace ludo {

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

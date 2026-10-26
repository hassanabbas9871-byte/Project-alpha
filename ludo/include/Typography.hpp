#pragma once

// Text and board shapes use the SAME camera and logical coordinates. A separate
// font atlas for each displayed pixel size keeps labels sharp without putting
// them in a second drawing pass with a potentially different DPI transform.
#include <raylib.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ludo {

class Typography {
public:
    explicit Typography(std::filesystem::path fontPath);
    ~Typography();
    Typography(const Typography&) = delete;
    Typography& operator=(const Typography&) = delete;

    // Begin one frame with the same scale/offset used by the geometry camera.
    // Positions supplied to add() and widths supplied to lines() stay logical.
    // scale*dpi is the camera's total zoom in framebuffer pixels. The offset is
    // accepted as part of the shared API; only the camera applies its translation.
    void begin(float scale, Vector2 offset, float dpi = 1.f);
    float measure(const std::string& value, float requestedSize) const;
    float lineHeight(float requestedSize) const;
    float fontSize(float requestedSize) const;
    std::vector<std::string> lines(const std::string& value, float logicalWidth,
                                   float requestedSize) const;
    void add(const std::string& value, float x, float y, float size, Color color) const;

    // Call flush() BEFORE EndMode2D(), while the board's camera is still active.
    // clear() discards obscured background text, for example before a modal.
    void flush() const;
    void clear() const;

private:
    struct Run {
        std::string value;
        Vector2 position; // Logical board-space coordinates, just like shapes.
        int pixels{};
        Color color{};
    };
    std::filesystem::path fontPath_;
    mutable std::map<int, Font> fonts_;
    mutable std::vector<Run> runs_;
    float scale_{1.f};
    float dpi_{1.f};

    int pixelSize(float requestedSize) const;
    const Font& font(int pixels) const;
    void unloadAtlases();
};

} // namespace ludo

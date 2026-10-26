#pragma once

// Text is rendered in real window pixels after the geometry camera has ended.
// A separate atlas for each displayed size avoids shrinking one huge bitmap
// font and then enlarging it again with the rest of the interface.
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
    // dpi is framebuffer pixels per window coordinate (Windows display scaling).
    void begin(float scale, Vector2 offset, float dpi = 1.f);
    float measure(const std::string& value, float requestedSize) const;
    float lineHeight(float requestedSize) const;
    float fontSize(float requestedSize) const;
    std::vector<std::string> lines(const std::string& value, float logicalWidth,
                                   float requestedSize) const;
    void add(const std::string& value, float x, float y, float size, Color color) const;

    // Call flush() after EndMode2D(). clear() discards obscured background text,
    // for example before queuing the text in a modal pause dialog.
    void flush() const;
    void clear() const;

private:
    struct Run {
        std::string value;
        Vector2 position;
        int pixels{};
        Color color{};
    };
    std::filesystem::path fontPath_;
    mutable std::map<int, Font> fonts_;
    mutable std::vector<Run> runs_;
    float scale_{1.f};
    float dpi_{1.f};
    Vector2 offset_{};

    int pixelSize(float requestedSize) const;
    const Font& font(int pixels) const;
    void unloadAtlases();
};

} // namespace ludo

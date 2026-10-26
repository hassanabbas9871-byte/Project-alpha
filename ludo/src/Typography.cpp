#include "Typography.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace std;

namespace ludo {
namespace {
constexpr float MinimumSize = 24.f; // Landscape panels leave room for larger labels.
constexpr float PixelSpacing = 1.f;

// A UTF-8 character may occupy several bytes. Long words must be broken at a
// character boundary, rather than leaving half a character on either line.
size_t characterEnd(const string& text, size_t start) {
    const auto byte = static_cast<unsigned char>(text[start]);
    size_t bytes = 1;
    if ((byte & 0xE0u) == 0xC0u) bytes = 2;
    else if ((byte & 0xF0u) == 0xE0u) bytes = 3;
    else if ((byte & 0xF8u) == 0xF0u) bytes = 4;
    return min(start + bytes, text.size());
}
} // namespace

Typography::Typography(filesystem::path fontPath) : fontPath_(std::move(fontPath)) {
    error_code error;
    if (!filesystem::is_regular_file(fontPath_, error))
        throw runtime_error("Ludo's readable font is missing. Keep assets/DejaVuSans.ttf beside the executable, or rebuild to copy the bundled assets.");
}

Typography::~Typography() {
    unloadAtlases();
}

void Typography::unloadAtlases() {
    // The default font belongs to raylib. Only atlases we actually loaded may
    // be unloaded here, and this object must die before CloseWindow().
    const unsigned defaultTexture = GetFontDefault().texture.id;
    for (const auto& [pixels, loaded] : fonts_) {
        (void)pixels;
        if (loaded.texture.id != 0 && loaded.texture.id != defaultTexture) UnloadFont(loaded);
    }
    fonts_.clear();
}

void Typography::begin(float scale, Vector2 offset, float dpi) {
    // A long window-resize gesture can request many different glyph sizes.
    // Retire those old atlases only between frames, after all previous text
    // has been drawn, so the cache cannot grow for the lifetime of the game.
    runs_.clear();
    if (fonts_.size() > 48) unloadAtlases();
    scale_ = isfinite(scale) && scale > 0 ? scale : 1.f;
    dpi_ = isfinite(dpi) && dpi > 0 ? dpi : 1.f;
    offset_ = offset;
}

int Typography::pixelSize(float requestedSize) const {
    const float logical = isfinite(requestedSize) ? max(MinimumSize, requestedSize) : MinimumSize;
    // Integer glyph sizes and pixel-snapped drawing prevent a second soft,
    // fractional resampling step. The upper bound also avoids absurd atlases
    // if a caller accidentally supplies a wildly incorrect viewport scale.
    // Smaller desktop windows still get readable labels. Measurement uses this
    // same size, so wrapping and button centering include the actual glyphs.
    return static_cast<int>(clamp(round(logical * scale_ * dpi_), 18.f, 512.f));
}

const Font& Typography::font(int pixels) const {
    if (const auto found = fonts_.find(pixels); found != fonts_.end()) return found->second;

    const Font fallback = GetFontDefault();
    Font loaded = LoadFontEx(fontPath_.string().c_str(), pixels, nullptr, 0);
    if (loaded.texture.id == 0 || loaded.texture.id == fallback.texture.id) {
        throw runtime_error("Ludo could not load assets/DejaVuSans.ttf. Rebuild to copy a valid bundled font beside the executable.");
    }
    // At native size each source texel maps directly to one display pixel.
    // Point sampling retains the TrueType rasterizer's antialiased edge pixels
    // without blurring them with adjacent transparent atlas pixels.
    SetTextureFilter(loaded.texture, TEXTURE_FILTER_POINT);
    return fonts_.emplace(pixels, loaded).first->second;
}

float Typography::fontSize(float requestedSize) const {
    return static_cast<float>(pixelSize(requestedSize)) / (scale_ * dpi_);
}

float Typography::measure(const string& value, float requestedSize) const {
    const int pixels = pixelSize(requestedSize);
    return MeasureTextEx(font(pixels), value.c_str(), static_cast<float>(pixels), PixelSpacing).x / (scale_ * dpi_);
}

float Typography::lineHeight(float requestedSize) const {
    return ceil(static_cast<float>(pixelSize(requestedSize)) * 1.35f) / (scale_ * dpi_);
}

vector<string> Typography::lines(const string& value, float logicalWidth, float requestedSize) const {
    vector<string> result;
    if (logicalWidth <= 0 || !isfinite(logicalWidth)) return result;

    size_t paragraphStart = 0;
    do {
        const size_t newline = value.find('\n', paragraphStart);
        const string paragraph = value.substr(paragraphStart, newline == string::npos ? string::npos : newline - paragraphStart);
        istringstream words(paragraph);
        string current, word;
        bool hadWords = false;
        while (words >> word) {
            hadWords = true;
            const string candidate = current.empty() ? word : current + " " + word;
            if (measure(candidate, requestedSize) <= logicalWidth) {
                current = candidate;
                continue;
            }
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }

            // Ordinary words stay whole. A single overlong identifier or a
            // narrow panel is split using measured glyphs, never a guessed
            // number of characters. Every source character is retained.
            if (measure(word, requestedSize) <= logicalWidth) current = word;
            else {
                for (size_t start = 0; start < word.size();) {
                    const size_t end = characterEnd(word, start);
                    const string character = word.substr(start, end - start);
                    if (!current.empty() && measure(current + character, requestedSize) > logicalWidth) {
                        result.push_back(current);
                        current.clear();
                    }
                    current += character;
                    start = end;
                }
            }
        }
        if (!current.empty()) result.push_back(current);
        else if (!hadWords) result.emplace_back(); // Preserve deliberate blank paragraphs.
        if (newline == string::npos) break;
        paragraphStart = newline + 1;
    } while (paragraphStart <= value.size());
    return result;
}

void Typography::add(const string& value, float x, float y, float size, Color color) const {
    if (value.empty()) return;
    runs_.push_back({value, {round((offset_.x + x * scale_) * dpi_) / dpi_,
                             round((offset_.y + y * scale_) * dpi_) / dpi_}, pixelSize(size), color});
}

void Typography::flush() const {
    // No camera or render texture should surround this pass. The atlas is
    // sampled at its original size directly into the actual window framebuffer.
    for (const auto& run : runs_) {
        DrawTextEx(font(run.pixels), run.value.c_str(), run.position,
                   static_cast<float>(run.pixels) / dpi_, PixelSpacing / dpi_, run.color);
    }
    runs_.clear();
}

void Typography::clear() const { runs_.clear(); }

} // namespace ludo

#pragma once

// The rest of the game sends simple messages such as "attack happened".
// It does not need to understand sound buffers or include raylib's headers.
#include <memory>

namespace veil {
class Audio {
public:
    // A false argument is truly silent: it does not even open an audio device.
    // Automated screenshots and machines without speakers can use this path.
    explicit Audio(bool enabled = true);
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void setMuted(bool muted);
    bool muted() const;
    void update(float dt, bool battle);
    void click();
    void attack();
    void hurt();
    void victory();
    void defeat();

private:
    // unique_ptr owns one hidden implementation and deletes it automatically.
    // Its definition lives in Audio.cpp, keeping the graphics API out of here.
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool muted_{true};
};
} // namespace veil

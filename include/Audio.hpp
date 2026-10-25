#pragma once

// A header is a promise about what a class can do. The implementation of these
// functions lives in Audio.cpp, so the rest of the game need not know how a
// synthesizer works (or even include SFML's audio headers).
#include <memory>

namespace skyline {

// All music and sound effects are created mathematically at startup. There are
// no audio files to download, and no recordings with additional licenses.
class Audio {
public:
    // Passing false creates a silent controller WITHOUT opening an audio device.
    // This is useful on servers and when taking automated screenshots.
    explicit Audio(bool enabled = true);
    ~Audio();

    // An audio controller owns its buffers. Copying it could leave sounds
    // referring to somebody else's buffers, so copying is deliberately disabled.
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // Muting stops current notes. Unmuting lazily creates audio resources if this
    // controller was originally constructed in silent mode.
    void setMuted(bool muted);
    [[nodiscard]] bool muted() const;

    // Each named function is a tiny, readable message from gameplay to audio.
    void jump();
    void dash();
    void collect();
    void die();
    void checkpoint();
    void finish();

    // dt means "delta time": the number of seconds since the previous update.
    // Music advances during gameplay and rests on title / pause / finish screens.
    void update(float dt, bool playing);

private:
    // This is the "implementation" pattern: a unique_ptr owns one private
    // object, and automatically deletes it when Audio is destroyed.
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool muted_ = true;
};

} // namespace skyline

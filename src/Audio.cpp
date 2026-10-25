#include "Audio.hpp"

#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

// This beginner-friendly shortcut lets us write vector instead of std::vector.
// Keep it inside .cpp files: putting it in a header would affect every file that
// includes that header and can cause confusing name clashes in larger projects.
using namespace std;

namespace skyline {
namespace {

// Digital sound is a list of numbers describing air pressure over time.
// 44,100 samples per second is the familiar audio-CD sampling rate.
constexpr unsigned SampleRate = 44100;
constexpr double Tau = 2.0 * numbers::pi;

// Named indices make the bank easier to understand than magic numbers.
enum SoundId : size_t {
    JumpSound,
    DashSound,
    CollectSound,
    DeathSound,
    CheckpointSound,
    FinishSound,
    FirstMusicNote,
    SoundCount = FirstMusicNote + 8
};

// Build one little instrument. A tone can glide between pitches, contain a
// chord (several simultaneous notes), and mix in a little filtered-sounding
// noise for the dash. Everything is deterministic, so every run sounds alike.
vector<int16_t> synthesize(const vector<double>& frequencies, double seconds,
                            double endPitch = 1.0, double noiseAmount = 0.0) {
    const size_t sampleCount = static_cast<size_t>(seconds * SampleRate);
    vector<int16_t> samples(sampleCount);
    vector<double> phases(frequencies.size(), 0.0);
    unsigned randomState = 0x51A7u;

    for (size_t index = 0; index < sampleCount; ++index) {
        // Progress runs from zero to one as the sound plays.
        const double time = static_cast<double>(index) / SampleRate;
        const double progress = time / seconds;
        const double pitch = 1.0 + (endPitch - 1.0) * progress;

        // Fade in for 8 ms, then fade out smoothly. An abrupt change from silence
        // to a loud sample would make an unpleasant click in the speakers.
        const double attack = min(time / 0.008, 1.0);
        const double release = pow(max(0.0, 1.0 - progress), 1.8);
        const double envelope = attack * release;
        double wave = 0.0;

        for (size_t note = 0; note < frequencies.size(); ++note) {
            // Accumulating the phase avoids discontinuities during a pitch glide.
            phases[note] += Tau * frequencies[note] * pitch / SampleRate;
            // A soft fundamental plus a quiet overtone gives a warm synth tone.
            wave += sin(phases[note]) + 0.15 * sin(phases[note] * 2.0);
        }
        wave /= static_cast<double>(max(size_t{1}, frequencies.size()));

        // A small linear congruential generator creates repeatable white noise.
        randomState = randomState * 1664525u + 1013904223u;
        const double noise = static_cast<double>((randomState >> 8) & 0xffffu)
                                 / 32767.5 - 1.0;
        wave = wave * (1.0 - noiseAmount) + noise * noiseAmount;

        // SFML expects signed 16-bit samples: values between -32768 and 32767.
        // A generous margin prevents clipping when several sounds overlap.
        samples[index] = static_cast<int16_t>(clamp(wave * envelope * 13500.0,
                                                    -32767.0, 32767.0));
    }
    return samples;
}

} // namespace

struct Audio::Impl {
    // Declaration order matters! C++ destroys members in reverse order.
    // Sounds therefore disappear BEFORE the buffers they refer to.
    array<sf::SoundBuffer, SoundCount> buffers;
    // SFML 3 requires each Sound to start with a buffer. optional lets us
    // leave each voice empty until the generated buffers are ready.
    array<optional<sf::Sound>, 16> voices;
    size_t nextVoice = 0;
    size_t nextNote = 0;
    float musicCountdown = 0.0f;
    bool valid = true;

    Impl() {
        // This helper creates a sample list, then lets SFML copy it into a buffer.
        // The temporary vector can safely be destroyed after loadFromSamples.
        const auto load = [this](size_t id, const vector<double>& notes,
                                 double duration, double glide = 1.0,
                                 double noise = 0.0) {
            const auto samples = synthesize(notes, duration, glide, noise);
            if (!buffers[id].loadFromSamples(samples.data(), samples.size(),
                                             1, SampleRate, {sf::SoundChannel::Mono})) {
                valid = false;
            }
        };

        // Upward glides sound energetic; a falling glide clearly signals a fall.
        load(JumpSound, {280.0}, 0.15, 1.8);
        load(DashSound, {145.0}, 0.19, 0.55, 0.62);
        load(CollectSound, {659.25, 987.77}, 0.30, 1.04);
        load(DeathSound, {170.0, 175.0}, 0.38, 0.20, 0.12);
        load(CheckpointSound, {329.63, 493.88, 659.25}, 0.65);
        load(FinishSound, {329.63, 415.30, 493.88, 659.25}, 1.6);

        // A mellow E-minor arpeggio: one note at a time suggests a chord.
        constexpr array<double, 8> notes = {
            164.81, 246.94, 329.63, 493.88, 146.83, 220.0, 293.66, 440.0
        };
        for (size_t index = 0; index < notes.size(); ++index) {
            load(FirstMusicNote + index, {notes[index]}, 0.72);
        }
        // emplace constructs each Sound directly inside its optional slot.
        if (valid) {
            for (auto& voice : voices) voice.emplace(buffers[JumpSound]);
        }
    }

    void stop() {
        for (auto& voice : voices) {
            if (voice) voice->stop();
        }
    }

    void play(size_t soundId, float volume) {
        if (!valid) {
            return;
        }

        // A pool lets a pickup ring out while the player jumps. Prefer an idle
        // voice, and recycle the oldest slot only if all 16 are occupied.
        size_t chosen = nextVoice;
        for (size_t offset = 0; offset < voices.size(); ++offset) {
            const size_t candidate = (nextVoice + offset) % voices.size();
            if (voices[candidate]->getStatus() == sf::SoundSource::Status::Stopped) {
                chosen = candidate;
                break;
            }
        }
        auto& voice = *voices[chosen];
        voice.stop();
        voice.setBuffer(buffers[soundId]);
        voice.setVolume(volume); // SFML volume ranges from 0 (silent) to 100.
        voice.play();
        nextVoice = (chosen + 1) % voices.size();
    }
};

Audio::Audio(bool enabled) {
    if (enabled) {
        setMuted(false);
    }
}

// The destructor is defined here, where the compiler knows the full Impl type.
Audio::~Audio() = default;

void Audio::setMuted(bool muted) {
    muted_ = muted;
    if (muted) {
        if (impl_) {
            impl_->stop();
        }
        return;
    }

    // Silent test runs never enter this branch, so they need no sound device.
    if (!impl_) {
        impl_ = make_unique<Impl>();
    }
    muted_ = !impl_->valid;
}

bool Audio::muted() const {
    return muted_;
}

// These short wrappers keep sound selection and volume out of the game loop.
void Audio::jump() {
    if (!muted_) impl_->play(JumpSound, 28.0f);
}

void Audio::dash() {
    if (!muted_) impl_->play(DashSound, 32.0f);
}

void Audio::collect() {
    if (!muted_) impl_->play(CollectSound, 34.0f);
}

void Audio::die() {
    if (!muted_) impl_->play(DeathSound, 36.0f);
}

void Audio::checkpoint() {
    if (!muted_) impl_->play(CheckpointSound, 32.0f);
}

void Audio::finish() {
    if (!muted_) impl_->play(FinishSound, 37.0f);
}

void Audio::update(float dt, bool playing) {
    if (muted_ || !impl_) {
        return;
    }
    if (!playing) {
        // Begin gently when the player returns, without "catching up" old notes.
        impl_->musicCountdown = 0.15f;
        return;
    }
    impl_->musicCountdown -= clamp(dt, 0.0f, 0.1f);
    if (impl_->musicCountdown <= 0.0f) {
        impl_->play(FirstMusicNote + impl_->nextNote, 11.0f);
        impl_->nextNote = (impl_->nextNote + 1) % 8;
        impl_->musicCountdown += 0.32f;
    }
}

} // namespace skyline

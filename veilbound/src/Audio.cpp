#include "Audio.hpp"
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

using namespace std;

namespace veil {
namespace {
// A digital recording is a list of amplitudes: each signed 16-bit number is
// one instant of air pressure. These are all newly synthesized sounds, so no
// audio recordings or copyrighted soundtrack files need to be distributed.
constexpr unsigned SampleRate = 44100;
constexpr double Tau = numbers::pi_v<double> * 2.0;
enum SoundId : size_t { Click, Attack, Hurt, Victory, Defeat, BattleCue, Ambient, SoundCount };
constexpr size_t EffectCount = Ambient;

vector<int16_t> effect(SoundId id) {
    constexpr array<double, EffectCount> duration{.09, .22, .34, 1.05, .85, .60};
    constexpr array<double, EffectCount> frequency{540, 165, 180, 329.63, 146.83, 246.94};
    const double seconds = duration[id];
    vector<int16_t> samples(static_cast<size_t>(seconds * SampleRate));
    double phase = 0;
    unsigned random = 0xAB019u;
    for (size_t i = 0; i < samples.size(); ++i) {
        const double time = static_cast<double>(i) / SampleRate;
        const double progress = time / seconds;
        // A short fade-in avoids a click from jumping suddenly out of silence.
        const double envelope = min(1.0, time / .006) * pow(1.0 - progress, 2.0);
        const double glide = id == Hurt || id == Defeat ? 1.0 - progress * .75
                             : id == Attack ? 1.0 - progress * .5 : 1.0 + progress * .15;
        phase += Tau * frequency[id] * glide / SampleRate;
        double wave = sin(phase) + .12 * sin(phase * 2.0);
        if (id == Victory) wave = (wave + .6 * sin(phase * 1.25) + .6 * sin(phase * 1.5)) / 2.2;
        if (id == BattleCue) wave = (wave + .5 * sin(phase * 1.5)) / 1.5;
        random = random * 1664525u + 1013904223u;
        const double noise = static_cast<double>((random >> 8) & 0xffffu) / 32767.5 - 1.0;
        if (id == Attack) wave = wave * .35 + noise * .65;
        if (id == Hurt) wave = wave * .75 + noise * .25;
        samples[i] = static_cast<int16_t>(clamp(wave * envelope * 12500.0, -32767.0, 32767.0));
    }
    return samples;
}

vector<int16_t> ambience() {
    // A quiet twelve-second harmonic loop. Slowly changing E-minor-family
    // chords and scattered chimes suggest a distant mechanical music box.
    constexpr double seconds = 12.0;
    constexpr array<array<double, 3>, 4> chords{{
        {{82.4069, 123.4708, 164.8138}}, {{65.4064, 97.9989, 130.8128}},
        {{73.4162, 110.0, 146.8324}}, {{61.7354, 92.4986, 123.4708}}
    }};
    constexpr array<double, 8> chimes{329.6276, 493.8833, 391.9954, 293.6648,
                                     261.6256, 369.9944, 246.9417, 329.6276};
    vector<int16_t> samples(static_cast<size_t>(seconds * SampleRate));
    for (size_t i = 0; i < samples.size(); ++i) {
        const double time = static_cast<double>(i) / SampleRate;
        const size_t chord = min(size_t{3}, static_cast<size_t>(time / 3.0));
        const double fraction = fmod(time, 3.0) / 3.0;
        // Overlapping soft chord envelopes avoid abrupt changes at bar lines.
        const double blend = .5 - .5 * cos(fraction * numbers::pi_v<double>);
        double pad = 0;
        for (size_t voice = 0; voice < 3; ++voice) {
            pad += sin(Tau * chords[chord][voice] * time) * (1.0 - blend);
            pad += sin(Tau * chords[(chord + 1) % 4][voice] * time) * blend;
        }
        pad /= 3.0;
        const size_t note = min(size_t{7}, static_cast<size_t>(time / 1.5));
        const double noteTime = fmod(time, 1.5);
        const double bellEnvelope = min(1.0, noteTime / .01) * exp(-noteTime * 3.2);
        const double bell = (sin(Tau * chimes[note] * noteTime)
                            + .18 * sin(Tau * chimes[note] * 2.002 * noteTime)) * bellEnvelope;
        // Both ends fade all the way to silence before the next loop starts.
        const double edge = min({1.0, time / .25, (seconds - time) / .4});
        samples[i] = static_cast<int16_t>((pad * 2200.0 + bell * 3400.0) * edge);
    }
    return samples;
}
} // namespace

struct Audio::Impl {
    array<Sound, SoundCount> bank{};
    // Three aliases per effect let the same effect overlap without cutting
    // itself off. Aliases share sample data and must be unloaded BEFORE it.
    array<array<Sound, 3>, EffectCount> voices{};
    array<size_t, EffectCount> next{};
    bool ready{}, ownsDevice{}, wasBattle{};
    float musicVolume{.2f};

    Impl() {
        ownsDevice = !IsAudioDeviceReady();
        if (ownsDevice) InitAudioDevice();
        if (!IsAudioDeviceReady()) return; // A silent game remains playable.

        for (size_t id = 0; id < SoundCount; ++id) {
            const auto samples = id == Ambient ? ambience() : effect(static_cast<SoundId>(id));
            // LoadSoundFromWave copies these samples into its own buffer. The
            // temporary vector therefore remains alive for exactly long enough.
            Wave wave{};
            wave.frameCount = static_cast<unsigned>(samples.size());
            wave.sampleRate = SampleRate;
            wave.sampleSize = 16;
            wave.channels = 1;
            wave.data = const_cast<int16_t*>(samples.data());
            bank[id] = LoadSoundFromWave(wave);
            if (!IsSoundValid(bank[id])) return;
        }
        for (size_t id = 0; id < EffectCount; ++id) {
            for (auto& voice : voices[id]) {
                voice = LoadSoundAlias(bank[id]);
                if (!IsSoundValid(voice)) return;
            }
        }
        SetSoundVolume(bank[Ambient], musicVolume);
        ready = true;
    }

    ~Impl() {
        for (auto& group : voices)
            for (auto& voice : group)
                if (IsSoundValid(voice)) UnloadSoundAlias(voice);
        for (auto& sound : bank)
            if (IsSoundValid(sound)) UnloadSound(sound);
        if (ownsDevice && IsAudioDeviceReady()) CloseAudioDevice();
    }

    void stop() {
        if (!ready) return;
        for (auto& group : voices) for (auto& voice : group) StopSound(voice);
        StopSound(bank[Ambient]);
    }
    void play(SoundId id, float volume) {
        if (!ready) return;
        auto& voice = voices[id][next[id]];
        StopSound(voice);
        SetSoundVolume(voice, volume);
        PlaySound(voice);
        next[id] = (next[id] + 1) % voices[id].size();
    }
};

Audio::Audio(bool enabled) {
    if (enabled) setMuted(false);
}
Audio::~Audio() = default;

void Audio::setMuted(bool muted) {
    muted_ = muted;
    if (muted) {
        if (impl_) impl_->stop();
        return;
    }
    // Explicit unmute can retry device creation after a previous failure.
    // update() never repeatedly reopens a missing device by itself.
    if (!impl_ || !impl_->ready) impl_ = make_unique<Impl>();
    muted_ = !impl_->ready;
}
bool Audio::muted() const { return muted_; }

void Audio::update(float dt, bool battle) {
    if (muted_ || !impl_ || !impl_->ready) return;
    if (battle && !impl_->wasBattle) impl_->play(BattleCue, .14f);
    impl_->wasBattle = battle;
    // Exponential smoothing changes the music level gently during transitions.
    const float target = battle ? .3f : .2f;
    impl_->musicVolume += (target - impl_->musicVolume) * (1.f - exp(-clamp(dt, 0.f, .1f) * 3.f));
    SetSoundVolume(impl_->bank[Ambient], impl_->musicVolume);
    if (!IsSoundPlaying(impl_->bank[Ambient])) PlaySound(impl_->bank[Ambient]);
}
void Audio::click() { if (!muted_) impl_->play(Click, .16f); }
void Audio::attack() { if (!muted_) impl_->play(Attack, .24f); }
void Audio::hurt() { if (!muted_) impl_->play(Hurt, .25f); }
void Audio::victory() { if (!muted_) impl_->play(Victory, .22f); }
void Audio::defeat() { if (!muted_) impl_->play(Defeat, .24f); }
} // namespace veil

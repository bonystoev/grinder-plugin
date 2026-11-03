#pragma once

// Grinder — Saturation & Distortion DSP core.
// Framework-free, header-only, deterministic (seeded RNG) so it can be
// unit-tested headless without JUCE.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace grinder
{

//==============================================================================
// Small deterministic RNG (xorshift64*) — reproducible noise for tests.
class Rng
{
public:
    explicit Rng (uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept : state (seed | 1u) {}

    void setSeed (uint64_t seed) noexcept { state = seed | 1u; }

    // Uniform in [0, 1)
    float next01() noexcept
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        const uint64_t x = state * 2685821657736338717ull;
        return static_cast<float> (x >> 40) * (1.0f / 16777216.0f);
    }

    // Uniform in [-1, 1)
    float nextBipolar() noexcept { return next01() * 2.0f - 1.0f; }

private:
    uint64_t state;
};

//==============================================================================
// First-order low-pass (one-pole) used for envelopes and smoothing.
class OnePole
{
public:
    void prepare (double sampleRate, float cutoffHz) noexcept
    {
        const float wc = static_cast<float> (2.0 * pi * cutoffHz);
        coeff = std::exp (-wc / static_cast<float> (sampleRate));
    }

    inline float process (float x) noexcept
    {
        z = z * coeff + x * (1.0f - coeff);
        return z;
    }

    void reset (float value = 0.0f) noexcept { z = value; }
    float value() const noexcept { return z; }

private:
    static constexpr double pi = 3.14159265358979323846;
    float coeff = 0.0f;
    float z = 0.0f;
};

//==============================================================================
// Biquad (RBJ cookbook) — low/high cut, peaking, low/high shelf.
class Biquad
{
public:
    enum class Type { lowPass, highPass, peaking, lowShelf, highShelf };

    struct Coeffs
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    void prepare (double sampleRate, Type type, float freqHz, float q, float gainDb) noexcept
    {
        c = design (sampleRate, type, freqHz, q, gainDb);
        reset();
    }

    static Coeffs design (double sampleRate, Type type, float freqHz, float q, float gainDb) noexcept
    {
        const float w0 = static_cast<float> (2.0 * pi * freqHz / sampleRate);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float cw = std::cos (w0);
        const float A = std::pow (10.0f, gainDb / 40.0f);

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

        switch (type)
        {
            case Type::lowPass:
                b0 = (1.0f - cw) / 2.0f; b1 = 1.0f - cw; b2 = b0;
                a0 = 1.0f + alpha; a1 = -2.0f * cw; a2 = 1.0f - alpha;
                break;
            case Type::highPass:
                b0 = (1.0f + cw) / 2.0f; b1 = -(1.0f + cw); b2 = b0;
                a0 = 1.0f + alpha; a1 = -2.0f * cw; a2 = 1.0f - alpha;
                break;
            case Type::peaking:
                b0 = 1.0f + alpha * A; b1 = -2.0f * cw; b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A; a1 = -2.0f * cw; a2 = 1.0f - alpha / A;
                break;
            case Type::lowShelf:
            {
                const float sq = 2.0f * std::sqrt (A) * alpha;
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cw + sq);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cw - sq);
                a0 = (A + 1.0f) + (A - 1.0f) * cw + sq;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw);
                a2 = (A + 1.0f) + (A - 1.0f) * cw - sq;
                break;
            }
            case Type::highShelf:
            {
                const float sq = 2.0f * std::sqrt (A) * alpha;
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cw + sq);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cw - sq);
                a0 = (A + 1.0f) - (A - 1.0f) * cw + sq;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw);
                a2 = (A + 1.0f) - (A - 1.0f) * cw - sq;
                break;
            }
        }

        Coeffs out;
        out.b0 = b0 / a0; out.b1 = b1 / a0; out.b2 = b2 / a0;
        out.a1 = a1 / a0; out.a2 = a2 / a0;
        return out;
    }

    inline float process (float x) noexcept
    {
        const float y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }

    // Frequency response magnitude (for testing) of arbitrary coefficients.
    static float responseAt (const Coeffs& cf, float freqHz, double sampleRate) noexcept
    {
        const float w = static_cast<float> (2.0 * pi * freqHz / sampleRate);
        const float cw = std::cos (w), sw = std::sin (w);
        const float cw2 = std::cos (2.0f * w), sw2 = std::sin (2.0f * w);
        const float nr = cf.b0 + cf.b1 * cw + cf.b2 * cw2;
        const float ni = -(cf.b1 * sw + cf.b2 * sw2);
        const float dr = 1.0f + cf.a1 * cw + cf.a2 * cw2;
        const float di = -(cf.a1 * sw + cf.a2 * sw2);
        const float nMag = std::sqrt (nr * nr + ni * ni);
        const float dMag = std::sqrt (dr * dr + di * di);
        return nMag / (dMag + 1.0e-20f);
    }

    const Coeffs& coeffs() const noexcept { return c; }
    void setCoeffs (const Coeffs& cf) noexcept { c = cf; }
    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

private:
    static constexpr double pi = 3.14159265358979323846;
    Coeffs c;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};

//==============================================================================
// Windowed-sinc FIR with persistent history, for 2x up/down sampling stages.
class FirStage
{
public:
    explicit FirStage (float cutoff = 0.45f) noexcept { design (cutoff); }

    void design (float cutoff) noexcept
    {
        const int taps = NumTaps;
        const float centre = (taps - 1) / 2.0f;
        h.resize (static_cast<size_t> (taps));
        float sum = 0.0f;
        for (int i = 0; i < taps; ++i)
        {
            const float t = i - centre;
            const float sinc = (t == 0.0f) ? 1.0f
                               : std::sin (static_cast<float> (pi) * cutoff * t)
                                 / (static_cast<float> (pi) * cutoff * t);
            const float win = 0.42f - 0.5f * std::cos (2.0f * static_cast<float> (pi) * i / (taps - 1))
                              + 0.08f * std::cos (4.0f * static_cast<float> (pi) * i / (taps - 1));
            h[static_cast<size_t> (i)] = sinc * win;
            sum += h[static_cast<size_t> (i)];
        }
        for (auto& t : h) t /= sum;
        history.assign (static_cast<size_t> (taps - 1), 0.0f);
    }

    // In-place filtering of a block (data may alias the FIR history input).
    void processBlock (float* data, int numSamples) noexcept
    {
        const int hist = NumTaps - 1;
        std::vector<float> out (static_cast<size_t> (numSamples));
        for (int n = 0; n < numSamples; ++n)
        {
            float acc = h[0] * data[n];
            for (int k = 1; k <= hist; ++k)
            {
                const float past = (k <= n) ? data[n - k] : history[static_cast<size_t> (hist + n - k)];
                acc += h[static_cast<size_t> (k)] * past;
            }
            out[static_cast<size_t> (n)] = acc;
        }
        // Update history with the tail of the input block.
        if (numSamples >= hist)
            for (int k = 0; k < hist; ++k)
                history[static_cast<size_t> (k)] = data[numSamples - hist + k];
        else
        {
            // Shift: remove consumed samples, append new ones.
            std::vector<float> merged (history);
            merged.insert (merged.end(), data, data + numSamples);
            history.assign (merged.end() - hist, merged.end());
        }
        std::memcpy (data, out.data(), sizeof (float) * static_cast<size_t> (numSamples));
    }

    void reset() noexcept { std::fill (history.begin(), history.end(), 0.0f); }

private:
    static constexpr double pi = 3.14159265358979323846;

public:
    static constexpr int NumTaps = 23;

private:
    std::vector<float> h;
    std::vector<float> history;
};

// 4x oversampling built from zero-stuffing + FIR interpolation stages and
// FIR decimation stages on the way back down.
class Oversampler4x
{
public:
    void reset() noexcept
    {
        fir1.reset(); fir2.reset(); fir3.reset(); fir4.reset();
    }

    // Process a block at base rate; `processFn` receives the 4x-rate data.
    template <typename Fn>
    void processBlock (const float* in, float* out, int numSamples, Fn&& processFn)
    {
        up2.resize (static_cast<size_t> (numSamples) * 2);
        up4.resize (static_cast<size_t> (numSamples) * 4);

        for (int n = 0; n < numSamples; ++n)
        {
            up2[static_cast<size_t> (2 * n)] = in[n];
            up2[static_cast<size_t> (2 * n + 1)] = 0.0f;
        }
        fir1.processBlock (up2.data(), numSamples * 2);
        for (int n = 0; n < numSamples * 2; ++n) up2[static_cast<size_t> (n)] *= 2.0f;

        for (int n = 0; n < numSamples * 2; ++n)
        {
            up4[static_cast<size_t> (2 * n)] = up2[static_cast<size_t> (n)];
            up4[static_cast<size_t> (2 * n + 1)] = 0.0f;
        }
        fir2.processBlock (up4.data(), numSamples * 4);
        for (int n = 0; n < numSamples * 4; ++n) up4[static_cast<size_t> (n)] *= 2.0f;

        processFn (up4.data(), numSamples * 4);

        fir3.processBlock (up4.data(), numSamples * 4);
        for (int n = 0; n < numSamples * 2; ++n)
            up2[static_cast<size_t> (n)] = up4[static_cast<size_t> (2 * n)];
        fir4.processBlock (up2.data(), numSamples * 2);
        for (int n = 0; n < numSamples; ++n)
            out[n] = up2[static_cast<size_t> (2 * n)];
    }

private:
    FirStage fir1, fir2, fir3, fir4;
    std::vector<float> up2, up4;
};

//==============================================================================
// Parameters (plain data — JUCE side maps APVTS onto this).
struct Params
{
    enum class Character { tube, diode, fuzz, tape, cassette, dictaphone };

    float drive = 0.35f;           // 0..1
    Character character = Character::tube;
    float tiltDb = 0.0f;            // -12..+12
    float midCharacter = 0.0f;      // 0..1
    float lowCutHz = 80.0f;         // 20..800
    float presenceDb = 0.0f;        // -12..+12
    float wowFlutter = 0.15f;      // 0..1 (tape/cassette/dictaphone)
    float noiseDb = -60.0f;         // -80..-40; `noiseEnabled=false` for off
    bool noiseEnabled = true;
    float mix = 1.0f;               // 0..1
    float outputDb = 0.0f;          // -24..+12
    bool mixSoloTilt = false;      // tilt only on the distorted path
};

//==============================================================================
// Distortion characters.
namespace characters
{

// Tube: asymmetric soft clipping (tanh with bias).
inline float tube (float x, float drive) noexcept
{
    const float bias = 0.12f * drive;
    const float y = std::tanh (x * (1.0f + 2.0f * drive) + bias) - std::tanh (bias);
    const float norm = 1.0f / (1.0f - std::tanh (bias) * std::tanh (bias));
    return y * norm;
}

// Diode: two-stage soft/hard "diode ladder".
inline float diode (float x, float drive) noexcept
{
    const float a = 1.0f + 3.0f * drive;
    const float knee = 1.2f - 0.4f * drive;
    const float ax = std::abs (x) * a;
    const float soft = 1.0f - std::exp (-ax);
    const float hard = std::min (ax, knee) + std::max (0.0f, ax - knee) * 0.25f;
    const float y = (soft + hard) * 0.5f;
    return std::copysign (y, x) / (1.0f + drive * 0.6f);
}

// Fuzz: exponential hard clipping with a sharp ceiling.
inline float fuzz (float x, float drive) noexcept
{
    const float a = 2.0f + 8.0f * drive;
    const float y = std::copysign (1.0f, x) * (1.0f - std::exp (-std::abs (x) * a));
    const float ceil = 1.0f - std::exp (-3.0f * a);
    return y / ceil;
}

} // namespace characters

//==============================================================================
// Band machine engine (tape/cassette/dictaphone): magnetization-style
// saturation, wow & flutter, hiss/hum, compression, dropouts.
class BandMachine
{
public:
    void prepare (double sampleRate, Params::Character character) noexcept
    {
        sr = sampleRate;
        // Magnetization smoothing coefficient (~55 Hz "band smear").
        const float smearHz = (character == Params::Character::dictaphone) ? 110.0f : 55.0f;
        magCoef = std::exp (-2.0f * static_cast<float> (pi) * smearHz / static_cast<float> (sampleRate));

        lfo1Phase = 0.0f; lfo2Phase = 0.0f;
        lfo1Inc = 2.0f * static_cast<float> (pi) * 0.9f / static_cast<float> (sampleRate);   // wow
        lfo2Inc = 2.0f * static_cast<float> (pi) * 7.0f / static_cast<float> (sampleRate);   // flutter
        humPhase = 0.0f;
        humInc = 2.0f * static_cast<float> (pi) * 50.0f / static_cast<float> (sampleRate);

        flutterDelay.prepare (sampleRate);
        env.prepare (sampleRate, 20.0f);
        dropoutEnv = 1.0f;
        dropoutCounter = 64;
    }

    void setNoiseSeed (uint64_t seed) noexcept { rng.setSeed (seed); }

    inline float process (float x, Params::Character character, float drive,
                          float wowFlutterAmt, float noiseLevel, bool noiseEnabled) noexcept
    {
        // 1) Magnetization saturation (hysteresis-flavoured smearing).
        mag += (std::tanh (x * (1.0f + 5.0f * drive)) - mag) * (1.0f - magCoef);
        float y = mag + (x - mag) * 0.35f;

        // 2) Signal-dependent compression (band saturation), ~2:1 above 3 dB.
        const float envDb = 20.0f * std::log10 (std::abs (env.process (y)) + 1.0e-9f);
        const float thresholdDb = 3.0f;
        if (envDb > thresholdDb)
        {
            const float excess = (envDb - thresholdDb) * 0.5f;
            y *= std::pow (10.0f, -excess / 20.0f);
        }

        // 3) Wow & flutter (delay modulation).
        lfo1Phase += lfo1Inc;
        if (lfo1Phase >= twoPi) lfo1Phase -= twoPi;
        lfo2Phase += lfo2Inc;
        if (lfo2Phase >= twoPi) lfo2Phase -= twoPi;
        float wfDepth = wowFlutterAmt;
        if (character == Params::Character::cassette) wfDepth *= 1.8f;
        if (character == Params::Character::dictaphone) wfDepth *= 2.5f;
        const float delayMs = 0.6f * wfDepth
                              * (0.6f * std::sin (lfo1Phase) + 0.4f * std::sin (lfo2Phase));
        y = flutterDelay.process (y, delayMs);

        // 4) Noise: hiss (+ motor hum for dictaphone).
        if (noiseEnabled && noiseLevel > -80.0f)
        {
            const float amp = std::pow (10.0f, noiseLevel / 20.0f);
            const float hissLevel = (character == Params::Character::dictaphone) ? 1.8f
                                    : (character == Params::Character::cassette) ? 1.4f
                                                                                 : 1.0f;
            float noise = rng.nextBipolar() * hissLevel;
            if (character == Params::Character::dictaphone)
            {
                humPhase += humInc;
                if (humPhase >= twoPi) humPhase -= twoPi;
                noise += (0.5f * std::sin (humPhase) + 0.2f * std::sin (2.0f * humPhase)) * 2.0f;
            }
            y += noise * amp;
        }

        // 5) Dropouts (dictaphone: fairly often; cassette: rare).
        if (character == Params::Character::dictaphone || character == Params::Character::cassette)
        {
            if (--dropoutCounter <= 0)
            {
                const float rate = (character == Params::Character::dictaphone) ? 0.0012f
                                                                               : 0.00025f;
                if (rng.next01() < rate)
                {
                    dropoutEnv = 0.25f;
                    dropoutCounter = static_cast<int> (0.02f * sr);
                }
                else
                {
                    dropoutCounter = 64;
                }
            }
            dropoutEnv += (1.0f - dropoutEnv) * 0.0008f;
            y *= dropoutEnv;
        }

        return y;
    }

    void reset() noexcept
    {
        mag = 0.0f; dropoutEnv = 1.0f; dropoutCounter = 64;
        flutterDelay.reset();
        env.reset();
    }

private:
    // Fractional (linear-interp) delay for wow & flutter.
    class FractionalDelay
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            maxLen = static_cast<int> (0.012f * sampleRate) + 4;
            buffer.assign (static_cast<size_t> (maxLen), 0.0f);
            writeIdx = 0;
            sr = sampleRate;
        }

        inline float process (float x, float delayMs) noexcept
        {
            buffer[static_cast<size_t> (writeIdx)] = x;
            const float delaySamp = delayMs * static_cast<float> (sr) / 1000.0f;
            float readPos = static_cast<float> (writeIdx) - delaySamp;
            while (readPos < 0.0f) readPos += static_cast<float> (maxLen);
            while (readPos >= static_cast<float> (maxLen)) readPos -= static_cast<float> (maxLen);
            const int i0 = static_cast<int> (readPos);
            const int i1 = (i0 + 1) % maxLen;
            const float frac = readPos - static_cast<float> (i0);
            const float out = buffer[static_cast<size_t> (i0)] * (1.0f - frac)
                              + buffer[static_cast<size_t> (i1)] * frac;
            writeIdx = (writeIdx + 1) % maxLen;
            return out;
        }

        void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    private:
        std::vector<float> buffer;
        int maxLen = 0, writeIdx = 0;
        double sr = 48000.0;
    };

    static constexpr double pi = 3.14159265358979323846;
    static constexpr float twoPi = 6.28318530718f;

    double sr = 48000.0;
    float magCoef = 0.0f;
    float mag = 0.0f;
    float lfo1Phase = 0.0f, lfo2Phase = 0.0f, lfo1Inc = 0.0f, lfo2Inc = 0.0f;
    float humPhase = 0.0f, humInc = 0.0f;
    float dropoutEnv = 1.0f;
    int dropoutCounter = 64;
    FractionalDelay flutterDelay;
    OnePole env;
    Rng rng;
};

//==============================================================================
// The full engine, mono (the plugin instantiates one per channel).
class GrinderChannel
{
public:
    void prepare (double sampleRate, const Params& p) noexcept
    {
        sr = sampleRate;
        updateParams (p);
        inputEnv.prepare (sampleRate, 5.0f);
        band.prepare (sampleRate, p.character);
        band.setNoiseSeed (0xC0FFEEull);
        band.reset();
        oversampler.reset();
    }

    void updateParams (const Params& p) noexcept
    {
        if (p.character != params.character)
        {
            band.prepare (sr, p.character);
            band.setNoiseSeed (0xC0FFEEull);
        }
        params = p;

        // Tilt: low shelf -tilt/2 @ 250 Hz, high shelf +tilt/2 @ 2.5 kHz.
        tiltLow.prepare (sr, Biquad::Type::lowShelf, 250.0f, 0.707f, -p.tiltDb * 0.5f);
        tiltHigh.prepare (sr, Biquad::Type::highShelf, 2500.0f, 0.707f, p.tiltDb * 0.5f);
        lowCut.prepare (sr, Biquad::Type::highPass, p.lowCutHz, 0.707f, 0.0f);
        presence.prepare (sr, Biquad::Type::peaking, 2500.0f, 1.0f, p.presenceDb);

        headBump.prepare (sr, Biquad::Type::peaking, 60.0f, 1.0f,
                          (p.character == Params::Character::cassette) ? 3.0f : 0.0f);
        hfRolloff.prepare (sr, Biquad::Type::lowPass,
                          (p.character == Params::Character::cassette) ? 10000.0f : 18000.0f,
                          0.707f, 0.0f);
        bandHigh.prepare (sr, Biquad::Type::highPass, 400.0f, 0.707f, 0.0f);
        bandLow.prepare (sr, Biquad::Type::lowPass, 3000.0f, 0.707f, 0.0f);
        midBand.prepare (sr, Biquad::Type::peaking, 1200.0f, 1.2f, 6.0f * p.midCharacter);

        preGainDb = 24.0f * p.drive - 6.0f;
        if (isBandMachine (p.character)) preGainDb *= 0.6f;
        outTrimDb = -12.0f * p.drive * (isBandMachine (p.character) ? 0.4f : 1.0f);
    }

    // Process a block of samples (in-place allowed if in == out).
    void processBlock (const float* in, float* out, int numSamples) noexcept
    {
        wetPath.resize (static_cast<size_t> (numSamples));
        dryPath.resize (static_cast<size_t> (numSamples));

        // Tilt (shared state, processed once): wet always gets it; dry only
        // when mixSoloTilt is off (otherwise dry stays untouched).
        for (int n = 0; n < numSamples; ++n)
        {
            const float x = in[static_cast<size_t> (n)];
            const float tilted = tiltLow.process (tiltHigh.process (x));
            wetPath[static_cast<size_t> (n)] = tilted;
            dryPath[static_cast<size_t> (n)] = params.mixSoloTilt ? x : tilted;
        }

        // Distorted path continues: low cut -> presence.
        for (int n = 0; n < numSamples; ++n)
        {
            float x = wetPath[static_cast<size_t> (n)];
            x = lowCut.process (x);
            wetPath[static_cast<size_t> (n)] = presence.process (x);
        }

        const float preGain = std::pow (10.0f, preGainDb / 20.0f);
        const float outTrim = std::pow (10.0f, outTrimDb / 20.0f);
        const float outFinal = std::pow (10.0f, params.outputDb / 20.0f);
        const auto ch = params.character;

        if (ch == Params::Character::tube || ch == Params::Character::diode
            || ch == Params::Character::fuzz)
        {
            oversampler.processBlock (wetPath.data(), wetPath.data(), numSamples,
                [this, preGain, outTrim, ch] (float* data, int size)
                {
                    for (int i = 0; i < size; ++i)
                    {
                        const float x = data[static_cast<size_t> (i)] * preGain;
                        float y = 0.0f;
                        switch (ch)
                        {
                            case Params::Character::tube: y = characters::tube (x, params.drive); break;
                            case Params::Character::diode: y = characters::diode (x, params.drive); break;
                            case Params::Character::fuzz: y = characters::fuzz (x, params.drive); break;
                            default: break;
                        }
                        data[static_cast<size_t> (i)] = y * outTrim;
                    }
                });
        }
        else
        {
            // Band machines run at base rate (W&F + noise are sub-bandwidth anyway).
            for (int n = 0; n < numSamples; ++n)
            {
                const float x = wetPath[static_cast<size_t> (n)] * preGain;
                float y = band.process (x, ch, params.drive, params.wowFlutter,
                                        params.noiseDb, params.noiseEnabled);
                if (ch == Params::Character::cassette) y = headBump.process (y);
                if (ch == Params::Character::cassette) y = hfRolloff.process (y);
                if (ch == Params::Character::dictaphone) y = bandHigh.process (bandLow.process (y));
                y = midBand.process (y);
                wetPath[static_cast<size_t> (n)] = y * outTrim;
            }
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const float wet = wetPath[static_cast<size_t> (n)];
            out[static_cast<size_t> (n)] =
                (dryPath[static_cast<size_t> (n)] * (1.0f - params.mix) + wet * params.mix)
                * outFinal;
        }
    }

    void reset() noexcept
    {
        tiltLow.reset(); tiltHigh.reset(); lowCut.reset(); presence.reset();
        headBump.reset(); hfRolloff.reset(); bandHigh.reset(); bandLow.reset();
        midBand.reset(); inputEnv.reset(); band.reset(); oversampler.reset();
    }

    static bool isBandMachine (Params::Character c) noexcept
    {
        return c == Params::Character::tape || c == Params::Character::cassette
               || c == Params::Character::dictaphone;
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    double sr = 48000.0;
    Params params;
    Biquad tiltLow, tiltHigh, lowCut, presence, headBump, hfRolloff, bandHigh, bandLow, midBand;
    OnePole inputEnv;
    BandMachine band;
    Oversampler4x oversampler;
    std::vector<float> wetPath, dryPath;
    float preGainDb = 0.0f, outTrimDb = 0.0f;
};

} // namespace grinder

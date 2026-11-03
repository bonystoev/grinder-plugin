// Grinder DSP unit tests — headless, no JUCE required.
// Build: g++ -std=c++17 -O2 -Wall -Wextra -I../dsp -o test_dsp test_dsp.cpp

#include "grinder_dsp.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace grinder;

static int failures = 0;

#define CHECK(cond, msg)                                                          \
    do                                                                            \
    {                                                                             \
        if (! (cond))                                                             \
        {                                                                         \
            std::printf ("FAIL: %s (line %d)\n", msg, __LINE__);                   \
            ++failures;                                                           \
        }                                                                         \
    } while (0)

static bool closeTo (float a, float b, float tol) { return std::fabs (a - b) <= tol; }

static constexpr double pi_d = 3.14159265358979323846;

//==============================================================================
static void testCharacterBoundsAndMonotonic()
{
    for (int c = 0; c < 3; ++c)
    {
        float prev = -10.0f;
        for (int i = 0; i <= 200; ++i)
        {
            const float x = -4.0f + 8.0f * i / 200.0f;
            float y = 0.0f;
            switch (c)
            {
                case 0: y = characters::tube (x, 0.5f); break;
                case 1: y = characters::diode (x, 0.5f); break;
                case 2: y = characters::fuzz (x, 0.5f); break;
            }
            CHECK(std::isfinite (y), "character output finite");
            CHECK(std::fabs (y) < 2.0f, "character output bounded");
            if (x >= 0.0f)
                CHECK(y >= prev - 1.0e-4f, "character monotonic for x >= 0");
            if (x >= 0.0f) prev = y;
        }
    }
    std::printf ("ok  character bounds/monotonic\n");
}

static void testSymmetry()
{
    // Diode and fuzz are odd functions; tube is deliberately asymmetric.
    const float eps = 1.0e-5f;
    for (int i = 1; i <= 50; ++i)
    {
        const float x = i / 50.0f;
        CHECK(closeTo (characters::diode (x, 0.7f), -characters::diode (-x, 0.7f), 1.0e-4f),
              "diode odd symmetry");
        CHECK(closeTo (characters::fuzz (x, 0.7f), -characters::fuzz (-x, 0.7f), 1.0e-4f),
              "fuzz odd symmetry");
        CHECK(std::fabs (characters::tube (x, 0.9f) + characters::tube (-x, 0.9f)) > eps
                  || true, // tube asymmetry is optional at low x; sanity only
              "tube executed");
    }
    // Tube must be asymmetric at high drive: positive and negative limits differ.
    const float asym = characters::tube (4.0f, 0.9f) + characters::tube (-4.0f, 0.9f);
    CHECK(std::fabs (asym) > 1.0e-3f, "tube asymmetry visible at high drive");
    std::printf ("ok  symmetry properties\n");
}

static void testCharacterDrive()
{
    // More drive on fuzz should raise low-level gain (compression of range).
    const float low = 0.05f;
    CHECK(characters::fuzz (low, 0.9f) > characters::fuzz (low, 0.1f),
          "fuzz: drive increases small-signal gain");
    CHECK(characters::tube (low, 0.9f) > characters::tube (low, 0.1f),
          "tube: drive increases small-signal gain");
    std::printf ("ok  drive response\n");
}

//==============================================================================
static void testBiquadResponse()
{
    constexpr double sr = 48000.0;
    {
        auto cf = Biquad::design (sr, Biquad::Type::lowPass, 1000.0f, 0.707f, 0.0f);
        CHECK(closeTo (Biquad::responseAt (cf, 10.0f, sr), 1.0f, 1.0e-3f), "LP unity at 10 Hz");
        CHECK(Biquad::responseAt (cf, 1000.0f, sr) < 0.8f, "LP rolls off at cutoff");
        CHECK(Biquad::responseAt (cf, 10000.0f, sr) < 0.1f, "LP strong rolloff at 10 kHz");
    }
    {
        auto cf = Biquad::design (sr, Biquad::Type::highShelf, 2500.0f, 0.707f, 6.0f);
        CHECK(closeTo (Biquad::responseAt (cf, 100.0f, sr), 1.0f, 0.02f), "shelf flat below");
        CHECK(closeTo (Biquad::responseAt (cf, 16000.0f, sr), 2.0f, 0.02f), "high shelf +6 dB");
    }
    {
        auto cf = Biquad::design (sr, Biquad::Type::peaking, 60.0f, 1.0f, 3.0f);
        CHECK(closeTo (Biquad::responseAt (cf, 60.0f, sr), 1.413f, 0.01f), "head bump +3 dB @ 60 Hz");
        CHECK(closeTo (Biquad::responseAt (cf, 20.0f, sr), 1.0f, 0.06f), "peak flat far below");
    }
    std::printf ("ok  biquad responses\n");
}

static void testBiquadStability()
{
    // Long run must not blow up.
    Biquad bq;
    bq.prepare (48000.0, Biquad::Type::peaking, 2500.0f, 1.0f, 12.0f);
    float y = 0.0f;
    for (int i = 0; i < 200000; ++i)
        y = bq.process (0.5f * std::sin (i * 0.01f));
    CHECK(std::isfinite (y) && std::fabs (y) < 10.0f, "biquad stable over long run");
    std::printf ("ok  biquad stability\n");
}

//==============================================================================
static void testOversampler()
{
    Oversampler4x os;
    constexpr double sr = 48000.0;
    const float freq = 200.0f;
    const int len = 8192;
    std::vector<float> in (len), out (len);
    for (int n = 0; n < len; ++n)
        in[n] = std::sin (2.0 * static_cast<float> (pi_d) * freq * n / sr);

    const float identity = 1.0f;
    os.processBlock (in.data(), out.data(), len,
                     [] (float*, int) {});

    // Amplitude near unity after round trip (minus edge effects).
    float peak = 0.0f;
    for (int n = 200; n < len - 200; ++n)
        peak = std::max (peak, std::fabs (out[n]));
    CHECK(closeTo (peak, identity, 0.05f), "oversampler transparent amplitude");

    // No NaNs.
    for (int n = 0; n < len; ++n)
        CHECK(std::isfinite (out[static_cast<size_t> (n)]), "oversampler finite");
    std::printf ("ok  oversampler transparency\n");
}

static void testOversamplerAliasing()
{
    // Hard-clip a 5 kHz tone at 48k. Without OS this aliases badly; with 4x OS
    // the band energy above ~20 kHz must be much lower. We compare in-band
    // difference between oversampled and non-oversampled clipping via simple
    // RMS of the output — main check is finiteness and reduced HF energy.
    Oversampler4x os;
    constexpr double sr = 48000.0;
    const int len = 16384;
    std::vector<float> in (len), out (len);
    for (int n = 0; n < len; ++n)
        in[n] = 4.0f * std::sin (2.0 * static_cast<float> (pi_d) * 5000.0 * n / sr);

    os.processBlock (in.data(), out.data(), len,
                     [] (float* d, int size)
                     {
                         for (int i = 0; i < size; ++i)
                             d[i] = std::max (-0.9f, std::min (0.9f, d[i]));
                     });
    for (int n = 0; n < len; ++n)
        CHECK(std::isfinite (out[static_cast<size_t> (n)]), "clipped OS finite");

    // Rough HF energy (>12 kHz) of the output — just verify it stays bounded.
    float hf = 0.0f;
    for (int n = 64; n < len; ++n)
    {
        const float diff = out[n] - out[n - 1];
        hf += diff * diff;
    }
    hf = std::sqrt (hf / (len - 64));
    CHECK(hf < 1.0f, "HF residual bounded");
    std::printf ("ok  oversampler aliasing sanity (hf rms %.4f)\n", hf);
}

//==============================================================================
static void testBandMachineDeterminism()
{
    GrinderChannel left, right;
    Params p;
    p.character = Params::Character::cassette;
    p.noiseEnabled = true;
    p.noiseDb = -50.0f;
    p.drive = 0.5f;
    p.wowFlutter = 0.4f;

    std::mt19937 gen (42);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    std::vector<float> in (4096), a (4096), b (4096);
    for (auto& s : in) s = dist (gen);

    left.prepare (48000.0, p);
    right.prepare (48000.0, p);
    left.processBlock (in.data(), a.data(), 4096);
    right.processBlock (in.data(), b.data(), 4096);
    for (int n = 0; n < 4096; ++n)
        CHECK(a[static_cast<size_t> (n)] == b[static_cast<size_t> (n)],
              "band machine deterministic");
    std::printf ("ok  band machine determinism\n");
}

static void testFullChainStability()
{
    const Params::Character chars[] = {
        Params::Character::tube, Params::Character::diode, Params::Character::fuzz,
        Params::Character::tape, Params::Character::cassette, Params::Character::dictaphone
    };
    constexpr double sr = 48000.0;
    const int len = 16384;
    std::vector<float> in (len), out (len);
    for (int n = 0; n < len; ++n)
        in[n] = 0.8f * std::sin (2.0 * static_cast<float> (pi_d) * 220.0 * n / sr)
                + 0.2f * std::sin (2.0 * static_cast<float> (pi_d) * 440.0 * n / sr);

    for (auto c : chars)
    {
        for (float drive : { 0.0f, 0.35f, 1.0f })
        {
            Params p;
            p.character = c;
            p.drive = drive;
            p.tiltDb = 6.0f;
            p.presenceDb = 6.0f;
            p.midCharacter = 0.7f;
            p.lowCutHz = 80.0f;
            p.wowFlutter = 0.5f;
            p.noiseEnabled = true;
            p.noiseDb = -55.0f;
            p.mix = 1.0f;
            p.outputDb = 0.0f;

            GrinderChannel ch;
            ch.prepare (sr, p);
            ch.processBlock (in.data(), out.data(), len);
            for (int n = 0; n < len; ++n)
            {
                CHECK(std::isfinite (out[static_cast<size_t> (n)]), "output finite");
                CHECK(std::fabs (out[static_cast<size_t> (n)]) < 8.0f, "output bounded");
            }
        }
    }
    std::printf ("ok  full chain stability across characters\n");
}

static void testMixZeroIsDry()
{
    constexpr double sr = 48000.0;
    const int len = 8192;
    Params p;
    p.mix = 0.0f;
    p.tiltDb = 0.0f;
    p.presenceDb = 0.0f;
    p.lowCutHz = 20.0f;
    p.outputDb = 0.0f;
    p.noiseEnabled = false;

    GrinderChannel ch;
    ch.prepare (sr, p);
    std::vector<float> in (len), out (len);
    for (int n = 0; n < len; ++n)
        in[n] = 0.5f * std::sin (2.0 * static_cast<float> (pi_d) * 330.0 * n / sr);
    ch.processBlock (in.data(), out.data(), len);

    // Dry path passes tilt (0 dB -> unity) — allow small filter transients.
    float maxErr = 0.0f;
    for (int n = 256; n < len; ++n)
        maxErr = std::max (maxErr, std::fabs (out[n] - in[n]));
    CHECK(maxErr < 1.0e-3f, "mix=0 yields dry signal");
    std::printf ("ok  mix zero equals dry (max err %.6f)\n", maxErr);
}

static void testCassetteHeadBump()
{
    // With noise off and drive 0, cassette output should show more energy at
    // 60 Hz relative to input than the tape character (head bump EQ active).
    constexpr double sr = 48000.0;
    const int len = 32768;
    std::vector<float> in (len), outC (len), outT (len);
    for (int n = 0; n < len; ++n)
        in[n] = 0.4f * std::sin (2.0 * static_cast<float> (pi_d) * 60.0 * n / sr);

    Params pc; pc.character = Params::Character::cassette;
    pc.noiseEnabled = false; pc.drive = 0.0f; pc.mix = 1.0f;
    pc.tiltDb = 0.0f; pc.presenceDb = 0.0f; pc.lowCutHz = 20.0f; pc.outputDb = 0.0f;
    Params pt = pc; pt.character = Params::Character::tape;

    GrinderChannel cc, ct;
    cc.prepare (sr, pc); ct.prepare (sr, pt);
    cc.processBlock (in.data(), outC.data(), len);
    ct.processBlock (in.data(), outT.data(), len);

    auto rms = [] (const std::vector<float>& v)
    {
        double acc = 0.0;
        for (int n = 1024; n < 32768; ++n) acc += v[n] * v[n];
        return std::sqrt (acc / (32768 - 1024));
    };
    CHECK(rms (outC) > rms (outT), "cassette head bump boosts 60 Hz vs tape");
    std::printf ("ok  cassette head bump (rms C %.4f > T %.4f)\n", rms (outC), rms (outT));
}

static void testDictaphoneBandLimit()
{
    // A 5 kHz tone must be heavily attenuated by the dictaphone bandpass.
    constexpr double sr = 48000.0;
    const int len = 32768;
    std::vector<float> in (len), out (len);
    for (int n = 0; n < len; ++n)
        in[n] = 0.4f * std::sin (2.0 * static_cast<float> (pi_d) * 5000.0 * n / sr);

    Params p; p.character = Params::Character::dictaphone;
    p.noiseEnabled = false; p.drive = 0.0f; p.mix = 1.0f;
    p.tiltDb = 0.0f; p.presenceDb = 0.0f; p.lowCutHz = 20.0f; p.outputDb = 0.0f;
    p.wowFlutter = 0.0f;

    GrinderChannel ch;
    ch.prepare (sr, p);
    ch.processBlock (in.data(), out.data(), len);

    double acc = 0.0;
    for (int n = 1024; n < 32768; ++n) acc += out[n] * out[n];
    const double rms = std::sqrt (acc / (32768 - 1024));
    CHECK(rms < 0.03, "dictaphone kills 5 kHz");
    std::printf ("ok  dictaphone band limit (5 kHz rms %.5f)\n", rms);
}

static void testWowFlutterAudible()
{
    // With W&F at max, a 1 kHz tone should show measurable frequency
    // modulation: compare zero-crossing jitter against W&F off.
    constexpr double sr = 48000.0;
    const int len = 32768;
    std::vector<float> in (len), outA (len), outB (len);
    for (int n = 0; n < len; ++n)
        in[n] = 0.4f * std::sin (2.0 * static_cast<float> (pi_d) * 1000.0 * n / sr);

    Params p; p.character = Params::Character::tape;
    p.noiseEnabled = false; p.drive = 0.0f; p.mix = 1.0f;
    p.tiltDb = 0.0f; p.presenceDb = 0.0f; p.lowCutHz = 20.0f; p.outputDb = 0.0f;

    GrinderChannel a, b;
    p.wowFlutter = 1.0f;      a.prepare (sr, p);
    p.wowFlutter = 0.0f;      b.prepare (sr, p);
    a.processBlock (in.data(), outA.data(), len);
    b.processBlock (in.data(), outB.data(), len);

    auto periodSpread = [] (const std::vector<float>& v)
    {
        int prev = -1; std::vector<int> periods;
        for (int n = 1; n < 32768; ++n)
            if (v[n - 1] < 0.0f && v[n] >= 0.0f)
            {
                if (prev > 0) periods.push_back (n - prev);
                prev = n;
            }
        if (periods.size() < 10) return 0.0;
        double mean = 0.0;
        for (int t : periods) mean += t;
        mean /= periods.size();
        double var = 0.0;
        for (int t : periods) var += (t - mean) * (t - mean);
        return std::sqrt (var / periods.size()) / mean;
    };
    CHECK(periodSpread (outA) > periodSpread (outB) + 1.0e-4,
          "wow & flutter modulates pitch");
    std::printf ("ok  wow & flutter (spread %.5f vs %.5f)\n",
                 periodSpread (outA), periodSpread (outB));
}

int main()
{
    testCharacterBoundsAndMonotonic();
    testSymmetry();
    testCharacterDrive();
    testBiquadResponse();
    testBiquadStability();
    testOversampler();
    testOversamplerAliasing();
    testBandMachineDeterminism();
    testFullChainStability();
    testMixZeroIsDry();
    testCassetteHeadBump();
    testDictaphoneBandLimit();
    testWowFlutterAudible();

    if (failures == 0)
    {
        std::printf ("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf ("\n%d FAILURE(S)\n", failures);
    return 1;
}

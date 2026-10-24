// The whole sound of Weight without a host: the curves against the original's
// sheet, the low-end trick, the air, the colour, and the switches.
#include "dsp/Weight.h"
#include "TestSignal.h"

#include <cstdio>
#include <cstdlib>

namespace
{
constexpr double kSr = 96000.0;   // the plugin runs the engine at twice the host's rate

int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
    failures += ok ? 0 : 1;
}

wgt::Weight make (const wgt::Settings& s)
{
    wgt::Weight w;
    w.setSettings (s);
    w.prepare (kSr);
    return w;
}

std::vector<float> play (const wgt::Settings& s, std::vector<float> x)
{
    auto w = make (s);
    float* channels[] = { x.data() };
    w.process (channels, 1, (int) x.size());
    return x;
}

/** The amplitude of one frequency over the second half of a signal. */
double amplitude (const std::vector<float>& x, double hz)
{
    const auto from = x.size() / 2, n = x.size() - from;
    double re = 0.0, im = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const auto phase = 2.0 * M_PI * hz * (double) (from + i) / kSr;
        re += x[from + i] * std::cos (phase);
        im += x[from + i] * std::sin (phase);
    }

    return 2.0 * std::sqrt (re * re + im * im) / (double) n;
}

double db (double v) { return 20.0 * std::log10 (v + 1.0e-12); }

void printCurve (const char* name, const wgt::Settings& s)
{
    const auto w = make (s);
    std::printf ("    %-28s", name);

    for (double hz : { 20.0, 30.0, 60.0, 100.0, 200.0, 400.0, 1000.0, 3000.0, 5000.0, 10000.0, 15000.0, 20000.0 })
        std::printf (" %6.1f", w.responseDb (hz));

    std::printf ("\n");
}

double lowestBetween (const wgt::Weight& w, double from, double to)
{
    auto lowest = 1.0e9;

    for (auto hz = from; hz <= to; hz *= 1.02)
        lowest = std::min (lowest, w.responseDb (hz));

    return lowest;
}

double highestBetween (const wgt::Weight& w, double from, double to)
{
    auto highest = -1.0e9;

    for (auto hz = from; hz <= to; hz *= 1.02)
        highest = std::max (highest, w.responseDb (hz));

    return highest;
}
}

int main()
{
    std::printf ("Weight, offline\n");
    std::printf ("    %-28s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s %6s\n", "dB at", "20", "30", "60", "100", "200",
                 "400", "1k", "3k", "5k", "10k", "15k", "20k");

    wgt::Settings flat;
    auto lowBoost = flat;   lowBoost.lowBoost = 10.0f;
    auto lowAtten = flat;   lowAtten.lowAtten = 10.0f;
    auto trick = flat;      trick.lowBoost = 10.0f; trick.lowAtten = 10.0f;
    auto halfTrick = flat;  halfTrick.lowBoost = 5.0f; halfTrick.lowAtten = 3.0f;
    auto sharp = flat;      sharp.highBoost = 10.0f; sharp.bandwidth = 0.0f;
    auto broad = flat;      broad.highBoost = 10.0f; broad.bandwidth = 10.0f;
    auto air = flat;        air.highBoost = 5.0f; air.highFreq = 6;
    auto highAtten = flat;  highAtten.highAtten = 10.0f; highAtten.attenFreq = 2;
    auto highTrick = flat;  highTrick.highBoost = 8.0f; highTrick.bandwidth = 2.0f; highTrick.highAtten = 6.0f; highTrick.attenFreq = 2;

    printCurve ("flat", flat);
    printCurve ("low boost 10 @60", lowBoost);
    printCurve ("low atten 10 @60", lowAtten);
    printCurve ("boost 10 + atten 10 @60", trick);
    printCurve ("boost 5 + atten 3 @60", halfTrick);
    printCurve ("high boost 10 @10k sharp", sharp);
    printCurve ("high boost 10 @10k broad", broad);
    printCurve ("high boost 5 @16k bw 5", air);
    printCurve ("high atten 10 @20k", highAtten);
    printCurve ("boost 8 @10k + atten 6 @20k", highTrick);

    // The curves.
    {
        const auto w = make (flat);
        check (std::abs (lowestBetween (w, 10.0, 22000.0)) < 0.01 && std::abs (highestBetween (w, 10.0, 22000.0)) < 0.01,
               "every dial at 0: flat");
    }
    {
        const auto w = make (lowBoost);
        check (w.responseDb (30.0) > 12.0 && w.responseDb (30.0) < 15.0, "low boost 10: about 13.5 dB under the frequency");
        check (w.responseDb (60.0) > 9.0, "and most of it at the frequency itself");
        check (w.responseDb (1000.0) < 0.5, "and nothing left at 1 kHz");
    }
    {
        const auto w = make (lowAtten);
        check (w.responseDb (20.0) < -15.0, "low atten 10: about 17.5 dB down at the bottom");
        check (w.responseDb (2000.0) > -0.5, "and the mids untouched");
    }
    {
        const auto w = make (trick);
        const auto dip = lowestBetween (w, 100.0, 800.0);
        std::printf ("    the trick: %+.1f dB at 30 Hz, %+.1f dB at its lowest above\n", w.responseDb (30.0), dip);
        check (w.responseDb (30.0) > 2.0, "boost and atten together: the bottom still comes up");
        check (dip < -1.5, "and the low mids go down");
        check (std::abs (w.responseDb (3000.0)) < 0.3, "and the rest stays where it was");
    }
    {
        const auto s = make (sharp), b = make (broad);
        const auto peakSharp = highestBetween (s, 5000.0, 20000.0);
        check (peakSharp > 15.0 && peakSharp < 19.5, "high boost 10: up to 18 dB");
        check (s.responseDb (5000.0) < b.responseDb (5000.0) - 3.0, "sharp is narrower than broad");
        check (s.responseDb (18000.0) > 5.0, "and the air stays up above the bell");
        check (std::abs (s.responseDb (200.0)) < 0.3, "and the lows are untouched");
    }
    {
        const auto w = make (highAtten);
        check (w.responseDb (20000.0) < -6.0 && w.responseDb (5000.0) > -2.0, "high atten 10 at 20k: a gentle top shelf");
    }

    // The sound itself follows the curve.
    {
        const auto x = mdxtest::makeTone (1.0, kSr, 100.0, 0.05f);
        const auto out = play (lowBoost, x);
        const auto heard = db (amplitude (out, 100.0) / amplitude (x, 100.0)), drawn = make (lowBoost).responseDb (100.0);
        std::printf ("    100 Hz with low boost 10: %+.2f dB heard, %+.2f dB on the curve\n", heard, drawn);
        check (std::abs (heard - drawn) < 0.5, "what is heard is what the curve says");
    }

    // The colour.
    auto harmonics = [] (const wgt::Settings& s, double hz, float level)
    {
        const auto x = mdxtest::makeTone (1.0, kSr, hz, level);
        const auto out = play (s, x);
        const auto fundamental = amplitude (out, hz);
        return std::make_pair (db (amplitude (out, 2.0 * hz) / fundamental), db (amplitude (out, 3.0 * hz) / fundamental));
    };

    const auto minus18 = 0.126f;
    {
        const auto [h2, h3] = harmonics (flat, 1000.0, minus18);
        std::printf ("    flat, 1 kHz at -18 dBFS: H2 %.1f dB, H3 %.1f dB\n", h2, h3);
        check (h2 < -40.0 && h2 > -70.0, "flat, the tubes are there, softly");
    }
    {
        const auto [h2flat, h3flat] = harmonics (flat, 60.0, minus18);
        const auto [h2, h3] = harmonics (lowBoost, 60.0, minus18);
        std::printf ("    60 Hz at -18 dBFS: H2 %.1f dB flat, %.1f dB with the low boost at 10 (H3 %.1f, %.1f)\n",
                     h2flat, h2, h3flat, h3);
        check (h2 > h2flat + 10.0, "the low boost brings the weight: far more second harmonic");
        check (h2 < -15.0, "but it stays a colour, not a fuzz");
    }
    {
        const auto [h2, h3] = harmonics (sharp, 10000.0, minus18);
        std::printf ("    10 kHz at -18 dBFS through the high boost at 10: H2 %.1f dB, H3 %.1f dB\n", h2, h3);
        check (h2 < -55.0 && h3 < -55.0, "the highs stay clean");
    }

    // The switches.
    {
        auto s = trick;
        s.highBoost = 7.0f;
        s.eqIn = false;
        const auto w = make (s);
        check (std::abs (highestBetween (w, 10.0, 22000.0)) < 0.01 && std::abs (lowestBetween (w, 10.0, 22000.0)) < 0.01,
               "EQ out: the curves go flat");
    }
    {
        auto s = trick;
        s.bypass = true;
        const auto x = mdxtest::makeNoise (0.2, kSr, 0.5f);
        const auto out = play (s, x);
        auto worst = 0.0f;

        for (size_t i = 0; i < x.size(); ++i)
            worst = std::max (worst, std::abs (out[i] - x[i]));

        check (worst == 0.0f, "power off: the signal as it came in, to the bit");
    }
    {
        // Two channels, linked: the same sound on each.
        auto x = mdxtest::makeNoise (0.2, kSr, 0.3f), y = x;
        auto w = make (trick);
        float* channels[] = { x.data(), y.data() };
        w.process (channels, 2, (int) x.size());
        check (x == y, "both sides the same");
    }
    {
        // Turning the boost all the way during a note: no step in the sound.
        auto x = mdxtest::makeTone (0.5, kSr, 80.0, 0.25f);
        auto w = make (flat);
        float* channels[] = { x.data() };
        const int half = (int) x.size() / 2;
        w.process (channels, 1, half);
        w.setSettings (trick);
        channels[0] = x.data() + half;
        w.process (channels, 1, (int) x.size() - half);

        auto jump = 0.0f;

        for (size_t i = 1; i < x.size(); ++i)
            jump = std::max (jump, std::abs (x[i] - x[i - 1]));

        // An 80 Hz sine at 0.25, boosted by about 7 dB, moves at most ~0.003 a sample.
        check (jump < 0.01f, "dials glide: no click");
        check (std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v); }), "and nothing blows up");
    }

    std::printf (failures == 0 ? "all good\n" : "%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}

#include "Weight.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace wgt
{

namespace
{
    // Where the curves sit against the switch, set by reasoning from the
    // original's sheet rather than measured on one: the boost's shoulder just
    // above the frequency chosen, the cut a gentler slope centred well above
    // it. That difference is the whole low-end trick: both at once boost under
    // the frequency and dip the low mids above it.
    constexpr double kBoostCorner = 1.4, kBoostQ = 0.9;
    constexpr double kAttenCorner = 2.6;

    // The two low legs load each other in the passive network: with the boost
    // in, the cut no longer reaches as deep.
    constexpr double kAttenUnderBoost = 0.45;

    // The high boost is shared between the resonant bell of the original and a
    // broad shelf well above it: the air of a mastering high band, which keeps
    // rising where the bell falls back.
    constexpr double kBellShare = 0.85, kAirShare = 0.45, kAirAbove = 1.6, kAirQ = 0.5;

    // The bandwidth dial, from sharp to broad; a resonance narrows as it is pushed.
    constexpr double kSharpQ = 2.4, kBroadQ = 0.35;

    // The make-up stage. The tube takes what lies under kToneSplit, always,
    // a little; the weight takes the bass, as much as the low boost asks.
    constexpr double kToneSplit = 2000.0;
    constexpr double kTubeDrive = 0.46, kTubeBias = 0.3;
    constexpr double kWeightDrive = 0.72, kWeightBias = 0.35;

    constexpr double kPi = 3.14159265358979323846;

    void set (Biquad& f, double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        f.b0 = b0 / a0;
        f.b1 = b1 / a0;
        f.b2 = b2 / a0;
        f.a1 = a1 / a0;
        f.a2 = a2 / a0;
    }

    // The cookbook's second-order shapes (R. Bristow-Johnson).
    void lowShelf (Biquad& f, double fs, double hz, double q, double db) noexcept
    {
        const auto A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * hz / fs, c = std::cos (w);
        const auto k = 2.0 * std::sqrt (A) * std::sin (w) / (2.0 * q);

        set (f, A * ((A + 1.0) - (A - 1.0) * c + k), 2.0 * A * ((A - 1.0) - (A + 1.0) * c), A * ((A + 1.0) - (A - 1.0) * c - k),
             (A + 1.0) + (A - 1.0) * c + k, -2.0 * ((A - 1.0) + (A + 1.0) * c), (A + 1.0) + (A - 1.0) * c - k);
    }

    void highShelf (Biquad& f, double fs, double hz, double q, double db) noexcept
    {
        const auto A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * hz / fs, c = std::cos (w);
        const auto k = 2.0 * std::sqrt (A) * std::sin (w) / (2.0 * q);

        set (f, A * ((A + 1.0) + (A - 1.0) * c + k), -2.0 * A * ((A - 1.0) + (A + 1.0) * c), A * ((A + 1.0) + (A - 1.0) * c - k),
             (A + 1.0) - (A - 1.0) * c + k, 2.0 * ((A - 1.0) - (A + 1.0) * c), (A + 1.0) - (A - 1.0) * c - k);
    }

    void peak (Biquad& f, double fs, double hz, double q, double db) noexcept
    {
        const auto A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * hz / fs;
        const auto alpha = std::sin (w) / (2.0 * q);

        set (f, 1.0 + alpha * A, -2.0 * std::cos (w), 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * std::cos (w), 1.0 - alpha / A);
    }

    /** A first-order shelf, `db` at one end and nothing at the other, its slope
        centred on `hz`: the gentle shape of a single capacitor. */
    void gentleShelf (Biquad& f, double fs, double hz, double db, bool low) noexcept
    {
        const auto g = std::pow (10.0, db / 20.0), root = std::sqrt (g), k = std::tan (kPi * hz / fs);
        const auto zero = low ? k * root : k / root, pole = low ? k / root : k * root, gain = low ? 1.0 : g;

        set (f, gain * (1.0 + zero), gain * (zero - 1.0), 0.0, 1.0 + pole, pole - 1.0, 0.0);
    }

    double onePole (double hz, double fs) noexcept
    {
        return 1.0 - std::exp (-2.0 * kPi * hz / fs);
    }

    /** A tube stage's curve, biased so it bends one way before the other
        (even harmonics), and scaled so a small signal comes out as it went in. */
    double saturate (double u, double drive, double bias) noexcept
    {
        if (drive < 1.0e-9)
            return u;

        const auto t = std::tanh (drive * bias);
        return (std::tanh (drive * (u + bias)) - t) / (drive * (1.0 - t * t));
    }
}

//==============================================================================
double Biquad::magnitude (double w) const noexcept
{
    const auto z1 = std::polar (1.0, -w), z2 = z1 * z1;
    return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
}

void Biquad::clear() noexcept
{
    std::fill (std::begin (s1), std::end (s1), 0.0);
    std::fill (std::begin (s2), std::end (s2), 0.0);
}

//==============================================================================
void Weight::prepare (double sampleRate)
{
    fs = sampleRate;
    glide = 1.0 - std::exp (-(double) kChunk / (0.025 * fs));
    wetStep = 1.0 / (0.02 * fs);
    toneSplit = onePole (kToneSplit, fs);
    dcPole = 1.0 - 2.0 * kPi * 10.0 / fs;

    const auto& t = target;
    const double wanted[numDials] = { t.eqIn ? t.lowBoost : 0.0, t.eqIn ? t.lowAtten : 0.0, t.eqIn ? t.highBoost : 0.0,
                                      t.eqIn ? t.highAtten : 0.0, t.bandwidth };
    std::copy (std::begin (wanted), std::end (wanted), std::begin (dials));
    freqs[0] = std::clamp (t.lowFreq, 0, kNumLowFreqs - 1);
    freqs[1] = std::clamp (t.highFreq, 0, kNumHighFreqs - 1);
    freqs[2] = std::clamp (t.attenFreq, 0, kNumAttenFreqs - 1);
    wet = t.bypass ? 0.0 : 1.0;

    design();
    reset();
}

void Weight::reset() noexcept
{
    for (auto* f : { &boostShelf, &attenShelf, &bell, &air, &topCut })
        f->clear();

    std::fill (std::begin (tubeIn), std::end (tubeIn), 0.0);
    std::fill (std::begin (bassIn), std::end (bassIn), 0.0);
    std::fill (std::begin (dcX), std::end (dcX), 0.0);
    std::fill (std::begin (dcY), std::end (dcY), 0.0);
}

void Weight::design() noexcept
{
    const auto lb = dials[lowBoost] * 0.1, la = dials[lowAtten] * 0.1;
    const auto hb = dials[highBoost] * 0.1, ha = dials[highAtten] * 0.1, bw = dials[bandwidth] * 0.1;
    const auto top = 0.45 * fs;   // no corner closer to the edge than this
    const auto low = kLowFreqs[freqs[0]], high = kHighFreqs[freqs[1]], atten = kAttenFreqs[freqs[2]];

    lowShelf (boostShelf, fs, low * kBoostCorner, kBoostQ, kLowBoostDb * lb);
    gentleShelf (attenShelf, fs, low * kAttenCorner, -kLowAttenDb * la * (1.0 - kAttenUnderBoost * lb), true);

    const auto q = kSharpQ * std::pow (kBroadQ / kSharpQ, bw) * (0.6 + 0.4 * hb);
    peak (bell, fs, std::min (high, top), q, kHighBoostDb * kBellShare * hb);
    highShelf (air, fs, std::min (high * kAirAbove, top), kAirQ, kHighBoostDb * kAirShare * hb);
    gentleShelf (topCut, fs, std::min (atten, top), -kHighAttenDb * ha, false);

    // The weight gathers the bass around the frequency chosen.
    bassSplit = onePole (std::clamp (2.5 * low, 60.0, 250.0), fs);
}

void Weight::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);

    const auto& t = target;
    const double wanted[numDials] = { t.eqIn ? t.lowBoost : 0.0, t.eqIn ? t.lowAtten : 0.0, t.eqIn ? t.highBoost : 0.0,
                                      t.eqIn ? t.highAtten : 0.0, t.bandwidth };
    const int wantedFreqs[3] = { std::clamp (t.lowFreq, 0, kNumLowFreqs - 1), std::clamp (t.highFreq, 0, kNumHighFreqs - 1),
                                 std::clamp (t.attenFreq, 0, kNumAttenFreqs - 1) };
    const auto wetTarget = t.bypass ? 0.0 : 1.0;

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const auto n = std::min (kChunk, numSamples - start);

        // The dials glide; the switches jump, as they do on the front.
        auto changed = ! std::equal (std::begin (freqs), std::end (freqs), std::begin (wantedFreqs));
        std::copy (std::begin (wantedFreqs), std::end (wantedFreqs), std::begin (freqs));

        for (int k = 0; k < numDials; ++k)
        {
            if (dials[k] == wanted[k])
                continue;

            dials[k] += (wanted[k] - dials[k]) * glide;

            if (std::abs (wanted[k] - dials[k]) < 1.0e-4)
                dials[k] = wanted[k];

            changed = true;
        }

        if (changed)
            design();

        const auto weightDrive = kWeightDrive * dials[lowBoost] * 0.1;
        auto w = wet;

        for (int c = 0; c < numChannels; ++c)
        {
            auto* x = channels[c] + start;
            w = wet;

            for (int i = 0; i < n; ++i)
            {
                const double in = x[i];
                auto y = topCut.process (air.process (bell.process (attenShelf.process (boostShelf.process (in, c), c), c), c), c);

                tubeIn[c] += toneSplit * (y - tubeIn[c]);
                bassIn[c] += bassSplit * (y - bassIn[c]);

                // Only what the stage adds: its harmonics, freed of the DC its bias leaves.
                const auto colour = saturate (tubeIn[c], kTubeDrive, kTubeBias) - tubeIn[c]
                                    + saturate (bassIn[c], weightDrive, kWeightBias) - bassIn[c];
                dcY[c] = colour - dcX[c] + dcPole * dcY[c];
                dcX[c] = colour;
                y += dcY[c];

                w = w < wetTarget ? std::min (wetTarget, w + wetStep) : std::max (wetTarget, w - wetStep);
                x[i] = (float) (in + w * (y - in));
            }
        }

        wet = w;
    }
}

double Weight::responseDb (double hz) const noexcept
{
    const auto w = 2.0 * kPi * hz / fs;
    auto gain = 1.0;

    for (const auto* f : { &boostShelf, &attenShelf, &bell, &air, &topCut })
        gain *= f->magnitude (w);

    return 20.0 * std::log10 (gain);
}

} // namespace wgt

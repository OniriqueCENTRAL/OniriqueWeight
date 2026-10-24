// Onirique Weight — the sound: a passive program equalizer, and the tubes that make up for it
#pragma once

namespace wgt
{

constexpr int kMaxChannels = 2;

// The switches, as they are engraved on the front.
constexpr int kNumLowFreqs = 4, kNumHighFreqs = 7, kNumAttenFreqs = 3;
constexpr double kLowFreqs[kNumLowFreqs] = { 20.0, 30.0, 60.0, 100.0 };
constexpr double kHighFreqs[kNumHighFreqs] = { 3000.0, 4000.0, 5000.0, 8000.0, 10000.0, 12000.0, 16000.0 };
constexpr double kAttenFreqs[kNumAttenFreqs] = { 5000.0, 10000.0, 20000.0 };
constexpr const char* kLowFreqNames[kNumLowFreqs] = { "20", "30", "60", "100" };
constexpr const char* kHighFreqNames[kNumHighFreqs] = { "3", "4", "5", "8", "10", "12", "16" };
constexpr const char* kAttenFreqNames[kNumAttenFreqs] = { "5", "10", "20" };

// What each dial does at 10, as on the original's data sheet.
constexpr double kLowBoostDb = 13.5, kLowAttenDb = 17.5, kHighBoostDb = 18.0, kHighAttenDb = 16.0;

/** The front panel. The dials go from 0 to 10, as engraved. */
struct Settings
{
    float lowBoost = 0.0f, lowAtten = 0.0f;
    int lowFreq = 2;                    // 60 Hz
    float highBoost = 0.0f, bandwidth = 5.0f;
    int highFreq = 4;                   // 10 kHz
    float highAtten = 0.0f;
    int attenFreq = 1;                  // 10 kHz
    bool eqIn = true;                   // out: the curves go flat, the tubes stay
    bool bypass = false;                // the power switch off: the signal as it came in
};

/** A second-order section, coefficients normalised; one state per channel. */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double s1[kMaxChannels] = {}, s2[kMaxChannels] = {};

    double process (double x, int c) noexcept
    {
        const auto y = b0 * x + s1[c];
        s1[c] = b1 * x - a1 * y + s2[c];
        s2[c] = b2 * x - a2 * y;
        return y;
    }

    double magnitude (double w) const noexcept;
    void clear() noexcept;
};

/** One channel or two, linked: the curves of the passive network, then the
    make-up stage. That stage only colours what lies under the highs (the
    tube) and the bass (the weight, which grows with the low boost), so the
    top end stays clean and open however hard the lows are pushed. */
class Weight
{
public:
    /** Settings set before this are taken at once, without gliding to them. */
    void prepare (double sampleRate);
    void reset() noexcept;

    void setSettings (const Settings& s) noexcept { target = s; }
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    /** The curve as it stands, in dB at a frequency: the network only. */
    double responseDb (double hz) const noexcept;

private:
    enum { lowBoost, lowAtten, highBoost, highAtten, bandwidth, numDials };

    void design() noexcept;

    static constexpr int kChunk = 32;   // samples between two looks at the dials

    double fs = 88200.0;
    Settings target;
    double dials[numDials] = {}, glide = 0.0;
    int freqs[3] = { -1, -1, -1 };

    Biquad boostShelf, attenShelf, bell, air, topCut;

    // The make-up stage, per channel: what feeds the tube and the weight,
    // and the DC its bias leaves behind.
    double toneSplit = 0.0, bassSplit = 0.0, dcPole = 0.0;
    double tubeIn[kMaxChannels] = {}, bassIn[kMaxChannels] = {};
    double dcX[kMaxChannels] = {}, dcY[kMaxChannels] = {};

    double wet = 1.0, wetStep = 0.0;
};

} // namespace wgt

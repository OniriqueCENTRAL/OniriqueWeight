// Renders the editor to a PNG without a host, so a change to the front can be
// looked at straight from the build. A development tool, not part of the product.
//
//   weight_snapshot out.png [scale]
//   weight_snapshot --selftest       the plugin as a host drives it: blocks, twice the rate, bypass
#include "../Source/PluginEditor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
/** A tone through the whole plugin, in blocks larger than it was prepared for;
    what comes out at that frequency, in dB against what went in. */
double through (juce::AudioProcessor& processor, double hz, bool& finite)
{
    constexpr double sr = 48000.0;
    constexpr int block = 700;
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    double re = 0.0, im = 0.0;
    int n = 0;

    for (int b = 0, t = 0; b < 60; ++b)
    {
        for (int i = 0; i < block; ++i)
            for (int c = 0; c < 2; ++c)
                buffer.setSample (c, i, 0.1f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (t + i) / sr));

        processor.processBlock (buffer, midi);

        for (int i = 0; i < block; ++i)
        {
            const auto v = buffer.getSample (0, i);
            finite = finite && std::isfinite (v);

            if (b >= 30)
            {
                const auto phase = 2.0 * juce::MathConstants<double>::pi * hz * (t + i) / sr;
                re += v * std::cos (phase);
                im += v * std::sin (phase);
                ++n;
            }
        }

        t += block;
    }

    return 20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / n / 0.1);
}

int selfTest (juce::AudioProcessor& processor, juce::AudioProcessorValueTreeState& state)
{
    auto set = [&] (const char* id, float value)
    {
        auto* parameter = state.getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    int failures = 0;
    auto check = [&] (bool ok, const char* what)
    {
        std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what);
        failures += ok ? 0 : 1;
    };

    bool finite = true;
    std::printf ("  latency: %d samples\n", processor.getLatencySamples());
    check (processor.getLatencySamples() > 0, "the delay of the doubled rate is told to the host");

    const auto flat = through (processor, 1000.0, finite);
    std::printf ("  flat, 1 kHz: %+.2f dB\n", flat);
    check (std::abs (flat) < 0.1, "flat, it comes out as it went in");

    set ("lowboost", 10.0f);
    const auto boosted = through (processor, 30.0, finite);
    std::printf ("  low boost 10 at 60, 30 Hz: %+.2f dB\n", boosted);
    check (boosted > 12.0 && boosted < 15.0, "the low boost is heard through the plugin");

    const auto top = through (processor, 18000.0, finite);
    std::printf ("  and 18 kHz: %+.2f dB\n", top);
    check (std::abs (top) < 0.5, "the top end comes back down from the doubled rate intact");

    set ("bypass", 1.0f);
    const auto bypassed = through (processor, 30.0, finite);
    std::printf ("  power off, 30 Hz: %+.2f dB\n", bypassed);
    check (std::abs (bypassed) < 0.1, "power off: as it came in");
    check (finite, "nothing blows up");

    return failures == 0 ? 0 : 1;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png");
    const auto scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->setPlayConfigDetails (2, 2, 48000.0, 512);
    processor->prepareToPlay (48000.0, 512);

    // Something worth looking at: the low-end trick, and some air.
    auto& weight = static_cast<onirique::WeightProcessor&> (*processor);
    auto& state = weight.apvts;

    if (juce::String (argc > 1 ? argv[1] : "") == "--selftest")
        return selfTest (weight, state);

    for (auto [id, value] : { std::pair ("lowboost", 7.0f), std::pair ("lowatten", 4.0f), std::pair ("lowfreq", 2.0f),
                              std::pair ("highboost", 5.0f), std::pair ("bandwidth", 6.0f), std::pair ("highfreq", 4.0f),
                              std::pair ("highatten", 2.0f), std::pair ("attensel", 2.0f) })
    {
        auto* parameter = state.getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (weight.createEditor());
        editor->setSize (onirique::WeightEditor::kWidth, onirique::WeightEditor::kHeight);

        // This machine may hold no licence for the plugin: the activation page
        // is not what is being looked at.
        for (auto* child : editor->getChildren())
            if (dynamic_cast<onirique::license::Overlay*> (child) != nullptr)
                child->setVisible (false);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);

        file.deleteFile();
        juce::FileOutputStream stream (file);

        if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
            return 1;
    }

    std::printf ("%s\n", file.getFullPathName().toRawUTF8());
    return 0;
}

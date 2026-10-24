#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace onirique
{

namespace
{
    using Attributes = AudioParameterFloatAttributes;

    float parseNumber (const String& text)
    {
        return text.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getFloatValue();
    }

    /** A dial reads as engraved, and says what it does at that point. */
    Attributes dial (double maxDb, bool cut)
    {
        return Attributes()
            .withStringFromValueFunction ([maxDb, cut] (float v, int)
                                          {
                                              const auto db = maxDb * v * 0.1;
                                              return String (v, 1) + "  ·  " + (db < 0.05 ? "0" : (cut ? "-" : "+") + String (db, 1)) + " dB";
                                          })
            .withValueFromStringFunction ([] (const String& t) { return parseNumber (t.upToFirstOccurrenceOf ("·", false, false)); });
    }

    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        AudioProcessorValueTreeState::ParameterLayout layout;
        const wgt::Settings defaults;
        const NormalisableRange<float> dialRange { 0.0f, 10.0f, 0.01f };

        auto number = [&] (const char* id, const char* name, float value, const Attributes& attributes)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, dialRange, value, attributes));
        };

        auto choice = [&] (const char* id, const char* name, const auto& names, const char* unit, int value)
        {
            StringArray choices;

            for (auto* n : names)
                choices.add (String (n) + " " + unit);

            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, value));
        };

        // The low end, as on the front: boost and atten, then the frequency they share.
        number ("lowboost", "Low Boost", defaults.lowBoost, dial (wgt::kLowBoostDb, false));
        number ("lowatten", "Low Atten", defaults.lowAtten, dial (wgt::kLowAttenDb, true));
        choice ("lowfreq", "Low Frequency", wgt::kLowFreqNames, "Hz", defaults.lowFreq);

        // The highs: the boost and its bandwidth, and the shelf that cuts.
        number ("highboost", "High Boost", defaults.highBoost, dial (wgt::kHighBoostDb, false));
        number ("bandwidth", "Bandwidth", defaults.bandwidth,
                Attributes().withStringFromValueFunction ([] (float v, int)
                                                          {
                                                              return String (v, 1) + (v < 3.0f ? "  ·  sharp" : v > 7.0f ? "  ·  broad" : "");
                                                          })
                            .withValueFromStringFunction ([] (const String& t) { return parseNumber (t); }));
        choice ("highfreq", "High Frequency", wgt::kHighFreqNames, "kHz", defaults.highFreq);
        number ("highatten", "High Atten", defaults.highAtten, dial (wgt::kHighAttenDb, true));
        choice ("attensel", "Atten Select", wgt::kAttenFreqNames, "kHz", defaults.attenFreq);

        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "eqin", 1 }, "EQ In", defaults.eqIn));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", defaults.bypass));

        return layout;
    }
}

//==============================================================================
WeightProcessor::WeightProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Weight", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    lowBoost = get ("lowboost");   lowAtten = get ("lowatten");   lowFreq = get ("lowfreq");
    highBoost = get ("highboost"); bandwidth = get ("bandwidth"); highFreq = get ("highfreq");
    highAtten = get ("highatten"); attenFreq = get ("attensel");  eqIn = get ("eqin");
    bypass = get ("bypass");

    bypassParameter = apvts.getParameter ("bypass");
}

bool WeightProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

wgt::Settings WeightProcessor::readSettings() const noexcept
{
    wgt::Settings s;

    s.lowBoost = lowBoost->load();
    s.lowAtten = lowAtten->load();
    s.lowFreq = roundToInt (lowFreq->load());
    s.highBoost = highBoost->load();
    s.bandwidth = bandwidth->load();
    s.highFreq = roundToInt (highFreq->load());
    s.highAtten = highAtten->load();
    s.attenFreq = roundToInt (attenFreq->load());
    s.eqIn = eqIn->load() > 0.5f;
    s.bypass = bypass->load() > 0.5f;

    return s;
}

void WeightProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    license.refresh();

    // Twice the rate, through half-band filters whose delay is a whole number
    // of samples, so the host can make up for it exactly.
    maxBlock = jmax (1, samplesPerBlock);
    oversampling = std::make_unique<dsp::Oversampling<float>> ((size_t) jmax (1, getMainBusNumOutputChannels()), 1,
                                                               dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    setLatencySamples (roundToInt (oversampling->getLatencyInSamples()));

    weight.setSettings (readSettings());
    weight.prepare (sampleRate * 2.0);
}

void WeightProcessor::reset()
{
    weight.reset();

    if (oversampling != nullptr)
        oversampling->reset();
}

void WeightProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const auto numChannels = jmin (getMainBusNumOutputChannels(), wgt::kMaxChannels, buffer.getNumChannels());
    const auto numSamples = buffer.getNumSamples();

    if (oversampling == nullptr || numChannels < 1)
        return;

    weight.setSettings (readSettings());

    // Even bypassed, the sound goes up and down again: the delay the host
    // makes up for stays the same, and switching makes no jump.
    dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);

    // A host may send more than it announced: in pieces the filters were made for.
    for (int at = 0; at < numSamples; at += maxBlock)
    {
        auto part = block.getSubBlock ((size_t) at, (size_t) jmin (maxBlock, numSamples - at));
        auto up = oversampling->processSamplesUp (part);

        float* channels[wgt::kMaxChannels] = {};

        for (int c = 0; c < numChannels; ++c)
            channels[c] = up.getChannelPointer ((size_t) c);

        weight.process (channels, numChannels, (int) up.getNumSamples());
        oversampling->processSamplesDown (part);
    }

    // Last, and over every output: what an unlicensed plugin lets through.
    license.process (buffer, getSampleRate(), isNonRealtime());
}

AudioProcessorEditor* WeightProcessor::createEditor()
{
    return new WeightEditor (*this);
}

//==============================================================================
void WeightProcessor::getStateInformation (MemoryBlock& destination)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void WeightProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (ValueTree::fromXml (*xml));
}

} // namespace onirique

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new onirique::WeightProcessor();
}

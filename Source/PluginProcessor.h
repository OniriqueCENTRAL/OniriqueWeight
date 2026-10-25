// Onirique Weight — plugin entry point
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "License/LicenseGuard.h"
#include "dsp/Weight.h"
#include "ui/OniriquePresets.h"

namespace onirique
{

using namespace juce;

/** The plugin instance: the front panel's switches and dials in, and the
    block through the equalizer at twice the host's rate, so that the curves
    keep their shape right up to the top and the tubes do not fold back. */
class WeightProcessor final : public AudioProcessor
{
public:
    WeightProcessor();
    ~WeightProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override     { return false; }
    bool producesMidi() const override    { return false; }
    bool isMidiEffect() const override    { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    int getNumPrograms() override            { return 1; }
    int getCurrentProgram() override         { return 0; }
    void setCurrentProgram (int) override    {}
    const String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    AudioProcessorValueTreeState apvts;

    /** The stock presets, the user's, the A/B comparison: what the bar over the front shows. */
    PresetBank presets;

    /** The licence ONIRIQUE CENTRAL wrote on this machine for this plugin, under
        the identifier the plugin has in its catalogue. */
    onirique::license::Guard license { "onirique-weight" };

private:
    wgt::Settings readSettings() const noexcept;

    wgt::Weight weight;
    std::unique_ptr<dsp::Oversampling<float>> oversampling;
    int maxBlock = 0;

    std::atomic<float> *lowBoost, *lowAtten, *lowFreq, *highBoost, *bandwidth, *highFreq, *highAtten, *attenFreq,
                       *eqIn, *bypass;
    AudioProcessorParameter* bypassParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeightProcessor)
};

} // namespace onirique

// Onirique Weight — editor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "ui/Theme.h"

namespace onirique
{

/** The Onirique header over the front of an old program equalizer: the same
    switches in the same places, the low end on the left, the highs on the
    right, the power and its lamp at the end. The whole front is drawn at one
    size and scaled as one with the window. */
class WeightEditor final : public AudioProcessorEditor,
                           private Timer
{
public:
    explicit WeightEditor (WeightProcessor&);
    ~WeightEditor() override;

    void resized() override;

    static constexpr int kWidth = 1200, kHeaderHeight = 64, kPanelHeight = 331;
    static constexpr int kHeight = kHeaderHeight + kPanelHeight;

private:
    struct Front final : Component
    {
        std::function<void (Graphics&)> onPaint;
        void paint (Graphics& g) override { onPaint (g); }
    };

    /** A control of the front: what it is called in the header's readout,
        and where it sits on the panel. */
    struct Part
    {
        Component* component;
        const char* id;
        const char* name;
        Point<float> centre;   // on the panel, under the header
        float size;
    };

    void timerCallback() override;
    void paintHeader (Graphics&);
    void paintPanel (Graphics&);

    WeightProcessor& processor;
    theme::WeightLookAndFeel lookAndFeel;
    license::theme::Fonts fonts;

    Front front;
    Slider lowBoost, lowAtten, lowFreq, highBoost, bandwidth, highFreq, highAtten, attenSel;
    ToggleButton eqIn, power;
    std::vector<Part> parts;

    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    /** What the header's readout shows: the part in hand, or under the pointer. */
    String readout;
    bool lampLit = true;

    /** The sigil: with its ember glow for the header, plain for the panel. */
    Image logo, sigil;
    double openedAt = Time::getMillisecondCounterHiRes();

    TooltipWindow tooltips { this, 2000 };

    /** The licence badge and its activation page. Declared last and added
        last: it lies over everything else. */
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeightEditor)
};

} // namespace onirique

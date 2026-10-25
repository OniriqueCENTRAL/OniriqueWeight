// Onirique Weight — editor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "ui/Layout.h"
#include "ui/OniriqueBar.h"
#include "ui/Theme.h"

namespace onirique
{

/** The front of an old program equalizer, painted red, shown as photographs:
    the front itself, its lamp lit or not, and each part in the position its
    parameter is at (tools/photo.cpp renders them). The controls lie unseen
    over their parts. The whole front is drawn at one size and scaled as one
    with the window, the bar over it (ui/OniriqueBar.h) with it. */
class WeightEditor final : public AudioProcessorEditor,
                           private Timer
{
public:
    explicit WeightEditor (WeightProcessor&);
    ~WeightEditor() override;

    void resized() override;

    static constexpr int kWidth = layout::kWidth, kHeight = layout::kHeight;
    static constexpr int kBarHeight = 50, kTotalHeight = kBarHeight + kHeight;

private:
    struct Front final : Component
    {
        std::function<void (Graphics&)> onPaint, onPaintOver;
        void paint (Graphics& g) override { onPaint (g); }
        void paintOverChildren (Graphics& g) override { onPaintOver (g); }
    };

    /** A control of the front, the component that takes the pointer for it,
        and the photographs of its part. */
    struct Part
    {
        const layout::Control* control;
        Component* component;
        Image photos;
    };

    void timerCallback() override;
    void paintFront (Graphics&);
    void paintReadout (Graphics&);

    WeightProcessor& processor;
    theme::WeightLookAndFeel lookAndFeel;
    license::theme::Fonts fonts;

    Front front;
    nuit::Bar bar;
    Slider lowBoost, lowAtten, lowFreq, highBoost, bandwidth, highFreq, highAtten, attenSel;
    ToggleButton eqIn, power;
    std::vector<Part> parts;

    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    /** The front, lamp off and lit. */
    Image frontOff, frontOn;

    /** What the parts were last drawn at: a change is drawn again. */
    std::vector<float> shownValues;

    /** What the control in hand, or under the pointer, is set to; shown beside it. */
    const Part* shown = nullptr;
    String readout;

    TooltipWindow tooltips { this, 2000 };

    /** The licence badge and its activation page. Declared last and added
        last: it lies over everything else. */
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeightEditor)
};

} // namespace onirique

// Onirique Weight — editor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "ui/Theme.h"

namespace onirique
{

/** The front of an old program equalizer, painted red: the same switches in
    the same places, the low end on the left, the highs on the right, the
    power and its lamp at the end. The whole front is drawn at one size and
    scaled as one with the window. */
class WeightEditor final : public AudioProcessorEditor,
                           private Timer
{
public:
    explicit WeightEditor (WeightProcessor&);
    ~WeightEditor() override;

    void resized() override;

    static constexpr int kWidth = 1200, kHeight = 331;

private:
    struct Front final : Component
    {
        std::function<void (Graphics&)> onPaint, onPaintOver;
        void paint (Graphics& g) override { onPaint (g); }
        void paintOverChildren (Graphics& g) override { onPaintOver (g); }
    };

    /** A control of the front: what it is called in its readout, and where it sits. */
    struct Part
    {
        Component* component;
        const char* id;
        const char* name;
        Point<float> centre;
        float size;
    };

    void timerCallback() override;
    void paintPanel (Graphics&);
    void paintReadout (Graphics&);

    WeightProcessor& processor;
    theme::WeightLookAndFeel lookAndFeel;
    license::theme::Fonts fonts;

    Front front;
    Slider lowBoost, lowAtten, lowFreq, highBoost, bandwidth, highFreq, highAtten, attenSel;
    ToggleButton eqIn, power;
    std::vector<Part> parts;

    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    /** What the control in hand, or under the pointer, is set to; shown beside it. */
    const Part* shown = nullptr;
    String readout;
    bool lampLit = true;

    /** The sigil, white, as printed on the maker's plate; and the grain of the paint. */
    Image sigil, grain;

    TooltipWindow tooltips { this, 2000 };

    /** The licence badge and its activation page. Declared last and added
        last: it lies over everything else. */
    std::unique_ptr<onirique::license::Overlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WeightEditor)
};

} // namespace onirique

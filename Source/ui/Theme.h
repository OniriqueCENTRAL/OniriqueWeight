// Onirique Weight — the look: an old broadcast rack front, painted Onirique red
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Render3D.h"

namespace onirique::theme
{

using juce::Colour;

inline const Colour ember   { 0xffe23444 };
inline const Colour mist    { 0xfff6eeee };

// The rack front: the muted slate of the old broadcast racks, in red, with
// white screen-printed lettering.
inline const Colour slate     { 0xff5c3f42 };
inline const Colour slateDeep { 0xff4a3235 };
inline const Colour engraving { 0xffece9e4 };

inline juce::Font displayFont (float height, bool heavy = true)
{
    // Named outright: the "default sans-serif" placeholder does not survive
    // being put in FontOptions, and what comes out the other end is a serif.
   #if JUCE_WINDOWS
    const juce::String family ("Segoe UI");
   #elif JUCE_MAC
    const juce::String family ("Helvetica Neue");
   #else
    const juce::String family ("DejaVu Sans");
   #endif

    return juce::Font (juce::FontOptions (family, height, heavy ? juce::Font::bold : juce::Font::plain));
}

/** The controls of the front, each drawn as the part it stands for. A slider's
    "kind" property says which: "dial", "selector" or "bat"; a button's:
    "toggle" or "power". */
class WeightLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    // The description of a control, shown once the pointer has rested on it.
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    /** A part of the front in `square`, rendered for the pixels it covers. */
    void drawPart (juce::Graphics&, render::Part, juce::Rectangle<float> square, float angle);

private:
    render::Cache cache;
};

} // namespace onirique::theme

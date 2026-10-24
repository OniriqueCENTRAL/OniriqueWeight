// Onirique Weight — the look: an old broadcast rack front, under the Onirique header
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace onirique::theme
{

using juce::Colour;

// The palette of the Onirique range: the header and every lit state.
inline const Colour night   { 0xff0d0809 };
inline const Colour ember   { 0xffe23444 };
inline const Colour emberLit{ 0xffff5a68 };
inline const Colour mist    { 0xfff6eeee };
inline const Colour haze    { 0xffa88f92 };

// The rack front: slate-grey steel, white engraving, black bakelite.
inline const Colour slate     { 0xff4b5159 };
inline const Colour slateDeep { 0xff3f444c };
inline const Colour engraving { 0xffe9e8e3 };

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

/** The four-pointed star of the Onirique sigil. */
juce::Path star (juce::Point<float> centre, float radius);

/** The controls of the front, each drawn as the part it stands for. A slider's
    "kind" property says which: "dial" (the fluted knobs), "selector" (the bar
    knobs of the frequency switches), "bat" (the pointed knob of the atten
    selector); a button's: "toggle" (the bat switch), "power" (the round knob). */
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
};

/** What sits on the front without being touched. */
void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float slotAngle);
void drawLamp (juce::Graphics&, juce::Point<float> centre, float radius, bool lit);

} // namespace onirique::theme

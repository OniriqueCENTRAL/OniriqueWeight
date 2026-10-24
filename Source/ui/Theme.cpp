#include "Theme.h"

namespace onirique::theme
{

using namespace juce;

void WeightLookAndFeel::drawPart (Graphics& g, render::Part part, Rectangle<float> square, float angle)
{
    // As many pixels as the screen gives it, whatever the window's zoom.
    const auto pixels = roundToInt (square.getWidth() * g.getInternalContext().getPhysicalPixelScaleFactor());

    // Whatever was drawn just before may have left its transparency behind.
    g.setOpacity (1.0f);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (cache.get (part, pixels, angle), square);
}

void WeightLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                          float position, float startAngle, float endAngle, Slider& slider)
{
    const auto kind = slider.getProperties()["kind"].toString();
    const auto part = kind == "selector" ? render::Part::selector : kind == "bat" ? render::Part::bat : render::Part::dial;
    const auto size = (float) jmin (width, height);

    drawPart (g, part, Rectangle<float> (size, size).withCentre (Rectangle<int> (x, y, width, height).toFloat().getCentre()),
              startAngle + position * (endAngle - startAngle));
}

void WeightLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& button, bool, bool)
{
    const auto area = button.getLocalBounds().toFloat();
    const auto square = Rectangle<float> (jmin (area.getWidth(), area.getHeight()), jmin (area.getWidth(), area.getHeight()))
                            .withCentre (area.getCentre());

    // The power knob holds the bypass: on is off.
    if (button.getProperties()["kind"].toString() == "power")
        drawPart (g, render::Part::power, square, button.getToggleState() ? 0.0f : degreesToRadians (52.0f));
    else
        drawPart (g, render::Part::toggle, square, button.getToggleState() ? 0.0f : MathConstants<float>::pi);
}

//==============================================================================
namespace
{
    constexpr int kTooltipWidth = 250;

    TextLayout layoutTooltip (const String& text)
    {
        AttributedString s;
        s.setJustification (Justification::centredLeft);
        s.append (text, displayFont (12.5f, false), mist);

        TextLayout layout;
        layout.createLayout (s, (float) kTooltipWidth);
        return layout;
    }
}

Rectangle<int> WeightLookAndFeel::getTooltipBounds (const String& text, Point<int> screenPos, Rectangle<int> parentArea)
{
    const auto layout = layoutTooltip (text);
    const auto width = roundToInt (layout.getWidth()) + 22;
    const auto height = roundToInt (layout.getHeight()) + 14;

    // Below and to the right of the pointer, or on the other side when that
    // would run off the edge.
    return Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - width - 12 : screenPos.x + 18,
                           screenPos.y > parentArea.getCentreY() ? screenPos.y - height - 10 : screenPos.y + 18,
                           width, height)
               .constrainedWithin (parentArea);
}

void WeightLookAndFeel::drawTooltip (Graphics& g, const String& text, int width, int height)
{
    const auto bounds = Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setGradientFill (ColourGradient (Colour (0xff3d0911), 0.0f, 0.0f, Colour (0xff1c0508), 0.0f, (float) height, false));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (ember.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

    layoutTooltip (text).draw (g, bounds.reduced (11.0f, 7.0f));
}

} // namespace onirique::theme

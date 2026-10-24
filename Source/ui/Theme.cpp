#include "Theme.h"

namespace onirique::theme
{

using namespace juce;

namespace
{
    constexpr float kTwoPi = MathConstants<float>::twoPi, kHalfPi = MathConstants<float>::halfPi;

    /** A point at `radius` from `centre`, `angle` clockwise from twelve o'clock. */
    Point<float> at (Point<float> centre, float angle, float radius)
    {
        return { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
    }

    /** The shadow a part casts on the panel, the light coming from above. */
    void castShadow (Graphics& g, const Path& shape, float depth)
    {
        DropShadow (Colours::black.withAlpha (0.6f), jmax (2, roundToInt (depth * 1.6f)),
                    { roundToInt (depth * 0.25f), roundToInt (depth) })
            .drawForPath (g, shape);
    }

    void stroke (Graphics& g, Point<float> from, Point<float> to, float width)
    {
        Path line;
        line.startNewSubPath (from);
        line.lineTo (to);
        g.strokePath (line, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
    }

    /** A glossy black dome: the cap of every knob. */
    void fillDome (Graphics& g, Point<float> c, float r)
    {
        g.setGradientFill (ColourGradient (Colour (0xff2c2c30), c.x - r * 0.3f, c.y - r * 0.4f,
                                           Colour (0xff060607), c.x + r * 0.6f, c.y + r * 0.8f, true));
        g.fillEllipse (Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));

        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), c.x, c.y - r,
                                           Colours::transparentWhite, c.x, c.y - r * 0.1f, false));
        g.fillEllipse (Rectangle<float> (r * 1.5f, r * 0.95f).withCentre ({ c.x, c.y - r * 0.45f }));
    }

    /** The big fluted knob of the boost, atten and bandwidth dials. */
    void drawDial (Graphics& g, Point<float> c, float R, float angle, bool hot)
    {
        constexpr int flutes = 10;
        Path skirt;

        for (int i = 0; i <= 360; ++i)
        {
            const auto a = (float) i / 360.0f * kTwoPi;
            const auto p = at (c, a, R * (0.95f + 0.05f * std::cos ((float) flutes * a)));

            if (i == 0)
                skirt.startNewSubPath (p);
            else
                skirt.lineTo (p);
        }

        skirt.closeSubPath();
        castShadow (g, skirt, R * 0.14f);

        g.setGradientFill (ColourGradient (Colour (0xff46464a), c.x - R * 0.55f, c.y - R,
                                           Colour (0xff050506), c.x + R * 0.5f, c.y + R, false));
        g.fillPath (skirt);

        // Each flute: dark in its hollow, catching the light on its crest
        // when it faces up and to the left.
        for (int i = 0; i < flutes; ++i)
        {
            const auto crest = (float) i / (float) flutes * kTwoPi, hollow = crest + kTwoPi / (2.0f * (float) flutes);
            const auto light = jmax (0.0f, std::cos (crest + 0.6f));

            g.setColour (Colours::black.withAlpha (0.35f));
            stroke (g, at (c, hollow, R * 0.74f), at (c, hollow, R * 0.89f), R * 0.05f);
            g.setColour (Colours::white.withAlpha (0.03f + 0.09f * light));
            stroke (g, at (c, crest, R * 0.76f), at (c, crest, R * 0.94f), R * 0.04f);
        }

        // The cap rising from it: a bevel, a groove, the dome.
        const auto cap = R * 0.68f;
        g.setGradientFill (ColourGradient (Colour (0xff3c3c40), c.x, c.y - cap, Colour (0xff08080a), c.x, c.y + cap, false));
        g.fillEllipse (Rectangle<float> (2.0f * cap, 2.0f * cap).withCentre (c));
        g.setColour (Colours::black.withAlpha (0.65f));
        g.drawEllipse (Rectangle<float> (2.0f * cap, 2.0f * cap).withCentre (c), R * 0.02f);

        const auto groove = R * 0.56f;
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawEllipse (Rectangle<float> (2.0f * groove, 2.0f * groove).withCentre (c.translated (0.0f, R * 0.025f)), R * 0.025f);
        g.setColour (Colours::black.withAlpha (0.75f));
        g.drawEllipse (Rectangle<float> (2.0f * groove, 2.0f * groove).withCentre (c), R * 0.03f);

        fillDome (g, c, R * 0.5f);

        // The Onirique star, engraved at its heart, alight while it is handled.
        g.setColour ((hot ? emberLit : ember).withAlpha (hot ? 0.95f : 0.5f));
        g.fillPath (star (c, R * 0.15f));

        // The pointer, painted on the skirt.
        g.setColour (engraving);
        stroke (g, at (c, angle, R * 0.73f), at (c, angle, R * 0.95f), R * 0.06f);

        if (hot)
        {
            g.setColour (ember.withAlpha (0.3f));
            g.drawEllipse (Rectangle<float> (2.0f * R + 5.0f, 2.0f * R + 5.0f).withCentre (c), 1.5f);
        }
    }

    /** The bar knob of a frequency switch, on its round boss. */
    void drawSelector (Graphics& g, Point<float> c, float L, float angle, bool hot)
    {
        const auto boss = Rectangle<float> (L * 0.6f, L * 0.6f).withCentre (c);

        Path bar;
        bar.addRoundedRectangle (c.x - L * 0.15f, c.y - L * 0.62f, L * 0.3f, L, L * 0.07f);
        bar.applyTransform (AffineTransform::rotation (angle, c.x, c.y));

        Path both (bar);
        both.addEllipse (boss);
        castShadow (g, both, L * 0.1f);

        g.setGradientFill (ColourGradient (Colour (0xff36363a), c.x, boss.getY(), Colour (0xff0a0a0b), c.x, boss.getBottom(), false));
        g.fillEllipse (boss);

        // Lit along its left edge, dark along its right.
        ColourGradient fill (Colour (0xff545458), at (c, angle - kHalfPi, L * 0.15f), Colour (0xff040405), at (c, angle + kHalfPi, L * 0.15f), false);
        fill.addColour (0.35, Colour (0xff1c1c1f));
        g.setGradientFill (fill);
        g.fillPath (bar);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.strokePath (bar, PathStrokeType (1.0f));

        g.setColour (hot ? Colours::white : engraving.withAlpha (0.85f));
        stroke (g, at (c, angle, L * 0.55f), at (c, angle, L * 0.08f), L * 0.035f);

        if (hot)
        {
            g.setColour (ember.withAlpha (0.8f));
            g.fillPath (star (at (c, angle, L * 0.55f), L * 0.06f));
        }
    }

    /** The pointed knob of the atten selector. */
    void drawBat (Graphics& g, Point<float> c, float L, float angle, bool hot)
    {
        Path bat;
        bat.startNewSubPath (c.x, c.y - L * 0.66f);
        bat.quadraticTo (c.x + L * 0.09f, c.y - L * 0.3f, c.x + L * 0.16f, c.y + L * 0.04f);
        bat.quadraticTo (c.x + L * 0.26f, c.y + L * 0.34f, c.x, c.y + L * 0.34f);
        bat.quadraticTo (c.x - L * 0.26f, c.y + L * 0.34f, c.x - L * 0.16f, c.y + L * 0.04f);
        bat.quadraticTo (c.x - L * 0.09f, c.y - L * 0.3f, c.x, c.y - L * 0.66f);
        bat.closeSubPath();
        bat.applyTransform (AffineTransform::rotation (angle, c.x, c.y));

        const auto boss = Rectangle<float> (L * 0.62f, L * 0.62f).withCentre (c);
        Path both (bat);
        both.addEllipse (boss);
        castShadow (g, both, L * 0.1f);

        g.setGradientFill (ColourGradient (Colour (0xff36363a), c.x, boss.getY(), Colour (0xff0a0a0b), c.x, boss.getBottom(), false));
        g.fillEllipse (boss);

        ColourGradient fill (Colour (0xff4e4e52), at (c, angle - kHalfPi, L * 0.2f), Colour (0xff040405), at (c, angle + kHalfPi, L * 0.2f), false);
        fill.addColour (0.45, Colour (0xff1a1a1d));
        g.setGradientFill (fill);
        g.fillPath (bat);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.strokePath (bat, PathStrokeType (1.0f));

        // The ridge down its middle catches the light.
        g.setColour (Colours::white.withAlpha (hot ? 0.55f : 0.28f));
        stroke (g, at (c, angle, L * 0.6f), at (c, angle, L * 0.02f), L * 0.025f);
    }

    /** The bat-handled toggle on its knurled nut, up or down. */
    void drawToggle (Graphics& g, Rectangle<float> area, bool up, bool hot)
    {
        const auto S = jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const auto nut = Rectangle<float> (S * 0.46f, S * 0.46f).withCentre (c);

        Path nutShape;
        nutShape.addEllipse (nut);
        castShadow (g, nutShape, S * 0.05f);

        g.setGradientFill (ColourGradient (Colour (0xffe4e4e2), nut.getX(), nut.getY(), Colour (0xff5a5d62), nut.getRight(), nut.getBottom(), false));
        g.fillEllipse (nut);

        // The knurling round its edge.
        for (int i = 0; i < 40; ++i)
        {
            const auto a = (float) i / 40.0f * kTwoPi;
            g.setColour (Colours::black.withAlpha (i % 2 == 0 ? 0.35f : 0.12f));
            stroke (g, at (c, a, S * 0.19f), at (c, a, S * 0.225f), 1.0f);
        }

        const auto ring = nut.reduced (S * 0.065f);
        g.setGradientFill (ColourGradient (Colour (0xff8d9095), ring.getX(), ring.getY(), Colour (0xffd8d9da), ring.getRight(), ring.getBottom(), false));
        g.fillEllipse (ring);
        g.setColour (Colour (0xff0b0b0c));
        g.fillEllipse (Rectangle<float> (S * 0.15f, S * 0.15f).withCentre (c));

        // The handle, tapering out to its ball.
        const auto dir = up ? -1.0f : 1.0f;
        const auto tip = c.translated (0.0f, dir * S * 0.4f);

        Path handle;
        handle.startNewSubPath (c.x - S * 0.035f, c.y);
        handle.lineTo (tip.x - S * 0.05f, tip.y);
        handle.lineTo (tip.x + S * 0.05f, tip.y);
        handle.lineTo (c.x + S * 0.035f, c.y);
        handle.closeSubPath();
        handle.addEllipse (Rectangle<float> (S * 0.15f, S * 0.15f).withCentre (tip));
        castShadow (g, handle, S * 0.06f);

        g.setGradientFill (ColourGradient (Colour (0xff4a4a4e), tip.x - S * 0.06f, tip.y, Colour (0xff050506), tip.x + S * 0.06f, tip.y, false));
        g.fillPath (handle);
        g.setColour (Colours::white.withAlpha (hot ? 0.5f : 0.3f));
        g.fillEllipse (Rectangle<float> (S * 0.05f, S * 0.035f).withCentre (tip.translated (-S * 0.02f, -S * 0.03f)));
    }

    /** The small round knob of the power switch. */
    void drawPowerKnob (Graphics& g, Point<float> c, float r, bool on, bool hot)
    {
        Path body;
        body.addEllipse (Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
        castShadow (g, body, r * 0.25f);

        g.setColour (Colour (0xff070708));
        g.fillPath (body);
        fillDome (g, c, r * 0.92f);

        const auto angle = on ? degreesToRadians (52.0f) : 0.0f;
        g.setColour (hot ? Colours::white : engraving);
        stroke (g, at (c, angle, r * 0.45f), at (c, angle, r * 0.9f), r * 0.12f);
    }
}

//==============================================================================
Path star (Point<float> centre, float radius)
{
    Path p;
    const auto waist = radius * 0.16f;

    p.startNewSubPath (centre.x, centre.y - radius);
    p.lineTo (centre.x + waist, centre.y - waist);
    p.lineTo (centre.x + radius, centre.y);
    p.lineTo (centre.x + waist, centre.y + waist);
    p.lineTo (centre.x, centre.y + radius);
    p.lineTo (centre.x - waist, centre.y + waist);
    p.lineTo (centre.x - radius, centre.y);
    p.lineTo (centre.x - waist, centre.y - waist);
    p.closeSubPath();
    return p;
}

void WeightLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                          float position, float startAngle, float endAngle, Slider& slider)
{
    const auto area = Rectangle<int> (x, y, width, height).toFloat();
    const auto size = jmin (area.getWidth(), area.getHeight());
    const auto angle = startAngle + position * (endAngle - startAngle);
    const auto hot = slider.isMouseOverOrDragging();
    const auto kind = slider.getProperties()["kind"].toString();

    // Each sized so that it, and the shadow it casts, stay in its square.
    if (kind == "selector")
        drawSelector (g, area.getCentre(), size * 0.72f, angle, hot);
    else if (kind == "bat")
        drawBat (g, area.getCentre(), size * 0.68f, angle, hot);
    else
        drawDial (g, area.getCentre(), size * 0.42f, angle, hot);
}

void WeightLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& button, bool highlighted, bool down)
{
    const auto area = button.getLocalBounds().toFloat();
    const auto hot = highlighted || down;

    // The power knob holds the bypass: on is off.
    if (button.getProperties()["kind"].toString() == "power")
        drawPowerKnob (g, area.getCentre(), jmin (area.getWidth(), area.getHeight()) * 0.36f, ! button.getToggleState(), hot);
    else
        drawToggle (g, area, button.getToggleState(), hot);
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

//==============================================================================
void drawScrew (Graphics& g, Point<float> c, float r, float slotAngle)
{
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (2.0f * r + 3.0f, 2.0f * r + 3.0f).withCentre (c.translated (0.5f, 1.5f)));

    g.setGradientFill (ColourGradient (Colour (0xffdcdddf), c.x - r * 0.4f, c.y - r * 0.5f, Colour (0xff60636a), c.x + r, c.y + r, true));
    g.fillEllipse (Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawEllipse (Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c), 1.0f);

    // The slot, with the light catching its lower lip.
    const auto along = Point<float> (std::cos (slotAngle), std::sin (slotAngle)) * (r * 0.85f);
    g.setColour (Colours::white.withAlpha (0.35f));
    stroke (g, c - along + Point<float> (0.0f, 1.0f), c + along + Point<float> (0.0f, 1.0f), r * 0.22f);
    g.setColour (Colour (0xff2a2b2e));
    stroke (g, c - along, c + along, r * 0.22f);
}

void drawLamp (Graphics& g, Point<float> c, float r, bool lit)
{
    // The glow it throws on the panel when it is on.
    if (lit)
    {
        g.setGradientFill (ColourGradient (Colour (0xffff3040).withAlpha (0.45f), c.x, c.y,
                                           Colour (0x00ff3040), c.x + r * 3.2f, c.y, true));
        g.fillEllipse (Rectangle<float> (r * 6.4f, r * 6.4f).withCentre (c));
    }

    // The bezel.
    const auto bezel = Rectangle<float> (r * 2.9f, r * 2.9f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (bezel.expanded (1.5f).translated (0.5f, 2.0f));
    g.setGradientFill (ColourGradient (Colour (0xffd9dadc), bezel.getX(), bezel.getY(), Colour (0xff4c4f55), bezel.getRight(), bezel.getBottom(), false));
    g.fillEllipse (bezel);
    g.setColour (Colour (0xff1b1c1e));
    g.fillEllipse (Rectangle<float> (r * 2.25f, r * 2.25f).withCentre (c));

    // The jewel, cut in facets.
    const auto jewel = Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c);
    ColourGradient fill (lit ? Colour (0xffffd6d6) : Colour (0xff6a1a20), c.x - r * 0.3f, c.y - r * 0.35f,
                         lit ? Colour (0xff6e0008) : Colour (0xff140305), c.x + r, c.y + r, true);
    fill.addColour (0.35, lit ? Colour (0xffff2a38) : Colour (0xff3a080d));
    g.setGradientFill (fill);
    g.fillEllipse (jewel);

    for (int i = 0; i < 8; ++i)
    {
        const auto a = (float) i / 8.0f * kTwoPi + 0.2f;
        g.setColour (Colours::white.withAlpha (lit ? 0.16f : 0.06f));
        stroke (g, at (c, a, r * 0.3f), at (c, a, r * 0.95f), 0.8f);
    }

    g.setColour (Colours::white.withAlpha (lit ? 0.85f : 0.3f));
    g.fillEllipse (Rectangle<float> (r * 0.55f, r * 0.38f).withCentre ({ c.x - r * 0.35f, c.y - r * 0.4f }));
}

} // namespace onirique::theme

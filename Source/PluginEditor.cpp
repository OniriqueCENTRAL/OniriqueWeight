#include "PluginEditor.h"
#include "ui/OniriqueWindowSize.h"

#include <BinaryData.h>

namespace onirique
{

using namespace theme;

namespace
{
    constexpr float kDialRadius = 61.0f;    // where a dial's numbers sit
    constexpr float kSwitchRadius = 66.0f;  // and a switch's

    Image makeSigil()
    {
        const auto source = ImageCache::getFromMemory (BinaryData::sigil_png, BinaryData::sigil_pngSize);

        if (! source.isValid())
            return {};

        // Twice the size it is drawn at, so it stays sharp on a scaled display.
        constexpr int width = 88;
        Image result (Image::ARGB, width, width * source.getHeight() / source.getWidth(), true);
        Graphics g (result);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (source, result.getBounds().toFloat());
        return result;
    }

    /** The fine grain of the paint: a tile of noise, laid one grain to a screen pixel. */
    Image makeGrain()
    {
        Image tile (Image::ARGB, 128, 128, true);
        Random random (7);

        for (int y = 0; y < tile.getHeight(); ++y)
            for (int x = 0; x < tile.getWidth(); ++x)
                tile.setPixelAt (x, y, (random.nextBool() ? Colours::white : Colours::black).withAlpha (0.05f * random.nextFloat()));

        return tile;
    }

    // Angles in degrees, clockwise from twelve o'clock, as the scales are printed.
    Point<float> around (Point<float> centre, float degrees, float radius)
    {
        const auto a = degreesToRadians (degrees);
        return { centre.x + radius * std::sin (a), centre.y - radius * std::cos (a) };
    }

    // Where the lamp sits, and how large its bezel is.
    const Point<float> kLamp { 967.0f, 178.0f };
    constexpr float kLampRadius = 21.0f;
}

WeightEditor::WeightEditor (WeightProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      sigil (makeSigil()),
      grain (makeGrain())
{
    setLookAndFeel (&lookAndFeel);

    front.onPaint = [this] (Graphics& g) { paintPanel (g); };
    front.onPaintOver = [this] (Graphics& g) { paintReadout (g); };
    front.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (front);

    // Where everything sits on the front, as on the original's.
    parts = {
        { &lowBoost,  "lowboost",  "LOW BOOST",      { 341.0f, 85.0f },  120.0f },
        { &lowAtten,  "lowatten",  "LOW ATTEN",      { 495.0f, 85.0f },  120.0f },
        { &highBoost, "highboost", "HIGH BOOST",     { 701.0f, 85.0f },  120.0f },
        { &highAtten, "highatten", "HIGH ATTEN",     { 857.0f, 85.0f },  120.0f },
        { &bandwidth, "bandwidth", "BANDWIDTH",      { 599.0f, 248.0f }, 120.0f },
        { &lowFreq,   "lowfreq",   "LOW FREQUENCY",  { 416.0f, 250.0f }, 116.0f },
        { &highFreq,  "highfreq",  "HIGH FREQUENCY", { 778.0f, 252.0f }, 116.0f },
        { &attenSel,  "attensel",  "ATTEN SEL",      { 1007.0f, 92.0f }, 106.0f },
        { &eqIn,      "eqin",      "EQ",             { 265.0f, 239.0f }, 92.0f },
        { &power,     "bypass",    "POWER",          { 1018.0f, 257.0f }, 58.0f },
    };

    auto& state = processor.apvts;

    auto rotary = [&] (Slider& s, const char* id, const char* kind, float fromDegrees, float toDegrees)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        s.setRotaryParameters (MathConstants<float>::twoPi + degreesToRadians (fromDegrees),
                               MathConstants<float>::twoPi + degreesToRadians (toDegrees), true);
        s.getProperties().set ("kind", kind);
        sliderAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, id, s));

        auto* parameter = state.getParameter (id);
        s.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        front.addAndMakeVisible (s);
    };

    rotary (lowBoost, "lowboost", "dial", -135.0f, 135.0f);
    rotary (lowAtten, "lowatten", "dial", -135.0f, 135.0f);
    rotary (highBoost, "highboost", "dial", -135.0f, 135.0f);
    rotary (highAtten, "highatten", "dial", -135.0f, 135.0f);
    rotary (bandwidth, "bandwidth", "dial", -135.0f, 135.0f);
    rotary (lowFreq, "lowfreq", "selector", -46.5f, 46.5f);
    rotary (highFreq, "highfreq", "selector", -90.0f, 90.0f);
    rotary (attenSel, "attensel", "bat", -30.0f, 30.0f);

    eqIn.getProperties().set ("kind", "toggle");
    power.getProperties().set ("kind", "power");

    for (auto* b : { &eqIn, &power })
    {
        buttonAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (state, b == &eqIn ? "eqin" : "bypass", *b));
        front.addAndMakeVisible (*b);
    }

    for (const auto& part : parts)
        part.component->setBounds (Rectangle<float> (part.size, part.size).withCentre (part.centre).toNearestInt());

    // What each control does, for a pointer that rests on it. In French, accents and all.
    auto describe = [] (SettableTooltipClient& c, const char* text) { c.setTooltip (String::fromUTF8 (text)); };

    describe (lowBoost, "Low Boost : le poids. Relève le bas autour de la fréquence choisie, jusqu'à +13,5 dB ; et plus il monte, plus les lampes épaississent la basse de leurs harmoniques chaudes, qui la font entendre même sur de petites enceintes.");
    describe (lowAtten, "Low Atten : retire du bas, jusqu'à -17,5 dB, en pente plus douce que le boost. Les deux ensemble, c'est le tour connu de cet égaliseur : le sub monte, et le bas-médium boueux se creuse juste au-dessus.");
    describe (lowFreq, "CPS : la fréquence du bas, 20, 30, 60 ou 100 Hz, la même pour le boost et l'atten.");
    describe (highBoost, "High Boost : l'air. Une cloche à la fréquence choisie (KCS), prolongée par une étagère très large au-dessus : le haut s'ouvre et reste léger au lieu de durcir. Jusqu'à +18 dB. Les aigus ne passent pas par les lampes : ils restent propres.");
    describe (bandwidth, "Bandwidth : la largeur de la cloche du High Boost, de SHARP (étroite, précise) à BROAD (très large, douce).");
    describe (highFreq, "KCS : la fréquence du High Boost, de 3 à 16 kHz.");
    describe (highAtten, "High Atten : adoucit le haut d'une étagère en pente douce, jusqu'à -16 dB, à la fréquence d'Atten Sel. Avec un boost juste en dessous, il arrondit le sommet du boost.");
    describe (attenSel, "Atten Sel : la fréquence du High Atten, 5, 10 ou 20 kHz.");
    describe (eqIn, "EQ : levier en haut, les courbes sont en service ; en bas, elles sont à plat, mais le son passe toujours par les lampes, pour leur seule couleur.");
    describe (power, "Power : éteint, le signal ressort tel qu'il est entré. La lampe bleue dit que l'appareil est allumé.");

    licenseOverlay = std::make_unique<onirique::license::Overlay> ("WEIGHT", processor.license);
    addAndMakeVisible (*licenseOverlay);

    // The front keeps its proportions; the window grows by zooming it.
    setResizable (true, true);
    setResizeLimits (900, 900 * kHeight / kWidth, 2100, 2100 * kHeight / kWidth);
    getConstrainer()->setFixedAspectRatio ((double) kWidth / (double) kHeight);

    // taille d'ouverture : la règle commune des plugins ONIRIQUE (ui/OniriqueWindowSize.h)
    const auto opening = onirique::window::openingSize ({}, "ONIRIQUE WEIGHT", { kWidth, kHeight },
                                                        { 900, 900 * kHeight / kWidth }, { 2100, 2100 * kHeight / kWidth });
    setSize (opening.x, opening.x * kHeight / kWidth);

    timerCallback();
    startTimerHz (30);
}

WeightEditor::~WeightEditor()
{
    onirique::window::rememberOnThisComputer ("ONIRIQUE WEIGHT", getWidth(), getHeight());
    setLookAndFeel (nullptr);
}

void WeightEditor::resized()
{
    const auto scale = (float) getWidth() / (float) kWidth;

    front.setBounds (0, 0, kWidth, kHeight);
    front.setTransform (AffineTransform::scale (scale));

    // The licence badge sits low on the right of the front, clear of the controls.
    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), scale, { 870, 297, 240, 28 }, Justification::centredRight);
}

void WeightEditor::timerCallback()
{
    // The part in hand first, else the one under the pointer.
    const Part* now = nullptr;

    for (const auto& part : parts)
        if (part.component->isMouseButtonDown())
            now = &part;

    if (now == nullptr)
        for (const auto& part : parts)
            if (part.component->isMouseOver())
                now = &part;

    String text;

    if (now != nullptr)
    {
        auto* parameter = processor.apvts.getParameter (now->id);
        const auto on = parameter->getValue() > 0.5f;

        text << now->name << "   ";

        if (now->component == &eqIn)
            text << (on ? "IN" : "OUT");
        else if (now->component == &power)
            text << (on ? "OFF" : "ON");
        else
            text << parameter->getCurrentValueAsText();
    }

    const auto lit = processor.apvts.getRawParameterValue ("bypass")->load() < 0.5f;

    if (now != shown || text != readout || lit != lampLit)
    {
        shown = now;
        readout = text;
        lampLit = lit;
        front.repaint();
    }
}

//==============================================================================
void WeightEditor::paintPanel (Graphics& g)
{
    const auto panel = Rectangle<float> ((float) kWidth, (float) kHeight);

    // The paint: the muted slate of the old racks, in red, lit from above, its
    // fine grain one to a screen pixel.
    g.setGradientFill (ColourGradient (slate.brighter (0.06f), 0.0f, 0.0f, slateDeep, 0.0f, panel.getBottom(), false));
    g.fillRect (panel);

    const auto pixel = g.getInternalContext().getPhysicalPixelScaleFactor();
    g.setFillType (FillType (grain, AffineTransform::scale (1.0f / pixel)));
    g.fillRect (panel);

    ColourGradient vignette (Colours::transparentBlack, panel.getCentreX(), panel.getCentreY(),
                             Colours::black.withAlpha (0.22f), 0.0f, panel.getCentreY(), true);
    g.setGradientFill (vignette);
    g.fillRect (panel);

    // The plate's edges: lit along the top, in shadow along the foot.
    g.setColour (Colours::white.withAlpha (0.12f));
    g.fillRect (panel.withHeight (1.0f));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (panel.withTrimmedTop (panel.getHeight() - 1.5f));

    // The rack ears: the slots, and the screws.
    for (auto y : { 94.0f, 240.0f })
    {
        for (auto x : { -16.0f, (float) kWidth - 28.0f })
        {
            const auto slot = Rectangle<float> (x, y - 7.5f, 44.0f, 15.0f);
            g.setColour (Colour (0xff070607));
            g.fillRoundedRectangle (slot, 7.5f);
            g.setColour (Colours::white.withAlpha (0.1f));
            g.drawHorizontalLine (roundToInt (slot.getBottom()), slot.getX() + 6.0f, slot.getRight() - 6.0f);
        }
    }

    auto screw = [&] (float x, float y, float slot)
    {
        lookAndFeel.drawPart (g, render::Part::screw, Rectangle<float> (30.0f, 30.0f).withCentre ({ x, y }), slot);
    };

    screw (87.0f, 65.0f, 0.5f);
    screw (87.0f, 281.0f, -0.3f);
    screw (1112.0f, 65.0f, 0.9f);
    screw (1112.0f, 281.0f, 0.2f);

    // The lettering.
    auto text = [&] (const String& s, Point<float> centre, float height, float tracking = 0.04f)
    {
        g.setFont (fonts->bodyFont (height, tracking));
        g.drawText (s, Rectangle<float> (220.0f, height * 1.6f).withCentre (centre), Justification::centred, false);
    };

    g.setColour (engraving);

    // The maker's plate.
    text ("ONIRIQUE", { 164.0f, 124.0f }, 18.0f, 0.12f);
    text ("PROGRAM EQUALIZER", { 164.0f, 148.0f }, 13.0f, 0.05f);
    text ("TYPE W-1", { 164.0f, 170.0f }, 13.0f, 0.08f);

    const auto box = Rectangle<float> (94.0f, 22.0f).withCentre ({ 164.0f, 198.0f });
    g.drawRoundedRectangle (box, 2.0f, 1.2f);
    text ("WEIGHT", box.getCentre(), 13.5f, 0.16f);

    if (sigil.isValid())
    {
        const auto w = (float) sigil.getWidth() * 0.45f, h = (float) sigil.getHeight() * 0.45f;
        g.setOpacity (0.92f);
        g.drawImage (sigil, Rectangle<float> (w, h).withCentre ({ 164.0f, 241.0f }));
        g.setOpacity (1.0f);
    }

    // The dials: 0 to 10 round each, its name above or below.
    auto dialScale = [&] (Point<float> centre)
    {
        for (int i = 0; i <= 10; ++i)
            text (String (i), around (centre, -135.0f + 27.0f * (float) i, kDialRadius), 13.0f, 0.0f);
    };

    for (auto [x, name] : { std::pair (341.0f, "BOOST"), std::pair (495.0f, "ATTEN"), std::pair (701.0f, "BOOST"), std::pair (857.0f, "ATTEN") })
    {
        dialScale ({ x, 85.0f });
        text (name, { x, 7.5f }, 12.5f, 0.08f);
    }

    dialScale ({ 599.0f, 248.0f });
    text ("SHARP", { 548.0f, 303.0f }, 8.5f, 0.08f);
    text ("BROAD", { 652.0f, 303.0f }, 8.5f, 0.08f);
    text ("BANDWIDTH", { 599.0f, 321.0f }, 12.5f, 0.08f);

    // The switches: their positions, their names.
    auto switchScale = [&] (Point<float> centre, const auto& names, float from, float step, float radius)
    {
        auto angle = from;

        for (auto* name : names)
        {
            text (name, around (centre, angle, radius), 13.0f, 0.0f);
            angle += step;
        }
    };

    switchScale (Point<float> (416.0f, 250.0f), wgt::kLowFreqNames, -46.5f, 31.0f, kSwitchRadius);
    text ("CPS", { 418.0f, 163.0f }, 12.5f, 0.08f);
    text ("LOW FREQUENCY", { 418.0f, 321.0f }, 12.5f, 0.08f);

    switchScale (Point<float> (778.0f, 252.0f), wgt::kHighFreqNames, -90.0f, 30.0f, kSwitchRadius);
    text ("KCS", { 780.0f, 164.0f }, 12.5f, 0.08f);
    text ("HIGH FREQUENCY", { 779.0f, 321.0f }, 12.5f, 0.08f);

    switchScale (Point<float> (1007.0f, 92.0f), wgt::kAttenFreqNames, -30.0f, 30.0f, 58.0f);
    text ("ATTEN SEL", { 1007.0f, 9.5f }, 12.5f, 0.08f);

    // The EQ toggle, and the power with its lamp.
    text ("IN", { 265.0f, 180.0f }, 11.0f, 0.1f);
    text ("OUT", { 265.0f, 298.0f }, 11.0f, 0.1f);
    text ("OFF", { 1019.0f, 213.0f }, 12.0f, 0.06f);
    text ("ON", { 1054.0f, 231.0f }, 12.0f, 0.06f);

    // Lit, the lamp throws its blue on the paint round it.
    if (lampLit)
    {
        g.setGradientFill (ColourGradient (Colour (0x7a2f8cff), kLamp, Colour (0x002f8cff), kLamp.translated (kLampRadius * 3.4f, 0.0f), true));
        g.fillEllipse (Rectangle<float> (kLampRadius * 6.8f, kLampRadius * 6.8f).withCentre (kLamp));
    }

    lookAndFeel.drawPart (g, lampLit ? render::Part::lampOn : render::Part::lampOff,
                          Rectangle<float> (kLampRadius * 2.0f / render::reach (render::Part::lampOn),
                                            kLampRadius * 2.0f / render::reach (render::Part::lampOn)).withCentre (kLamp),
                          0.0f);
}

void WeightEditor::paintReadout (Graphics& g)
{
    if (shown == nullptr || readout.isEmpty())
        return;

    // Beside the control: under it, or over it when there is no room below.
    const auto font = fonts->monoBoldFont (12.0f, 0.04f);
    const auto width = GlyphArrangement::getStringWidth (font, readout) + 26.0f;
    const auto bounds = shown->component->getBounds().toFloat();
    auto pill = Rectangle<float> (width, 24.0f).withCentre ({ bounds.getCentreX(), bounds.getBottom() + 14.0f });

    if (pill.getBottom() > (float) kHeight - 2.0f)
        pill.setY (bounds.getY() - 26.0f);

    pill = pill.constrainedWithin (Rectangle<float> ((float) kWidth, (float) kHeight).reduced (4.0f));

    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (pill.translated (0.0f, 2.0f), pill.getHeight() * 0.5f);
    g.setColour (Colour (0xe6140c0d));
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);

    g.setColour (mist);
    g.setFont (font);
    g.drawText (readout, pill, Justification::centred, false);
}

} // namespace onirique

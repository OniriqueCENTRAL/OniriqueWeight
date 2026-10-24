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

    Font titleFont (bool heavy)
    {
        return displayFont (23.0f, heavy).withExtraKerningFactor (0.09f);
    }

    constexpr int kTitleLeft = 72;
    constexpr int kLogoWidth = 44;
    constexpr int kLogoMargin = 8;   // room for the glow around it

    Image makeLogo (bool glowing)
    {
        const auto source = ImageCache::getFromMemory (BinaryData::sigil_png, BinaryData::sigil_pngSize);

        if (! source.isValid())
            return {};

        // Twice the size it is drawn at, so it stays sharp on a scaled display.
        const auto width = 2 * kLogoWidth;
        const auto height = width * source.getHeight() / source.getWidth();
        const auto margin = 2 * kLogoMargin;

        Image padded (Image::ARGB, width + 2 * margin, height + 2 * margin, true);

        {
            Graphics g (padded);
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (source, Rectangle<int> (margin, margin, width, height).toFloat());
        }

        if (! glowing)
            return padded;

        Image result (Image::ARGB, padded.getWidth(), padded.getHeight(), true);
        Graphics g (result);

        GlowEffect glow;
        glow.setGlowProperties (7.0f, ember.withAlpha (0.8f));
        glow.applyEffect (padded, g, 1.0f, 1.0f);

        return result;
    }

    // Angles in degrees, clockwise from twelve o'clock, as the scales are engraved.
    float radians (float degrees) { return degreesToRadians (degrees); }

    Point<float> around (Point<float> centre, float degrees, float radius)
    {
        return { centre.x + radius * std::sin (radians (degrees)), centre.y - radius * std::cos (radians (degrees)) };
    }
}

WeightEditor::WeightEditor (WeightProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    logo = makeLogo (true);
    sigil = makeLogo (false);

    front.onPaint = [this] (Graphics& g)
    {
        paintHeader (g);
        paintPanel (g);
    };
    front.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (front);

    // Where everything sits on the panel, as on the original's front.
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
        s.setRotaryParameters (MathConstants<float>::twoPi + radians (fromDegrees), MathConstants<float>::twoPi + radians (toDegrees), true);
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
        part.component->setBounds (Rectangle<float> (part.size, part.size)
                                       .withCentre (part.centre.translated (0.0f, (float) kHeaderHeight))
                                       .toNearestInt());

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
    describe (power, "Power : éteint, le signal ressort tel qu'il est entré. La lampe rouge dit que l'appareil est allumé.");

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

    // The licence badge sits at the right of the header, where the other plugins of the range have it.
    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), scale, { kWidth - 14 - 320, 17, 320, 30 }, Justification::centredRight);
}

void WeightEditor::timerCallback()
{
    // The part in hand first, else the one under the pointer.
    const Part* shown = nullptr;

    for (const auto& part : parts)
        if (part.component->isMouseButtonDown())
            shown = &part;

    if (shown == nullptr)
        for (const auto& part : parts)
            if (part.component->isMouseOver())
                shown = &part;

    readout = {};

    if (shown != nullptr)
    {
        auto* parameter = processor.apvts.getParameter (shown->id);
        const auto on = parameter->getValue() > 0.5f;

        readout << shown->name << "   ";

        if (shown->component == &eqIn)
            readout << (on ? "IN" : "OUT");
        else if (shown->component == &power)
            readout << (on ? "OFF" : "ON");
        else
            readout << parameter->getCurrentValueAsText();
    }

    const auto lit = processor.apvts.getRawParameterValue ("bypass")->load() < 0.5f;

    if (lit != lampLit)
    {
        lampLit = lit;
        front.repaint (Rectangle<int> (110, 110).withCentre ({ 967, kHeaderHeight + 178 }));
    }

    // The header is the only part of the front that moves by itself.
    front.repaint (0, 0, kWidth, kHeaderHeight);
}

//==============================================================================
void WeightEditor::paintHeader (Graphics& g)
{
    const auto area = Rectangle<float> (0.0f, 0.0f, (float) kWidth, (float) kHeaderHeight);
    const auto seconds = (Time::getMillisecondCounterHiRes() - openedAt) * 0.001;

    // The crimson of the range: darkest at the sigil, brightest behind the
    // name, and falling away again to the right and towards the foot.
    ColourGradient ground (Colour (0xff2c060b), 0.0f, 0.0f, Colour (0xff3b080e), area.getRight(), 0.0f, false);
    ground.addColour (0.32, Colour (0xff7a0e1c));
    ground.addColour (0.7, Colour (0xff5a0a14));
    g.setGradientFill (ground);
    g.fillRect (area);

    g.setGradientFill (ColourGradient (Colours::transparentBlack, 0.0f, area.getY(), Colour (0x73000000), 0.0f, area.getBottom(), false));
    g.fillRect (area);

    if (logo.isValid())
    {
        const auto logoWidth = (float) logo.getWidth() * 0.5f, logoHeight = (float) logo.getHeight() * 0.5f;
        g.drawImage (logo, { 16.0f - (float) kLogoMargin, area.getCentreY() - logoHeight * 0.5f, logoWidth, logoHeight });
    }

    // The name: the house in white, the product in a paler weight beside it.
    const auto nameWidth = GlyphArrangement::getStringWidth (titleFont (true), "ONIRIQUE");
    const auto productWidth = GlyphArrangement::getStringWidth (titleFont (false), "WEIGHT");

    g.setColour (Colours::white);
    g.setFont (titleFont (true));
    g.drawText ("ONIRIQUE", area.withTrimmedLeft ((float) kTitleLeft).withWidth (nameWidth + 4.0f), Justification::centredLeft, false);

    g.setColour (Colour (0xffe3b5ba));
    g.setFont (titleFont (false));
    g.drawText ("WEIGHT", area.withTrimmedLeft ((float) kTitleLeft + nameWidth + 10.0f).withWidth (productWidth + 8.0f),
                Justification::centredLeft, false);

    // Stars beside it, each breathing at its own pace.
    struct Spark { float x, y, radius, rate, offset; };
    static const Spark sparks[] = { { 10.0f, 17.0f, 5.5f, 0.9f, 0.0f },  { 27.0f, 11.0f, 3.0f, 1.3f, 1.7f },
                                    { 38.0f, 24.0f, 4.0f, 0.7f, 3.1f },  { 18.0f, 42.0f, 4.5f, 1.1f, 4.4f },
                                    { 34.0f, 48.0f, 2.5f, 1.6f, 2.3f },  { 46.0f, 38.0f, 2.0f, 1.0f, 5.2f } };

    const auto sparkLeft = (float) kTitleLeft + nameWidth + productWidth + 22.0f;

    for (const auto& s : sparks)
    {
        const auto glow = 0.5f + 0.5f * (float) std::sin (seconds * s.rate * 2.0 + s.offset);
        g.setColour (Colours::white.withAlpha (0.25f + 0.65f * glow));
        g.fillPath (star ({ sparkLeft + s.x, area.getY() + s.y }, s.radius * (0.8f + 0.2f * glow)));
    }

    // The readout: what the control in hand is set to, in plain numbers.
    if (readout.isNotEmpty())
    {
        const auto pill = Rectangle<float> (430.0f, 17.0f, 380.0f, 30.0f);
        const auto radius = pill.getHeight() * 0.5f;

        g.setColour (Colour (0x8c000000));
        g.fillRoundedRectangle (pill, radius);
        g.setColour (ember.withAlpha (0.55f));
        g.drawRoundedRectangle (pill, radius, 1.0f);

        const auto led = Rectangle<float> (6.0f, 6.0f).withCentre ({ pill.getX() + radius, pill.getCentreY() });
        g.setColour (ember.withAlpha (0.35f));
        g.fillEllipse (led.expanded (2.5f));
        g.setColour (emberLit);
        g.fillEllipse (led);

        g.setColour (mist);
        g.setFont (fonts->monoBoldFont (12.5f, 0.04f));
        g.drawText (readout, pill.withTrimmedLeft (radius * 1.6f).withTrimmedRight (radius), Justification::centred, false);
    }

    // The foot of the header: a faint rule, and a streak of light that runs
    // along it from left to right and comes round again.
    const auto footY = area.getBottom() - 2.0f;

    g.setColour (Colour (0x30ffd2d6));
    g.fillRect (0.0f, footY, area.getWidth(), 1.0f);

    const auto length = jmax (160.0f, area.getWidth() * 0.22f);
    const auto cycle = 3.6;
    const auto travelled = (float) (std::fmod (seconds, cycle) / cycle);
    const auto head = -40.0f + travelled * (area.getWidth() + length + 80.0f);
    const auto tail = head - length;

    ColourGradient halo (Colours::transparentBlack, tail, 0.0f, Colours::transparentBlack, head + 14.0f, 0.0f, false);
    halo.addColour (0.82, ember.withAlpha (0.55f));
    g.setGradientFill (halo);
    g.fillRect (tail, footY - 3.0f, length + 14.0f, 7.0f);

    ColourGradient streak (Colours::transparentWhite, tail, 0.0f, Colours::transparentWhite, head + 6.0f, 0.0f, false);
    streak.addColour (0.55, emberLit.withAlpha (0.9f));
    streak.addColour (0.93, Colours::white);
    g.setGradientFill (streak);
    g.fillRoundedRectangle (tail, footY - 0.5f, length + 6.0f, 2.0f, 1.0f);
}

void WeightEditor::paintPanel (Graphics& g)
{
    const auto top = (float) kHeaderHeight;
    const auto panel = Rectangle<float> (0.0f, top, (float) kWidth, (float) kPanelHeight);
    auto P = [top] (float x, float y) { return Point<float> (x, top + y); };

    // Slate-grey steel, lit from above, with the faint grain of a brushed plate.
    g.setGradientFill (ColourGradient (slate.brighter (0.08f), 0.0f, panel.getY(), slateDeep, 0.0f, panel.getBottom(), false));
    g.fillRect (panel);

    Random grain (7);

    for (auto y = panel.getY(); y < panel.getBottom(); y += 1.0f)
    {
        g.setColour ((grain.nextBool() ? Colours::white : Colours::black).withAlpha (0.004f + 0.01f * grain.nextFloat()));
        g.fillRect (0.0f, y, (float) kWidth, 1.0f);
    }

    // The header's shadow on the plate.
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.45f), 0.0f, top, Colours::transparentBlack, 0.0f, top + 10.0f, false));
    g.fillRect (panel.withHeight (10.0f));

    // The rack ears: the slots, and the screws.
    for (auto y : { 94.0f, 240.0f })
    {
        for (auto x : { -16.0f, (float) kWidth - 28.0f })
        {
            const auto slot = Rectangle<float> (x, top + y - 7.5f, 44.0f, 15.0f);
            g.setColour (Colour (0xff070809));
            g.fillRoundedRectangle (slot, 7.5f);
            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawHorizontalLine (roundToInt (slot.getBottom()), slot.getX() + 6.0f, slot.getRight() - 6.0f);
        }
    }

    drawScrew (g, P (87.0f, 65.0f), 11.0f, 0.5f);
    drawScrew (g, P (87.0f, 281.0f), 11.0f, -0.3f);
    drawScrew (g, P (1112.0f, 65.0f), 11.0f, 0.9f);
    drawScrew (g, P (1112.0f, 281.0f), 11.0f, 0.2f);

    // The engraving.
    auto text = [&] (const String& s, Point<float> centre, float height, float tracking = 0.04f)
    {
        g.setFont (fonts->bodyFont (height, tracking));
        g.drawText (s, Rectangle<float> (220.0f, height * 1.6f).withCentre (centre), Justification::centred, false);
    };

    g.setColour (engraving);

    // The maker's plate.
    text ("ONIRIQUE", P (164.0f, 124.0f), 18.0f, 0.12f);
    text ("PROGRAM EQUALIZER", P (164.0f, 148.0f), 13.0f, 0.05f);
    text ("TYPE W-1", P (164.0f, 170.0f), 13.0f, 0.08f);

    const auto box = Rectangle<float> (94.0f, 22.0f).withCentre (P (164.0f, 198.0f));
    g.drawRoundedRectangle (box, 2.0f, 1.2f);
    text ("WEIGHT", box.getCentre(), 13.5f, 0.16f);

    if (sigil.isValid())
    {
        const auto w = (float) sigil.getWidth() * 0.45f, h = (float) sigil.getHeight() * 0.45f;
        g.setOpacity (0.92f);
        g.drawImage (sigil, Rectangle<float> (w, h).withCentre (P (164.0f, 241.0f)));
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
        dialScale (P (x, 85.0f));
        text (name, P (x, 7.5f), 12.5f, 0.08f);
    }

    dialScale (P (599.0f, 248.0f));
    text ("SHARP", P (548.0f, 303.0f), 8.5f, 0.08f);
    text ("BROAD", P (652.0f, 303.0f), 8.5f, 0.08f);
    text ("BANDWIDTH", P (599.0f, 321.0f), 12.5f, 0.08f);

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

    switchScale (P (416.0f, 250.0f), wgt::kLowFreqNames, -46.5f, 31.0f, kSwitchRadius);
    text ("CPS", P (418.0f, 163.0f), 12.5f, 0.08f);
    text ("LOW FREQUENCY", P (418.0f, 321.0f), 12.5f, 0.08f);

    switchScale (P (778.0f, 252.0f), wgt::kHighFreqNames, -90.0f, 30.0f, kSwitchRadius);
    text ("KCS", P (780.0f, 164.0f), 12.5f, 0.08f);
    text ("HIGH FREQUENCY", P (779.0f, 321.0f), 12.5f, 0.08f);

    switchScale (P (1007.0f, 92.0f), wgt::kAttenFreqNames, -30.0f, 30.0f, 58.0f);
    text ("ATTEN SEL", P (1007.0f, 9.5f), 12.5f, 0.08f);

    // The EQ toggle, and the power with its lamp.
    text ("IN", P (265.0f, 180.0f), 11.0f, 0.1f);
    text ("OUT", P (265.0f, 298.0f), 11.0f, 0.1f);
    text ("OFF", P (1019.0f, 213.0f), 12.0f, 0.06f);
    text ("ON", P (1054.0f, 231.0f), 12.0f, 0.06f);

    drawLamp (g, P (967.0f, 178.0f), 14.0f, lampLit);
}

} // namespace onirique

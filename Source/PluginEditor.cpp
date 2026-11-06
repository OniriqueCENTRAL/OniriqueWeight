#include "PluginEditor.h"
#include "ui/OniriqueWindowSize.h"

#include <BinaryData.h>

namespace onirique
{

using namespace theme;

namespace
{
    Image photo (const String& file)
    {
        int size = 0;
        const auto* data = BinaryData::getNamedResource (file.removeCharacters ("-").replaceCharacter ('.', '_').toRawUTF8(), size);
        return data != nullptr ? ImageCache::getFromMemory (data, size) : Image();
    }
}

WeightEditor::WeightEditor (WeightProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      bar (p.presets, p.apvts, "WEIGHT", photo ("sigil.png"), kUnits, kUnits),
      frontOff (photo ("front-off.jpg")),
      frontOn (photo ("front-on.jpg"))
{
    setLookAndFeel (&lookAndFeel);

    front.onPaint = [this] (Graphics& g)
    {
        {
            // la façade aux coins arrondis du verre de VOCALBOX
            Graphics::ScopedSaveState save (g);
            Path clip;
            clip.addRoundedRectangle (0.0f, 0.0f, (float) kWidth, (float) kHeight, 14.0f);
            g.reduceClipRegion (clip);
            paintFront (g);
        }
        paintGlow (g);
    };
    front.onPaintOver = [this] (Graphics& g) { paintReadout (g); };
    front.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (front);
    addAndMakeVisible (bar);
    bar.onThemeChanged = [this] { themeSeen = -1; };
    jassert (nuit::Bar::heightFor (kUnits) == kBarHeight);

    auto& state = processor.apvts;
    Component* components[] = { &lowBoost, &lowAtten, &highBoost, &highAtten, &bandwidth, &lowFreq, &highFreq, &attenSel, &eqIn, &power };
    static_assert (std::size (layout::kControls) == 10);

    for (size_t i = 0; i < std::size (layout::kControls); ++i)
    {
        const auto& control = layout::kControls[i];
        auto* component = components[i];
        parts.push_back ({ &control, component, photo (String (control.photo) + ".png") });

        if (auto* slider = dynamic_cast<Slider*> (component))
        {
            slider->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
            slider->setRotaryParameters (MathConstants<float>::twoPi + degreesToRadians (control.fromDegrees),
                                         MathConstants<float>::twoPi + degreesToRadians (control.toDegrees), true);
            sliderAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, control.id, *slider));

            auto* parameter = state.getParameter (control.id);
            slider->setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        }
        else if (auto* button = dynamic_cast<ToggleButton*> (component))
        {
            buttonAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (state, control.id, *button));
        }

        component->setBounds (Rectangle<float> (control.size, control.size).withCentre ({ control.x, control.y }).toNearestInt());
        front.addAndMakeVisible (component);
    }

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

    // The front and its bar keep their proportions; the window grows by zooming them, from 75 to 200 %.
    setResizable (true, true);
    setResizeLimits (kUnits * 3 / 4, kTotalHeight * 3 / 4, kUnits * 2, kTotalHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) kUnits / (double) kTotalHeight);

    // taille d'ouverture : la règle commune des plugins ONIRIQUE (ui/OniriqueWindowSize.h)
    const auto opening = onirique::window::openingSize ({}, "ONIRIQUE WEIGHT", { kUnits, kTotalHeight },
                                                        { kUnits * 3 / 4, kTotalHeight * 3 / 4 }, { kUnits * 2, kTotalHeight * 2 });
    setSize (opening.x, opening.x * kTotalHeight / kUnits);

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
    const auto scale = (float) getWidth() / (float) kUnits;

    front.setBounds (kMargin, kBarHeight + kMargin, kWidth, kHeight);
    front.setTransform (AffineTransform::scale (scale));

    // The licence badge has its place in the bar, beside the menu.
    bar.setPillWidth (licenseOverlay != nullptr ? licenseOverlay->pillWidth() : 0);
    bar.place (scale, getLocalBounds());

    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), scale, bar.getPillArea(), Justification::centredRight);
}

void WeightEditor::timerCallback()
{
    // la couleur de l'interface (menu ⋯, ou changée dans un autre plugin ONIRIQUE) : la peinture et le fond suivent
    if (onirique::look::changedSince (themeSeen))
    {
        retint();
        daBackdrop = {};
        repaint();
        front.repaint();
    }

    // la lueur de la pièce touchée s'allume et s'éteint en douceur
    {
        // seulement au survol d'un potard : rien sur les boutons, et rien pendant qu'on le tourne
        const bool knob = shown != nullptr && dynamic_cast<Slider*> (shown->component) != nullptr;
        const float target = knob && ! ModifierKeys::currentModifiers.isAnyMouseButtonDown() ? 1.0f : 0.0f;
        if (shown != nullptr) glowPart = shown;
        if (std::abs (glow - target) > 0.01f) { glow += (target - glow) * 0.3f; front.repaint(); }
        else glow = target;
    }

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
        auto* parameter = processor.apvts.getParameter (now->control->id);
        const auto on = parameter->getValue() > 0.5f;

        text << now->control->name << "   ";

        if (now->component == &eqIn)
            text << (on ? "IN" : "OUT");
        else if (now->component == &power)
            text << (on ? "OFF" : "ON");
        else
            text << parameter->getCurrentValueAsText();
    }

    std::vector<float> values;

    for (const auto& part : parts)
        values.push_back (processor.apvts.getParameter (part.control->id)->getValue());

    if (now != shown || text != readout || values != shownValues)
    {
        shown = now;
        readout = text;
        shownValues = values;
        front.repaint();
    }
}

//==============================================================================
void WeightEditor::paintFront (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    const auto lit = processor.apvts.getRawParameterValue ("bypass")->load() < 0.5f;
    g.drawImage (lit ? frontOn : frontOff, Rectangle<float> ((float) kWidth, (float) kHeight));

    for (const auto& part : parts)
    {
        const auto& c = *part.control;

        if (! part.photos.isValid())
            continue;

        // Which photograph: the nearest position. Between two of a dial's,
        // the nearest is turned the rest of the way, so it moves smoothly.
        const auto position = processor.apvts.getParameter (c.id)->getValue() * (float) (c.frames - 1);
        const auto k = jlimit (0, c.frames - 1, roundToInt (position));
        const auto rest = c.kind == layout::Kind::dial ? (position - (float) k) * (c.toDegrees - c.fromDegrees) / (float) (c.frames - 1) : 0.0f;

        const auto frame = layout::frameOf (c.kind);
        const auto pixels = roundToInt (frame.size() * (float) layout::kPhotoScale);
        const auto columns = jmin (c.frames, 8);
        const auto shot = part.photos.getClippedImage ({ (k % columns) * pixels, (k / columns) * pixels, pixels, pixels });

        g.drawImageTransformed (shot, AffineTransform::scale (frame.size() / (float) pixels)
                                          .translated (c.x - frame.before, c.y - frame.before)
                                          .rotated (degreesToRadians (rest), c.x, c.y));
    }
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

    da::valueBubble (g, pill, readout, font, mist);   // la bulle de valeur de VOCALBOX
}

//==============================================================================
void WeightEditor::paint (Graphics& g)
{
    const float u = (float) getWidth() / (float) kUnits;
    if (daBackdrop.getWidth() != getWidth() || daBackdrop.getHeight() != getHeight())
        daBackdrop = da::backdrop (getWidth(), getHeight(), (float) getWidth() / 1100.0f);
    g.drawImageAt (daBackdrop, 0, 0);
    da::photoCard (g, Rectangle<float> ((float) kMargin, (float) (kBarHeight + kMargin), (float) kWidth, (float) kHeight) * u, 14.0f * u, u);
}

void WeightEditor::retint()
{
    std::vector<Image*> all { &frontOff, &frontOn };
    for (auto& part : parts) all.push_back (&part.photos);

    if (daOriginals.empty())
        for (auto* image : all)
            daOriginals.push_back (*image);

    for (size_t i = 0; i < all.size() && i < daOriginals.size(); ++i)
        *all[i] = da::tintPhoto (daOriginals[i]);
}

void WeightEditor::paintGlow (Graphics& g)
{
    if (glowPart == nullptr || glow <= 0.01f)
        return;

    const auto& c = *glowPart->control;
    da::partGlow (g, { c.x, c.y }, c.size * 0.5f, glow);
}

} // namespace onirique

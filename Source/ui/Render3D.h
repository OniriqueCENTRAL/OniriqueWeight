// Onirique Weight — the parts of the front, rendered in relief
#pragma once

#include <juce_graphics/juce_graphics.h>

#include <map>
#include <tuple>

namespace onirique::render
{

/** The parts of the front. Each is rendered pixel by pixel from its shape in
    relief, under one studio light fixed above and to the left: turning a knob
    turns its shape, never its light, as on a photograph of the real thing. */
enum class Part
{
    dial,       // the big fluted knob of the boost, atten and bandwidth dials
    selector,   // the bar knob of a frequency switch
    bat,        // the pointed knob of the atten selector
    toggle,     // the bat-handled toggle; its angle is where the handle points
    power,      // the small round knob of the power switch
    lampOn,
    lampOff,
    screw       // its angle is the slot's
};

/** A square image `pixels` wide, the part centred in it with room for the
    shadow it casts on the panel, turned by `angle` (radians, clockwise from
    twelve o'clock). */
juce::Image draw (Part, int pixels, float angle);

/** How far the part reaches from the centre, as a fraction of half the
    square: the rest is room for its shadow. */
float reach (Part);

/** Parts already rendered, by shape, size and angle. */
class Cache
{
public:
    const juce::Image& get (Part, int pixels, float angle);

private:
    std::map<std::tuple<int, int, int>, juce::Image> images;
};

} // namespace onirique::render

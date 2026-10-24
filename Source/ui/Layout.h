// Onirique Weight — where everything sits on the front, shared by the editor
// and by the renderer that photographs the parts (tools/photo.cpp)
#pragma once

namespace onirique::layout
{

// The front, in points; its photographs have this many pixels to the point.
constexpr int kWidth = 1200, kHeight = 331;
constexpr int kPhotoScale = 2;

enum class Kind { dial, selector, bat, toggle, power };

/** A control, and the photographs it is shown with: one per position of a
    switch, or a run over the travel of a dial. */
struct Control
{
    const char* id;          // its parameter
    const char* name;        // as its readout calls it
    const char* photo;       // the run of photographs it takes its look from
    Kind kind;
    float x, y;              // its centre
    float size;              // the square that takes the pointer
    int frames;
    float fromDegrees, toDegrees;   // the first and last photographs, clockwise from twelve o'clock
};

// Each photograph is ordered by the parameter's value: the toggle's first is
// OUT (handle down), the power's first is ON (the bypass off).
inline constexpr Control kControls[] = {
    { "lowboost",  "LOW BOOST",      "dial",     Kind::dial,     341.0f,  85.0f,  120.0f, 64, -135.0f, 135.0f },
    { "lowatten",  "LOW ATTEN",      "dial",     Kind::dial,     495.0f,  85.0f,  120.0f, 64, -135.0f, 135.0f },
    { "highboost", "HIGH BOOST",     "dial",     Kind::dial,     701.0f,  85.0f,  120.0f, 64, -135.0f, 135.0f },
    { "highatten", "HIGH ATTEN",     "dial",     Kind::dial,     857.0f,  85.0f,  120.0f, 64, -135.0f, 135.0f },
    { "bandwidth", "BANDWIDTH",      "dial",     Kind::dial,     599.0f,  248.0f, 120.0f, 64, -135.0f, 135.0f },
    { "lowfreq",   "LOW FREQUENCY",  "lowfreq",  Kind::selector, 416.0f,  250.0f, 116.0f, 4,  -46.5f,  46.5f },
    { "highfreq",  "HIGH FREQUENCY", "highfreq", Kind::selector, 778.0f,  252.0f, 116.0f, 7,  -90.0f,  90.0f },
    { "attensel",  "ATTEN SEL",      "attensel", Kind::bat,      1007.0f, 92.0f,  106.0f, 3,  -30.0f,  30.0f },
    { "eqin",      "EQ",             "eqin",     Kind::toggle,   265.0f,  239.0f, 92.0f,  2,  180.0f,  0.0f },
    { "bypass",    "POWER",          "power",    Kind::power,    1018.0f, 257.0f, 58.0f,  2,  52.0f,   0.0f },
};

/** Where a part's photograph lies around its centre: the part, and room for
    the shadow it casts down and to the right, away from the light. */
struct Frame
{
    float before, after;     // from the centre to the left and top edges, and to the right and bottom ones

    constexpr float size() const { return before + after; }
};

constexpr Frame frameOf (Kind kind)
{
    switch (kind)
    {
        case Kind::dial:     return { 62.0f, 96.0f };
        case Kind::selector: return { 67.0f, 103.0f };
        case Kind::bat:      return { 60.0f, 88.0f };
        case Kind::toggle:   return { 53.0f, 96.0f };
        case Kind::power:    return { 32.0f, 48.0f };
    }

    return { 0.0f, 0.0f };
}

// What is part of the front itself, photographed with it.
struct Screw { float x, y, slot; };
inline constexpr Screw kScrews[] = { { 87.0f, 65.0f, 0.5f }, { 87.0f, 281.0f, -0.3f },
                                     { 1112.0f, 65.0f, 0.9f }, { 1112.0f, 281.0f, 0.2f } };
constexpr float kScrewRadius = 11.0f;

constexpr float kLampX = 967.0f, kLampY = 178.0f, kLampRadius = 21.0f;

// The rack ears' slots: rounded, cut through the front at both ends.
constexpr float kSlotY[] = { 94.0f, 240.0f };
constexpr float kSlotLength = 28.0f, kSlotHeight = 15.0f;

} // namespace onirique::layout

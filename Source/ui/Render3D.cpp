#include "Render3D.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <array>
#include <memory>
#include <thread>
#include <vector>

namespace onirique::render
{

using namespace juce;

namespace
{
    constexpr float kPi = MathConstants<float>::pi, kTwoPi = MathConstants<float>::twoPi;

    struct Vec { float x, y, z; };

    Vec operator+ (Vec a, Vec b)    { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vec operator* (Vec a, float k)  { return { a.x * k, a.y * k, a.z * k }; }
    float dot (Vec a, Vec b)        { return a.x * b.x + a.y * b.y + a.z * b.z; }
    Vec normalise (Vec v)           { return v * (1.0f / std::sqrt (dot (v, v))); }
    Vec mix (Vec a, Vec b, float t) { return a * (1.0f - t) + b * t; }

    float clamp01 (float v) { return jlimit (0.0f, 1.0f, v); }
    float sq (float v)      { return v * v; }

    /** 0 at a, 1 at b, smoothly; a may be above b. */
    float smooth (float a, float b, float v)
    {
        const auto t = clamp01 ((v - a) / (b - a));
        return t * t * (3.0f - 2.0f * t);
    }

    /** A rounded edge: rises from nothing at the outline to `radius` at
        `radius` inside it, as a quarter circle. */
    float fillet (float inside, float radius)
    {
        const auto t = 1.0f - jmin (1.0f, jmax (0.0f, inside) / radius);
        return radius * std::sqrt (jmax (0.0f, 1.0f - t * t));
    }

    float wrap (float a)
    {
        return a - kTwoPi * std::floor ((a + kPi) / kTwoPi);
    }

    //==============================================================================
    enum class Material { bakelite, chrome, nickel, glass };

    struct Surface
    {
        float height = 0.0f;
        Material material = Material::bakelite;
        float paint = 0.0f;     // white paint over it
        float ember = 0.0f;     // the Onirique star, inlaid
        float shade = 1.0f;     // less in a hollow the light hardly reaches
        float core = 0.0f;      // glass: how near the bulb behind it
    };

    /** Units: the part's own size. x to the right, y down, as on the screen. */
    struct Shape
    {
        Shape (float m, float l) : margin (m), lift (l) {}
        virtual ~Shape() = default;

        /** How far inside the outline; negative outside. */
        virtual float inside (float x, float y) const = 0;
        virtual Surface surface (float x, float y) const = 0;

        float margin;   // half the square
        float lift;     // how high it stands off the panel, for its shadow
    };

    /** Where a point lies against a part turned by `angle`: along its pointer,
        across it, and at what angle from it. */
    struct Turned
    {
        Turned (float x, float y, float angle)
            : along (x * std::sin (angle) - y * std::cos (angle)),
              across (x * std::cos (angle) + y * std::sin (angle)),
              radius (std::sqrt (x * x + y * y)),
              theta (wrap (std::atan2 (x, -y) - angle))
        {}

        float along, across, radius, theta;
    };

    //==============================================================================
    // The light: one large soft studio light above and to the left, a dim room
    // everywhere else.
    const Vec kLight = normalise ({ -0.45f, -0.7f, 0.8f });

    float softRoom (Vec r)
    {
        const auto toward = 0.62f * -r.y + 0.42f * -r.x + 0.45f * r.z;
        return 0.03f + 0.75f * smooth (0.35f, 0.95f, toward) + 0.06f * smooth (-0.4f, 0.4f, -r.y);
    }

    /** The same room as a mirror sees it: a sharp horizon, and the light. */
    float sharpRoom (Vec r)
    {
        const auto up = -r.y * 0.85f - r.x * 0.25f + r.z * 0.2f;
        const auto sky = 0.3f + 0.55f * smooth (0.0f, 0.6f, up);
        const auto ground = 0.04f + 0.1f * smooth (-0.9f, -0.05f, up);
        const auto box = smooth (0.82f, 0.92f, 0.62f * -r.y + 0.42f * -r.x + 0.45f * r.z);
        return (up > 0.0f ? sky : ground) + 0.9f * box;
    }

    Vec shade (const Surface& s, Vec n, bool lit)
    {
        const auto half = normalise ({ kLight.x, kLight.y, kLight.z + 1.0f });
        const auto ndl = jmax (0.0f, dot (n, kLight)), ndh = jmax (0.0f, dot (n, half)), ndv = jmax (0.0f, n.z);
        const Vec mirror { 2.0f * n.z * n.x, 2.0f * n.z * n.y, 2.0f * n.z * n.z - 1.0f };
        const auto grazing = sq (sq (1.0f - ndv)) * (1.0f - ndv);
        const auto fresnel = [grazing] (float f0) { return f0 + (1.0f - f0) * grazing; };
        const auto grey = [] (float v) { return Vec { v, v, v }; };

        Vec c {};

        switch (s.material)
        {
            case Material::bakelite:
            {
                const auto h16 = sq (sq (sq (sq (ndh))));
                const auto spec = 0.6f * sq (sq (h16)) * h16;   // ndh^80
                c = grey (0.014f * (0.35f + ndl) + spec + softRoom (mirror) * (0.06f + 0.94f * fresnel (0.04f)));

                if (s.paint > 0.0f)
                    c = mix (c, grey (0.78f * (0.3f + 0.8f * ndl) + 0.2f * std::pow (ndh, 30.0f)), s.paint);

                if (s.ember > 0.0f)
                    c = mix (c, Vec { 0.75f, 0.06f, 0.08f } * (0.35f + 0.8f * ndl) + grey (spec), s.ember);

                break;
            }

            case Material::chrome:
            {
                const auto v = 0.9f * sharpRoom (mirror) + std::pow (ndh, 300.0f);
                c = { v * 0.97f, v, v * 1.03f };
                break;
            }

            case Material::nickel:
            {
                const auto v = 0.75f * (0.6f * sharpRoom (mirror) + 0.4f * softRoom (mirror)) + 0.5f * std::pow (ndh, 60.0f);
                c = { v, v * 0.97f, v * 0.91f };
                break;
            }

            case Material::glass:
            {
                const Vec blue { 0.05f, 0.32f, 1.0f };

                if (lit)
                    c = blue * (0.35f + 1.2f * sq (s.core)) + grey (0.9f * smooth (0.55f, 1.0f, s.core))
                        + blue * (0.6f * std::pow (ndl, 6.0f));
                else
                    c = blue * (0.03f + 0.05f * std::pow (ndl, 4.0f)) + grey (0.01f);

                c = c + grey (1.3f * std::pow (ndh, 250.0f) + sharpRoom (mirror) * fresnel (0.05f));
                break;
            }
        }

        return c * s.shade;
    }

    //==============================================================================
    /** The big fluted knob: ten rounded flutes round its skirt, a raised cap
        with a ring cut in it, the pointer painted on the skirt, the star at
        its heart. */
    struct Dial final : Shape
    {
        explicit Dial (float a) : Shape (1.2f, 1.0f), angle (a) {}

        static float flute (float theta) { return 0.5f + 0.5f * std::cos (10.0f * theta); }

        /** How far inside the Onirique star at the heart: four points, straight sides. */
        static float inStar (float theta, float r)
        {
            constexpr float tip = 0.13f, waist = 0.3f * tip;
            const auto phi = std::abs (wrap (theta - kPi * 0.5f * std::round (theta / (kPi * 0.5f))));
            const float ax = tip, ay = 0.0f, bx = waist * 0.7071f, by = waist * 0.7071f;
            const auto ex = bx - ax, ey = by - ay;
            const auto edge = (ax * ey - ay * ex) / (std::cos (phi) * ey - std::sin (phi) * ex);
            return edge - r;
        }
        static float edge (float theta)  { return 0.93f + 0.07f * std::sqrt (flute (theta)); }

        float inside (float x, float y) const override
        {
            const Turned p (x, y, angle);
            return edge (p.theta) - p.radius;
        }

        Surface surface (float x, float y) const override
        {
            const Turned p (x, y, angle);
            const auto r = p.radius, in = edge (p.theta) - r;
            constexpr float cap = 0.64f;

            Surface s;
            s.height = fillet (in, 0.07f) + 0.2f * std::pow (clamp01 ((in - 0.07f) / (edge (p.theta) - 0.07f - cap)), 0.8f)
                       - 0.05f * sq (1.0f - flute (p.theta)) * smooth (0.62f, 0.8f, r);

            if (r < cap)
            {
                const auto ring = std::exp (-sq ((r - 0.47f) / 0.02f));
                s.height = jmax (s.height, 0.3f + fillet (cap - r, 0.08f) + 0.04f * (1.0f - sq (r / cap)) - 0.03f * ring);
                s.shade = 1.0f - 0.5f * ring;
            }

            s.paint = smooth (0.042f, 0.028f, std::abs (p.theta) * r) * smooth (0.68f, 0.72f, r) * smooth (0.97f, 0.93f, r);

            s.ember = 0.85f * smooth (-0.008f, 0.008f, inStar (p.theta, r));
            return s;
        }

        float angle;
    };

    /** The bar knob of a frequency switch, on its round boss. */
    struct Selector final : Shape
    {
        explicit Selector (float a) : Shape (0.7f, 0.8f), angle (a) {}

        static float inBar (const Turned& p)
        {
            constexpr float halfWidth = 0.15f, halfLength = 0.5f, round = 0.07f;
            const auto qx = std::abs (p.across) - (halfWidth - round), qy = std::abs (p.along - 0.12f) - (halfLength - round);
            return round - std::sqrt (sq (jmax (qx, 0.0f)) + sq (jmax (qy, 0.0f))) - jmin (jmax (qx, qy), 0.0f);
        }

        float inside (float x, float y) const override
        {
            const Turned p (x, y, angle);
            return jmax (inBar (p), 0.3f - p.radius);
        }

        Surface surface (float x, float y) const override
        {
            const Turned p (x, y, angle);
            const auto bar = inBar (p), boss = 0.3f - p.radius;

            Surface s;
            s.height = jmax (bar > 0.0f ? 0.2f * smooth (0.0f, 0.02f, bar) + fillet (bar, 0.1f) - 0.04f * sq (p.across / 0.15f) : 0.0f,
                             0.08f * smooth (0.0f, 0.02f, boss) + fillet (boss, 0.06f));
            s.paint = smooth (0.024f, 0.014f, std::abs (p.across)) * smooth (0.08f, 0.12f, p.along) * smooth (0.58f, 0.54f, p.along);
            return s;
        }

        float angle;
    };

    /** The pointed knob of the atten selector: a blade tapering to its tip. */
    struct Bat final : Shape
    {
        explicit Bat (float a) : Shape (0.74f, 0.8f), angle (a) {}

        /** Inside a capsule whose two ends are not the same size (after I. Quilez). */
        static float inBlade (const Turned& p)
        {
            constexpr float r1 = 0.24f, r2 = 0.025f, h = 0.7f;
            const auto x = std::abs (p.across), y = p.along + 0.08f;
            const auto b = (r1 - r2) / h, a = std::sqrt (1.0f - b * b), k = -b * x + a * y;

            if (k < 0.0f)
                return r1 - std::sqrt (x * x + y * y);

            if (k > a * h)
                return r2 - std::sqrt (x * x + sq (y - h));

            return r1 - (a * x + b * y);
        }

        float inside (float x, float y) const override
        {
            const Turned p (x, y, angle);
            return jmax (inBlade (p), 0.32f - p.radius);
        }

        Surface surface (float x, float y) const override
        {
            const Turned p (x, y, angle);
            const auto blade = inBlade (p), boss = 0.32f - p.radius;

            Surface s;
            s.height = jmax (blade > 0.0f ? 0.12f * smooth (0.0f, 0.02f, blade) + 0.24f * std::sqrt (clamp01 (blade / 0.2f)) : 0.0f,
                             0.06f * smooth (0.0f, 0.02f, boss) + fillet (boss, 0.05f));
            return s;
        }

        float angle;
    };

    /** The toggle: a chrome nut in its knurled ring, and the black handle
        standing out of it towards the viewer, its ball at the end. */
    struct Toggle final : Shape
    {
        explicit Toggle (float a) : Shape (1.0f, 1.2f), angle (a) {}

        static constexpr float kNut = 0.42f, kLength = 0.74f, kBall = 0.115f;

        static float rodRadius (float along) { return 0.05f + 0.03f * along / kLength; }

        static float inLever (const Turned& p)
        {
            const auto rod = p.along >= 0.0f && p.along <= kLength ? rodRadius (p.along) - std::abs (p.across) : -1.0f;
            const auto ball = kBall - std::sqrt (sq (p.along - 0.78f) + sq (p.across));
            return jmax (rod, ball);
        }

        float inside (float x, float y) const override
        {
            const Turned p (x, y, angle);
            return jmax (kNut - p.radius, inLever (p));
        }

        Surface surface (float x, float y) const override
        {
            const Turned p (x, y, angle);
            const auto r = p.radius;

            Surface s;
            s.material = Material::chrome;

            if (r < 0.3f)
            {
                s.height = 0.12f + 0.12f * std::sqrt (jmax (0.0f, 1.0f - sq (r / 0.3f)));
                s.shade = r < 0.06f ? 0.2f : 1.0f;
            }
            else
            {
                s.height = 0.04f + fillet (kNut - r, 0.04f) + (r > 0.34f ? 0.012f * smooth (-0.2f, 0.2f, std::cos (60.0f * p.theta)) : 0.0f);
            }

            if (r >= kNut)
                s.height = 0.0f;

            // The handle, rising towards the viewer as it goes.
            auto lever = -1.0f;

            if (p.along >= 0.0f && p.along <= kLength && std::abs (p.across) < rodRadius (p.along))
                lever = 0.25f + 0.45f * p.along + std::sqrt (sq (rodRadius (p.along)) - sq (p.across));

            const auto d = std::sqrt (sq (p.along - 0.78f) + sq (p.across));

            if (d < kBall)
                lever = jmax (lever, 0.6f + std::sqrt (sq (kBall) - sq (d)));

            if (lever > s.height)
            {
                s.height = lever;
                s.material = Material::bakelite;
                s.shade = 1.0f;
            }

            return s;
        }

        float angle;
    };

    /** The small round knob of the power switch, its pointer painted on. */
    struct Power final : Shape
    {
        explicit Power (float a) : Shape (1.35f, 0.7f), angle (a) {}

        float inside (float x, float y) const override { return 1.0f - std::sqrt (x * x + y * y); }

        Surface surface (float x, float y) const override
        {
            const Turned p (x, y, angle);
            Surface s;
            s.height = fillet (1.0f - p.radius, 0.18f) + 0.25f * jmax (0.0f, 1.0f - sq (p.radius));
            s.paint = smooth (0.07f, 0.045f, std::abs (p.theta) * p.radius) * smooth (0.33f, 0.38f, p.radius) * smooth (0.88f, 0.84f, p.radius);
            return s;
        }

        float angle;
    };

    /** The jewel lamp: a cut glass dome in a chrome bezel, a bulb behind it. */
    struct Lamp final : Shape
    {
        Lamp() : Shape (1.4f, 0.4f) {}

        float inside (float x, float y) const override { return 1.0f - std::sqrt (x * x + y * y); }

        Surface surface (float x, float y) const override
        {
            const Turned p (x, y, 0.0f);
            constexpr float glass = 0.68f;
            Surface s;

            if (p.radius > glass)
            {
                s.material = Material::chrome;
                s.height = 0.18f * std::sqrt (jmax (0.0f, 1.0f - sq ((p.radius - 0.84f) / 0.16f)));
                return s;
            }

            // Twelve facets round the dome.
            constexpr float sector = kTwoPi / 12.0f;
            const auto facet = p.radius * std::cos (p.theta - sector * std::round (p.theta / sector));

            s.material = Material::glass;
            s.height = 0.12f + 0.3f * std::sqrt (jmax (0.0f, 1.0f - sq (facet / glass)));
            s.core = 1.0f - p.radius / glass;
            return s;
        }
    };

    /** A slotted nickel screw. */
    struct Screw final : Shape
    {
        explicit Screw (float a) : Shape (1.36f, 0.25f), angle (a) {}

        float inside (float x, float y) const override { return 1.0f - std::sqrt (x * x + y * y); }

        Surface surface (float x, float y) const override
        {
            const Turned p (x, y, angle);
            const auto slot = smooth (0.14f, 0.09f, std::abs (p.across)) * smooth (0.95f, 0.85f, p.radius);

            Surface s;
            s.material = Material::nickel;
            s.height = fillet (1.0f - p.radius, 0.12f) + 0.2f * jmax (0.0f, 1.0f - sq (p.radius)) - 0.16f * slot;
            s.shade = 1.0f - 0.65f * slot;
            return s;
        }

        float angle;
    };

    std::unique_ptr<Shape> makeShape (Part part, float angle)
    {
        switch (part)
        {
            case Part::dial:     return std::make_unique<Dial> (angle);
            case Part::selector: return std::make_unique<Selector> (angle);
            case Part::bat:      return std::make_unique<Bat> (angle);
            case Part::toggle:   return std::make_unique<Toggle> (angle);
            case Part::power:    return std::make_unique<Power> (angle);
            case Part::lampOn:
            case Part::lampOff:  return std::make_unique<Lamp>();
            case Part::screw:    return std::make_unique<Screw> (angle);
        }

        return std::make_unique<Lamp>();
    }

    //==============================================================================
    /** Light as the screen shows it, gamma 2.2, read from a table. */
    float toScreen (float v)
    {
        static const auto table = []
        {
            std::array<float, 4097> t {};

            for (size_t i = 0; i < t.size(); ++i)
                t[i] = std::pow ((float) i / 4096.0f, 1.0f / 2.2f);

            return t;
        }();

        return table[(size_t) (clamp01 (v) * 4096.0f)];
    }

    /** rows (from, to) over 0 to count, shared between the machine's cores
        when there are enough of them to be worth it. Highlights fade into
        numbers so small the processor crawls over them: they count as nothing. */
    template <typename Rows>
    void inParallel (int count, Rows body)
    {
        const auto rows = [&body] (int from, int to)
        {
            const ScopedNoDenormals noDenormals;
            body (from, to);
        };

        const auto cores = (int) jlimit (1u, 8u, std::thread::hardware_concurrency());

        if (count < 64 || cores == 1)
        {
            rows (0, count);
            return;
        }

        const auto step = (count + cores - 1) / cores;
        std::vector<std::thread> threads;

        for (int from = step; from < count; from += step)
            threads.emplace_back (rows, from, jmin (count, from + step));

        rows (0, jmin (count, step));

        for (auto& t : threads)
            t.join();
    }

    Image render (const Shape& shape, int n, bool lit)
    {
        const auto m = shape.margin, px = 2.0f * m / (float) n;
        const auto w = n + 2;   // a pixel more all round, for the slopes at the edge
        auto at = [m, px] (int i) { return -m + ((float) i - 0.5f) * px; };

        std::vector<Surface> surfaces ((size_t) (w * w));

        inParallel (w, [&] (int from, int to)
        {
            for (int j = from; j < to; ++j)
                for (int i = 0; i < w; ++i)
                    surfaces[(size_t) (j * w + i)] = shape.surface (at (i), at (j));
        });

        auto height = [&] (int i, int j) { return surfaces[(size_t) (j * w + i)].height; };

        // The shadow falls down and to the right, away from the light, and
        // spreads the further the part stands off the panel.
        const auto dx = 0.05f * shape.lift, dy = 0.1f * shape.lift, blur = 0.05f + 0.12f * shape.lift;

        // Written in plain memory, pixel by pixel: the screen's own kind of image
        // would be fetched and sent back at every one.
        Image image (Image::ARGB, n, n, true, SoftwareImageType());
        Image::BitmapData data (image, Image::BitmapData::writeOnly);

        inParallel (n, [&] (int from, int to)
        {
          for (int y = from; y < to; ++y)
          {
            for (int x = 0; x < n; ++x)
            {
                const auto X = at (x + 1), Y = at (y + 1);
                const auto in = shape.inside (X, Y);
                const auto cover = clamp01 (in / px + 0.5f);
                const auto shadow = jmin (0.8f, 0.55f * smooth (-blur, blur * 0.5f, shape.inside (X - dx, Y - dy))
                                                    + 0.3f * smooth (-0.06f, 0.0f, in));

                Vec colour {};

                if (cover > 0.0f)
                {
                    const auto sx = (height (x + 2, y + 1) - height (x, y + 1)) / (2.0f * px);
                    const auto sy = (height (x + 1, y + 2) - height (x + 1, y)) / (2.0f * px);
                    colour = shade (surfaces[(size_t) ((y + 1) * w + x + 1)], normalise ({ -sx, -sy, 1.0f }), lit);
                }

                // Into the screen's colours, and over the shadow.
                auto encode = [cover] (float v) { return (uint8) roundToInt (255.0f * cover * toScreen (v)); };
                const auto alpha = cover + (1.0f - cover) * shadow;

                reinterpret_cast<PixelARGB*> (data.getPixelPointer (x, y))
                    ->setARGB ((uint8) roundToInt (255.0f * alpha), encode (colour.x), encode (colour.y), encode (colour.z));
            }
          }
        });

        return image;
    }
}

//==============================================================================
float reach (Part part)
{
    return 1.0f / makeShape (part, 0.0f)->margin;
}

Image draw (Part part, int pixels, float angle)
{
    return render (*makeShape (part, angle), jmax (8, pixels), part == Part::lampOn);
}

const Image& Cache::get (Part part, int pixels, float angle)
{
    const auto key = std::make_tuple ((int) part, pixels, roundToInt (angle * 2000.0f));

    if (const auto found = images.find (key); found != images.end())
        return found->second;

    // Turning a knob leaves a trail of images it no longer shows.
    if (images.size() > 160)
        images.clear();

    return images[key] = draw (part, pixels, angle);
}

} // namespace onirique::render

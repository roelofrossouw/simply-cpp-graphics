#pragma once

#include <color.h>

namespace sc {
    // Transparency is the colour's alpha, for strokes, fills and text alike: 0 leaves the part out.
    // sc-pdf and sc-mupdf paint partly transparent colours as they are; sc-image paints them opaque.

    /// How overlapping parts of a filled path count as inside: nonzero (the default: an area wound
    /// round in the same direction twice is still inside) or even-odd (every crossing of an outline
    /// toggles inside and outside, so a subpath inside another cuts a hole, as in a ring or an "o").
    enum class fill_rule { nonzero, even_odd };

    /// How a shape's outline is painted: a colour and a width, in the canvas's units (pixels on an
    /// image, points on a PDF). The outline is centred on the shape's edge. stroke::none() (or a
    /// transparent colour, or a width of 0) leaves the outline out.
    ///
    ///     canvas.draw(box, sc::stroke{sc::color::Black, 0.5});
    ///     canvas.draw(box, sc::color::Red);                       // a colour is a stroke 1 wide
    ///
    /// Planned, each an engine may support or ignore (the call stays the same; an engine that can't
    /// do one draws the plain outline):
    ///   - a dash pattern: dashes and gaps (dotted, dash-dot, or custom lengths) and a phase;
    ///   - line caps (butt, round, square) and joins (miter with a miter limit, round, bevel);
    ///   - a nib: the pen's shape and angle (round, or a flat calligraphic nib), for varying width;
    ///   - a paint other than a flat colour: a gradient or pattern, as a fill can have.
    class stroke {
    public:
        /// Black, 1 wide.
        stroke() = default;

        /// A colour converts to a stroke 1 wide.
        stroke(const sc::color &colour, const double width = 1) : colour_(colour), width_(width) {
        }

        /// No outline.
        static stroke none() { return {sc::color{0, 0, 0, 0}, 0}; }

        [[nodiscard]] const sc::color &colour() const { return colour_; }

        [[nodiscard]] double width() const { return width_; }

        /// Whether it paints anything: a colour that isn't transparent, and a width above 0.
        [[nodiscard]] bool visible() const { return colour_.alpha() > 0 && width_ > 0; }

        bool operator==(const stroke &) const = default;

    private:
        // Literal values, not color::Black: a stroke at namespace scope elsewhere may be made before
        // sc-core's colour constants are.
        sc::color colour_{0, 0, 0};
        double width_ = 1;
    };

    /// How a shape's inside is painted: a colour, and the fill rule for a path whose subpaths
    /// overlap. fill::none() (or a transparent colour) leaves it out, and is the default, so a shape
    /// is only outlined unless it's given a fill.
    ///
    ///     canvas.draw(box, sc::stroke::none(), sc::color{0.9, 0.95, 1});               // a colour is a fill
    ///     canvas.draw(ring, sc::stroke::none(), sc::fill{sc::color::Blue, sc::fill_rule::even_odd});
    ///
    /// Planned, each an engine may support or ignore (an engine that can't do one fills with the
    /// nearest flat colour):
    ///   - gradients: linear and radial, with colour stops;
    ///   - patterns and images, tiled or stretched.
    class fill {
    public:
        /// None: transparent.
        fill() = default;

        /// A colour converts to a fill, by the nonzero rule.
        fill(const sc::color &colour, const fill_rule rule = fill_rule::nonzero) : colour_(colour), rule_(rule) {
        }

        /// No inside.
        static fill none() { return {}; }

        [[nodiscard]] const sc::color &colour() const { return colour_; }

        /// Only paths can overlap themselves; a canvas that can't fill by a rule (one that flattens
        /// paths into separate polygons) ignores it.
        [[nodiscard]] fill_rule rule() const { return rule_; }

        /// Whether it paints anything: a colour that isn't transparent.
        [[nodiscard]] bool visible() const { return colour_.alpha() > 0; }

        bool operator==(const fill &) const = default;

    private:
        sc::color colour_{0, 0, 0, 0}; // transparent
        fill_rule rule_ = fill_rule::nonzero;
    };
}

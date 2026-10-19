#pragma once

#include <color.h>
#include <point.h>
#include <transform.h>

#include <optional>
#include <vector>

namespace sc {
    // Transparency is the colour's alpha, for strokes, fills and text alike: 0 leaves the part out.
    // sc-pdf and sc-mupdf paint partly transparent colours as they are; sc-image paints them opaque.

    /// How overlapping parts of a filled path count as inside: nonzero (the default) or even-odd (a
    /// subpath inside another cuts a hole, as in a ring).
    enum class fill_rule { nonzero, even_odd };

    /// How a shape's outline is painted: a colour and a width in the canvas's units, centred on the
    /// edge. A transparent colour or a width of 0 leaves it out.
    ///
    ///     canvas.draw(box, sc::stroke{sc::color::Black, 0.5});
    ///
    /// Planned (an engine that can't do one draws the plain outline): dashes, caps and joins, a nib,
    /// gradient and pattern paint.
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

        [[nodiscard]] bool visible() const { return colour_.alpha() > 0 && width_ > 0; }

        bool operator==(const stroke &) const = default;

    private:
        // Literal values, not color::Black: a stroke at namespace scope elsewhere may be made before
        // sc-core's colour constants are.
        sc::color colour_{0, 0, 0};
        double width_ = 1;
    };

    /// A colour changing along a line, from (offset 0) to (offset 1), in the canvas's coordinates;
    /// along any line across it the colour is the same, and past the ends the end colours carry on.
    ///
    ///     sc::linear_gradient green{{0, 100}, {0, 0}, {{0, sc::color{"#20b038"}}, {1, sc::color{"#60d66a"}}}};
    ///     canvas.draw(bubble, sc::appearance::filled(green));
    class linear_gradient {
    public:
        struct stop {
            double offset; // 0 to 1
            sc::color colour;

            bool operator==(const stop &) const = default;
        };

        /// The stops in order of offset (each kept within 0 to 1); none is black.
        linear_gradient(point from, point to, std::vector<stop> stops);

        [[nodiscard]] point from() const { return from_; }
        [[nodiscard]] point to() const { return to_; }
        [[nodiscard]] const std::vector<stop> &stops() const { return stops_; }

        /// How far along at is: 0 at from, 1 at to, below 0 or above 1 past them.
        [[nodiscard]] double offset_of(const point &at) const;

        /// The colour at offset, between the stops either side of it (each channel and alpha
        /// straight across).
        [[nodiscard]] sc::color colour_at(double offset) const;

        /// Through a transform, any: its colour lines stay parallel.
        [[nodiscard]] linear_gradient transformed(const transform &change) const;

        bool operator==(const linear_gradient &) const = default;

    private:
        point from_, to_;
        std::vector<stop> stops_;
    };

    /// How a shape's inside is painted: a colour or a linear gradient, and a fill rule. None
    /// (transparent) by default.
    ///
    ///     canvas.draw(ring, sc::appearance::filled(sc::fill{sc::color::Blue, sc::fill_rule::even_odd}));
    ///
    /// sc-image draws gradients; sc-pdf and sc-mupdf fill with the gradient's middle colour for now.
    /// Planned: radial gradients, patterns and images (an engine that can't do one fills with the
    /// nearest flat colour).
    class fill {
    public:
        /// None: transparent.
        fill() = default;

        /// A colour converts to a fill, by the nonzero rule.
        fill(const sc::color &colour, const fill_rule rule = fill_rule::nonzero) : colour_(colour), rule_(rule) {
        }

        /// So does a gradient. Its middle colour stands in on a canvas that can't draw gradients.
        fill(const linear_gradient &gradient, const fill_rule rule = fill_rule::nonzero)
            : colour_(gradient.colour_at(0.5)), rule_(rule), gradient_(gradient) {
        }

        /// No inside.
        static fill none() { return {}; }

        [[nodiscard]] const sc::color &colour() const { return colour_; }

        /// For paths; a canvas that flattens paths into separate polygons ignores it.
        [[nodiscard]] fill_rule rule() const { return rule_; }

        /// The gradient, if it's one.
        [[nodiscard]] const std::optional<linear_gradient> &gradient() const { return gradient_; }

        [[nodiscard]] bool visible() const {
            if (!gradient_) return colour_.alpha() > 0;
            for (const auto &stop: gradient_->stops())
                if (stop.colour.alpha() > 0) return true;
            return false;
        }

        bool operator==(const fill &) const = default;

    private:
        sc::color colour_{0, 0, 0, 0}; // transparent
        fill_rule rule_ = fill_rule::nonzero;
        std::optional<linear_gradient> gradient_;
    };

    /// How a shape looks: a stroke for its outline (black, 1 wide) and a fill for its inside (none).
    /// A colour or a stroke converts to an outline only; braces give both:
    ///
    ///     canvas.draw(box);                                                  // black outline
    ///     canvas.draw(box, sc::color::Red);                                  // red outline
    ///     canvas.draw(box, {sc::stroke{sc::color::Black, 2}, sc::color{0.9, 0.95, 1}});   // outline, fill
    ///     canvas.draw(dot, sc::appearance::filled(sc::color::Red));          // fill only
    class appearance {
    public:
        appearance() = default;

        /// An outline in this colour, 1 wide, and no fill.
        appearance(const sc::color &outline) : stroke_(outline) {
        }

        appearance(const sc::stroke &stroke, const sc::fill &fill = {}) : stroke_(stroke), fill_(fill) {
        }

        /// Only the inside: no outline.
        static appearance filled(const sc::fill &fill) { return {sc::stroke::none(), fill}; }

        /// Nothing at all.
        static appearance none() { return {sc::stroke::none(), sc::fill::none()}; }

        [[nodiscard]] const sc::stroke &stroke() const { return stroke_; }
        const sc::stroke &stroke(const sc::stroke &value) { return stroke_ = value; }

        [[nodiscard]] const sc::fill &fill() const { return fill_; }
        const sc::fill &fill(const sc::fill &value) { return fill_ = value; }

        [[nodiscard]] bool visible() const { return stroke_.visible() || fill_.visible(); }

        bool operator==(const appearance &) const = default;

    private:
        sc::stroke stroke_;
        sc::fill fill_;
    };
}

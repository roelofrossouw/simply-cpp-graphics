#pragma once

#include "canvas.h"
#include "paint.h"
#include "text.h"

#include <geometry.h>
#include <transform.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sc {
    /// A shape and how it looks: one thing in a drawing. clip, when set, limits what's painted to
    /// inside it.
    struct drawable {
        sc::shape shape;
        sc::appearance appearance;
        std::optional<path> clip;

        /// The shape's bounds, within the clip's when there's one (the outline's width not included).
        [[nodiscard]] rect bounds() const;

        bool operator==(const drawable &) const = default;
    };

    /// A line of text and its fill, in a drawing: its baseline starts at text.position.
    struct label {
        sc::text text;
        sc::fill fill{sc::color{0, 0, 0}}; // black; a literal, as paint.h's defaults are
        std::optional<path> clip;

        /// The box its glyphs take, by its font's measurements (font.h): as wide as it advances,
        /// from the ascender above the baseline to the descender below. A font that isn't installed
        /// is estimated from its size.
        [[nodiscard]] rect bounds() const;

        bool operator==(const label &other) const;
    };

    /// A picture as data: a page size and the things on it, in order, each a drawable (a shape and
    /// its appearance) or a label (text and its fill). Draw it on any canvas, change it as a whole
    /// (transform it, flip it across its page), and read or write it as SVG.
    ///
    /// It's a canvas too: drawing on it records what's drawn, so a page can be drawn once and then
    /// measured, flipped, or replayed onto a PDF or an image.
    ///
    ///     sc::drawing badge{{200, 100}};
    ///     badge.add(sc::rounded_rect{sc::rect{0, 0, 200, 100}, 12}, sc::appearance::filled(sc::color{"steelblue"}));
    ///     badge.add(sc::text{"simply-cpp", {20, 60}, sc::font::bold("sans-serif", 24)}, sc::color::White);
    ///     badge.flip_horizontal();                      // mirrored across its page
    ///     pdf.draw(badge, sc::rect{50, 50, 200, 100});  // scaled to fit, centred
    ///     badge.save_svg("badge.svg");
    class drawing : public canvas {
    public:
        using item = std::variant<drawable, label>;

        /// Empty, with no page: its bounds() stand in for one.
        drawing() = default;

        /// Empty, on a page this size (from the origin), in its own units.
        explicit drawing(sc::size page);

        drawing(const drawing &) = default;
        drawing(drawing &&) noexcept = default;
        drawing &operator=(const drawing &) = default;
        drawing &operator=(drawing &&) noexcept = default;

        /// Reads SVG: paths, the basic shapes (rect, circle, ellipse, line, polyline, polygon), text,
        /// groups, transforms, clip paths, <defs> and <use>, flat colours and opacity, presentation
        /// attributes and style="...". Shapes keep their kind where their transform allows (see
        /// sc::transform::apply()). A mask is read as a clip of its shapes (their luminance aside).
        /// The page is the viewBox, moved to the origin, or else the width and height. Not read:
        /// gradients (a fallback colour is), patterns, markers, filters, CSS stylesheets.
        /// Throws std::runtime_error when it isn't SVG.
        [[nodiscard]] static drawing from_svg(std::string_view svg);

        /// Reads an SVG file; throws std::runtime_error when it can't be read or isn't SVG.
        [[nodiscard]] static drawing load_svg(const std::filesystem::path &file);

        /// As SVG: every shape as its own element (a rotated_rect as a rotated <rect>, a group as a
        /// <g>), labels as <text>, clips as <clipPath>s. from_svg() reads it back the same, but for
        /// a group, which comes back as its shapes.
        [[nodiscard]] std::string to_svg() const;

        /// Writes to_svg() to a file; throws std::runtime_error when it can't.
        void save_svg(const std::filesystem::path &file) const;

        /// Its page's size, from the origin; empty for none.
        [[nodiscard]] sc::size page() const { return page_; }
        void page(const sc::size &value) { page_ = value; }

        /// Adds a shape as it is (where drawing it through the canvas calls records what the
        /// canvas's primitives get: a rect as a polygon, say).
        void add(sc::shape shape, sc::appearance look = {}, std::optional<path> clip = {});

        /// Adds a line of text, black by default.
        void add(sc::text text, sc::fill fill = sc::color{0, 0, 0}, std::optional<path> clip = {});

        void add(item thing);

        [[nodiscard]] const std::vector<item> &items() const { return items_; }
        [[nodiscard]] std::size_t size() const { return items_.size(); }
        [[nodiscard]] bool empty() const { return items_.empty(); }
        void clear() { items_.clear(); }

        /// The area its items cover together; an empty rect when it has none.
        [[nodiscard]] rect bounds() const;

        /// Everything through the transform: shapes and clips (keeping their kind where they can),
        /// outlines' widths and text's sizes scaled with it, and each label moved to where its box
        /// lands (its glyphs aren't mirrored or turned). The page stays as it is.
        drawing &apply(const transform &change);

        /// Mirrored left to right, or top to bottom, about the middle of its page (of its bounds
        /// when it has no page): the whole drawing flipped, shapes mirrored and moved across.
        drawing &flip_horizontal();
        drawing &flip_vertical();

        bool operator==(const drawing &other) const { return page_ == other.page_ && items_ == other.items_; }

    protected:
        // Recording: each primitive is added as an item, inside the innermost clip pushed.
        void paint_polygon(const polygon &shape, const sc::appearance &look) override;

        void paint_circle(const circle &shape, const sc::appearance &look) override;

        void paint_line(const line &shape, const sc::stroke &stroke) override;

        void paint_text(const text &sample, const sc::fill &fill) override;

        void paint_rounded_rect(const rounded_rect &shape, const sc::appearance &look) override;

        void paint_path(const path &shape, const sc::appearance &look) override;

        void push_clip(const path &shape) override;

        void pop_clip() override;

        // Text measured by the font's own measurements (font.h), as sc-pdf and sc-mupdf measure it.
        double measure_width(const text &sample) override;

        double measure_x_height(const font &font) override;

        double measure_cap_height(const font &font) override;

    private:
        sc::size page_;
        std::vector<item> items_;
        std::vector<path> clips_; // pushed while recording
    };
}

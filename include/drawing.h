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
    /// A shape and how it looks, in a drawing; clip, when set, limits it to inside the clip.
    struct drawable {
        sc::shape shape;
        sc::appearance appearance;
        std::optional<path> clip;

        /// The shape's bounds within the clip's, the outline's width not included.
        [[nodiscard]] rect bounds() const;

        bool operator==(const drawable &) const = default;
    };

    /// A line of text and its fill, in a drawing.
    struct label {
        sc::text text;
        sc::fill fill{sc::color{0, 0, 0}}; // black; a literal, as paint.h's defaults are
        std::optional<path> clip;

        /// The box its glyphs take: its advance wide, from the font's ascender to its descender
        /// (estimated from the size when the font isn't installed), within the clip's bounds.
        [[nodiscard]] rect bounds() const;

        bool operator==(const label &other) const;
    };

    /// A raster image in a drawing: its unit square ((0, 0) its top-left corner, (1, 1) its
    /// bottom-right) through placement, so it moves, turns and mirrors as shapes do.
    struct figure {
        sc::bitmap bitmap;
        sc::transform placement;
        std::optional<path> clip;

        /// The placement that stretches the unit square over position.
        [[nodiscard]] static sc::transform filling(const rect &position);

        /// Where its corners land, within the clip's bounds.
        [[nodiscard]] rect bounds() const;

        bool operator==(const figure &) const = default;
    };

    /// A picture as data: a page size and its items in order, drawables, labels and figures. Draw it on any
    /// canvas, transform or flip it as a whole, read or write it as SVG. It's a canvas too: drawing on
    /// it records what's drawn, to measure, flip or replay later.
    ///
    ///     sc::drawing badge{{200, 100}};
    ///     badge.add(sc::rounded_rect{sc::rect{0, 0, 200, 100}, 12}, sc::appearance::filled(sc::color{"steelblue"}));
    ///     badge.add(sc::text{"simply-cpp", {20, 60}, sc::font::bold("sans-serif", 24)}, sc::color::White);
    ///     badge.flip_horizontal();                      // mirrored across its page
    ///     pdf.draw(badge, sc::rect{50, 50, 200, 100});  // scaled to fit, centred
    ///     badge.save_svg("badge.svg");
    class drawing : public canvas {
    public:
        using item = std::variant<drawable, label, figure>;

        /// Empty, with no page: its bounds() stand in for one.
        drawing() = default;

        /// Empty, on a page this size (from the origin), in its own units.
        explicit drawing(sc::size page);

        drawing(const drawing &) = default;
        drawing(drawing &&) noexcept = default;
        drawing &operator=(const drawing &) = default;
        drawing &operator=(drawing &&) noexcept = default;

        /// Reads SVG: paths, basic shapes, text, groups, transforms, clip paths, <defs>/<use>, flat
        /// colours and opacity, presentation attributes and style="...". Shapes keep their kind
        /// where their transform allows; a mask is read as a clip of its shapes. The page is the
        /// width and height in pixels (units converted), the viewBox fitted into it; else the
        /// viewBox's size. Linear gradients are read (stops, units, gradientTransform, href), and
        /// images that are PNG or JPEG data: URIs (preserveAspectRatio too). Not read: radial
        /// gradients (their middle colour is), patterns, markers, filters, stylesheets.
        /// Throws std::runtime_error when it isn't SVG.
        ///
        /// TODO: radial gradients, and masks by their luminance rather than as clips.
        [[nodiscard]] static drawing from_svg(std::string_view svg);

        /// Reads an SVG file; throws std::runtime_error when it can't be read or isn't SVG.
        [[nodiscard]] static drawing load_svg(const std::filesystem::path &file);

        /// As SVG, each shape its own element (a rotated_rect a rotated <rect>, a group a <g>, a
        /// figure an <image> of a data: URI), so from_svg() reads it back the same, a group as its
        /// shapes. Without a page, the page runs from the origin to what's drawn.
        [[nodiscard]] std::string to_svg() const;

        /// Writes to_svg() to a file; throws std::runtime_error when it can't.
        void save_svg(const std::filesystem::path &file) const;

        /// Its page's size, from the origin; empty for none.
        [[nodiscard]] sc::size page() const { return page_; }
        void page(const sc::size &value) { page_ = value; }

        /// Adds a shape as it is (drawing it through the canvas calls records the primitives: a
        /// rect as a polygon).
        void add(sc::shape shape, sc::appearance look = {}, std::optional<path> clip = {});

        /// Adds a line of text, black by default.
        void add(sc::text text, sc::fill fill = sc::color{0, 0, 0}, std::optional<path> clip = {});

        /// Adds a raster image stretched to fill position.
        void add(sc::bitmap picture, const rect &position, std::optional<path> clip = {});

        void add(item thing);

        [[nodiscard]] const std::vector<item> &items() const { return items_; }
        [[nodiscard]] std::size_t size() const { return items_.size(); }
        [[nodiscard]] bool empty() const { return items_.empty(); }
        void clear() { items_.clear(); }

        /// The area its items cover together; an empty rect when it has none.
        [[nodiscard]] rect bounds() const;

        /// Its page, from the origin; its bounds() when it has none.
        [[nodiscard]] rect frame() const;

        /// Every shape, figure and clip through the transform, outlines and text sizes scaled with
        /// it; a label moves to where its box lands, its glyphs not mirrored or turned. The page stays.
        drawing &apply(const transform &change);

        /// Mirrored about the middle of its page (of its bounds when it has none).
        drawing &flip_horizontal();
        drawing &flip_vertical();

        bool operator==(const drawing &other) const { return page_ == other.page_ && items_ == other.items_; }

        /// Recorded with what's drawn after it (the innermost clip only, while clips nest).
        void push_clip(const path &shape) override;

        void pop_clip() override;

    protected:
        // Recording: each primitive becomes an item, inside the innermost clip pushed.
        void paint_polygon(const polygon &shape, const sc::appearance &look) override;

        void paint_circle(const circle &shape, const sc::appearance &look) override;

        void paint_line(const line &shape, const sc::stroke &stroke) override;

        void paint_text(const text &sample, const sc::fill &fill) override;

        void paint_rounded_rect(const rounded_rect &shape, const sc::appearance &look) override;

        void paint_path(const path &shape, const sc::appearance &look) override;

        void paint_bitmap(const bitmap &picture, const transform &placement) override;

        // By the font's measurements (font.h), as sc-pdf and sc-mupdf measure.
        double measure_width(const text &sample) override;

        double measure_x_height(const font &font) override;

        double measure_cap_height(const font &font) override;

    private:
        sc::size page_;
        std::vector<item> items_;
        std::vector<path> clips_; // pushed while recording
    };
}

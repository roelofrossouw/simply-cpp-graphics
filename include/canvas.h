#pragma once

#include "font.h"
#include "paint.h"
#include "paragraph.h"
#include "svg.h"
#include "text.h"

#include <circle.h>
#include <geometry.h>
#include <color.h>
#include <line.h>
#include <path.h>
#include <point.h>
#include <polygon.h>
#include <rect.h>
#include <rotated_rect.h>
#include <rounded_rect.h>

#include <string>
#include <vector>

namespace sc {
    /// Something to draw on, such as an sc::image or an sc::pdf, so the same calls draw on either.
    ///
    /// Every shape is drawn with draw(shape, stroke, fill): its outline painted by the stroke (black,
    /// 1 wide, by default) and its inside by the fill (none by default). A colour converts to either,
    /// so draw(box, sc::color::Red) outlines in red and draw(box, sc::stroke::none(), sc::color::Red)
    /// fills. A line has no inside: draw(line, stroke). Text is filled: draw(text, fill), black by
    /// default. Coordinates are the canvas's own (pixels on an image, points on a PDF), from the
    /// top-left corner with y down. Each shape works in either flavour (rect or rect_i, ...); any
    /// other sc::geometry draws through its path, and an sc::shape as whichever shape it holds.
    ///
    /// A canvas implements a few primitives (polygon, circle, line, text and, if it can do better
    /// than a polygon, rounded rectangles and paths); the draw() calls turn everything else into
    /// those. Which stroke and fill features show is up to the canvas (see paint.h).
    class canvas {
    public:
        virtual ~canvas() = default;

        template<typename T>
        void draw(const rect_<T> &shape, const sc::stroke &stroke = {}, const sc::fill &fill = {}) {
            draw_polygon(static_cast<polygon>(rect(shape)), stroke, fill);
        }

        /// Its corners rounded with its radius, at most half the shorter side; 0 is square.
        template<Numeric T>
        void draw(const rounded_rect_<T> &shape, const sc::stroke &stroke = {}, const sc::fill &fill = {}) {
            draw_rounded_rect(rounded_rect(shape), stroke, fill);
        }

        /// A rectangle turned angle degrees about its centre, clockwise (y points down).
        template<Numeric T>
        void draw(const rotated_rect_<T> &shape, const sc::stroke &stroke = {}, const sc::fill &fill = {}) {
            draw_polygon(static_cast<polygon>(shape), stroke, fill);
        }

        /// A closed polygon through the points; fewer than two draw nothing.
        template<Numeric T>
        void draw(const polygon_<T> &shape, const sc::stroke &stroke = {}, const sc::fill &fill = {}) {
            draw_polygon(polygon(shape), stroke, fill);
        }

        /// A radius of 0 draws nothing.
        template<Numeric T>
        void draw(const circle_<T> &shape, const sc::stroke &stroke = {}, const sc::fill &fill = {}) {
            draw_circle(circle(shape), stroke, fill);
        }

        /// A straight line: it has no inside to fill.
        template<Numeric T>
        void draw(const line_<T> &shape, const sc::stroke &stroke = {}) {
            draw_line(line(shape), stroke);
        }

        /// A path of lines and curves; open subpaths are only stroked.
        void draw(const path &shape, const sc::stroke &stroke = {}, const sc::fill &fill = {});

        /// Any other sc::geometry (a shape of your own: bounds() and to_path()), drawn as its path.
        /// sc-core's shapes have the overloads above, so engines draw them natively.
        template<geometry G>
        void draw(const G &shape, const sc::stroke &stroke = {}, const sc::fill &fill = {}) {
            draw(to_path(shape), stroke, fill);
        }

        /// Whichever shape it holds, as that shape (a line by its stroke alone).
        void draw(const shape &held, const sc::stroke &stroke = {}, const sc::fill &fill = {});

        /// An SVG drawing, scaled to fit position with its aspect ratio kept and centred in it, in
        /// its own strokes and fills (stroke widths scale with it).
        void draw(const svg &drawing, const rect &position);

        /// A line of text, its baseline starting at sample.position, its glyphs filled (black by
        /// default). Planned: a stroke after the fill, for outlined text.
        void draw(const text &sample, const sc::fill &fill = color::Black);

        /// Text wrapped into text.area and aligned, measured with this canvas's text_width() so the
        /// lines fit as it draws them: the first baseline the font's ascender below the top (the
        /// installed face's, font.ascender(); else its capitals' height), then a line every
        /// text.line_spacing x font size while they fit the area's height. Returns the text that
        /// didn't fit, from its first line on (empty when it all did), to go on in another area.
        std::string draw(const paragraph &text, const sc::fill &fill = color::Black);

        /// How far sample advances in its own font, in the canvas's units: the width it takes when
        /// drawn. 0 for empty text or a size of 0.
        [[nodiscard]] double text_width(const text &sample);

        /// The height of a lowercase x and of a capital H in font, in the canvas's units.
        [[nodiscard]] double text_x_height(const font &font = {});

        [[nodiscard]] double text_cap_height(const font &font = {});

    private:
        void draw_rounded_rect(const rounded_rect &shape, const sc::stroke &stroke, const sc::fill &fill);

        void draw_polygon(const polygon &shape, const sc::stroke &stroke, const sc::fill &fill);

        void draw_circle(const circle &shape, const sc::stroke &stroke, const sc::fill &fill);

        void draw_line(const line &shape, const sc::stroke &stroke);

    protected:
        // The primitives. draw() has already skipped what draws nothing and passes a stroke or fill
        // that wouldn't show as stroke::none() or fill::none(), so a primitive only checks visible().
        virtual void paint_polygon(const polygon &shape, const sc::stroke &stroke, const sc::fill &fill) = 0;

        virtual void paint_circle(const circle &shape, const sc::stroke &stroke, const sc::fill &fill) = 0;

        virtual void paint_line(const line &shape, const sc::stroke &stroke) = 0;

        virtual void paint_text(const text &sample, const sc::fill &fill) = 0;

        /// By default the path's curves are cut into straight segments: each closed subpath is
        /// drawn as a polygon (so subpaths fill separately, whatever the fill rule: no holes), open
        /// ones as lines. A canvas with curves, or that can fill several outlines at once, can do
        /// better.
        virtual void paint_path(const path &shape, const sc::stroke &stroke, const sc::fill &fill);

        /// Limits what's painted to inside shape (by the nonzero rule; a clip rule is planned) until
        /// the matching pop_clip(); clips nest. By default a canvas doesn't clip.
        virtual void push_clip(const path &shape);

        virtual void pop_clip();

        // Text measurements: content isn't empty and the size is above 0.
        virtual double measure_width(const text &sample) = 0;

        virtual double measure_x_height(const font &font) = 0;

        virtual double measure_cap_height(const font &font) = 0;

        /// The radius is above 0 and at most half the shorter side. By default the corners are arcs
        /// of short straight segments; a canvas with curves can do better.
        virtual void paint_rounded_rect(const rounded_rect &shape, const sc::stroke &stroke, const sc::fill &fill);
    };
}

#pragma once

#include "font.h"
#include "paragraph.h"
#include "path.h"
#include "svg.h"
#include "text.h"

#include <circle.h>
#include <color.h>
#include <line.h>
#include <point.h>
#include <polygon.h>
#include <rect.h>
#include <rotated_rect.h>

#include <string>
#include <vector>

namespace sc {
    /// Something to draw on, such as an sc::image or an sc::pdf, so the same calls draw on either.
    ///
    /// Every shape is drawn with draw(shape, stroke, fill, width): its outline in stroke, width wide,
    /// and its inside in fill. A transparent colour (alpha 0, such as color::Transparent, the fill's
    /// default) leaves that part out. Coordinates are the canvas's own (pixels on an image, points on
    /// a PDF), from the top-left corner with y down.
    ///
    /// Lines have no fill: draw(line, stroke, width).
    ///
    /// A canvas implements a few primitives (polygon, circle, line, text and, if it can do better
    /// than a polygon, rounded rectangles); the draw() calls here turn everything else into those.
    class canvas {
    public:
        virtual ~canvas() = default;

        // Each shape in either flavour (rect or rect_i, circle or circle_i, ...): drawn in doubles.

        /// A rectangle, its corners rounded with radius when that's above 0 (at most half the
        /// shorter side).
        template<typename T>
        void draw(const rect_<T> &shape, const color &stroke = color::Black, const color &fill = color::Transparent,
                  const double width = 1, const double radius = 0) {
            draw_rect(rect(shape), stroke, fill, width, radius);
        }

        /// A rectangle turned angle degrees about its centre, clockwise (y points down).
        template<Numeric T>
        void draw(const rotated_rect_<T> &shape, const color &stroke = color::Black,
                  const color &fill = color::Transparent, const double width = 1) {
            draw_polygon(static_cast<polygon>(shape), stroke, fill, width);
        }

        /// A closed polygon through the points; fewer than two draw nothing.
        template<Numeric T>
        void draw(const polygon_<T> &shape, const color &stroke = color::Black, const color &fill = color::Transparent,
                  const double width = 1) {
            draw_polygon(polygon(shape), stroke, fill, width);
        }

        /// A circle; a radius of 0 draws nothing.
        template<Numeric T>
        void draw(const circle_<T> &shape, const color &stroke = color::Black, const color &fill = color::Transparent,
                  const double width = 1) {
            draw_circle(circle(shape), stroke, fill, width);
        }

        /// A straight line, width wide: a line has no inside to fill.
        template<Numeric T>
        void draw(const line_<T> &shape, const color &stroke = color::Black, const double width = 1) {
            draw_segment(line(shape), stroke, width);
        }

        /// A path of lines and curves; open subpaths are only stroked.
        void draw(const path &shape, const color &stroke = color::Black, const color &fill = color::Transparent,
                  double width = 1);

        /// An SVG drawing, scaled to fit position with its aspect ratio kept and centred in it, in
        /// its own colours (stroke widths scale with it).
        void draw(const svg &drawing, const rect &position);

        /// A line of text, its baseline starting at sample.position, in colour.
        void draw(const text &sample, const color &colour = color::Black);

        /// Text wrapped into text.area and aligned, measured with this canvas's text_width() so the
        /// lines fit as it draws them: the first baseline the font's ascender below the top (the
        /// installed face's, font.ascender(); else its capitals' height), then a line every
        /// text.line_spacing x font size while they fit the area's height. Returns the text that
        /// didn't fit, from its first line on (empty when it all did), to go on in another area.
        std::string draw(const paragraph &text, const color &colour = color::Black);

        /// Each of shapes, the same way.
        template<typename Geometry>
        void draw(const std::vector<Geometry> &shapes, const color &stroke = color::Black,
                  const color &fill = color::Transparent, const double width = 1) {
            for (const auto &shape: shapes) draw(shape, stroke, fill, width);
        }

        template<Numeric T>
        void draw(const std::vector<line_<T> > &shapes, const color &stroke = color::Black, const double width = 1) {
            for (const auto &shape: shapes) draw(shape, stroke, width);
        }

        /// How far sample advances in its own font, in the canvas's units: the width it takes when
        /// drawn. 0 for empty text or a size of 0.
        [[nodiscard]] double text_width(const text &sample);

        /// The height of a lowercase x and of a capital H in font, in the canvas's units.
        [[nodiscard]] double text_x_height(const font &font = {});

        [[nodiscard]] double text_cap_height(const font &font = {});

    private:
        void draw_rect(const rect &shape, const color &stroke, const color &fill, double width, double radius);

        void draw_polygon(const polygon &shape, const color &stroke, const color &fill, double width);

        void draw_circle(const circle &shape, const color &stroke, const color &fill, double width);

        void draw_segment(const line &shape, const color &stroke, double width);

        void draw_path(const path &shape, const color &stroke, const color &fill, double width);

    protected:
        // The primitives. draw() has already left out a transparent stroke or fill (passing it as
        // color::Transparent, with a width of 0 for no stroke) and skipped what draws nothing.
        virtual void paint_polygon(const polygon &shape, const color &stroke, const color &fill, double width) = 0;

        virtual void paint_circle(const circle &shape, const color &stroke, const color &fill, double width) = 0;

        virtual void paint_line(const point &from, const point &to, const color &colour, double width) = 0;

        virtual void paint_text(const text &sample, const color &colour) = 0;

        /// By default the path's curves are cut into straight segments: each closed subpath is
        /// drawn as a polygon (so subpaths fill separately: no holes), open ones as lines. A canvas
        /// with curves, or that can fill several outlines at once, can do better.
        virtual void paint_path(const path &shape, const color &stroke, const color &fill, double width);

        /// Limits what's painted to inside shape until the matching pop_clip(); clips nest. By
        /// default a canvas doesn't clip.
        virtual void push_clip(const path &shape);

        virtual void pop_clip();

        // Text measurements: content isn't empty and the size is above 0.
        virtual double measure_width(const text &sample) = 0;

        virtual double measure_x_height(const font &font) = 0;

        virtual double measure_cap_height(const font &font) = 0;

        /// radius is above 0 and at most half the shorter side. By default the corners are arcs of
        /// short straight segments; a canvas with curves can do better.
        virtual void paint_rounded_rect(const rect &shape, double radius, const color &stroke, const color &fill, double width);
    };
}

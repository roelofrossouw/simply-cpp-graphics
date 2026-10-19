#pragma once

#include "bitmap.h"
#include "font.h"
#include "paint.h"
#include "paragraph.h"
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
#include <transform.h>

#include <string>
#include <vector>

namespace sc {
    class drawing;

    /// Something to draw on (an sc::image, an sc::pdf, an sc::drawing), so the same calls draw on any.
    /// Shapes take an sc::appearance (paint.h), a line a stroke, text a fill. Coordinates are the
    /// canvas's own (pixels on an image, points on a PDF), from the top-left corner, y down.
    ///
    /// An engine implements the protected primitives; draw() reduces everything to them.
    class canvas {
    public:
        virtual ~canvas() = default;

        template<typename T>
        void draw(const rect_<T> &shape, const sc::appearance &look = {}) {
            draw_polygon(static_cast<polygon>(rect(shape)), look);
        }

        /// Its corners rounded with its radius, at most half the shorter side; 0 is square.
        template<Numeric T>
        void draw(const rounded_rect_<T> &shape, const sc::appearance &look = {}) {
            draw_rounded_rect(rounded_rect(shape), look);
        }

        /// A rectangle turned angle degrees about its centre, clockwise (y points down).
        template<Numeric T>
        void draw(const rotated_rect_<T> &shape, const sc::appearance &look = {}) {
            draw_polygon(static_cast<polygon>(shape), look);
        }

        /// A closed polygon through the points; fewer than two draw nothing.
        template<Numeric T>
        void draw(const polygon_<T> &shape, const sc::appearance &look = {}) {
            draw_polygon(polygon(shape), look);
        }

        /// A radius of 0 draws nothing.
        template<Numeric T>
        void draw(const circle_<T> &shape, const sc::appearance &look = {}) {
            draw_circle(circle(shape), look);
        }

        /// A straight line: it has no inside to fill.
        template<Numeric T>
        void draw(const line_<T> &shape, const sc::stroke &stroke = {}) {
            draw_line(line(shape), stroke);
        }

        /// A path of lines and curves; an open subpath is filled as if closed (as SVG and PDF fill it)
        /// and stroked open.
        void draw(const path &shape, const sc::appearance &look = {});

        /// Any other sc::geometry (a shape of your own: bounds() and to_path()), drawn as its path.
        template<geometry G>
        void draw(const G &shape, const sc::appearance &look = {}) {
            draw(to_path(shape), look);
        }

        /// Each of the group's shapes, as itself, in this appearance (a line by its stroke alone).
        void draw(const group &shapes, const sc::appearance &look = {});

        /// Whichever shape it holds, as that shape (a line by the appearance's stroke alone).
        void draw(const shape &held, const sc::appearance &look = {});

        /// A drawing's items at its own coordinates, in order.
        void draw(const drawing &picture);

        /// A drawing's page (its bounds when it has none) fitted into position, aspect ratio kept,
        /// centred.
        ///
        ///     canvas.draw(sc::drawing::load_svg("logo.svg"), box);
        void draw(const drawing &picture, const rect &position);

        /// A raster image stretched to fill position, as HTML places one (keep its size()'s aspect
        /// ratio to keep it undistorted); an empty position draws nothing.
        void draw(const bitmap &picture, const rect &position);

        /// A line of text, its baseline starting at sample.position.
        void draw(const text &sample, const sc::fill &fill = color::Black);

        /// Text wrapped into text.area and aligned, measured with this canvas's text_width() so the
        /// lines fit as it draws them. Returns what didn't fit the area's height (empty when it all
        /// did), to go on in another area.
        std::string draw(const paragraph &text, const sc::fill &fill = color::Black);

        /// Limits what's drawn to inside shape (by the nonzero rule; a clip rule is planned) until
        /// the matching pop_clip(); clips nest. A canvas implements them (by default it doesn't clip).
        ///
        ///     canvas.push_clip(sc::to_path(sc::circle{{50, 50}, 40}));
        ///     canvas.draw(photo, box);                      // a round photo
        ///     canvas.pop_clip();
        virtual void push_clip(const path &shape);

        virtual void pop_clip();

        /// How far sample advances when drawn, in the canvas's units; 0 for empty text or a size of 0.
        [[nodiscard]] double text_width(const text &sample);

        /// The height of a lowercase x and of a capital H in font, in the canvas's units.
        [[nodiscard]] double text_x_height(const font &font = {});

        [[nodiscard]] double text_cap_height(const font &font = {});

    private:
        void draw_rounded_rect(const rounded_rect &shape, const sc::appearance &look);

        void draw_polygon(const polygon &shape, const sc::appearance &look);

        void draw_circle(const circle &shape, const sc::appearance &look);

        void draw_line(const line &shape, const sc::stroke &stroke);

    protected:
        // The primitives. draw() has skipped what draws nothing and passes a stroke or fill that
        // wouldn't show as none(), so a primitive only checks visible(); one of the two always is.
        virtual void paint_polygon(const polygon &shape, const sc::appearance &look) = 0;

        virtual void paint_circle(const circle &shape, const sc::appearance &look) = 0;

        virtual void paint_line(const line &shape, const sc::stroke &stroke) = 0;

        virtual void paint_text(const text &sample, const sc::fill &fill) = 0;

        /// The image's unit square ((0, 0) its top-left corner, (1, 1) its bottom-right) through
        /// placement onto the canvas: any affine transform, so a drawing can turn or mirror it.
        virtual void paint_bitmap(const bitmap &picture, const transform &placement) = 0;

        /// By default curves are cut into straight segments and each subpath filled as a polygon
        /// (separately whatever the fill rule: no holes), open ones stroked as lines.
        virtual void paint_path(const path &shape, const sc::appearance &look);

        // Text measurements: content isn't empty and the size is above 0.
        virtual double measure_width(const text &sample) = 0;

        virtual double measure_x_height(const font &font) = 0;

        virtual double measure_cap_height(const font &font) = 0;

        /// The radius is above 0 and at most half the shorter side. By default the corners are arcs
        /// of short straight segments.
        virtual void paint_rounded_rect(const rounded_rect &shape, const sc::appearance &look);
    };
}

// sc::drawing is a canvas too; including it here keeps canvas.h the one header to draw with.
#include "drawing.h"

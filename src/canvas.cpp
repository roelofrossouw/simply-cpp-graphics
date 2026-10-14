#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace sc {
    namespace {
        // Nothing to stroke without a colour or a width, nothing to fill without a colour.
        bool visible(const color &stroke, const color &fill, const double width) {
            return (stroke.alpha() > 0 && width > 0) || fill.alpha() > 0;
        }

        // The stroke as the primitives get it: transparent with no width when it doesn't show.
        std::pair<color, double> outline(const color &stroke, const double width) {
            if (stroke.alpha() > 0 && width > 0) return {stroke, width};
            return {color::Transparent, 0};
        }
    }

    void canvas::draw_rect(const rect &shape, const color &stroke, const color &fill, const double width, const double radius) {
        if (!visible(stroke, fill, width)) return;
        const double limit = std::min(shape.width(), shape.height()) / 2;
        if (radius > 0 && limit > 0) {
            const auto [line, line_width] = outline(stroke, width);
            paint_rounded_rect(shape, std::min(radius, limit), line, fill, line_width);
        } else {
            draw_polygon(static_cast<polygon>(shape), stroke, fill, width);
        }
    }

    void canvas::draw_polygon(const polygon &shape, const color &stroke, const color &fill, const double width) {
        if (shape.size() < 2 || !visible(stroke, fill, width)) return;
        const auto [line, line_width] = outline(stroke, width);
        paint_polygon(shape, line, fill, line_width);
    }

    void canvas::draw_circle(const circle &shape, const color &stroke, const color &fill, const double width) {
        if (shape.radius() <= 0 || !visible(stroke, fill, width)) return;
        const auto [line, line_width] = outline(stroke, width);
        paint_circle(shape, line, fill, line_width);
    }

    void canvas::draw(const text &sample, const color &colour) {
        if (sample.content.empty() || colour.alpha() <= 0 || sample.font.size <= 0) return;
        paint_text(sample, colour);
    }

    void canvas::draw(const path &shape, const color &stroke, const color &fill, const double width) {
        draw_path(shape, stroke, fill, width);
    }

    void canvas::draw_path(const path &shape, const color &stroke, const color &fill, const double width) {
        if (shape.empty() || !visible(stroke, fill, width)) return;
        const auto [line, line_width] = outline(stroke, width);
        paint_path(shape, line, fill, line_width);
    }

    void canvas::draw(const svg &drawing, const rect &position) {
        if (drawing.width() <= 0 || drawing.height() <= 0) return;
        const double scale = std::min(position.width() / drawing.width(), position.height() / drawing.height());
        if (scale <= 0) return;
        const point offset{position.left() + (position.width() - drawing.width() * scale) / 2,
                           position.top() + (position.height() - drawing.height() * scale) / 2};
        for (const auto &element: drawing.elements()) {
            if (element.clip) push_clip(element.clip->transformed(scale, scale, offset));
            draw_path(element.shape.transformed(scale, scale, offset), element.stroke, element.fill, element.width * scale);
            if (element.clip) pop_clip();
        }
    }

    void canvas::paint_path(const path &shape, const color &stroke, const color &fill, const double width) {
        for (const auto &[points, closed]: shape.flattened()) {
            if (closed) {
                paint_polygon(points, stroke, fill, width);
            } else if (width > 0) {
                for (std::size_t i = 1; i < points.size(); ++i) paint_line(points[i - 1], points[i], stroke, width);
            }
        }
    }

    void canvas::push_clip(const path &) {
    }

    void canvas::pop_clip() {
    }

    double canvas::text_width(const text &sample) {
        if (sample.content.empty() || sample.font.size <= 0) return 0;
        return measure_width(sample);
    }

    double canvas::text_x_height(const font &font) {
        return font.size > 0 ? measure_x_height(font) : 0;
    }

    double canvas::text_cap_height(const font &font) {
        return font.size > 0 ? measure_cap_height(font) : 0;
    }

    void canvas::draw_segment(const line &shape, const color &stroke, const double width) {
        if (stroke.alpha() <= 0 || width <= 0) return;
        paint_line(shape.from(), shape.to(), stroke, width);
    }

    void canvas::paint_rounded_rect(const rect &shape, const double radius, const color &stroke, const color &fill,
                                    const double width) {
        // Each corner a quarter circle of short segments, around clockwise (y down) from the top left.
        constexpr int segments = 8;
        const point centres[] = {
            {shape.left() + radius, shape.top() + radius}, {shape.right() - radius, shape.top() + radius},
            {shape.right() - radius, shape.bottom() - radius}, {shape.left() + radius, shape.bottom() - radius}
        };
        std::vector<point> points;
        points.reserve(4 * (segments + 1));
        for (int corner = 0; corner < 4; ++corner) {
            const double start = std::numbers::pi * (1 + corner / 2.0); // top left starts at 180 degrees
            for (int step = 0; step <= segments; ++step) {
                const double angle = start + std::numbers::pi / 2 * step / segments;
                points.emplace_back(centres[corner].x() + radius * std::cos(angle), centres[corner].y() + radius * std::sin(angle));
            }
        }
        paint_polygon(polygon{std::move(points)}, stroke, fill, width);
    }
}

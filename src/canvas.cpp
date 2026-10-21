#include "canvas.h"

#include "glyphs.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <variant>

namespace sc {
    namespace {
        // The appearance as the primitives get it: a stroke or fill that wouldn't show as none.
        appearance shown(const appearance &look) {
            return {look.stroke().visible() ? look.stroke() : stroke::none(), look.fill().visible() ? look.fill() : fill::none()};
        }
    }

    void canvas::draw_rounded_rect(const rounded_rect &shape, const sc::appearance &look) {
        if (!look.visible()) return;
        const rect box = shape.rect();
        const double radius = std::min(shape.radius(), std::min(box.width(), box.height()) / 2);
        if (radius > 0) paint_rounded_rect(rounded_rect{box, radius}, shown(look));
        else draw_polygon(static_cast<polygon>(box), look);
    }

    void canvas::draw_polygon(const polygon &shape, const sc::appearance &look) {
        if (shape.size() < 2 || !look.visible()) return;
        paint_polygon(shape, shown(look));
    }

    void canvas::draw_circle(const circle &shape, const sc::appearance &look) {
        if (shape.radius() <= 0 || !look.visible()) return;
        paint_circle(shape, shown(look));
    }

    void canvas::draw_line(const line &shape, const sc::stroke &stroke) {
        if (stroke.visible()) paint_line(shape, stroke);
    }

    void canvas::draw(const text &sample, const sc::fill &fill) {
        if (sample.content.empty() || !fill.visible() || sample.font.size <= 0) return;
        const auto parts = detail::runs(sample.font, sample.content);
        if (parts.size() == 1 && !parts[0].fallback && !parts[0].colour) {
            paint_text(sample, fill);
            return;
        }
        // Run by run along the baseline: each in the face that has it, emoji as bitmaps.
        double x = sample.position.x();
        const double y = sample.position.y();
        for (const auto &run: parts) {
            if (run.colour) {
                const auto laid = detail::lay_out_colour(*run.face, run.text, sample.font.size);
                for (const auto &glyph: laid.glyphs)
                    paint_bitmap(glyph.picture, figure::filling(rect{x + glyph.left, y + glyph.top, glyph.width, glyph.height}));
                x += laid.advance;
            } else {
                const text piece{run.text, {x, y}, detail::font_of(sample.font, run)};
                paint_text(piece, fill);
                x += measure_width(piece);
            }
        }
    }

    void canvas::draw(const bitmap &picture, const rect &position) {
        if (position.width() <= 0 || position.height() <= 0) return;
        paint_bitmap(picture, figure::filling(position));
    }

    void canvas::draw(const path &shape, const sc::appearance &look) {
        if (shape.empty() || !look.visible()) return;
        paint_path(shape, shown(look));
    }

    void canvas::draw(const group &shapes, const sc::appearance &look) {
        for (const auto &item: shapes) draw(item, look);
    }

    void canvas::draw(const shape &held, const sc::appearance &look) {
        std::visit([&](const auto &each) {
            if constexpr (std::same_as<std::decay_t<decltype(each)>, line>) draw(each, look.stroke());
            else draw(each, look);
        }, held.value());
    }

    void canvas::draw(const drawing &picture) {
        for (const auto &item: picture.items()) {
            std::visit([this](const auto &thing) {
                if (thing.clip) push_clip(*thing.clip);
                using kind = std::decay_t<decltype(thing)>;
                if constexpr (std::same_as<kind, drawable>) draw(thing.shape, thing.appearance);
                else if constexpr (std::same_as<kind, figure>) paint_bitmap(thing.bitmap, thing.placement);
                else draw(thing.text, thing.fill);
                if (thing.clip) pop_clip();
            }, item);
        }
    }

    void canvas::draw(const drawing &picture, const rect &position) {
        const rect frame = picture.frame();
        if (frame.width() <= 0 || frame.height() <= 0) return;
        const double scale = std::min(position.width() / frame.width(), position.height() / frame.height());
        if (scale <= 0) return;
        const point offset{position.left() + (position.width() - frame.width() * scale) / 2,
                           position.top() + (position.height() - frame.height() * scale) / 2};
        drawing fitted = picture;
        fitted.apply(transform::translate(-frame.left(), -frame.top()).then(transform::scale(scale)).then(transform::translate(offset)));
        draw(fitted);
    }

    void canvas::paint_path(const path &shape, const sc::appearance &look) {
        for (const auto &[points, closed]: shape.flattened()) {
            if (closed) {
                paint_polygon(points, look);
                continue;
            }
            if (look.fill().visible() && points.size() > 2) paint_polygon(points, appearance::filled(look.fill()));
            if (look.stroke().visible())
                for (std::size_t i = 1; i < points.size(); ++i) paint_line(line{points[i - 1], points[i]}, look.stroke());
        }
    }

    void canvas::push_clip(const path &) {
    }

    void canvas::pop_clip() {
    }

    double canvas::text_width(const text &sample) {
        if (sample.content.empty() || sample.font.size <= 0) return 0;
        const auto parts = detail::runs(sample.font, sample.content);
        if (parts.size() == 1 && !parts[0].fallback && !parts[0].colour) return measure_width(sample);
        double width = 0;
        for (const auto &run: parts) {
            if (run.colour) width += detail::lay_out_colour(*run.face, run.text, sample.font.size).advance;
            else width += measure_width(text{run.text, sample.position, detail::font_of(sample.font, run)});
        }
        return width;
    }

    double canvas::text_x_height(const font &font) {
        return font.size > 0 ? measure_x_height(font) : 0;
    }

    double canvas::text_cap_height(const font &font) {
        return font.size > 0 ? measure_cap_height(font) : 0;
    }

    void canvas::paint_rounded_rect(const rounded_rect &shape, const sc::appearance &look) {
        // Each corner a quarter circle of short segments, around clockwise (y down) from the top left.
        constexpr int segments = 8;
        const rect box = shape.rect();
        const double radius = shape.radius();
        const point centres[] = {
            {box.left() + radius, box.top() + radius}, {box.right() - radius, box.top() + radius},
            {box.right() - radius, box.bottom() - radius}, {box.left() + radius, box.bottom() - radius}
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
        paint_polygon(polygon{std::move(points)}, look);
    }
}

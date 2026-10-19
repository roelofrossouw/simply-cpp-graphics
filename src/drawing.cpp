#include "drawing.h"

#include <algorithm>
#include <utility>

namespace sc {
    namespace {
        // A label's font measurements, estimated from its size when the font isn't installed.
        double ascender_of(const font &spec) {
            const double measured = spec.ascender();
            return measured > 0 ? measured : spec.size * 0.8;
        }

        double descender_of(const font &spec) {
            const double measured = spec.descender();
            return measured > 0 || spec.face() ? measured : spec.size * 0.2;
        }

        double width_of(const text &sample) {
            if (sample.content.empty()) return 0;
            const double measured = sample.font.width(sample.content);
            return measured > 0 ? measured : static_cast<double>(sample.content.size()) * sample.font.size * 0.5;
        }

        rect within(const rect &box, const std::optional<path> &clip) { return clip ? box.intersect(clip->bounds()) : box; }
    }

    rect drawable::bounds() const { return within(shape.bounds(), clip); }

    rect label::bounds() const {
        const double ascender = ascender_of(text.font);
        const rect box{text.position.x(), text.position.y() - ascender, width_of(text), ascender + descender_of(text.font)};
        return within(box, clip);
    }

    bool label::operator==(const label &other) const {
        return text.content == other.text.content && text.position == other.text.position && text.font == other.text.font &&
               fill == other.fill && clip == other.clip;
    }

    drawing::drawing(const sc::size page) : page_(page) {
    }

    void drawing::add(sc::shape shape, sc::appearance look, std::optional<path> clip) {
        items_.emplace_back(drawable{std::move(shape), std::move(look), std::move(clip)});
    }

    void drawing::add(sc::text text, sc::fill fill, std::optional<path> clip) {
        items_.emplace_back(label{std::move(text), std::move(fill), std::move(clip)});
    }

    void drawing::add(item thing) { items_.push_back(std::move(thing)); }

    rect drawing::bounds() const {
        bool any = false;
        rect covered;
        for (const auto &item: items_) {
            const rect box = std::visit([](const auto &thing) { return thing.bounds(); }, item);
            if (any) covered.include(box);
            else covered = box;
            any = true;
        }
        return covered;
    }

    drawing &drawing::apply(const transform &change) {
        const double scale = change.scale_factor();
        const auto moved_clip = [&](const std::optional<path> &clip) -> std::optional<path> {
            if (clip) return change.apply(*clip);
            return std::nullopt;
        };
        for (auto &item: items_) {
            if (auto *shape = std::get_if<drawable>(&item)) {
                const auto &stroke = shape->appearance.stroke();
                shape->shape = change.apply(shape->shape);
                shape->appearance.stroke(sc::stroke{stroke.colour(), stroke.width() * scale});
                if (const auto &gradient = shape->appearance.fill().gradient())
                    shape->appearance.fill(sc::fill{gradient->transformed(change), shape->appearance.fill().rule()});
                shape->clip = moved_clip(shape->clip);
            } else {
                auto &text = std::get<label>(item);
                // The box the glyphs took, through the transform; the text set again at its corner.
                auto unclipped = text;
                unclipped.clip.reset();
                const rect landed = change.apply(unclipped.bounds()).bounds();
                text.text.font.size *= scale;
                if (const auto &gradient = text.fill.gradient()) text.fill = sc::fill{gradient->transformed(change), text.fill.rule()};
                text.text.position = {landed.left(), landed.top() + ascender_of(text.text.font)};
                text.clip = moved_clip(text.clip);
            }
        }
        return *this;
    }

    namespace {
        rect frame_of(const drawing &picture) {
            return picture.page().area() > 0 ? rect{0, 0, picture.page().width(), picture.page().height()} : picture.bounds();
        }
    }

    drawing &drawing::flip_horizontal() { return apply(transform::flip_horizontal(frame_of(*this).center().x())); }

    drawing &drawing::flip_vertical() { return apply(transform::flip_vertical(frame_of(*this).center().y())); }

    namespace {
        std::optional<path> innermost(const std::vector<path> &clips) {
            if (clips.empty()) return std::nullopt;
            return clips.back();
        }
    }

    void drawing::paint_polygon(const polygon &shape, const sc::appearance &look) { add(shape, look, innermost(clips_)); }

    void drawing::paint_circle(const circle &shape, const sc::appearance &look) { add(shape, look, innermost(clips_)); }

    void drawing::paint_line(const line &shape, const sc::stroke &stroke) { add(shape, stroke, innermost(clips_)); }

    void drawing::paint_text(const text &sample, const sc::fill &fill) { add(sample, fill, innermost(clips_)); }

    void drawing::paint_rounded_rect(const rounded_rect &shape, const sc::appearance &look) { add(shape, look, innermost(clips_)); }

    void drawing::paint_path(const path &shape, const sc::appearance &look) { add(shape, look, innermost(clips_)); }

    void drawing::push_clip(const path &shape) { clips_.push_back(shape); }

    void drawing::pop_clip() {
        if (!clips_.empty()) clips_.pop_back();
    }

    double drawing::measure_width(const text &sample) { return width_of(sample); }

    double drawing::measure_x_height(const font &font) {
        const double measured = font.x_height();
        return measured > 0 ? measured : font.size * 0.5;
    }

    double drawing::measure_cap_height(const font &font) {
        const double measured = font.cap_height();
        return measured > 0 ? measured : font.size * 0.7;
    }
}

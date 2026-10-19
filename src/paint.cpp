#include "paint.h"

#include <algorithm>
#include <utility>

namespace sc {
    linear_gradient::linear_gradient(const point from, const point to, std::vector<stop> stops)
        : from_(from), to_(to), stops_(std::move(stops)) {
        for (auto &each: stops_) each.offset = std::clamp(each.offset, 0.0, 1.0);
        std::ranges::stable_sort(stops_, {}, &stop::offset);
        if (stops_.empty()) stops_.push_back({0, color{0, 0, 0}});
    }

    double linear_gradient::offset_of(const point &at) const {
        const double dx = to_.x() - from_.x(), dy = to_.y() - from_.y(), length = dx * dx + dy * dy;
        if (length <= 0) return 0;
        return ((at.x() - from_.x()) * dx + (at.y() - from_.y()) * dy) / length;
    }

    color linear_gradient::colour_at(const double offset) const {
        if (offset <= stops_.front().offset) return stops_.front().colour;
        if (offset >= stops_.back().offset) return stops_.back().colour;
        const auto after = std::ranges::find_if(stops_, [&](const stop &each) { return each.offset >= offset; });
        const auto &[end, high] = *after;
        const auto &[start, low] = *(after - 1);
        const double t = end > start ? (offset - start) / (end - start) : 1;
        const auto mix = [t](const double a, const double b) { return a + (b - a) * t; };
        return {mix(low.red(), high.red()), mix(low.green(), high.green()), mix(low.blue(), high.blue()), mix(low.alpha(), high.alpha())};
    }

    linear_gradient linear_gradient::transformed(const transform &change) const {
        // The offset is a linear function of the point, t(x) = (x - from) . n with n = (to - from) / |to - from|^2.
        // Through the transform, t'(y) = t(T^-1 y) = y . g + k: a gradient from where t' = 0 along g, 1 / |g| long.
        const double dx = to_.x() - from_.x(), dy = to_.y() - from_.y(), length = dx * dx + dy * dy;
        if (length <= 0) return {change.apply(from_), change.apply(to_), stops_};
        const point n{dx / length, dy / length};
        const transform back = change.inverse();
        const point g{back.a() * n.x() + back.b() * n.y(), back.c() * n.x() + back.d() * n.y()};
        const double k = (back.e() - from_.x()) * n.x() + (back.f() - from_.y()) * n.y();
        const double g2 = g.x() * g.x() + g.y() * g.y();
        if (g2 <= 0) return {change.apply(from_), change.apply(to_), stops_};
        const point start{-k * g.x() / g2, -k * g.y() / g2};
        return {start, {start.x() + g.x() / g2, start.y() + g.y() / g2}, stops_};
    }
}

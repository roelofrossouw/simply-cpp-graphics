#include "path.h"

#include <algorithm>
#include <cmath>

namespace sc {
    path &path::move_to(const point &to) {
        steps_.push_back({kind::move, to});
        return *this;
    }

    path &path::line_to(const point &to) {
        if (steps_.empty()) return move_to(to);
        steps_.push_back({kind::line, to});
        return *this;
    }

    path &path::cubic_to(const point &first, const point &second, const point &to) {
        if (steps_.empty()) move_to(first);
        steps_.push_back({kind::cubic, to, first, second});
        return *this;
    }

    path &path::close() {
        if (!steps_.empty() && steps_.back().type != kind::close) steps_.push_back({kind::close, {}});
        return *this;
    }

    rect path::bounds() const {
        bool any = false;
        double left = 0, top = 0, right = 0, bottom = 0;
        const auto include = [&](const point &p) {
            if (!any) {
                left = right = p.x();
                top = bottom = p.y();
                any = true;
                return;
            }
            left = std::min(left, p.x());
            right = std::max(right, p.x());
            top = std::min(top, p.y());
            bottom = std::max(bottom, p.y());
        };
        for (const auto &s: steps_) {
            if (s.type == kind::close) continue;
            include(s.to);
            if (s.type == kind::cubic) {
                include(s.first);
                include(s.second);
            }
        }
        return rect::ltrb(left, top, right, bottom);
    }

    path path::transformed(const double scale_x, const double scale_y, const point &offset) const {
        const auto map = [&](const point &p) { return point{p.x() * scale_x + offset.x(), p.y() * scale_y + offset.y()}; };
        path result;
        result.steps_.reserve(steps_.size());
        for (const auto &s: steps_) {
            if (s.type == kind::close) result.steps_.push_back(s);
            else result.steps_.push_back({s.type, map(s.to), map(s.first), map(s.second)});
        }
        return result;
    }

    std::vector<path::outline> path::flattened(const double tolerance) const {
        std::vector<outline> result;
        std::vector<point> current;
        point start, at;
        const auto finish = [&](const bool closed) {
            if (current.size() >= 2) result.push_back({polygon{std::move(current)}, closed});
            current.clear();
        };
        for (const auto &s: steps_) {
            switch (s.type) {
                case kind::move:
                    finish(false);
                    start = at = s.to;
                    current.push_back(at);
                    break;
                case kind::line:
                    if (current.empty()) current.push_back(at); // carrying on after a close
                    at = s.to;
                    current.push_back(at);
                    break;
                case kind::cubic: {
                    if (current.empty()) current.push_back(at);
                    // Enough segments that none strays more than tolerance from the curve: the
                    // control polygon's length bounds the curve's, and its sag shrinks with 1/n^2.
                    const double reach = std::hypot(s.first.x() - at.x(), s.first.y() - at.y()) +
                                         std::hypot(s.second.x() - s.first.x(), s.second.y() - s.first.y()) +
                                         std::hypot(s.to.x() - s.second.x(), s.to.y() - s.second.y());
                    const int segments = std::clamp(static_cast<int>(std::ceil(std::sqrt(reach / std::max(tolerance, 1e-3)))), 1, 256);
                    for (int i = 1; i <= segments; ++i) {
                        const double t = static_cast<double>(i) / segments, u = 1 - t;
                        const double a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
                        current.emplace_back(a * at.x() + b * s.first.x() + c * s.second.x() + d * s.to.x(),
                                             a * at.y() + b * s.first.y() + c * s.second.y() + d * s.to.y());
                    }
                    at = s.to;
                    break;
                }
                case kind::close:
                    finish(true);
                    at = start;
                    break;
            }
        }
        finish(false);
        return result;
    }
}

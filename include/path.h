#pragma once

#include <point.h>
#include <polygon.h>
#include <rect.h>

#include <vector>

namespace sc {
    /// A shape of straight lines and cubic Bezier curves, such as an SVG path: one or more
    /// subpaths, each started by move_to() and optionally closed by close(). Coordinates are y down.
    /// How overlapping subpaths fill (a hole in an "o") is the fill's, not the path's: sc::fill_rule.
    class path {
    public:
        enum class kind { move, line, cubic, close };

        /// One step: to is where it ends; a cubic's control points are first and second.
        struct step {
            kind type;
            point to;
            point first{};
            point second{};

            bool operator==(const step &) const = default;
        };

        path &move_to(const point &to);

        path &line_to(const point &to);

        path &cubic_to(const point &first, const point &second, const point &to);

        /// Closes the current subpath, back to where it started.
        path &close();

        [[nodiscard]] const std::vector<step> &steps() const noexcept { return steps_; }
        [[nodiscard]] bool empty() const noexcept { return steps_.empty(); }

        /// The bounding box of every point, control points included (so it may be a little large
        /// around curves). Empty for an empty path.
        [[nodiscard]] rect bounds() const;

        /// Scaled about the origin, then moved by offset.
        [[nodiscard]] path transformed(double scale_x, double scale_y, const point &offset = {}) const;

        /// One subpath as straight segments: its points, curves cut into short lines, and whether
        /// it was closed.
        struct outline {
            polygon points;
            bool closed;
        };

        /// The subpaths as straight segments, curves cut into segments no more than tolerance
        /// off the curve, for canvases that only draw straight lines.
        [[nodiscard]] std::vector<outline> flattened(double tolerance = 0.25) const;

        bool operator==(const path &) const = default;

    private:
        std::vector<step> steps_;
    };
}

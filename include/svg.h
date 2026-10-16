#pragma once

#include "paint.h"

#include <path.h>

#include <color.h>

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace sc {
    /// An SVG drawing, read into what a canvas draws: paths with their fill, stroke and stroke
    /// width, in the drawing's own units with its viewBox at the origin, y down. Covers paths, the
    /// basic shapes (rect with rx/ry, circle, polygon, line), groups, transforms, clip paths,
    /// <defs>/<use> and flat fill and stroke colours: icons and logos, not gradients, text or
    /// filters. Draw it with canvas::draw(svg, position).
    class svg {
    public:
        /// One painted path: its stroke and fill (none when the SVG leaves one out); clip, when
        /// set, limits the paint to inside it.
        struct element {
            path shape;
            sc::stroke stroke = sc::stroke::none();
            sc::fill fill;
            std::optional<path> clip;
        };

        /// Reads SVG text; throws std::runtime_error when it isn't SVG (bad XML, or the root isn't
        /// <svg>).
        explicit svg(std::string_view text);

        /// Reads an SVG file; throws std::runtime_error when it can't be read or isn't SVG.
        [[nodiscard]] static svg load(const std::filesystem::path &file);

        /// The drawing's size, in its own units (its width and height, or its viewBox's).
        [[nodiscard]] double width() const noexcept { return width_; }
        [[nodiscard]] double height() const noexcept { return height_; }

        [[nodiscard]] const std::vector<element> &elements() const noexcept { return elements_; }

    private:
        double width_ = 0;
        double height_ = 0;
        std::vector<element> elements_;
    };
}

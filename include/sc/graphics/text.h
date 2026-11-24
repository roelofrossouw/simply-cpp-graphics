#pragma once

#include <sc/graphics/font.h>
#include <sc/geometry/point.h>

#include <string>

namespace sc::graphics {
    /// A line of text to draw on a canvas: its content (UTF-8), where its baseline starts, and its
    /// font.
    ///
    ///     sc::graphics::text{"Total", {50, 100}, {"serif", 14}}
    struct text {
        std::string content;
        point position;
        sc::graphics::font font;
    };
}

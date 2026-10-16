#pragma once

#include "font.h"
#include "point.h"

#include <string>

namespace sc {
    /// A line of text to draw on a canvas: its content (UTF-8), where its baseline starts, and its
    /// font.
    ///
    ///     sc::text{"Total", {50, 100}, {"serif", 14}}
    struct text {
        std::string content;
        point position;
        sc::font font;
    };
}

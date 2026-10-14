#pragma once

#include <compare>
#include <string>
#include <utility>

namespace sc {
    /// A typeface and size, for sc::text. family is a name ("DejaVu Sans", "Roboto") or a generic one
    /// ("sans-serif", "serif", "monospace"); size is the em size, in the canvas's units (points on a
    /// PDF, pixels on an image); weight is a CSS weight from 100 (thin) to 900 (black), 400 regular
    /// and 700 bold; style, when set, is a style name ("Condensed Bold") that wins over weight and
    /// italic where the canvas can use it. A canvas picks the closest font it has.
    struct font {
        std::string family = "sans-serif";
        double size = 12;
        int weight = 400;
        bool italic = false;
        std::string style;

        static font bold(std::string family = "sans-serif", const double size = 12) { return {std::move(family), size, 700}; }

        auto operator<=>(const font &) const = default;
    };
}

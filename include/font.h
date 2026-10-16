#pragma once

#include "font_face.h"
#include "typeface.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sc {
    /// A font to draw text in, as asked for: a typeface (family), a style of it and a size. family is
    /// a name ("DejaVu Sans", "Roboto") or a generic one ("sans-serif", "serif", "monospace"); size is
    /// the em size, in the canvas's units (points on a PDF, pixels on an image); weight is a CSS
    /// weight from 100 (thin) to 900 (black), 400 regular and 700 bold; style, when set, is a style
    /// name ("Condensed Bold") that wins over weight and italic where the canvas can use it.
    ///
    /// face() finds the installed face closest to it (an
    /// sc::font_face, whose typeface() is its family), and the measurements are that face at this
    /// size: how sc-pdf and sc-mupdf draw it. Canvases with fonts of their own (sc-image's) measure
    /// with canvas::text_width() instead.
    struct font {
        std::string family = "sans-serif";
        double size = 12;
        int weight = 400;
        bool italic = false;
        std::string style;

        static font bold(std::string family = "sans-serif", const double size = 12) { return {std::move(family), size, 700}; }

        /// The installed face closest to it (fontconfig: a substitute family when this one isn't
        /// installed); empty when no font is installed at all. Found once per family, weight, slant
        /// and style, and kept.
        [[nodiscard]] std::optional<font_face> face() const;

        /// Whether its family is installed, rather than substituted; a generic family
        /// ("sans-serif") is available when any font is.
        [[nodiscard]] bool available() const;

        /// Its face's measurements at this size (see sc::font_face); 0 without a face.
        [[nodiscard]] double width(std::string_view text) const;

        [[nodiscard]] double ascender() const;

        [[nodiscard]] double descender() const;

        [[nodiscard]] double line_height() const;

        [[nodiscard]] double x_height() const;

        [[nodiscard]] double cap_height() const;

        auto operator<=>(const font &) const = default;
    };
}

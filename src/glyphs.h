#pragma once

// Text split into runs a font can draw, and colour glyphs (emoji) as bitmaps. Used by sc::font and
// sc::canvas; not installed.

#include "bitmap.h"
#include "font.h"
#include "font_face.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sc::detail {
    // A piece of text and the face that draws it: the font's own, another that has its characters
    // (fallback), or a colour (emoji) face, whose glyphs are bitmaps.
    struct text_run {
        std::string text;
        std::optional<font_face> face; // empty only when no font is installed at all
        bool fallback = false;
        bool colour = false;
    };

    // The text in runs, in order: clusters (a character with its combining marks, an emoji sequence)
    // the font covers stay in it; others go to a face that has them, emoji to a colour face.
    std::vector<text_run> runs(const font &spec, std::string_view text);

    // The font that draws a run: the spec itself, or its fallback face at the spec's size.
    font font_of(const font &spec, const text_run &run);

    // A colour run laid out: each glyph's bitmap and where it goes, from the pen on the baseline
    // (y down, so top is usually negative), in the size's units, and how far the run advances.
    struct colour_glyph {
        bitmap picture;
        double left, top, width, height;
        double pen; // the pen's x when it's drawn, from the run's start
    };

    struct colour_layout {
        std::vector<colour_glyph> glyphs;
        double advance = 0;
    };

    colour_layout lay_out_colour(const font_face &face, std::string_view text, double size);
}

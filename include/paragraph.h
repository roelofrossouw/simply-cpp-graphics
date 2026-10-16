#pragma once

#include "font.h"

#include <rect.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sc {
    enum class align { left, center, right, justify };

    /// Text to set in a rectangle, for canvas::draw(paragraph): wrapped at the rectangle's width, line
    /// after line from its top, as many lines as fit its height.
    ///
    ///     const auto rest = canvas.draw(sc::paragraph{text, {50, 100, 495, 600}, {"serif", 11}, sc::align::justify});
    ///     if (!rest.empty()) { /* on the next page */ }
    struct paragraph {
        std::string content;    // UTF-8; a newline starts a new line, a blank line leaves one empty
        rect area;
        sc::font font;
        sc::align align = sc::align::left; // justify stretches every line but a paragraph's last
        double line_spacing = 1.2;          // baselines line_spacing x font.size apart
    };

    /// content (UTF-8) as lines at most width wide, by measure: broken at spaces (dropped there) and
    /// at newlines; a word wider than width is split between characters. With a width of 0 or less,
    /// only newlines break lines.
    [[nodiscard]] std::vector<std::string> wrap(std::string_view content, double width,
                                                const std::function<double(std::string_view)> &measure);

    /// The same, measured in font (its installed face, font.width()): how sc-pdf and sc-mupdf draw it.
    /// Without any font installed, only newlines break lines.
    [[nodiscard]] std::vector<std::string> wrap(std::string_view content, const font &font, double width);
}

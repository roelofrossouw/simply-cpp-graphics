#pragma once

#include <size.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sc {
    /// Paper sizes for the PDF engines (sc-pdf, sc-mupdf), portrait in points (1/72 inch): A3 842x1191,
    /// A4 595x842, A5 420x595, Letter 612x792, Legal 612x1008.
    enum class page_size { a3, a4, a5, letter, legal };

    enum class orientation { portrait, landscape };

    /// A page's width and height in points; landscape swaps them.
    ///
    ///     sc::pdf pdf{"wide.pdf", sc::page_size::a3, sc::orientation::landscape};   // 1191 x 842
    inline size page_dimensions(const page_size paper, const orientation turned = orientation::portrait) {
        size portrait{595, 842};
        switch (paper) {
            case page_size::a3: portrait = {842, 1191}; break;
            case page_size::a5: portrait = {420, 595}; break;
            case page_size::letter: portrait = {612, 792}; break;
            case page_size::legal: portrait = {612, 1008}; break;
            case page_size::a4: break;
        }
        return turned == orientation::landscape ? size{portrait.height(), portrait.width()} : portrait;
    }

    /// A size by its name, in any case: "a3", "A4", "a5", "letter" or "legal" (a command line's
    /// --size); std::invalid_argument for any other.
    inline page_size page_size_from(std::string_view name) {
        std::string lower{name};
        std::ranges::transform(lower, lower.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower == "a3") return page_size::a3;
        if (lower == "a4") return page_size::a4;
        if (lower == "a5") return page_size::a5;
        if (lower == "letter") return page_size::letter;
        if (lower == "legal") return page_size::legal;
        throw std::invalid_argument{"Unknown page size \"" + std::string{name} + "\": a3, a4, a5, letter or legal"};
    }
}

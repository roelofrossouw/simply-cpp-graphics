#pragma once

#include <distance.h>
#include <size.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sc {
    /// Paper sizes for the PDF engines (sc-pdf, sc-mupdf, sc-pdflite): ISO's in whole millimetres as
    /// ISO 216 has them (A3 297 x 420, A4 210 x 297, A5 148 x 210 mm), Letter and Legal in inches
    /// (8.5 x 11 and 8.5 x 14). In points (1/72 inch) the ISO sizes have fractions: A4 is
    /// 595.276 x 841.89.
    enum class page_size { A3, A4, A5, letter, legal };

    enum class orientation { portrait, landscape };

    /// A page's width and height in points; landscape swaps them.
    ///
    ///     sc::pdf pdf{"wide.pdf", sc::page_size::A3, sc::orientation::landscape};   // 420 x 297 mm
    inline size page_dimensions(const page_size paper, const orientation turned = orientation::portrait) {
        using dist::inch;
        using dist::mm;
        size portrait{mm(210), mm(297)};
        switch (paper) {
            case page_size::A3: portrait = {mm(297), mm(420)}; break;
            case page_size::A5: portrait = {mm(148), mm(210)}; break;
            case page_size::letter: portrait = {inch(8.5), inch(11)}; break;
            case page_size::legal: portrait = {inch(8.5), inch(14)}; break;
            case page_size::A4: break;
        }
        return turned == orientation::landscape ? size{portrait.height(), portrait.width()} : portrait;
    }

    /// A size by its name, in any case: "A3", "A4", "A5", "letter" or "legal" (a command line's
    /// --size); std::invalid_argument for any other.
    inline page_size page_size_from(std::string_view name) {
        std::string lower{name};
        std::ranges::transform(lower, lower.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower == "a3") return page_size::A3;
        if (lower == "a4") return page_size::A4;
        if (lower == "a5") return page_size::A5;
        if (lower == "letter") return page_size::letter;
        if (lower == "legal") return page_size::legal;
        throw std::invalid_argument{"Unknown page size \"" + std::string{name} + "\": A3, A4, A5, letter or legal"};
    }
}

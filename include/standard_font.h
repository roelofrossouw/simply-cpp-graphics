#pragma once

#include "font.h"

#include <optional>
#include <string>
#include <string_view>

namespace sc {
    /// One of PDF's built-in fonts: Helvetica, Times and Courier, each upright, bold, italic and bold
    /// italic, which every PDF reader has, so a PDF can use them without embedding anything. Their
    /// metrics are Adobe's (the Core 14 AFM files): widths in thousandths of the size for the
    /// Windows-1252 characters they're written in (WinAnsiEncoding), ascender, descender, cap
    /// and x heights.
    ///
    ///     if (const auto helvetica = sc::standard_font::of(sc::font{"Arial", 12, 700}))
    ///         helvetica->name();                 // "Helvetica-Bold"
    ///         helvetica->width("Total", 12);     // as every reader draws it
    class standard_font {
    public:
        /// The built-in font for a font's family, weight (600 and up bold) and slant: Helvetica for
        /// "Helvetica" or "Arial", Times for "Times", "Times-Roman" or "Times New Roman", Courier for
        /// "Courier" or "Courier New" (any case); nothing for another family.
        [[nodiscard]] static std::optional<standard_font> of(const font &spec);

        /// Its PDF name: "Helvetica", "Helvetica-BoldOblique", "Times-Roman", "Courier-Bold", ...
        [[nodiscard]] const std::string &name() const { return name_; }

        /// How far text (UTF-8) advances at size; characters outside Windows-1252 count 0 (see
        /// encode()), as the font doesn't have them.
        [[nodiscard]] double width(std::string_view text, double size) const;

        [[nodiscard]] double ascender(double size) const;
        [[nodiscard]] double descender(double size) const; // positive, below the baseline
        [[nodiscard]] double cap_height(double size) const;
        [[nodiscard]] double x_height(double size) const;

        /// A character's byte in Windows-1252 (WinAnsiEncoding), or nothing when it has none.
        [[nodiscard]] static std::optional<char> encode(char32_t character);

    private:
        std::string name_;
        int face_ = 0; // which of the twelve
    };
}

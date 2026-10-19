#pragma once

#include "font.h"

#include <array>
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
    ///     const sc::standard_font bold{sc::standard_font::face::helvetica_bold};
    ///     bold.name();                           // "Helvetica-Bold"
    ///     bold.width("Total", 12);               // as every reader draws it
    ///     sc::standard_font::of(sc::font{"Arial", 12, 700});   // the same, for a CSS-style font
    ///
    /// The faces are an enum, so a misspelt font is a compile error rather than a missing font.
    /// (PDF has two more built-in fonts, Symbol and ZapfDingbats, with encodings of their own; they
    /// aren't here.)
    class standard_font {
    public:
        enum class face {
            helvetica, helvetica_bold, helvetica_oblique, helvetica_bold_oblique,
            times_roman, times_bold, times_italic, times_bold_italic,
            courier, courier_bold, courier_oblique, courier_bold_oblique
        };

        /// Every face, in the order above.
        static constexpr std::array<face, 12> faces{
            face::helvetica, face::helvetica_bold, face::helvetica_oblique, face::helvetica_bold_oblique,
            face::times_roman, face::times_bold, face::times_italic, face::times_bold_italic,
            face::courier, face::courier_bold, face::courier_oblique, face::courier_bold_oblique};

        explicit standard_font(face which);

        /// A face's PDF name: "Helvetica", "Helvetica-BoldOblique", "Times-Roman", ...
        [[nodiscard]] static std::string_view name(face which);

        /// The face with this PDF name (exactly: "Helvetica-Bold"), as a reader of PDFs finds it in a
        /// font's /BaseFont; nothing for another name.
        [[nodiscard]] static std::optional<face> named(std::string_view pdf_name);

        /// The built-in font for a font's family, weight (600 and up bold) and slant: Helvetica for
        /// "Helvetica" or "Arial", Times for "Times", "Times-Roman" or "Times New Roman", Courier for
        /// "Courier" or "Courier New" (any case); nothing for another family.
        [[nodiscard]] static std::optional<standard_font> of(const font &spec);

        [[nodiscard]] face which() const { return face_; }

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
        face face_;
        std::string name_;
    };
}

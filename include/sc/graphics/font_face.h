#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace sc {
    class typeface;

    /// One style of a typeface in one font file: DejaVu Sans Bold, in DejaVuSans-Bold.ttf. A
    /// typeface's faces() lists them, its face() picks the closest; a font (sc::font) is a face at a
    /// size, and font.face() finds it.
    ///
    ///     const auto face = sc::font{"DejaVu Sans", 11, 700}.face();
    ///     face->typeface().name();      // "DejaVu Sans"
    ///     face->weight();               // 700, its own (the closest the typeface has)
    ///     face->width("Total due", 11); // at 11 points (or pixels): as sc-pdf and sc-mupdf draw it
    ///
    /// Measurements take the size (the em, in the canvas's units) and are in those units. Widths are
    /// the glyphs' advances, without kerning, as the canvases draw text. A font_face is a cheap
    /// handle; the file is only read when it's measured, by FreeType state kept per thread, so faces
    /// can be used from any thread.
    class font_face {
    public:
        /// The typeface (family) it's a style of.
        [[nodiscard]] sc::typeface typeface() const;

        /// Its family's name, its style's name ("Bold Oblique"), CSS weight (100 thin to 900 black,
        /// 400 regular, 700 bold) and slant.
        [[nodiscard]] const std::string &family() const;

        [[nodiscard]] const std::string &style() const;

        [[nodiscard]] int weight() const;

        [[nodiscard]] bool italic() const;

        /// The file, and the face's index in it (a .ttc collection holds several).
        [[nodiscard]] const std::string &file() const;

        [[nodiscard]] int index() const;

        /// How far it rises above the baseline and falls below it (both positive), the distance
        /// between baselines it suggests, and the heights of a lowercase x and a capital H, at size.
        /// 0 when the file can't be read as a font.
        [[nodiscard]] double ascender(double size) const;

        [[nodiscard]] double descender(double size) const;

        [[nodiscard]] double line_height(double size) const;

        [[nodiscard]] double x_height(double size) const;

        [[nodiscard]] double cap_height(double size) const;

        /// How far text (UTF-8) advances at size: its glyphs' advances, no kerning.
        [[nodiscard]] double width(std::string_view text, double size) const;

        /// Whether it has a glyph for every character of text (else those show as boxes, or not at all).
        [[nodiscard]] bool covers(std::string_view text) const;

        /// The same file and face.
        bool operator==(const font_face &other) const { return file() == other.file() && index() == other.index(); }

        struct impl;

        explicit font_face(std::shared_ptr<const impl> impl) : impl_(std::move(impl)) {
        }

    private:
        std::shared_ptr<const impl> impl_;
    };
}

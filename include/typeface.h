#pragma once

#include "font_face.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sc {
    /// A typeface, or font family: DejaVu Sans, Roboto, with every style installed of it (regular,
    /// bold, italic, thin, condensed, ...), usually across several files. Its faces are those styles;
    /// a face at a size is a font.
    ///
    ///     if (const auto roboto = sc::typeface::find("Roboto")) {
    ///         for (const auto &face: roboto->faces()) std::cout << face.style() << ' ' << face.weight() << '\n';
    ///         const auto bold = roboto->face(700);
    ///     }
    ///
    /// Found through fontconfig, and only its scalable fonts (TrueType, OpenType, Type 1). Generic
    /// names ("sans-serif", "serif", "monospace") aren't typefaces: sc::font resolves them to one.
    class typeface {
    public:
        /// The installed typeface of that name (ignoring case); empty when there's none.
        [[nodiscard]] static std::optional<typeface> find(std::string_view name);

        /// Every installed typeface, by name.
        [[nodiscard]] static std::vector<typeface> installed();

        [[nodiscard]] const std::string &name() const { return name_; }

        /// Its styles, upright ones first, each from light to heavy.
        [[nodiscard]] std::vector<font_face> faces() const;

        /// The style closest to a CSS weight (400 regular, 700 bold), slant and, when given, style
        /// name ("Condensed Bold"); empty only when the typeface has no faces left (uninstalled).
        [[nodiscard]] std::optional<font_face> face(int weight = 400, bool italic = false, std::string_view style = {}) const;

        bool operator==(const typeface &) const = default;

    private:
        friend class font_face;

        explicit typeface(std::string name) : name_(std::move(name)) {
        }

        std::string name_;
    };
}

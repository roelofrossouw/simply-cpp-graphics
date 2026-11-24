#pragma once

#include <sc/graphics/font_face.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sc::graphics {
    /// A typeface, or font family: DejaVu Sans, Roboto, with every style installed of it (regular,
    /// bold, italic, thin, condensed, ...), usually across several files. Its faces are those styles;
    /// a face at a size is a font.
    ///
    ///     if (const auto roboto = sc::graphics::typeface::find("Roboto")) {
    ///         for (const auto &face: roboto->faces()) std::cout << face.style() << ' ' << face.weight() << '\n';
    ///         const auto bold = roboto->face(700);
    ///     }
    ///
    /// Found through fontconfig, and only its scalable fonts (TrueType, OpenType, Type 1). Generic
    /// names ("sans-serif", "serif", "monospace") aren't typefaces: sc::graphics::font resolves them to one.
    class typeface {
    public:
        /// The installed typeface of that name (ignoring case); empty when there's none.
        [[nodiscard]] static std::optional<typeface> find(std::string_view name);

        /// Every installed typeface, by name.
        [[nodiscard]] static std::vector<typeface> installed();

        /// Adds the fonts in a folder (and its subfolders) to those found, for this process only,
        /// as if they were installed: fonts kept with an application (Arial's TTFs on a share, say).
        /// Call it before any font is looked up, as lookups are kept. False when the folder isn't
        /// one or fontconfig can't read it.
        static bool add_folder(const std::filesystem::path &folder);

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

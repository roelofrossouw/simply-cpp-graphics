// sc::typeface, sc::font_face and sc::font's face(): fontconfig finds the fonts, FreeType reads them.
#include "font.h"
#include "font_face.h"
#include "typeface.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>

namespace sc {
    struct font_face::impl {
        std::string file;
        int index = 0;
        std::string family;
        std::string style;
        int weight = 400;
        bool italic = false;
    };

    namespace {
        // fontconfig's default configuration, loaded once and kept for the whole process: FcFini()
        // while another thread still looks fonts up would pull it away underneath.
        bool fontconfig_ready() {
            static std::once_flag initialised;
            static bool available = false;
            std::call_once(initialised, [] { available = FcInit() == FcTrue; });
            return available;
        }

        const FcChar8 *fc_text(const std::string &text) { return reinterpret_cast<const FcChar8 *>(text.c_str()); }

        std::string lower(std::string text) {
            std::ranges::transform(text, text.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string string_of(FcPattern *pattern, const char *object, const int n = 0) {
            FcChar8 *text = nullptr;
            return FcPatternGetString(pattern, object, n, &text) == FcResultMatch ? reinterpret_cast<const char *>(text) : "";
        }

        // fontconfig's generic families: asking for one is always answered exactly.
        bool generic(const std::string &family) {
            static const std::set<std::string> names{"sans-serif", "sans", "serif", "monospace", "mono", "cursive", "fantasy", "system-ui", "emoji"};
            return names.contains(lower(family));
        }

        // A face from a fontconfig pattern holding its file, index, family, style, weight and slant.
        std::optional<font_face> face_of(FcPattern *pattern) {
            auto face = std::make_shared<font_face::impl>();
            face->file = string_of(pattern, FC_FILE);
            if (face->file.empty()) return std::nullopt;
            FcPatternGetInteger(pattern, FC_INDEX, 0, &face->index);
            face->family = string_of(pattern, FC_FAMILY);
            face->style = string_of(pattern, FC_STYLE);
            double weight = FC_WEIGHT_REGULAR;
            if (FcPatternGetDouble(pattern, FC_WEIGHT, 0, &weight) != FcResultMatch) {
                int whole = FC_WEIGHT_REGULAR;
                if (FcPatternGetInteger(pattern, FC_WEIGHT, 0, &whole) == FcResultMatch) weight = whole;
            }
            face->weight = FcWeightToOpenType(static_cast<int>(weight));
            int slant = FC_SLANT_ROMAN;
            FcPatternGetInteger(pattern, FC_SLANT, 0, &slant);
            face->italic = slant != FC_SLANT_ROMAN;
            return font_face{std::move(face)};
        }

        // The face fontconfig picks for a family, CSS weight, slant and style, and whether it has
        // that family (or a generic one was asked for), not a substitute.
        std::pair<std::optional<font_face>, bool> match(const std::string &family, const int weight, const bool italic, const std::string &style) {
            if (!fontconfig_ready()) return {std::nullopt, false};
            FcPattern *pattern = FcPatternCreate();
            FcPatternAddString(pattern, FC_FAMILY, fc_text(family));
            FcPatternAddInteger(pattern, FC_WEIGHT, FcWeightFromOpenType(std::clamp(weight, 1, 1000)));
            FcPatternAddInteger(pattern, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
            if (!style.empty()) FcPatternAddString(pattern, FC_STYLE, fc_text(style));
            FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
            FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
            FcDefaultSubstitute(pattern);
            std::optional<font_face> found;
            bool exact = generic(family);
            FcResult result;
            if (FcPattern *matched = FcFontMatch(nullptr, pattern, &result)) {
                found = face_of(matched);
                // A font can carry several family names (localised ones, say): any of them counts.
                for (int i = 0; !exact; ++i) {
                    const auto name = string_of(matched, FC_FAMILY, i);
                    if (name.empty()) break;
                    exact = lower(name) == lower(family);
                }
                FcPatternDestroy(matched);
            }
            FcPatternDestroy(pattern);
            return {found, found && exact};
        }

        // fontconfig's scalable fonts, of one family (or all, for an empty name).
        template<typename Each>
        void list(const std::string &family, Each each) {
            if (!fontconfig_ready()) return;
            FcPattern *pattern = FcPatternCreate();
            if (!family.empty()) FcPatternAddString(pattern, FC_FAMILY, fc_text(family));
            FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
            FcObjectSet *objects = FcObjectSetBuild(FC_FILE, FC_INDEX, FC_FAMILY, FC_STYLE, FC_WEIGHT, FC_SLANT, nullptr);
            if (FcFontSet *fonts = FcFontList(nullptr, pattern, objects)) {
                for (int i = 0; i < fonts->nfont; ++i) each(fonts->fonts[i]);
                FcFontSetDestroy(fonts);
            }
            FcObjectSetDestroy(objects);
            FcPatternDestroy(pattern);
        }

        // The next code point of UTF-8 text from i, moving i past it; malformed bytes count as U+FFFD.
        char32_t next_codepoint(const std::string_view text, std::size_t &i) {
            const auto byte = [&](const std::size_t at) { return static_cast<unsigned char>(text[at]); };
            const unsigned char first = byte(i);
            const int length = first < 0x80 ? 1 : (first >> 5) == 6 ? 2 : (first >> 4) == 14 ? 3 : (first >> 3) == 30 ? 4 : 0;
            if (length == 0 || i + length > text.size()) {
                ++i;
                return 0xFFFD;
            }
            char32_t code = length == 1 ? first : first & (0x7F >> length);
            for (int k = 1; k < length; ++k) {
                if ((byte(i + k) & 0xC0) != 0x80) {
                    ++i;
                    return 0xFFFD;
                }
                code = (code << 6) | (byte(i + k) & 0x3F);
            }
            i += length;
            return code;
        }

        // A face opened with FreeType, and its metrics in font units.
        struct opened {
            FT_Face face = nullptr;
            double units_per_em = 1000;
            double ascender = 0, descender = 0, line_height = 0, x_height = 0, cap_height = 0;
        };

        // How far letter rises above the baseline, in font units: its outline's top. 0 without it.
        long glyph_top(FT_Face face, const char letter) {
            if (FT_Load_Char(face, letter, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING) != 0) return 0;
            return face->glyph->metrics.horiBearingY;
        }

        // FreeType's library and the faces this thread opened, by file and index. FreeType objects
        // aren't thread-safe, so each thread has its own; internal linkage keeps one per copy of
        // sc-graphics, so nothing is destroyed twice.
        struct freetype {
            FT_Library library = nullptr;
            std::map<std::pair<std::string, int>, opened> faces;

            freetype() {
                if (FT_Init_FreeType(&library) != 0) library = nullptr;
            }

            ~freetype() {
                for (const auto &[key, face]: faces) {
                    if (face.face) FT_Done_Face(face.face);
                }
                if (library) FT_Done_FreeType(library);
            }

            freetype(const freetype &) = delete;

            freetype &operator=(const freetype &) = delete;

            // The face, opened the first time (a failure is remembered too: face is nullptr).
            const opened &open(const std::string &file, const int index) {
                const auto key = std::pair{file, index};
                if (const auto found = faces.find(key); found != faces.end()) return found->second;
                opened result;
                if (library && FT_New_Face(library, file.c_str(), index, &result.face) != 0) result.face = nullptr;
                if (result.face && (!FT_IS_SCALABLE(result.face) || result.face->units_per_EM == 0)) {
                    FT_Done_Face(result.face);
                    result.face = nullptr;
                }
                if (FT_Face face = result.face) {
                    result.units_per_em = face->units_per_EM;
                    result.ascender = face->ascender;
                    result.descender = -face->descender;
                    result.line_height = face->height;
                    // x and cap heights as the font declares them (OS/2 version 2 and later), else
                    // the letters' own height, as sc-pdf measures them.
                    const auto *os2 = static_cast<const TT_OS2 *>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
                    result.x_height = os2 && os2->version >= 2 && os2->sxHeight > 0 ? os2->sxHeight : glyph_top(face, 'x');
                    result.cap_height = os2 && os2->version >= 2 && os2->sCapHeight > 0 ? os2->sCapHeight : glyph_top(face, 'H');
                }
                return faces.emplace(key, result).first->second;
            }
        };

        const opened &open(const font_face &face) {
            thread_local freetype instance;
            return instance.open(face.file(), face.index());
        }

        double scaled(const opened &face, const double units, const double size) { return units * size / face.units_per_em; }
    }

    // font_face

    sc::typeface font_face::typeface() const {
        if (auto found = typeface::find(impl_->family)) return *found;
        return sc::typeface{impl_->family}; // uninstalled since: the name it had
    }

    const std::string &font_face::family() const { return impl_->family; }

    const std::string &font_face::style() const { return impl_->style; }

    int font_face::weight() const { return impl_->weight; }

    bool font_face::italic() const { return impl_->italic; }

    const std::string &font_face::file() const { return impl_->file; }

    int font_face::index() const { return impl_->index; }

    double font_face::ascender(const double size) const { const auto &f = open(*this); return scaled(f, f.ascender, size); }

    double font_face::descender(const double size) const { const auto &f = open(*this); return scaled(f, f.descender, size); }

    double font_face::line_height(const double size) const { const auto &f = open(*this); return scaled(f, f.line_height, size); }

    double font_face::x_height(const double size) const { const auto &f = open(*this); return scaled(f, f.x_height, size); }

    double font_face::cap_height(const double size) const { const auto &f = open(*this); return scaled(f, f.cap_height, size); }

    double font_face::width(const std::string_view text, const double size) const {
        const auto &f = open(*this);
        if (!f.face) return 0;
        double units = 0;
        for (std::size_t i = 0; i < text.size();) {
            const char32_t code = next_codepoint(text, i);
            FT_Fixed advance = 0;
            if (FT_Get_Advance(f.face, FT_Get_Char_Index(f.face, code), FT_LOAD_NO_SCALE, &advance) == 0) units += static_cast<double>(advance);
        }
        return scaled(f, units, size);
    }

    bool font_face::covers(const std::string_view text) const {
        const auto &f = open(*this);
        if (!f.face) return false;
        for (std::size_t i = 0; i < text.size();) {
            const char32_t code = next_codepoint(text, i);
            if (code < 0x20) continue; // control characters (a newline) aren't drawn
            if (FT_Get_Char_Index(f.face, code) == 0) return false;
        }
        return true;
    }

    // typeface

    std::optional<typeface> typeface::find(const std::string_view name) {
        // fontconfig lists a family by any of its names, ignoring case; the typeface keeps the one
        // that matched, as the font spells it.
        std::string found;
        list(std::string{name}, [&](FcPattern *font) {
            for (int i = 0; found.empty(); ++i) {
                const auto family = string_of(font, FC_FAMILY, i);
                if (family.empty()) break;
                if (lower(family) == lower(std::string{name})) found = family;
            }
        });
        if (found.empty()) return std::nullopt;
        return typeface{found};
    }

    std::vector<typeface> typeface::installed() {
        std::map<std::string, std::string> names; // by lowercase name, so each is listed once
        list({}, [&](FcPattern *font) {
            if (const auto family = string_of(font, FC_FAMILY); !family.empty()) names.emplace(lower(family), family);
        });
        std::vector<typeface> result;
        for (const auto &[key, name]: names) result.push_back(typeface{name});
        return result;
    }

    std::vector<font_face> typeface::faces() const {
        std::vector<font_face> result;
        list(name_, [&](FcPattern *font) {
            if (auto face = face_of(font)) result.push_back(std::move(*face));
        });
        std::ranges::sort(result, {}, [](const font_face &face) { return std::tuple{face.italic(), face.weight(), face.style()}; });
        return result;
    }

    std::optional<font_face> typeface::face(const int weight, const bool italic, const std::string_view style) const {
        auto [found, exact] = match(name_, weight, italic, std::string{style});
        if (found && exact) return found;
        // fontconfig went to another family: the closest of this one's own, by slant, then weight.
        const auto own = faces();
        if (own.empty()) return std::nullopt;
        return *std::ranges::min_element(own, {}, [&](const font_face &face) {
            return std::pair{face.italic() != italic, std::abs(face.weight() - weight)};
        });
    }

    // font

    namespace {
        struct lookup {
            std::optional<font_face> face;
            bool exact = false;
        };

        // font.face(), found once per family, weight, slant and style: fontconfig's matching takes a
        // while, and text is measured often.
        const lookup &looked_up(const font &spec) {
            static std::mutex guard;
            static std::map<std::tuple<std::string, int, bool, std::string>, lookup> found;
            const auto key = std::tuple{spec.family, spec.weight, spec.italic, spec.style};
            const std::lock_guard lock{guard};
            if (const auto known = found.find(key); known != found.end()) return known->second;
            auto [face, exact] = match(spec.family, spec.weight, spec.italic, spec.style);
            return found.emplace(key, lookup{std::move(face), exact}).first->second;
        }
    }

    std::optional<font_face> font::face() const { return looked_up(*this).face; }

    bool font::available() const { return looked_up(*this).exact; }

    double font::width(const std::string_view text) const {
        const auto &[face, exact] = looked_up(*this);
        return face ? face->width(text, size) : 0;
    }

    double font::ascender() const { const auto &found = looked_up(*this).face; return found ? found->ascender(size) : 0; }

    double font::descender() const { const auto &found = looked_up(*this).face; return found ? found->descender(size) : 0; }

    double font::line_height() const { const auto &found = looked_up(*this).face; return found ? found->line_height(size) : 0; }

    double font::x_height() const { const auto &found = looked_up(*this).face; return found ? found->x_height(size) : 0; }

    double font::cap_height() const { const auto &found = looked_up(*this).face; return found ? found->cap_height(size) : 0; }
}

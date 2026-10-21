// sc::typeface, sc::font_face and sc::font's face(): fontconfig finds the fonts, FreeType reads them.
#include "font.h"
#include "font_face.h"
#include "glyphs.h"
#include "typeface.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_TRUETYPE_TABLES_H
#include <hb.h>

#include <core.h>

#include <algorithm>
#include <array>
#include <cstdint>
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

        // Values made once per key and kept, shared by all threads. Making one runs outside the lock,
        // so two threads may both make it; the first one stored is kept.
        template<typename Key, typename Value>
        class memo {
        public:
            template<typename Make>
            const Value &get(const Key &key, Make make) {
                {
                    const std::lock_guard lock{guard_};
                    if (const auto known = values_.find(key); known != values_.end()) return known->second;
                }
                Value made = make();
                const std::lock_guard lock{guard_};
                return values_.emplace(key, std::move(made)).first->second;
            }

        private:
            std::mutex guard_;
            std::map<Key, Value> values_;
        };

        const FcChar8 *fc_text(const std::string &text) { return reinterpret_cast<const FcChar8 *>(text.c_str()); }

        std::string string_of(FcPattern *pattern, const char *object, const int n = 0) {
            FcChar8 *text = nullptr;
            return FcPatternGetString(pattern, object, n, &text) == FcResultMatch ? reinterpret_cast<const char *>(text) : "";
        }

        // fontconfig's generic families: asking for one is always answered exactly.
        bool generic(const std::string &family) {
            static const std::set<std::string> names{"sans-serif", "sans", "serif", "monospace", "mono", "cursive", "fantasy", "system-ui", "emoji"};
            return names.contains(lowercase(family));
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
                    exact = lowercase(name) == lowercase(family);
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
                // Outline fonts, and colour ones whose glyphs are bitmaps (Noto Color Emoji's CBDT),
                // whose em FreeType leaves at 0: their head table has it.
                double em = result.face ? result.face->units_per_EM : 0;
                if (result.face && em == 0 && FT_HAS_COLOR(result.face)) {
                    if (const auto *head = static_cast<const TT_Header *>(FT_Get_Sfnt_Table(result.face, FT_SFNT_HEAD))) em = head->Units_Per_EM;
                }
                if (result.face && ((!FT_IS_SCALABLE(result.face) && !FT_HAS_COLOR(result.face)) || em == 0)) {
                    FT_Done_Face(result.face);
                    result.face = nullptr;
                }
                if (FT_Face face = result.face) {
                    result.units_per_em = em;
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
                if (lowercase(family) == lowercase(name)) found = family;
            }
        });
        if (found.empty()) return std::nullopt;
        return typeface{found};
    }

    std::vector<typeface> typeface::installed() {
        std::map<std::string, std::string> names; // by lowercase name, so each is listed once
        list({}, [&](FcPattern *font) {
            if (const auto family = string_of(font, FC_FAMILY); !family.empty()) names.emplace(lowercase(family), family);
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
            static memo<std::tuple<std::string, int, bool, std::string>, lookup> found;
            return found.get({spec.family, spec.weight, spec.italic, spec.style}, [&] {
                auto [face, exact] = match(spec.family, spec.weight, spec.italic, spec.style);
                return lookup{std::move(face), exact};
            });
        }
    }

    std::optional<font_face> font::face() const { return looked_up(*this).face; }

    bool font::available() const { return looked_up(*this).exact; }

    double font::width(const std::string_view text) const {
        double total = 0;
        for (const auto &run: detail::runs(*this, text)) {
            if (!run.face) continue;
            total += run.colour ? detail::lay_out_colour(*run.face, run.text, size).advance : run.face->width(run.text, size);
        }
        return total;
    }

    double font::ascender() const { const auto &found = looked_up(*this).face; return found ? found->ascender(size) : 0; }

    double font::descender() const { const auto &found = looked_up(*this).face; return found ? found->descender(size) : 0; }

    double font::line_height() const { const auto &found = looked_up(*this).face; return found ? found->line_height(size) : 0; }

    double font::x_height() const { const auto &found = looked_up(*this).face; return found ? found->x_height(size) : 0; }

    double font::cap_height() const { const auto &found = looked_up(*this).face; return found ? found->cap_height(size) : 0; }

    // ---------------------------------------------------------------- runs and colour glyphs

    namespace {
        bool between(const char32_t code, const char32_t first, const char32_t last) { return code >= first && code <= last; }

        // Shown as emoji by default (Unicode's Emoji_Presentation, roughly): most of the planes'
        // pictographs, and the few in the BMP; the rest of the BMP's symbols (✓, ★, ☎) are text
        // unless a VS16 follows.
        bool emoji_by_default(const char32_t c) {
            if (c >= 0x1F000) return between(c, 0x1F000, 0x1FAFF);
            static const std::pair<char32_t, char32_t> ranges[] = {
                {0x231A, 0x231B}, {0x23E9, 0x23EC}, {0x23F0, 0x23F0}, {0x23F3, 0x23F3}, {0x25FD, 0x25FE}, {0x2614, 0x2615},
                {0x2648, 0x2653}, {0x267F, 0x267F}, {0x2693, 0x2693}, {0x26A1, 0x26A1}, {0x26AA, 0x26AB}, {0x26BD, 0x26BE},
                {0x26C4, 0x26C5}, {0x26CE, 0x26CE}, {0x26D4, 0x26D4}, {0x26EA, 0x26EA}, {0x26F2, 0x26F3}, {0x26F5, 0x26F5},
                {0x26FA, 0x26FA}, {0x26FD, 0x26FD}, {0x2705, 0x2705}, {0x270A, 0x270B}, {0x2728, 0x2728}, {0x274C, 0x274C},
                {0x274E, 0x274E}, {0x2753, 0x2755}, {0x2757, 0x2757}, {0x2795, 0x2797}, {0x27B0, 0x27B0}, {0x27BF, 0x27BF},
                {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55}};
            return std::ranges::any_of(ranges, [&](const auto &range) { return between(c, range.first, range.second); });
        }

        bool regional_indicator(const char32_t c) { return between(c, 0x1F1E6, 0x1F1FF); }

        // Characters that belong to the cluster before them: combining marks, variation selectors,
        // joiners, skin tones, keycaps, tags.
        bool extends(const char32_t c) {
            return between(c, 0x0300, 0x036F) || between(c, 0x1AB0, 0x1AFF) || between(c, 0x20D0, 0x20FF) || between(c, 0xFE00, 0xFE0F) ||
                   c == 0x200D || between(c, 0x1F3FB, 0x1F3FF) || between(c, 0xE0020, 0xE007F);
        }

        struct cluster {
            std::string text;
            std::vector<char32_t> codes;
            bool emoji = false;
        };

        // UTF-8 text as clusters: a character with what extends it; after a ZWJ the next character
        // too; regional indicators in pairs (a flag).
        std::vector<cluster> clusters_of(const std::string_view text) {
            std::vector<cluster> out;
            bool joined = false;
            for (std::size_t i = 0; i < text.size();) {
                const std::size_t start = i;
                const char32_t code = next_codepoint(text, i);
                const bool pairs = !out.empty() && regional_indicator(code) && out.back().codes.size() == 1 && regional_indicator(out.back().codes[0]);
                if (out.empty() || !(joined || extends(code) || pairs)) out.emplace_back();
                auto &current = out.back();
                current.text.append(text.substr(start, i - start));
                current.codes.push_back(code);
                joined = code == 0x200D;
            }
            for (auto &each: out) {
                const bool text_style = std::ranges::find(each.codes, char32_t{0xFE0E}) != each.codes.end();
                const bool emoji_style = std::ranges::find(each.codes, char32_t{0xFE0F}) != each.codes.end() ||
                                         std::ranges::find(each.codes, char32_t{0x20E3}) != each.codes.end();
                each.emoji = !text_style && (emoji_style || emoji_by_default(each.codes.front()));
            }
            return out;
        }

        // What a face must have for a cluster: its characters, not counting the invisible ones that
        // only steer how they're shown.
        bool face_covers(const font_face &face, const cluster &each) {
            const auto &f = open(face);
            if (!f.face) return false;
            return std::ranges::all_of(each.codes, [&](const char32_t c) {
                return c < 0x20 || between(c, 0xFE00, 0xFE0F) || c == 0x200D || FT_Get_Char_Index(f.face, c) != 0;
            });
        }

        bool has_colour(const font_face &face) {
            const auto &f = open(face);
            return f.face && FT_HAS_COLOR(f.face);
        }

        // The installed face that has code, as fontconfig ranks them for the weight and slant; for
        // emoji, a colour one first.
        std::optional<font_face> find_fallback(const char32_t code, const int weight, const bool italic, const bool emoji) {
            std::optional<font_face> result;
            if (fontconfig_ready()) {
                FcPattern *pattern = FcPatternCreate();
                FcCharSet *wanted = FcCharSetCreate();
                FcCharSetAddChar(wanted, code);
                FcPatternAddCharSet(pattern, FC_CHARSET, wanted);
                if (emoji) FcPatternAddString(pattern, FC_FAMILY, fc_text("emoji"));
                FcPatternAddInteger(pattern, FC_WEIGHT, FcWeightFromOpenType(std::clamp(weight, 1, 1000)));
                FcPatternAddInteger(pattern, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
                FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
                FcDefaultSubstitute(pattern);
                FcResult status;
                if (FcFontSet *sorted = FcFontSort(nullptr, pattern, FcFalse, nullptr, &status)) {
                    std::optional<font_face> plain;
                    for (int i = 0; i < sorted->nfont && !result; ++i) {
                        FcCharSet *has = nullptr;
                        if (FcPatternGetCharSet(sorted->fonts[i], FC_CHARSET, 0, &has) != FcResultMatch || !FcCharSetHasChar(has, code)) continue;
                        auto face = face_of(sorted->fonts[i]);
                        if (!face || !open(*face).face) continue;
                        if (!emoji || has_colour(*face)) result = face;
                        else if (!plain) plain = face;
                    }
                    if (!result) result = plain;
                    FcFontSetDestroy(sorted);
                }
                FcCharSetDestroy(wanted);
                FcPatternDestroy(pattern);
            }
            return result;
        }

        std::optional<font_face> fallback_for(const char32_t code, const int weight, const bool italic, const bool emoji) {
            static memo<std::tuple<char32_t, int, bool, bool>, std::optional<font_face> > found;
            return found.get({code, weight, italic, emoji}, [&] { return find_fallback(code, weight, italic, emoji); });
        }
    }

    std::vector<detail::text_run> detail::runs(const font &spec, const std::string_view text) {
        std::vector<text_run> out;
        const auto own = spec.face();
        // Plain ASCII in a font that has it all: one run, as before (the usual case, kept quick).
        if (own && std::ranges::all_of(text, [](const char c) { return static_cast<unsigned char>(c) < 0x80; }) && own->covers(text))
            return {{std::string{text}, own, false, false}};
        for (const auto &each: clusters_of(text)) {
            text_run piece{each.text, own, false, false};
            const bool emoji_wanted = each.emoji;
            if (!own || emoji_wanted || !face_covers(*own, each)) {
                const auto other = fallback_for(each.codes.front(), spec.weight, spec.italic, emoji_wanted);
                if (other && face_covers(*other, each)) {
                    piece.face = other;
                    piece.fallback = !(own && *other == *own);
                } else if (!own || !face_covers(*own, each)) {
                    piece.face = other ? other : own; // nothing has it: drawn as the font can
                    piece.fallback = other.has_value() && !(own && *other == *own);
                }
            }
            piece.colour = piece.face && emoji_wanted && has_colour(*piece.face);
            const bool same = !out.empty() && out.back().colour == piece.colour && out.back().fallback == piece.fallback &&
                              out.back().face.has_value() == piece.face.has_value() && (!piece.face || *out.back().face == *piece.face);
            if (same) out.back().text += piece.text;
            else out.push_back(std::move(piece));
        }
        if (out.empty()) out.push_back({std::string{text}, own, false, false});
        return out;
    }

    font detail::font_of(const font &spec, const text_run &run) {
        if (!run.fallback || !run.face) return spec;
        return font{run.face->family(), spec.size, run.face->weight(), run.face->italic(), run.face->style()};
    }

    namespace {
        // A PNG of straight (not premultiplied) RGBA rows, its pixels stored uncompressed in the
        // zlib stream (a canvas compresses it again, or keeps it as it is).
        std::string png_of(const std::vector<unsigned char> &rgba, const unsigned width, const unsigned height) {
            const auto crc = [](const std::string &data) {
                static const auto table = [] {
                    std::array<std::uint32_t, 256> t{};
                    for (std::uint32_t n = 0; n < 256; ++n) {
                        std::uint32_t c = n;
                        for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                        t[n] = c;
                    }
                    return t;
                }();
                std::uint32_t c = 0xFFFFFFFFu;
                for (const unsigned char byte: data) c = table[(c ^ byte) & 0xFF] ^ (c >> 8);
                return c ^ 0xFFFFFFFFu;
            };
            const auto big = [](std::string &out, const std::uint32_t value) {
                for (int shift = 24; shift >= 0; shift -= 8) out += static_cast<char>((value >> shift) & 0xFF);
            };
            const auto chunk = [&](std::string &out, const char *type, const std::string &data) {
                big(out, static_cast<std::uint32_t>(data.size()));
                const std::string typed = type + data;
                out += typed;
                big(out, crc(typed));
            };
            std::string raw;
            raw.reserve((width * 4 + 1) * height);
            for (unsigned y = 0; y < height; ++y) {
                raw += '\0'; // no filter
                raw.append(reinterpret_cast<const char *>(rgba.data() + std::size_t{y} * width * 4), std::size_t{width} * 4);
            }
            std::string zlib{"\x78\x01", 2};
            for (std::size_t at = 0; at < raw.size() || at == 0; at += 65535) {
                const std::size_t length = std::min<std::size_t>(65535, raw.size() - at);
                zlib += static_cast<char>(at + length >= raw.size() ? 1 : 0);
                zlib += static_cast<char>(length & 0xFF);
                zlib += static_cast<char>(length >> 8);
                zlib += static_cast<char>(~length & 0xFF);
                zlib += static_cast<char>((~length >> 8) & 0xFF);
                zlib.append(raw, at, length);
                if (raw.empty()) break;
            }
            std::uint32_t a = 1, b = 0;
            for (const unsigned char byte: raw) {
                a = (a + byte) % 65521;
                b = (b + a) % 65521;
            }
            big(zlib, b << 16 | a);
            std::string header;
            big(header, width);
            big(header, height);
            header += std::string{"\x08\x06\x00\x00\x00", 5}; // 8-bit RGBA
            std::string out{"\x89PNG\r\n\x1a\n", 8};
            chunk(out, "IHDR", header);
            chunk(out, "IDAT", zlib);
            chunk(out, "IEND", {});
            return out;
        }

        // A colour glyph rendered once: its bitmap, and its box in ems from the pen on the baseline.
        struct rendered {
            bitmap picture;
            double left, top, width, height; // ems; top above the baseline
        };

        // Renders glyph in colour, from the strike nearest 128 pixels (a bitmap font's) or at 128
        // pixels (a COLR font's). Nothing for a blank glyph or one FreeType can't colour.
        std::optional<rendered> draw_glyph(const font_face &face, const unsigned glyph) {
            std::optional<rendered> result;
            if (FT_Face f = open(face).face) {
                double ppem = 128;
                bool sized = false;
                if (FT_HAS_FIXED_SIZES(f) && f->num_fixed_sizes > 0) {
                    int best = 0;
                    for (int i = 1; i < f->num_fixed_sizes; ++i)
                        if (std::abs(f->available_sizes[i].y_ppem / 64.0 - 128) < std::abs(f->available_sizes[best].y_ppem / 64.0 - 128)) best = i;
                    sized = FT_Select_Size(f, best) == 0;
                    if (sized) ppem = f->available_sizes[best].y_ppem / 64.0;
                }
                if (!sized) sized = FT_Set_Pixel_Sizes(f, 0, 128) == 0;
                if (sized && FT_Load_Glyph(f, glyph, FT_LOAD_COLOR | FT_LOAD_RENDER) == 0) {
                    const FT_Bitmap &image = f->glyph->bitmap;
                    if (image.pixel_mode == FT_PIXEL_MODE_BGRA && image.width > 0 && image.rows > 0) {
                        std::vector<unsigned char> rgba(std::size_t{image.width} * image.rows * 4);
                        for (unsigned y = 0; y < image.rows; ++y) {
                            const unsigned char *row = image.buffer + static_cast<std::ptrdiff_t>(y) * image.pitch;
                            for (unsigned x = 0; x < image.width; ++x) {
                                const unsigned char *bgra = row + x * 4;
                                unsigned char *out = rgba.data() + (std::size_t{y} * image.width + x) * 4;
                                const unsigned alpha = bgra[3];
                                // FreeType's colours are premultiplied; PNG's aren't.
                                const auto straight = [&](const unsigned c) { return static_cast<unsigned char>(alpha ? std::min(255u, c * 255 / alpha) : 0); };
                                out[0] = straight(bgra[2]);
                                out[1] = straight(bgra[1]);
                                out[2] = straight(bgra[0]);
                                out[3] = static_cast<unsigned char>(alpha);
                            }
                        }
                        result = rendered{bitmap{png_of(rgba, image.width, image.rows)}, f->glyph->bitmap_left / ppem,
                                          f->glyph->bitmap_top / ppem, image.width / ppem, image.rows / ppem};
                    }
                }
            }
            return result;
        }

        const std::optional<rendered> &render(const font_face &face, const unsigned glyph) {
            static memo<std::tuple<std::string, int, unsigned>, std::optional<rendered> > done;
            return done.get({face.file(), face.index(), glyph}, [&] { return draw_glyph(face, glyph); });
        }

        // HarfBuzz's font for a face, made once and kept: shaping only reads it.
        hb_font_t *shaper(const font_face &face) {
            static std::mutex guard;
            static std::map<std::pair<std::string, int>, hb_font_t *> fonts;
            const std::lock_guard lock{guard};
            const auto key = std::pair{face.file(), face.index()};
            if (const auto found = fonts.find(key); found != fonts.end()) return found->second;
            hb_blob_t *blob = hb_blob_create_from_file(face.file().c_str());
            hb_face_t *made = hb_face_create(blob, static_cast<unsigned>(face.index()));
            hb_font_t *font = hb_font_create(made); // scaled in font units
            hb_face_destroy(made);
            hb_blob_destroy(blob);
            return fonts.emplace(key, font).first->second;
        }
    }

    detail::colour_layout detail::lay_out_colour(const font_face &face, const std::string_view text, const double size) {
        colour_layout out;
        hb_font_t *font = shaper(face);
        const double units = std::max(1u, hb_face_get_upem(hb_font_get_face(font)));
        hb_buffer_t *buffer = hb_buffer_create();
        hb_buffer_add_utf8(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(font, buffer, nullptr, 0);
        unsigned count = 0;
        const hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &count);
        const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &count);
        double pen = 0;
        for (unsigned i = 0; i < count; ++i) {
            const double scale = size / units;
            if (const auto glyph = render(face, infos[i].codepoint)) {
                out.glyphs.push_back({glyph->picture, pen + positions[i].x_offset * scale + glyph->left * size,
                                      -(positions[i].y_offset * scale + glyph->top * size), glyph->width * size, glyph->height * size});
            }
            pen += positions[i].x_advance * scale;
        }
        hb_buffer_destroy(buffer);
        out.advance = pen;
        return out;
    }
}


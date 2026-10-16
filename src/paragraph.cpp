#include "paragraph.h"

#include "canvas.h"
#include "font.h"

#include <algorithm>

namespace sc {
    namespace {
        // A line: where it is in the content, and whether its paragraph (up to a newline) ends with it.
        struct line_range {
            std::size_t begin;
            std::size_t end;
            bool paragraph_end;
        };

        bool space(const char c) { return c == ' ' || c == '\t' || c == '\r'; }

        // The byte after the UTF-8 character starting at i.
        std::size_t next_character(const std::string_view text, std::size_t i) {
            ++i;
            while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) ++i;
            return i;
        }

        std::vector<line_range> wrap_lines(const std::string_view content, const double width,
                                           const std::function<double(std::string_view)> &measure) {
            std::vector<line_range> lines;
            const auto fits = [&](const std::size_t begin, const std::size_t end) {
                return width <= 0 || measure(content.substr(begin, end - begin)) <= width;
            };
            std::size_t start = 0;
            while (start <= content.size()) {
                const std::size_t newline = std::min(content.find('\n', start), content.size());
                std::size_t line_begin = std::string_view::npos, line_end = 0;
                std::size_t i = start;
                while (i < newline) {
                    while (i < newline && space(content[i])) ++i;
                    if (i >= newline) break;
                    std::size_t word_end = i;
                    while (word_end < newline && !space(content[word_end])) ++word_end;
                    if (line_begin != std::string_view::npos && fits(line_begin, word_end)) {
                        line_end = word_end; // the word joins the line
                    } else {
                        if (line_begin != std::string_view::npos) lines.push_back({line_begin, line_end, false});
                        line_begin = i;
                        line_end = word_end;
                        // A word wider than the line: as many characters as fit, at least one, per line.
                        while (!fits(line_begin, line_end)) {
                            std::size_t cut = next_character(content, line_begin);
                            for (std::size_t next = next_character(content, cut); cut < line_end && next <= line_end && fits(line_begin, next);
                                 next = next_character(content, next)) {
                                cut = next;
                            }
                            if (cut >= line_end) break;
                            lines.push_back({line_begin, cut, false});
                            line_begin = cut;
                        }
                    }
                    i = word_end;
                }
                if (line_begin == std::string_view::npos) lines.push_back({start, start, true}); // a blank line
                else lines.push_back({line_begin, line_end, true});
                if (newline >= content.size()) break;
                start = newline + 1;
            }
            return lines;
        }

        // The words of a line, for justified text.
        std::vector<std::string_view> words(const std::string_view line) {
            std::vector<std::string_view> result;
            for (std::size_t i = 0; i < line.size();) {
                while (i < line.size() && space(line[i])) ++i;
                const std::size_t begin = i;
                while (i < line.size() && !space(line[i])) ++i;
                if (i > begin) result.push_back(line.substr(begin, i - begin));
            }
            return result;
        }
    }

    std::vector<std::string> wrap(const std::string_view content, const double width,
                                  const std::function<double(std::string_view)> &measure) {
        std::vector<std::string> result;
        for (const auto &[begin, end, paragraph_end]: wrap_lines(content, width, measure)) result.emplace_back(content.substr(begin, end - begin));
        return result;
    }

    std::vector<std::string> wrap(const std::string_view content, const font &font, const double width) {
        if (!font.face()) return wrap(content, 0, [](std::string_view) { return 0.0; });
        return wrap(content, width, [&](const std::string_view text) { return font.width(text); });
    }

    std::string canvas::draw(const paragraph &text, const color &colour) {
        if (text.content.empty() || text.font.size <= 0) return {};
        const auto measure = [&](const std::string_view part) { return text_width(sc::text{std::string{part}, {}, text.font}); };
        const auto lines = wrap_lines(text.content, text.area.width(), measure);

        // The first baseline sits the font's ascender below the top, so nothing rises out of the
        // rectangle; a line fits while its descender stays inside it. Without an installed font
        // (a canvas with fonts of its own), capitals' height stands in for the ascender.
        const bool installed = text.font.face().has_value();
        const double ascent = installed ? text.font.ascender() : text_cap_height(text.font);
        const double descent = installed ? text.font.descender() : text.font.size * 0.2;
        const double step = text.line_spacing * text.font.size;
        double baseline = text.area.top() + ascent;
        for (std::size_t n = 0; n < lines.size(); ++n, baseline += step) {
            if (baseline + descent > text.area.bottom() + 1e-9) {
                // What didn't fit, from its first line on.
                return text.content.substr(lines[n].begin);
            }
            const auto &[begin, end, paragraph_end] = lines[n];
            const std::string_view content = std::string_view{text.content}.substr(begin, end - begin);
            if (content.empty()) continue;
            if (text.align == align::justify && !paragraph_end) {
                const auto parts = words(content);
                if (parts.size() > 1) {
                    double used = 0;
                    for (const auto part: parts) used += measure(part);
                    const double gap = (text.area.width() - used) / static_cast<double>(parts.size() - 1);
                    double x = text.area.left();
                    for (const auto part: parts) {
                        draw(sc::text{std::string{part}, {x, baseline}, text.font}, colour);
                        x += measure(part) + gap;
                    }
                    continue;
                }
            }
            const double width = measure(content);
            double x = text.area.left();
            if (text.align == align::right) x = text.area.right() - width;
            else if (text.align == align::center) x = text.area.left() + (text.area.width() - width) / 2;
            draw(sc::text{std::string{content}, {x, baseline}, text.font}, colour);
        }
        return {};
    }
}

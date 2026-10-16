// Wrapping (sc::wrap) and setting a paragraph on a canvas (canvas::draw(paragraph)): where lines
// break, where each is drawn, alignment, and what doesn't fit.
#include <canvas.h>
#include <font.h>
#include <paragraph.h>

#include "sc_test.h"

#include <string>
#include <vector>

namespace {
    // One unit per character: lines are easy to work out.
    double characters(const std::string_view text) { return static_cast<double>(text.size()); }

    // Records the text it's given and where; a character is half an em wide, as in tests/canvas.cpp.
    struct recording_canvas : sc::canvas {
        struct placed {
            std::string content;
            double x;
            double y;
        };

        std::vector<placed> texts;

    protected:
        void paint_polygon(const sc::polygon &, const sc::color &, const sc::color &, double) override {}
        void paint_circle(const sc::circle &, const sc::color &, const sc::color &, double) override {}
        void paint_line(const sc::point &, const sc::point &, const sc::color &, double) override {}
        void paint_text(const sc::text &sample, const sc::color &) override { texts.push_back({sample.content, sample.position.x(), sample.position.y()}); }
        double measure_width(const sc::text &sample) override { return sample.content.size() * sample.font.size / 2; }
        double measure_x_height(const sc::font &font) override { return font.size / 2; }
        double measure_cap_height(const sc::font &font) override { return font.size * 0.7; }
    };

    using lines = std::vector<std::string>;
}

int main() {
    SECTION("Lines break at spaces, as wide as fits");
    CHECK_EQ(sc::wrap("the quick brown fox", 10, characters), (lines{"the quick", "brown fox"}));
    CHECK_EQ(sc::wrap("the quick brown fox", 9, characters), (lines{"the quick", "brown fox"}));
    CHECK_EQ(sc::wrap("the quick brown fox", 8, characters), (lines{"the", "quick", "brown", "fox"}));
    CHECK_EQ(sc::wrap("  spaced   out  ", 20, characters), (lines{"spaced   out"})); // ends trimmed, inside kept

    SECTION("Newlines, blank lines, and words too wide for a line");
    CHECK_EQ(sc::wrap("one\ntwo\n\nthree", 20, characters), (lines{"one", "two", "", "three"}));
    CHECK_EQ(sc::wrap("abcdefghij kl", 4, characters), (lines{"abcd", "efgh", "ij", "kl"}));
    CHECK_EQ(sc::wrap("ab cdefghij", 4, characters), (lines{"ab", "cdef", "ghij"}));
    CHECK_EQ(sc::wrap("wide", 0.5, characters), (lines{"w", "i", "d", "e"})); // at least a character a line
    CHECK_EQ(sc::wrap("\xC3\xA9\xC3\xA9\xC3\xA9", 4, characters), (lines{"\xC3\xA9\xC3\xA9", "\xC3\xA9"})); // UTF-8 kept whole
    CHECK_EQ(sc::wrap("no wrapping here", 0, characters), (lines{"no wrapping here"}));
    CHECK_EQ(sc::wrap("", 10, characters), (lines{""}));

    SECTION("Wrapping by an installed font");
    {
        const sc::font font{"sans-serif", 10};
        const auto wrapped = sc::wrap("simply-cpp wraps text in the widths sc-pdf draws it with", font, 100);
        CHECK_LT(std::size_t{1}, wrapped.size());
        for (const auto &line: wrapped) CHECK_LT(font.width(line), 100.0 + 1e-9);
    }

    // The canvas's widths: half the size (10) per character, so 5 units each; the area is 50 wide.
    const sc::font font{"sans-serif", 10};
    const bool installed = font.face().has_value();
    const double ascent = installed ? font.ascender() : 7;

    SECTION("A paragraph: lines from the top, line_spacing apart");
    {
        recording_canvas canvas;
        const auto rest = canvas.draw(sc::paragraph{"one two three four", {20, 100, 50, 200}, font});
        CHECK(rest.empty());
        CHECK_EQ(canvas.texts.size(), std::size_t{2}); // "one two" (35) and "three four" (50)
        if (canvas.texts.size() == 2) {
            CHECK_EQ(canvas.texts[0].content, std::string{"one two"});
            CHECK_EQ(canvas.texts[1].content, std::string{"three four"});
            CHECK_EQ(canvas.texts[0].x, 20.0);
            CHECK_BETWEEN(canvas.texts[0].y, 100 + ascent - 1e-9, 100 + ascent + 1e-9);
            CHECK_BETWEEN(canvas.texts[1].y - canvas.texts[0].y, 12 - 1e-9, 12 + 1e-9); // 1.2 x 10
        }
    }

    SECTION("Alignment");
    {
        recording_canvas right, center;
        (void) right.draw(sc::paragraph{"one two", {20, 100, 50, 200}, font, sc::align::right});
        (void) center.draw(sc::paragraph{"one two", {20, 100, 50, 200}, font, sc::align::center});
        CHECK_EQ(right.texts.at(0).x, 35.0);   // 70 - 35
        CHECK_EQ(center.texts.at(0).x, 27.5);  // 20 + (50 - 35) / 2
    }

    SECTION("Justified: words spread to both edges, but not on a paragraph's last line");
    {
        recording_canvas canvas;
        (void) canvas.draw(sc::paragraph{"aa bb cc dd ee\nlast line", {0, 0, 50, 200}, font, sc::align::justify});
        // "aa bb cc" (40) is stretched: 3 words of 10 and gaps of 10; "dd ee" ends its paragraph.
        CHECK_EQ(canvas.texts.size(), std::size_t{5});
        if (canvas.texts.size() == 5) {
            CHECK_EQ(canvas.texts[0].x, 0.0);
            CHECK_EQ(canvas.texts[1].x, 20.0);
            CHECK_EQ(canvas.texts[2].x, 40.0);
            CHECK_EQ(canvas.texts[3].content, std::string{"dd ee"});
            CHECK_EQ(canvas.texts[3].x, 0.0);
            CHECK_EQ(canvas.texts[4].content, std::string{"last line"});
        }
    }

    SECTION("What doesn't fit the height comes back");
    {
        recording_canvas canvas;
        // Room for two lines: the second baseline at ascent + 12, plus a descender, inside 30.
        const double height = ascent + 12 + (installed ? font.descender() : 2) + 0.5;
        const auto rest = canvas.draw(sc::paragraph{"one two three four five six", {0, 0, 50, height}, font});
        CHECK_EQ(canvas.texts.size(), std::size_t{2});
        CHECK_EQ(rest, std::string{"five six"});
        recording_canvas next;
        CHECK((next.draw(sc::paragraph{rest, {0, 0, 50, height}, font})).empty()); // goes on elsewhere
        CHECK_EQ(next.texts.at(0).content, std::string{"five six"});
        recording_canvas tiny;
        CHECK_EQ(tiny.draw(sc::paragraph{"nothing fits", {0, 0, 50, 2}, font}), std::string{"nothing fits"});
        CHECK(tiny.texts.empty());
    }

    TEST_SUMMARY();
}

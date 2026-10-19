// Text a font can't draw: its characters drawn in a face that has them, emoji as colour bitmaps
// (when a colour emoji font is installed), measured as they're drawn.
#include <canvas.h>
#include <typeface.h>

#include "sc_test.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <variant>

namespace {
    template<typename Item>
    std::size_t count(const sc::drawing &picture) {
        return static_cast<std::size_t>(std::ranges::count_if(picture.items(), [](const auto &item) { return std::holds_alternative<Item>(item); }));
    }

    std::string texts(const sc::drawing &picture) {
        std::string out;
        for (const auto &item: picture.items())
            if (const auto *text = std::get_if<sc::label>(&item)) out += "[" + text->text.content + "]";
        return out;
    }
}

int main() {
    const sc::font plain{"sans-serif", 20};
    if (!plain.face()) {
        SECTION("No fonts installed: nothing to check");
        TEST_SUMMARY();
    }
    const bool colour_font = sc::typeface::find("Noto Color Emoji") || sc::typeface::find("Apple Color Emoji") ||
                             sc::typeface::find("Segoe UI Emoji");

    SECTION("Plain text stays one label");
    {
        sc::drawing picture;
        picture.draw(sc::text{"Hello there", {10, 30}, plain});
        CHECK_EQ(texts(picture), std::string{"[Hello there]"});
    }

    if (colour_font) {
        SECTION("Emoji: colour bitmaps between the text, measured as drawn");
        sc::drawing picture;
        const sc::text sample{"Thumbs 👍 and 🇿🇦 ok", {10, 30}, plain};
        picture.draw(sample);
        CHECK_EQ(texts(picture), std::string{"[Thumbs ][ and ][ ok]"});
        CHECK_EQ(count<sc::figure>(picture), std::size_t{2}); // the flag is one glyph, not two letters
        const auto figures = [&] {
            std::vector<sc::rect> boxes;
            for (const auto &item: picture.items())
                if (const auto *figure = std::get_if<sc::figure>(&item)) boxes.push_back(figure->bounds());
            return boxes;
        }();
        // About an em wide, around the baseline, after "Thumbs ".
        CHECK_LT(14.0, figures[0].width());
        CHECK_LT(figures[0].width(), 30.0);
        CHECK_LT(figures[0].top(), 30.0);
        CHECK_LT(25.0, figures[0].bottom());
        CHECK_LT(10 + plain.width("Thumbs") , figures[0].left());
        // The width is what's drawn: the last label ends there.
        const double width = picture.text_width(sample);
        CHECK_EQ(width, plain.width(sample.content));
        const auto &last = std::get<sc::label>(picture.items().back());
        CHECK_LT(std::abs(last.text.position.x() + plain.width(" ok") - (10 + width)), 0.01);

        SECTION("Joined emoji and skin tones are one glyph each");
        sc::drawing family;
        family.draw(sc::text{"👩‍👩‍👧 👍🏽", {0, 30}, plain});
        CHECK_EQ(count<sc::figure>(family), std::size_t{2});

        SECTION("A VS15 asks for text: no bitmap");
        sc::drawing heart;
        heart.draw(sc::text{"I ❤︎ it", {0, 30}, plain});
        CHECK_EQ(count<sc::figure>(heart), std::size_t{0});
    }

    SECTION("Symbols the font lacks come from a face that has them");
    {
        sc::drawing picture;
        picture.draw(sc::text{"Done ✓ ★", {10, 30}, plain});
        bool found = false;
        for (const auto &item: picture.items())
            if (const auto *text = std::get_if<sc::label>(&item))
                if (text->text.content.find("✓") != std::string::npos) {
                    found = true;
                    const auto face = text->text.font.face();
                    CHECK(face.has_value());
                    if (face) CHECK(face->covers("✓"));
                }
        CHECK(found);
    }
    TEST_SUMMARY();
}

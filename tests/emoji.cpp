// Text a font can't draw: its characters drawn in a face that has them, emoji as colour bitmaps
// (when a colour emoji font is installed), measured as they're drawn.
#include <sc/graphics/canvas.h>
#include <sc/graphics/typeface.h>

#include "sc_test.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <variant>

namespace {
    template<typename Item>
    std::size_t count(const sc::graphics::drawing &picture) {
        return static_cast<std::size_t>(std::ranges::count_if(picture.items(), [](const auto &item) { return std::holds_alternative<Item>(item); }));
    }

    std::string texts(const sc::graphics::drawing &picture) {
        std::string out;
        for (const auto &item: picture.items())
            if (const auto *text = std::get_if<sc::graphics::label>(&item)) out += "[" + text->text.content + "]";
        return out;
    }
}

int main() {
    const sc::graphics::font plain{"sans-serif", 20};
    if (!plain.face()) {
        SECTION("No fonts installed: nothing to check");
        TEST_SUMMARY();
    }
    const bool colour_font = sc::graphics::typeface::find("Noto Color Emoji") || sc::graphics::typeface::find("Apple Color Emoji") ||
                             sc::graphics::typeface::find("Segoe UI Emoji");

    SECTION("Plain text stays one label");
    {
        sc::graphics::drawing picture;
        picture.draw(sc::graphics::text{"Hello there", {10, 30}, plain});
        CHECK_EQ(texts(picture), std::string{"[Hello there]"});
    }

    if (colour_font) {
        SECTION("Emoji: colour bitmaps between the text, measured as drawn");
        sc::graphics::drawing picture;
        const sc::graphics::text sample{"Thumbs 👍 and 🇿🇦 ok", {10, 30}, plain};
        picture.draw(sample);
        CHECK_EQ(texts(picture), std::string{"[Thumbs ][ and ][ ok]"});
        CHECK_EQ(count<sc::graphics::figure>(picture), std::size_t{2}); // the flag is one glyph, not two letters
        const auto figures = [&] {
            std::vector<sc::rect> boxes;
            for (const auto &item: picture.items())
                if (const auto *figure = std::get_if<sc::graphics::figure>(&item)) boxes.push_back(figure->bounds());
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
        const auto &last = std::get<sc::graphics::label>(picture.items().back());
        CHECK_LT(std::abs(last.text.position.x() + plain.width(" ok") - (10 + width)), 0.01);

        SECTION("Joined emoji and skin tones are one glyph each");
        sc::graphics::drawing family;
        family.draw(sc::graphics::text{"👩‍👩‍👧 👍🏽", {0, 30}, plain});
        CHECK_EQ(count<sc::graphics::figure>(family), std::size_t{2});

        SECTION("A VS15 asks for text: no bitmap");
        sc::graphics::drawing heart;
        heart.draw(sc::graphics::text{"I ❤︎ it", {0, 30}, plain});
        CHECK_EQ(count<sc::graphics::figure>(heart), std::size_t{0});
    }

    SECTION("Symbols the font lacks come from a face that has them");
    {
        sc::graphics::drawing picture;
        picture.draw(sc::graphics::text{"Done ✓ ★", {10, 30}, plain});
        bool found = false;
        for (const auto &item: picture.items())
            if (const auto *text = std::get_if<sc::graphics::label>(&item))
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

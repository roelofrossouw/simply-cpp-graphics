// sc::standard_font: which built-in PDF font a family is, and Adobe's metrics for it.
#include <standard_font.h>

#include "sc_test.h"

#include <cmath>
#include <string>

int main() {
    // name() and encode()'s results are taken out of the optional first: CHECK_EQ keeps a
    // reference to what it compares, which a temporary optional's member doesn't outlive.
    const auto name_of = [](const sc::font &spec) { return std::string{sc::standard_font::of(spec)->name()}; };
    const auto byte_of = [](const char32_t c) { return static_cast<unsigned char>(*sc::standard_font::encode(c)); };

    SECTION("Families to built-in fonts, by weight and slant");
    {
        CHECK_EQ(name_of(sc::font{"Helvetica"}), std::string{"Helvetica"});
        CHECK_EQ(name_of(sc::font{"arial", 12, 700}), std::string{"Helvetica-Bold"});
        CHECK_EQ(name_of(sc::font{"Times New Roman", 12, 400, true}), std::string{"Times-Italic"});
        CHECK_EQ(name_of(sc::font{"Times", 12, 700, true}), std::string{"Times-BoldItalic"});
        CHECK_EQ(name_of(sc::font{"Courier New", 12, 400, true}), std::string{"Courier-Oblique"});
        CHECK(!sc::standard_font::of(sc::font{"DejaVu Sans"}));
        CHECK(!sc::standard_font::of(sc::font{"sans-serif"})); // generic: the caller decides
    }

    SECTION("Adobe's widths, in thousandths of the size");
    {
        const auto helvetica = *sc::standard_font::of(sc::font{"Helvetica"});
        CHECK_EQ(helvetica.width(" ", 1000), 278.0);
        CHECK_EQ(helvetica.width("a", 1000), 556.0);
        CHECK_EQ(helvetica.width("W", 1000), 944.0);
        CHECK_EQ(helvetica.width("Wai", 10), (944 + 556 + 222) * 10 / 1000.0);
        CHECK_EQ(sc::standard_font::of(sc::font{"Times"})->width("a", 1000), 444.0);
        CHECK_EQ(sc::standard_font::of(sc::font{"Courier", 12, 700})->width("iW", 1000), 1200.0);
        CHECK_EQ(helvetica.width("€", 1000), 556.0); // Windows-1252's 0x80
        CHECK_EQ(helvetica.width("é", 1000), 556.0);
        CHECK_EQ(helvetica.width("😀", 1000), 0.0); // not in the font
        CHECK_EQ(helvetica.ascender(1000), 718.0);
        CHECK_EQ(helvetica.descender(1000), 207.0);
    }

    SECTION("Windows-1252");
    {
        CHECK_EQ(byte_of(U'A'), static_cast<unsigned char>('A'));
        CHECK_EQ(byte_of(U'€'), 0x80u);
        CHECK_EQ(byte_of(U'é'), 0xE9u);
        CHECK(!sc::standard_font::encode(U'Ω'));
    }
    TEST_SUMMARY();
}

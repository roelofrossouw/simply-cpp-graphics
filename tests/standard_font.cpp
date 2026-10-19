// sc::standard_font: which built-in PDF font a family is, and Adobe's metrics for it.
#include <standard_font.h>

#include "sc_test.h"

#include <cmath>
#include <string>

int main() {
    SECTION("Families to built-in fonts, by weight and slant");
    {
        CHECK_EQ(sc::standard_font::of(sc::font{"Helvetica"})->name(), std::string{"Helvetica"});
        CHECK_EQ(sc::standard_font::of(sc::font{"arial", 12, 700})->name(), std::string{"Helvetica-Bold"});
        CHECK_EQ(sc::standard_font::of(sc::font{"Times New Roman", 12, 400, true})->name(), std::string{"Times-Italic"});
        CHECK_EQ(sc::standard_font::of(sc::font{"Times", 12, 700, true})->name(), std::string{"Times-BoldItalic"});
        CHECK_EQ(sc::standard_font::of(sc::font{"Courier New", 12, 400, true})->name(), std::string{"Courier-Oblique"});
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
        CHECK_EQ(*sc::standard_font::encode(U'A'), 'A');
        CHECK_EQ(static_cast<unsigned char>(*sc::standard_font::encode(U'€')), 0x80u);
        CHECK_EQ(static_cast<unsigned char>(*sc::standard_font::encode(U'é')), 0xE9u);
        CHECK(!sc::standard_font::encode(U'Ω'));
    }
    TEST_SUMMARY();
}

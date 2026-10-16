// Typefaces, their faces and fonts, against the fonts installed here (the build servers have
// DejaVu, macOS its own): finding them, their styles, telling a substitute, metrics and widths.
#include <font.h>

#include "sc_test.h"

#include <cctype>
#include <string>

int main() {
    SECTION("A font finds its face, and the face its typeface");
    const sc::font sans{"sans-serif", 10};
    const auto face = sans.face();
    CHECK(face.has_value());
    if (!face) TEST_SUMMARY();
    CHECK(!face->file().empty());
    CHECK(!face->family().empty());
    CHECK_EQ(face->typeface().name(), face->family());
    CHECK(sans.available()); // a generic family: any font answers it
    CHECK(face->typeface().face().has_value());

    SECTION("Typefaces: by name, ignoring case, with their styles");
    {
        const auto family = face->typeface();
        auto upper = family.name();
        for (auto &c: upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        CHECK(sc::typeface::find(upper).has_value());
        if (const auto found = sc::typeface::find(upper)) CHECK_EQ(found->name(), family.name());
        CHECK(!sc::typeface::find("No Such Font Family 7Q").has_value());
        const auto faces = family.faces();
        CHECK(!faces.empty());
        for (const auto &each: faces) {
            CHECK_EQ(each.family(), family.name());
            CHECK_BETWEEN(each.weight(), 1, 1000);
        }
        bool installed = false;
        for (const auto &each: sc::typeface::installed()) installed = installed || each == family;
        CHECK(installed);
    }

    SECTION("A typeface's closest style");
    {
        const auto family = face->typeface();
        const auto regular = family.face(400);
        const auto bold = family.face(700);
        CHECK(regular.has_value());
        if (regular && bold) {
            CHECK_LT(regular->weight(), bold->weight() + 1); // as heavy or heavier
            CHECK_EQ(regular->family(), bold->family());
        }
        if (const auto italic = family.face(400, true)) CHECK_EQ(italic->family(), family.name()); // its own, even if upright
    }

    SECTION("A family that isn't installed: a substitute, not available");
    {
        const sc::font missing{"No Such Font Family 7Q", 10};
        CHECK(missing.face().has_value()); // fontconfig answers with something
        CHECK(!missing.available());
        if (const auto substitute = missing.face()) CHECK((sc::font{substitute->family(), 10}.available()));
    }

    SECTION("Metrics, at the font's size");
    {
        CHECK_LT(0.0, sans.x_height());
        CHECK_LT(sans.x_height(), sans.cap_height());
        CHECK_LT(sans.cap_height(), sans.ascender() + 1e-9);
        CHECK_LT(0.0, sans.descender());
        CHECK_LT(sans.ascender(), sans.line_height() + 1e-9);
        CHECK_BETWEEN(sans.cap_height(), 5.0, 9.0); // capitals are 60 to 80% of the size
        CHECK_BETWEEN(face->cap_height(20), 2 * sans.cap_height() - 1e-9, 2 * sans.cap_height() + 1e-9);
    }

    SECTION("Widths: the advances, no kerning");
    {
        CHECK_EQ(sans.width(""), 0.0);
        CHECK_LT(sans.width("iiii"), sans.width("WWWW"));
        CHECK_BETWEEN(sans.width("AV"), sans.width("A") + sans.width("V") - 1e-9, sans.width("A") + sans.width("V") + 1e-9);
        CHECK_BETWEEN(face->width("Hello, world", 20), 2 * sans.width("Hello, world") - 1e-9, 2 * sans.width("Hello, world") + 1e-9);
        CHECK_LT(0.0, sans.width("€ é ü")); // UTF-8
        const sc::font bold = sc::font::bold("sans-serif", 10);
        CHECK_LT(sans.width("Hello"), bold.width("Hello") + 1e-9); // bold is at least as wide
    }

    SECTION("Coverage");
    {
        CHECK(face->covers("Hello, world\n"));
        CHECK(!face->covers("\xF4\x8F\xBF\xBF")); // U+10FFFF: no font has it
    }

    TEST_SUMMARY();
}

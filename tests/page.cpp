// Page sizes: ISO's in whole millimetres, Letter and Legal in inches, each in points, landscape swapping them, and sizes read by name.
#include <distance.h>
#include <page.h>

#include "sc_test.h"

#include <stdexcept>

int main() {
    SECTION("Each paper, portrait: ISO's in whole millimetres, the others in inches");
    {
        const auto mm = [](const double points) { return sc::distance(points).millimetres(); };
        CHECK_NEAR(mm(sc::page_dimensions(sc::page_size::A4).width()), 210.0, 1e-9);
        CHECK_NEAR(mm(sc::page_dimensions(sc::page_size::A4).height()), 297.0, 1e-9);
        CHECK_NEAR(sc::page_dimensions(sc::page_size::A4).width(), 595.2756, 1e-4); // fractions of a point
        CHECK_NEAR(sc::page_dimensions(sc::page_size::A4).height(), 841.8898, 1e-4);
        CHECK_NEAR(mm(sc::page_dimensions(sc::page_size::A3).height()), 420.0, 1e-9);
        CHECK_NEAR(mm(sc::page_dimensions(sc::page_size::A5).width()), 148.0, 1e-9);
        CHECK_EQ(sc::page_dimensions(sc::page_size::letter).width(), 612.0); // 8.5 x 11 inches
        CHECK_EQ(sc::page_dimensions(sc::page_size::letter).height(), 792.0);
        CHECK_EQ(sc::page_dimensions(sc::page_size::legal).height(), 1008.0);
    }

    SECTION("Landscape swaps width and height");
    {
        const auto wide = sc::page_dimensions(sc::page_size::A3, sc::orientation::landscape);
        CHECK_NEAR(sc::distance(wide.width()).millimetres(), 420.0, 1e-9);
        CHECK_NEAR(sc::distance(wide.height()).millimetres(), 297.0, 1e-9);
    }

    SECTION("A size by its name, in any case");
    {
        CHECK(sc::page_size_from("A3") == sc::page_size::A3);
        CHECK(sc::page_size_from("a3") == sc::page_size::A3); // any case
        CHECK(sc::page_size_from("A4") == sc::page_size::A4);
        CHECK(sc::page_size_from("Letter") == sc::page_size::letter);
        CHECK_THROWS_AS(sc::page_size_from("b5"), std::invalid_argument);
    }

    TEST_SUMMARY();
}

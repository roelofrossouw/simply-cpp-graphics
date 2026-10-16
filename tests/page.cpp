// Page sizes: each paper's points, landscape swapping them, and sizes read by name.
#include <page.h>

#include "sc_test.h"

#include <stdexcept>

int main() {
    SECTION("Each paper, portrait, in points");
    {
        CHECK_EQ(sc::page_dimensions(sc::page_size::a4).width(), 595.0);
        CHECK_EQ(sc::page_dimensions(sc::page_size::a4).height(), 842.0);
        CHECK_EQ(sc::page_dimensions(sc::page_size::a3).height(), 1191.0);
        CHECK_EQ(sc::page_dimensions(sc::page_size::a5).width(), 420.0);
        CHECK_EQ(sc::page_dimensions(sc::page_size::letter).height(), 792.0);
        CHECK_EQ(sc::page_dimensions(sc::page_size::legal).height(), 1008.0);
    }

    SECTION("Landscape swaps width and height");
    {
        const auto wide = sc::page_dimensions(sc::page_size::a3, sc::orientation::landscape);
        CHECK_EQ(wide.width(), 1191.0);
        CHECK_EQ(wide.height(), 842.0);
    }

    SECTION("A size by its name, in any case");
    {
        CHECK(sc::page_size_from("a3") == sc::page_size::a3);
        CHECK(sc::page_size_from("A4") == sc::page_size::a4);
        CHECK(sc::page_size_from("Letter") == sc::page_size::letter);
        CHECK_THROWS_AS(sc::page_size_from("b5"), std::invalid_argument);
    }

    TEST_SUMMARY();
}

// sc::stroke and sc::fill: what they hold, their defaults, colours converting to them, and when
// they paint nothing.
#include <paint.h>

#include "sc_test.h"

int main() {
    SECTION("A stroke: black and 1 wide by default; a colour converts to one");
    {
        const sc::stroke plain;
        CHECK(plain.colour() == sc::color::Black);
        CHECK_EQ(plain.width(), 1.0);
        CHECK(plain.visible());
        const sc::stroke red = sc::color::Red;
        CHECK(red.colour() == sc::color::Red);
        CHECK_EQ(red.width(), 1.0);
        const sc::stroke hairline{sc::color::Blue, 0.25};
        CHECK_EQ(hairline.width(), 0.25);
        CHECK(hairline == (sc::stroke{sc::color::Blue, 0.25}));
        CHECK(hairline != red);
    }

    SECTION("A stroke that paints nothing");
    {
        CHECK(!sc::stroke::none().visible());
        CHECK(!(sc::stroke{sc::color::Transparent, 2}).visible());
        CHECK(!(sc::stroke{sc::color::Black, 0}).visible());
    }

    SECTION("A fill: none by default; a colour converts to one");
    {
        CHECK(!sc::fill{}.visible());
        CHECK(sc::fill{} == sc::fill::none());
        const sc::fill gold = sc::color{"gold"};
        CHECK(gold.visible());
        CHECK(gold.colour() == sc::color{"gold"});
        CHECK(!sc::fill{sc::color::Transparent}.visible());
    }

    SECTION("A fill's rule: nonzero unless it's given");
    {
        CHECK(sc::fill{sc::color::Red}.rule() == sc::fill_rule::nonzero);
        const sc::fill ring{sc::color::Red, sc::fill_rule::even_odd};
        CHECK(ring.rule() == sc::fill_rule::even_odd);
        CHECK(ring != sc::fill{sc::color::Red});
    }

    TEST_SUMMARY();
}

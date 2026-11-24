// sc::graphics::stroke, sc::graphics::fill and sc::graphics::appearance: what they hold, their defaults, colours converting to
// them, and when they paint nothing.
#include <sc/graphics/paint.h>

#include "sc_test.h"

int main() {
    SECTION("A stroke: black and 1 wide by default; a colour converts to one");
    {
        const sc::graphics::stroke plain;
        CHECK(plain.colour() == sc::color::Black);
        CHECK_EQ(plain.width(), 1.0);
        CHECK(plain.visible());
        const sc::graphics::stroke red = sc::color::Red;
        CHECK(red.colour() == sc::color::Red);
        CHECK_EQ(red.width(), 1.0);
        const sc::graphics::stroke hairline{sc::color::Blue, 0.25};
        CHECK_EQ(hairline.width(), 0.25);
        CHECK(hairline == (sc::graphics::stroke{sc::color::Blue, 0.25}));
        CHECK(hairline != red);
    }

    SECTION("A stroke that paints nothing");
    {
        CHECK(!sc::graphics::stroke::none().visible());
        CHECK(!(sc::graphics::stroke{sc::color::Transparent, 2}).visible());
        CHECK(!(sc::graphics::stroke{sc::color::Black, 0}).visible());
    }

    SECTION("A fill: none by default; a colour converts to one");
    {
        CHECK(!sc::graphics::fill{}.visible());
        CHECK(sc::graphics::fill{} == sc::graphics::fill::none());
        const sc::graphics::fill gold = sc::color{"gold"};
        CHECK(gold.visible());
        CHECK(gold.colour() == sc::color{"gold"});
        CHECK(!sc::graphics::fill{sc::color::Transparent}.visible());
    }

    SECTION("A fill's rule: nonzero unless it's given");
    {
        CHECK(sc::graphics::fill{sc::color::Red}.rule() == sc::graphics::fill_rule::nonzero);
        const sc::graphics::fill ring{sc::color::Red, sc::graphics::fill_rule::even_odd};
        CHECK(ring.rule() == sc::graphics::fill_rule::even_odd);
        CHECK(ring != sc::graphics::fill{sc::color::Red});
    }

    SECTION("An appearance: a black outline and no fill by default");
    {
        const sc::graphics::appearance plain;
        CHECK(plain.stroke() == sc::graphics::stroke{});
        CHECK(plain.fill() == sc::graphics::fill::none());
        CHECK(plain.visible());
    }

    SECTION("A colour or a stroke converts to an appearance: an outline only");
    {
        const sc::graphics::appearance red = sc::color::Red;
        CHECK(red.stroke() == (sc::graphics::stroke{sc::color::Red, 1}));
        CHECK(!red.fill().visible());
        const sc::graphics::appearance thick = sc::graphics::stroke{sc::color::Blue, 3};
        CHECK_EQ(thick.stroke().width(), 3.0);
        CHECK(!thick.fill().visible());
    }

    SECTION("Braces give a stroke and a fill; filled() and none()");
    {
        const sc::graphics::appearance both = {sc::color::Black, sc::color{"gold"}};
        CHECK(both.stroke() == (sc::graphics::stroke{sc::color::Black, 1}));
        CHECK(both.fill() == sc::graphics::fill{sc::color{"gold"}});
        const auto ring = sc::graphics::appearance::filled(sc::graphics::fill{sc::color::Red, sc::graphics::fill_rule::even_odd});
        CHECK(!ring.stroke().visible());
        CHECK(ring.fill().rule() == sc::graphics::fill_rule::even_odd);
        CHECK(ring.visible());
        CHECK(!sc::graphics::appearance::none().visible());
        CHECK(!(sc::graphics::appearance{sc::graphics::stroke::none(), sc::color::Transparent}).visible());
        CHECK(both != ring);
    }

    SECTION("Changing an appearance's stroke or fill");
    {
        sc::graphics::appearance look;
        look.fill(sc::color::Green);
        look.stroke(sc::graphics::stroke::none());
        CHECK(look == sc::graphics::appearance::filled(sc::color::Green));
    }

    SECTION("A linear gradient: offsets along it, colours between its stops");
    {
        const sc::graphics::linear_gradient across{{0, 0}, {100, 0}, {{1, sc::color::Blue}, {0, sc::color::Red}}}; // stops sorted
        CHECK(across.stops().front().colour == sc::color::Red);
        CHECK_NEAR(across.offset_of({50, 999}), 0.5, 1e-12); // the same all down a line across it
        CHECK_NEAR(across.offset_of({-50, 0}), -0.5, 1e-12);
        const auto middle = across.colour_at(0.5);
        CHECK_NEAR(middle.red(), 0.5, 1e-9);
        CHECK_NEAR(middle.blue(), 0.5, 1e-9);
        CHECK(across.colour_at(-1) == sc::color::Red); // past the ends, the end colours
        CHECK(across.colour_at(7) == sc::color::Blue);

        // Through a transform: what was at an offset is still there.
        const auto t = sc::transform::rotate(90).then(sc::transform::scale(2, 0.5)).then(sc::transform::translate(10, 20));
        const auto moved = across.transformed(t);
        for (const sc::point p: {sc::point{25, 7}, sc::point{80, -30}, sc::point{0, 0}})
            CHECK_NEAR(moved.offset_of(t.apply(p)), across.offset_of(p), 1e-9);

        const sc::graphics::fill fill = across; // a gradient converts to a fill
        CHECK(fill.gradient().has_value());
        CHECK(fill.visible());
        CHECK_NEAR(fill.colour().red(), 0.5, 1e-9); // its middle colour stands in
        CHECK(!(sc::graphics::fill{sc::graphics::linear_gradient{{0, 0}, {1, 0}, {{0, sc::color::Transparent}}}}).visible());
    }

    TEST_SUMMARY();
}

// sc::path: building, bounds, transforming, and flattening curves into straight segments.
#include <path.h>

#include "sc_test.h"

#include <cmath>

int main() {
    SECTION("Building");
    {
        sc::path shape;
        CHECK(shape.empty());
        shape.move_to({1, 2}).line_to({5, 2}).cubic_to({6, 2}, {6, 6}, {5, 6}).close();
        CHECK_EQ(shape.steps().size(), size_t{4});
        CHECK(shape.steps()[2].type == sc::path::kind::cubic);
        CHECK_EQ(shape.steps()[2].first, (sc::point{6, 2}));
        shape.close(); // twice is once
        CHECK_EQ(shape.steps().size(), size_t{4});

        sc::path implicit;
        implicit.line_to({3, 3}); // a path starts with a move
        CHECK(implicit.steps()[0].type == sc::path::kind::move);
    }

    SECTION("Bounds and transforming");
    {
        sc::path shape;
        shape.move_to({1, 2}).line_to({5, 2}).line_to({5, 8});
        const auto box = shape.bounds();
        CHECK_EQ(box.left(), 1.0);
        CHECK_EQ(box.bottom(), 8.0);
        const auto moved = shape.transformed(2, 3, {10, 20});
        CHECK_EQ(moved.steps()[1].to, (sc::point{20, 26}));
        CHECK(sc::path{}.bounds().width() == 0);
    }

    SECTION("Flattening");
    {
        sc::path circle_ish;
        circle_ish.move_to({10, 0}).cubic_to({10, 5.5}, {5.5, 10}, {0, 10}).close();
        const auto outlines = circle_ish.flattened(0.1);
        CHECK_EQ(outlines.size(), size_t{1});
        CHECK(outlines[0].closed);
        CHECK_LT(size_t{5}, outlines[0].points.size());
        for (const auto &point: outlines[0].points) CHECK_LT(std::hypot(point.x(), point.y()), 10.6); // on or near the arc

        sc::path two;
        two.move_to({0, 0}).line_to({1, 0}).move_to({5, 5}).line_to({6, 5}).line_to({6, 6}).close();
        const auto parts = two.flattened();
        CHECK_EQ(parts.size(), size_t{2});
        CHECK(!parts[0].closed);
        CHECK(parts[1].closed);
        CHECK_EQ(parts[1].points.size(), size_t{3});
    }
    TEST_SUMMARY();
}

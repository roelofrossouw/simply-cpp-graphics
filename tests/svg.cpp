// sc::svg reads SVG into paths with their colours: shapes, groups, transforms, viewBox and errors.
#include <svg.h>

#include "sc_test.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

int main() {
    SECTION("Shapes and colours");
    {
        const sc::svg drawing{R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="320" height="200">)svg"
                              R"svg(<rect width="320" height="200" rx="16" fill="#2a6f97"/>)svg"
                              R"svg(<circle cx="160" cy="100" r="70" fill="#f4d35e"/>)svg"
                              R"svg(<path d="M120 130 Q160 170 200 130" stroke="#2a6f97" stroke-width="8" fill="none"/></svg>)svg"};
        CHECK_EQ(drawing.width(), 320.0);
        CHECK_EQ(drawing.height(), 200.0);
        CHECK_EQ(drawing.elements().size(), size_t{3});
        const auto &background = drawing.elements()[0];
        CHECK(background.fill.colour() == sc::color::from_hex("#2a6f97"));
        CHECK(!background.stroke.visible());
        const auto box = background.shape.bounds();
        CHECK_NEAR(box.width(), 320.0, 1e-9);
        CHECK_NEAR(box.height(), 200.0, 1e-9);
        CHECK_NEAR(box.top(), 0.0, 1e-9); // y down: the top is at 0

        const auto &circle = drawing.elements()[1].shape.bounds();
        CHECK_NEAR(circle.left(), 90.0, 0.5);
        CHECK_NEAR(circle.top(), 30.0, 0.5);

        const auto &smile = drawing.elements()[2];
        CHECK(!smile.fill.visible());
        CHECK_EQ(smile.stroke.width(), 8.0);
        CHECK_LT(130.0, smile.shape.bounds().bottom()); // the curve bends down the page
    }

    SECTION("viewBox and transforms");
    {
        const sc::svg drawing{R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="100 100 50 20">)svg"
                              R"svg(<g transform="translate(10,5)"><rect x="100" y="100" width="10" height="10" fill="#000000"/></g></svg>)svg"};
        CHECK_EQ(drawing.width(), 50.0);
        CHECK_EQ(drawing.height(), 20.0);
        const auto box = drawing.elements().at(0).shape.bounds();
        CHECK_NEAR(box.left(), 10.0, 1e-9); // the viewBox starts at the origin, then translated
        CHECK_NEAR(box.top(), 5.0, 1e-9);
    }

    SECTION("Clip paths");
    {
        const sc::svg disc{R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">)svg"
                           R"svg(<defs><clipPath id="round"><circle cx="50" cy="50" r="45"/></clipPath></defs>)svg"
                           R"svg(<rect clip-path="url(#round)" width="100" height="50" fill="#e9c46a"/></svg>)svg"};
        const auto &clip = disc.elements().at(0).clip;
        CHECK(clip.has_value());
        if (clip) {
            const auto box = clip->bounds();
            CHECK_NEAR(box.left(), 5.0, 1.0); // the circle where it was drawn, not at the origin
            CHECK_NEAR(box.top(), 5.0, 1.0);
            CHECK_NEAR(box.width(), 90.0, 1.0);
        }
    }

    SECTION("Files and mistakes");
    {
        const auto file = std::filesystem::temp_directory_path() / "sc-graphics-test.svg";
        std::ofstream{file} << R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"><rect width="4" height="4"/></svg>)";
        CHECK_EQ(sc::svg::load(file).elements().size(), size_t{1});
        std::filesystem::remove(file);
        CHECK_THROWS_AS(sc::svg::load(file), std::runtime_error);
        CHECK_THROWS_AS(sc::svg{"not svg at all"}, std::runtime_error);
        CHECK_THROWS_AS(sc::svg{"<html></html>"}, std::runtime_error);
    }
    SECTION("fill-rule is the element's fill's");
    {
        const sc::svg ring{R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10">)svg"
                           R"svg(<path fill-rule="evenodd" fill="#000000" d="M0 0 H10 V10 H0 Z M3 3 V7 H7 V3 Z"/>)svg"
                           R"svg(<rect width="4" height="4" fill="#ff0000"/></svg>)svg"};
        CHECK_EQ(ring.elements().size(), std::size_t{2});
        if (ring.elements().size() == 2) {
            CHECK(ring.elements()[0].fill.rule() == sc::fill_rule::even_odd);
            CHECK(ring.elements()[1].fill.rule() == sc::fill_rule::nonzero);
        }
    }

    TEST_SUMMARY();
}

// sc::drawing: reading SVG (shapes keeping their kind, paint, transforms, viewBox, clips, text,
// <use>, style="..."), writing it back, bounds, transforms and flips across the page, recording
// what's drawn on it, and the mistakes that throw.
#include <drawing.h>

#include "sc_test.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace {
    const sc::drawable &shape_at(const sc::drawing &picture, const std::size_t i) { return std::get<sc::drawable>(picture.items().at(i)); }

    template<typename T>
    bool holds(const sc::drawable &item) { return std::holds_alternative<T>(item.shape.value()); }

    bool near(const sc::rect &r, const double left, const double top, const double width, const double height, const double tolerance = 1e-9) {
        return std::abs(r.left() - left) < tolerance && std::abs(r.top() - top) < tolerance && std::abs(r.width() - width) < tolerance &&
               std::abs(r.height() - height) < tolerance;
    }
}

int main() {
    SECTION("Shapes keep their kind, with their paint");
    {
        const auto picture = sc::drawing::from_svg(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="320" height="200">)svg"
                                                   R"svg(<rect width="320" height="200" rx="16" fill="#2a6f97"/>)svg"
                                                   R"svg(<circle cx="160" cy="100" r="70" fill="#f4d35e"/>)svg"
                                                   R"svg(<path d="M120 130 Q160 170 200 130" stroke="#2a6f97" stroke-width="8" fill="none"/></svg>)svg");
        CHECK_EQ(picture.page().width(), 320.0);
        CHECK_EQ(picture.page().height(), 200.0);
        CHECK_EQ(picture.size(), std::size_t{3});
        const auto &background = shape_at(picture, 0);
        CHECK(holds<sc::rounded_rect>(background));
        CHECK(background.appearance.fill().colour() == sc::color::from_hex("#2a6f97"));
        CHECK(!background.appearance.stroke().visible());
        CHECK(near(background.bounds(), 0, 0, 320, 200));
        CHECK(holds<sc::circle>(shape_at(picture, 1)));
        CHECK(near(shape_at(picture, 1).bounds(), 90, 30, 140, 140));
        const auto &smile = shape_at(picture, 2);
        CHECK(holds<sc::path>(smile));
        CHECK(!smile.appearance.fill().visible());
        CHECK_EQ(smile.appearance.stroke().width(), 8.0);
        CHECK_NEAR(smile.bounds().bottom(), 150.0, 1e-9); // the curve's lowest point, exactly
    }

    SECTION("The other shapes, style=\"...\", opacity and fill-rule");
    {
        const auto picture = sc::drawing::from_svg(
            R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">)svg"
            R"svg(<line x1="0" y1="0" x2="10" y2="10" stroke="red"/>)svg"
            R"svg(<polyline points="0,0 10,0 10,10" fill="none" stroke="blue"/>)svg"
            R"svg(<polygon points="0,0 10,0 5,8" style="fill: green; stroke: black; stroke-width: 2"/>)svg"
            R"svg(<ellipse cx="50" cy="50" rx="20" ry="10" fill="black" fill-opacity="0.5"/>)svg"
            R"svg(<path fill-rule="evenodd" d="M0 0 H10 V10 H0 Z M3 3 V7 H7 V3 Z" opacity="0.5" fill="#000"/>)svg"
            R"svg(<path d="M10 50 a40 40 0 0 1 80 0" fill="none" stroke="#000"/>)svg"
            R"svg(<rect width="5" height="5" fill="none"/></svg>)svg"); // paints nothing: left out
        CHECK_EQ(picture.size(), std::size_t{6});
        CHECK(holds<sc::line>(shape_at(picture, 0)));
        CHECK(shape_at(picture, 0).appearance.stroke().colour() == sc::color::Red);
        CHECK(!shape_at(picture, 0).appearance.fill().visible()); // a line has no inside
        CHECK(holds<sc::path>(shape_at(picture, 1)));             // a polyline is open
        CHECK(holds<sc::polygon>(shape_at(picture, 2)));
        CHECK(shape_at(picture, 2).appearance.fill().colour() == sc::color{"green"});
        CHECK_EQ(shape_at(picture, 2).appearance.stroke().width(), 2.0);
        CHECK(near(shape_at(picture, 3).bounds(), 30, 40, 40, 20, 1e-6)); // the ellipse
        CHECK_NEAR(shape_at(picture, 3).appearance.fill().colour().alpha(), 0.5, 1e-12);
        CHECK(shape_at(picture, 4).appearance.fill().rule() == sc::fill_rule::even_odd);
        CHECK_NEAR(shape_at(picture, 4).appearance.fill().colour().alpha(), 0.5, 1e-12);
        CHECK(near(shape_at(picture, 5).bounds(), 10, 10, 80, 40, 1e-6)); // a half circle arc: its top at 50 - 40
    }

    SECTION("viewBox, transforms, <use> and nested groups");
    {
        const auto picture = sc::drawing::from_svg(
            R"svg(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="100 100 50 20">)svg"
            R"svg(<defs><rect id="dot" width="2" height="2"/></defs>)svg"
            R"svg(<g transform="translate(10,5)"><rect x="100" y="100" width="10" height="10" fill="#000000"/></g>)svg"
            R"svg(<g transform="rotate(90 100 100)"><rect x="100" y="100" width="10" height="4"/></g>)svg"
            R"svg(<g transform="rotate(30 100 100)"><rect x="100" y="100" width="10" height="4"/></g>)svg"
            R"svg(<use xlink:href="#dot" x="120" y="110" fill="red"/></svg>)svg");
        CHECK_EQ(picture.page().width(), 50.0);
        CHECK_EQ(picture.page().height(), 20.0);
        CHECK_EQ(picture.size(), std::size_t{4});
        CHECK(near(shape_at(picture, 0).bounds(), 10, 5, 10, 10)); // the viewBox at the origin, then translated
        CHECK(holds<sc::rect>(shape_at(picture, 1)));              // a quarter turn: still a rect
        CHECK(near(shape_at(picture, 1).bounds(), -4, 0, 4, 10));
        CHECK(holds<sc::rotated_rect>(shape_at(picture, 2)));
        CHECK_NEAR(std::get<sc::rotated_rect>(shape_at(picture, 2).shape.value()).angle(), 30.0, 1e-9);
        CHECK(holds<sc::rect>(shape_at(picture, 3)));
        CHECK(near(shape_at(picture, 3).bounds(), 20, 10, 2, 2));
        CHECK(shape_at(picture, 3).appearance.fill().colour() == sc::color::Red);
    }

    SECTION("Clip paths");
    {
        const auto disc = sc::drawing::from_svg(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">)svg"
                                                R"svg(<defs><clipPath id="round"><circle cx="50" cy="50" r="45"/></clipPath></defs>)svg"
                                                R"svg(<rect clip-path="url(#round)" width="100" height="50" fill="#e9c46a"/></svg>)svg");
        const auto &clip = shape_at(disc, 0).clip;
        CHECK(clip.has_value());
        if (clip) CHECK(near(clip->bounds(), 5, 5, 90, 90, 1e-9)); // where the circle was drawn
        CHECK(near(shape_at(disc, 0).bounds(), 5, 5, 90, 45, 1e-9)); // the rect within the clip
    }

    SECTION("A mask is read as a clip of its shapes, through <use> and groups");
    {
        const auto badge = sc::drawing::from_svg(
            R"svg(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="100" height="40">)svg"
            R"svg(<defs><rect id="shape" x="10" y="10" width="20" height="20"/></defs>)svg"
            R"svg(<mask id="m" fill="white"><g transform="translate(5,0)"><use xlink:href="#shape"/></g></mask>)svg"
            R"svg(<g mask="url(#m)"><rect width="100" height="40" fill="black"/></g></svg>)svg");
        CHECK_EQ(badge.size(), std::size_t{1});
        CHECK(shape_at(badge, 0).clip.has_value());
        CHECK(near(shape_at(badge, 0).bounds(), 15, 10, 20, 20)); // the full rect, within the mask's shape
    }

    SECTION("Text: a label with its font and fill");
    {
        const auto picture = sc::drawing::from_svg(
            R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100">)svg"
            R"svg(<text x="10" y="40" font-family="'DejaVu Sans', sans-serif" font-size="20" font-weight="bold" fill="navy">)svg"
            R"svg(Hello <tspan font-style="italic">there</tspan></text>)svg"
            R"svg(<g transform="scale(2)"><text x="5" y="30" style="font-size: 10px">Twice</text></g></svg>)svg");
        CHECK_EQ(picture.size(), std::size_t{2});
        const auto &hello = std::get<sc::label>(picture.items().at(0));
        CHECK_EQ(hello.text.content, std::string{"Hello there"});
        CHECK_EQ(hello.text.font.family, std::string{"DejaVu Sans"});
        CHECK_EQ(hello.text.font.size, 20.0);
        CHECK_EQ(hello.text.font.weight, 700);
        CHECK(hello.fill.colour() == sc::color{"navy"});
        CHECK(hello.text.position == (sc::point{10, 40}));
        CHECK_LT(0.0, hello.bounds().width());
        CHECK_LT(hello.bounds().top(), 40.0); // the glyphs rise above the baseline
        const auto &twice = std::get<sc::label>(picture.items().at(1));
        CHECK_EQ(twice.text.font.size, 20.0); // scaled with its group
        CHECK(twice.text.position == (sc::point{10, 60}));
    }

    SECTION("Writing SVG and reading it back");
    {
        sc::drawing picture{{200, 100}};
        picture.add(sc::rect{10, 10, 50, 20}, {sc::stroke{sc::color::Black, 2}, sc::color{"gold"}});
        picture.add(sc::rounded_rect{sc::rect{70, 10, 50, 20}, 5}, sc::appearance::filled(sc::color{0, 0, 1, 0.5}));
        picture.add(sc::rotated_rect{{150, 25}, {40, 10}, 20});
        picture.add(sc::circle{{30, 70}, 15}, sc::appearance::filled(sc::fill{sc::color::Red, sc::fill_rule::even_odd}));
        picture.add(sc::line{{0, 99}, {200, 99}}, sc::color::Green);
        picture.add(sc::polygon{{100, 60}, {120, 90}, {80, 90}});
        sc::path wave;
        wave.move_to({130, 60}).cubic_to({140, 50}, {150, 70}, {160, 60});
        picture.add(wave, sc::color::Blue, sc::path{}.move_to({0, 0}).line_to({200, 0}).line_to({200, 100}).close());
        picture.add(sc::text{"A & B <c>", {10, 50}, {"serif", 12, 400, true}}, sc::color::Black);
        const auto svg = picture.to_svg();
        CHECK(svg.find("A &amp; B &lt;c&gt;") != std::string::npos);
        CHECK(svg.find("<clipPath id=\"clip1\">") != std::string::npos);

        const auto back = sc::drawing::from_svg(svg);
        CHECK(back.page() == picture.page());
        CHECK_EQ(back.size(), picture.size());
        for (std::size_t i = 0; i < 7; ++i) {
            const auto &mine = shape_at(picture, i), &read = shape_at(back, i);
            CHECK_EQ(read.shape.value().index(), mine.shape.value().index()); // the same kind
            CHECK(near(read.bounds(), mine.bounds().left(), mine.bounds().top(), mine.bounds().width(), mine.bounds().height(), 1e-6));
            CHECK(read.appearance.fill() == mine.appearance.fill());
            CHECK(read.appearance.stroke() == mine.appearance.stroke());
        }
        CHECK(shape_at(back, 6).clip.has_value());
        CHECK(std::get<sc::label>(back.items().at(7)) == std::get<sc::label>(picture.items().at(7)));

        const auto file = std::filesystem::temp_directory_path() / "sc-graphics-drawing.svg";
        picture.save_svg(file);
        CHECK_EQ(sc::drawing::load_svg(file).size(), picture.size());
        std::filesystem::remove(file);
    }

    SECTION("Bounds, transforms and flipping across the page");
    {
        sc::drawing page{{595, 842}};
        CHECK(near(page.bounds(), 0, 0, 0, 0));
        page.add(sc::rect{50, 50, 100, 40}, sc::stroke{sc::color::Black, 2});
        page.add(sc::circle{{300, 80}, 20});
        CHECK(near(page.bounds(), 50, 50, 270, 50));
        page.flip_horizontal(); // across the page: the rect to the right margin
        CHECK(holds<sc::rect>(shape_at(page, 0)));
        CHECK(near(shape_at(page, 0).bounds(), 445, 50, 100, 40));
        CHECK(near(shape_at(page, 1).bounds(), 275, 60, 40, 40));
        page.apply(sc::transform::scale(0.5));
        CHECK_EQ(shape_at(page, 0).appearance.stroke().width(), 1.0); // outlines scale with it
        CHECK(page.page() == (sc::size{595, 842}));                     // the page stays

        sc::drawing words{{200, 100}};
        words.add(sc::text{"left", {10, 50}, {"sans-serif", 10}});
        const auto before = std::get<sc::label>(words.items()[0]).bounds();
        words.flip_horizontal();
        const auto after = std::get<sc::label>(words.items()[0]).bounds();
        CHECK_NEAR(after.right(), 200 - before.left(), 1e-6); // its box mirrored; the text itself reads the same way
        CHECK_NEAR(after.width(), before.width(), 1e-6);
        words.apply(sc::transform::scale(2));
        CHECK_EQ(std::get<sc::label>(words.items()[0]).text.font.size, 20.0);
    }

    SECTION("Drawing on a drawing records it");
    {
        sc::drawing record{{100, 100}};
        record.draw(sc::rect{10, 10, 20, 20}, sc::color::Red); // recorded as the canvas paints it: a polygon
        record.draw(sc::circle{{50, 50}, 5});
        record.draw(sc::text{"hi", {10, 90}});
        CHECK_EQ(record.size(), std::size_t{3});
        CHECK(holds<sc::polygon>(shape_at(record, 0)));
        CHECK(holds<sc::circle>(shape_at(record, 1)));
        CHECK(std::holds_alternative<sc::label>(record.items()[2]));
        CHECK_LT(0.0, record.text_width(sc::text{"hi"}));

        sc::drawing copy;
        copy.draw(record); // replayed at its own coordinates
        CHECK_EQ(copy.size(), record.size());
        sc::drawing fitted;
        fitted.draw(record, sc::rect{0, 0, 50, 50}); // half the size
        CHECK(near(shape_at(fitted, 0).bounds(), 5, 5, 10, 10, 1e-9));
    }

    SECTION("Files and mistakes");
    {
        const auto file = std::filesystem::temp_directory_path() / "sc-graphics-test.svg";
        std::ofstream{file} << R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="4"><rect width="4" height="4"/></svg>)";
        CHECK_EQ(sc::drawing::load_svg(file).size(), size_t{1});
        std::filesystem::remove(file);
        CHECK_THROWS_AS(sc::drawing::load_svg(file), std::runtime_error);
        CHECK_THROWS_AS(sc::drawing::from_svg("not svg at all"), std::runtime_error);
        CHECK_THROWS_AS(sc::drawing::from_svg("<html></html>"), std::runtime_error);
    }

    TEST_SUMMARY();
}

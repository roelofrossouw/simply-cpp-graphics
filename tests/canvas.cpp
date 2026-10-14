// sc::canvas turns every draw() into a few primitives; a canvas that records them shows which, with
// what, and what it leaves out.
#include <canvas.h>

#include "sc_test.h"

#include <cmath>
#include <string>
#include <vector>

namespace {
    struct recording_canvas : sc::canvas {
        std::vector<std::string> calls;
        sc::polygon last_polygon;
        sc::color last_stroke, last_fill;
        double last_width = -1;

    protected:
        void paint_polygon(const sc::polygon &shape, const sc::color &stroke, const sc::color &fill, const double width) override {
            calls.push_back("polygon " + std::to_string(shape.size()));
            last_polygon = shape;
            last_stroke = stroke;
            last_fill = fill;
            last_width = width;
        }

        void paint_circle(const sc::circle &shape, const sc::color &stroke, const sc::color &fill, const double width) override {
            calls.push_back("circle " + std::to_string(static_cast<int>(shape.radius())));
            last_stroke = stroke;
            last_fill = fill;
            last_width = width;
        }

        void paint_line(const sc::point &, const sc::point &, const sc::color &, const double width) override {
            calls.push_back("line " + std::to_string(static_cast<int>(width)));
        }

        void paint_text(const sc::text &sample, const sc::color &) override {
            calls.push_back("text " + sample.content + " " + sample.font.family);
        }

        // Half an em per character, x-height half the size, capitals 0.7 of it.
        double measure_width(const sc::text &sample) override { return sample.content.size() * sample.font.size / 2; }

        double measure_x_height(const sc::font &font) override { return font.size / 2; }

        double measure_cap_height(const sc::font &font) override { return font.size * 0.7; }

        void push_clip(const sc::path &shape) override { calls.push_back("clip " + std::to_string(shape.steps().size())); }

        void pop_clip() override { calls.push_back("unclip"); }
    };
}

int main() {
    SECTION("Shapes become primitives");
    {
        recording_canvas canvas;
        canvas.draw(sc::rect{10, 20, 30, 40});
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 4"});
        CHECK_EQ(canvas.last_polygon[2], (sc::point{40, 60}));
        CHECK(canvas.last_stroke == sc::color::Black); // the defaults: black outline, no fill, 1 wide
        CHECK(canvas.last_fill == sc::color::Transparent);
        CHECK_EQ(canvas.last_width, 1.0);

        canvas.draw(sc::rect_i{1, 2, 3, 4}, sc::color::Red, sc::color::Blue, 2);
        CHECK_EQ(canvas.last_polygon[0], (sc::point{1, 2}));
        CHECK(canvas.last_fill == sc::color::Blue);
        CHECK_EQ(canvas.last_width, 2.0);

        canvas.draw(sc::rotated_rect{{0, 0}, {2, 2}, 45});
        CHECK_NEAR(canvas.last_polygon[0].y(), -std::sqrt(2.0), 1e-12); // exact corners

        canvas.draw(sc::rotated_rect_i{{10, 10}, {4, 2}, 90}); // every flavour of each shape
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 4"});
        CHECK_NEAR(canvas.last_polygon[0].x(), 11.0, 1e-12);

        canvas.draw(sc::polygon_i{{0, 0}, {5, 0}, {5, 5}});
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 3"});

        canvas.draw(sc::circle{{5, 5}, 3});
        canvas.draw(sc::circle_i{{5, 5}, 4});
        CHECK_EQ(canvas.calls.back(), std::string{"circle 4"});

        canvas.draw(sc::text{"Hello", {10, 20}, {"serif", 14}});
        CHECK_EQ(canvas.calls.back(), std::string{"text Hello serif"});
        canvas.draw(sc::line{{0, 0}, {10, 10}}, sc::color::Black, 3);
        CHECK_EQ(canvas.calls.back(), std::string{"line 3"});
        canvas.draw(sc::line_i{{0, 0}, {10, 10}});
        CHECK_EQ(canvas.calls.back(), std::string{"line 1"});
        canvas.draw(std::vector<sc::line>{{{0, 0}, {1, 1}}, {{1, 1}, {2, 0}}}, sc::color::Red, 2);
        CHECK_EQ(canvas.calls.back(), std::string{"line 2"});

        canvas.draw(std::vector<sc::circle>{{{1, 1}, 1}, {{2, 2}, 2}}, sc::color::Green);
        CHECK_EQ(canvas.calls.back(), std::string{"circle 2"});
    }

    SECTION("Rounded corners");
    {
        recording_canvas canvas;
        canvas.draw(sc::rect{0, 0, 100, 50}, sc::color::Black, sc::color::Transparent, 1, 10);
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 36"}); // four arcs of 8 segments
        const auto bounds = static_cast<sc::rect>(canvas.last_polygon);
        CHECK_NEAR(bounds.left(), 0.0, 1e-9);
        CHECK_NEAR(bounds.bottom(), 50.0, 1e-9);
        CHECK_NEAR(canvas.last_polygon[0].x(), 0.0, 1e-9); // starts on the left edge, 10 down
        CHECK_NEAR(canvas.last_polygon[0].y(), 10.0, 1e-9);
        canvas.draw(sc::rect{0, 0, 100, 50}, sc::color::Black, sc::color::Transparent, 1, 500); // clamped to 25
        const auto clamped = static_cast<sc::rect>(canvas.last_polygon);
        CHECK_NEAR(clamped.height(), 50.0, 1e-9);
    }

    SECTION("What draws nothing");
    {
        recording_canvas canvas;
        canvas.draw(sc::rect{0, 0, 10, 10}, sc::color::Transparent);                 // no stroke, no fill
        canvas.draw(sc::rect{0, 0, 10, 10}, sc::color::Black, sc::color::Transparent, 0); // no width
        canvas.draw(sc::polygon{{1, 1}});
        canvas.draw(sc::circle{{1, 1}, 0});
        canvas.draw(sc::text{"", {0, 0}});
        canvas.draw(sc::text{"x", {0, 0}}, sc::color::Transparent);
        canvas.draw(sc::line{{0, 0}, {1, 1}}, sc::color::Black, 0);
        CHECK(canvas.calls.empty());

        canvas.draw(sc::rect{0, 0, 10, 10}, sc::color::Black, sc::color::Red, 0); // fill only
        CHECK(canvas.last_stroke == sc::color::Transparent);
        CHECK_EQ(canvas.last_width, 0.0);
        CHECK(canvas.last_fill == sc::color::Red);
    }

    SECTION("Measuring text");
    {
        recording_canvas canvas;
        CHECK_EQ(canvas.text_width(sc::text{"abcd", {5, 5}, {"serif", 10}}), 20.0);
        CHECK_EQ(canvas.text_width(sc::text{}), 0.0);                           // nothing to measure
        CHECK_EQ(canvas.text_width(sc::text{"abcd", {}, {"serif", 0}}), 0.0);
        CHECK_EQ(canvas.text_x_height(), 6.0);                                  // the default font, 12
        CHECK_NEAR(canvas.text_cap_height({"serif", 20}), 14.0, 1e-12);
    }

    SECTION("Paths and SVG");
    {
        recording_canvas canvas;
        sc::path triangle;
        triangle.move_to({0, 0}).line_to({10, 0}).line_to({5, 8}).close();
        canvas.draw(triangle, sc::color::Black, sc::color::Red);
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 3"}); // by default, a closed subpath is a polygon

        sc::path open;
        open.move_to({0, 0}).cubic_to({0, 10}, {10, 10}, {10, 0});
        canvas.calls.clear();
        canvas.draw(open, sc::color::Black, sc::color::Transparent, 2);
        CHECK_LT(size_t{3}, canvas.calls.size()); // an open curve: short lines
        CHECK_EQ(canvas.calls.back(), std::string{"line 2"});

        const sc::svg badge{R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100">)svg"
                            R"svg(<rect width="200" height="100" fill="#ff0000"/></svg>)svg"};
        canvas.calls.clear();
        canvas.draw(badge, sc::rect{0, 0, 100, 100}); // half size, centred vertically
        CHECK_EQ(canvas.calls.size(), size_t{1});
        const auto bounds = static_cast<sc::rect>(canvas.last_polygon);
        CHECK_NEAR(bounds.left(), 0.0, 1e-9);
        CHECK_NEAR(bounds.top(), 25.0, 1e-9);
        CHECK_NEAR(bounds.width(), 100.0, 1e-9);
        CHECK_NEAR(bounds.height(), 50.0, 1e-9);
        CHECK(canvas.last_fill == sc::color::Red);

        const sc::svg clipped{R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10">)svg"
                              R"svg(<defs><clipPath id="c"><rect width="5" height="5"/></clipPath></defs>)svg"
                              R"svg(<rect clip-path="url(#c)" width="10" height="10" fill="#000000"/></svg>)svg"};
        canvas.calls.clear();
        canvas.draw(clipped, sc::rect{0, 0, 10, 10});
        CHECK(!canvas.calls.empty() && canvas.calls.front().starts_with("clip"));
        CHECK(!canvas.calls.empty() && canvas.calls.back() == "unclip");
    }

    SECTION("Fonts");
    {
        const sc::font plain;
        CHECK_EQ(plain.family, std::string{"sans-serif"});
        CHECK_EQ(plain.size, 12.0);
        CHECK_EQ(plain.weight, 400);
        CHECK_EQ(sc::font::bold("serif", 20).weight, 700);
        const sc::font small{"serif"}, large{"serif", 14};
        CHECK(small < large);
    }
    TEST_SUMMARY();
}
